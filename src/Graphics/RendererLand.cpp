/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The renderer is one of the locator's implementations
#define LOCATOR_IMPLEMENTATIONS

#include <cstdint>

#include <algorithm>
#include <array>
#include <initializer_list>
#include <limits>
#include <memory>
#include <optional>
#include <span>
#include <utility>
#include <vector>

#include <LNDFile.h>
#include <bgfx/bgfx.h>
#include <glm/gtc/type_ptr.hpp>

#include "3D/ChimneySmoke.h"
#include "3D/Clouds.h"
#include "3D/InfluenceCircle.h"
#include "3D/L3DMesh.h"
#include "3D/LandBlock.h"
#include "3D/LandColourStamps.h"
#include "3D/LandIslandInterface.h"
#include "3D/LandLightFrame.h"
#include "3D/LandLightTable.h"
#include "3D/Lightning.h"
#include "3D/Rain.h"
#include "3D/SnowCover.h"
#include "3D/StormClouds.h"
#include "3D/TempleDoors.h"
#include "3D/TempleInteriorInterface.h"
#include "3D/TempleMap.h"
#include "Camera/Camera.h"
#include "Common/MainThreadQueue.h"
#include "ECS/BoatLaunchRules.h"
#include "ECS/Components/Mesh.h"
#include "ECS/Components/Sky.h"
#include "ECS/Components/Sprite.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/Weather.h"
#include "ECS/PosedModel.h"
#include "ECS/Registry.h"
#include "ECS/Systems/AlignmentSystemInterface.h"
#include "ECS/Systems/HandSystemInterface.h"
#include "ECS/Systems/LandDetailSystemInterface.h"
#include "ECS/Systems/LandShadeSystemInterface.h"
#include "ECS/Systems/ObjectDrawListSystemInterface.h"
#include "ECS/Systems/RenderingSystemInterface.h"
#include "ECS/Systems/SkyClockSystemInterface.h"
#include "ECS/Systems/SkySystemInterface.h"
#include "ECS/Systems/SnowSystemInterface.h"
#include "ECS/Systems/TimeSystemInterface.h"
#include "ECS/Systems/WeatherSystemInterface.h"
#include "EngineConfig.h"
#include "Game.h"
#include "Graphics/BoneBudget.h"
#include "Graphics/CreatureReflection.h"
#include "Graphics/DebugLines.h"
#include "Graphics/FrameBuffer.h"
#include "Graphics/GraphicsHandleBgfx.h"
#include "Graphics/GroundBlobs.h"
#include "Graphics/HandLight.h"
#include "Graphics/HandReflection.h"
#include "Graphics/HandWaterGlow.h"
#include "Graphics/LandFootprints.h"
#include "Graphics/LandShadowBake.h"
#include "Graphics/Mesh.h"
#include "Graphics/MeshDetail.h"
#include "Graphics/ModelLight.h"
#include "Graphics/Moon.h"
#include "Graphics/ObjectShadows.h"
#include "Graphics/RendererDetail.h"
#include "Graphics/SeaRows.h"
#include "Graphics/ShaderManager.h"
#include "Graphics/Sun.h"
#include "Graphics/ViewFrustum.h"
#include "Locator.h"
#include "Profiler.h"
#include "Renderer.h"
#include "Resources/Rereads.h"
#include "Resources/ResourceManager.h"
#include "Resources/ResourcesInterface.h"
#include "Scenery/ObjectDrawList.h"
#include "Visual/DetailLevel.h"
#include "entt/locator/locator.hpp"

using namespace openblack;
using namespace openblack::graphics;
using namespace openblack::graphics::detail;
using namespace openblack::ecs::systems;

namespace
{
/// Whether a land block is in view. The draw list keeps the answers of the frame, so the land shade, the land detail and
/// the list make the test one time for each block.
bool SharedBlockInView(const glm::mat4& viewProjection, float nearPlane, const object_draw_list::Block& block)
{
	if (Locator::objectDrawListSystem::has_value())
	{
		return Locator::objectDrawListSystem::value().BlockInView(viewProjection, nearPlane, block);
	}
	return object_draw_list::BlockInView(viewProjection, nearPlane, block);
}
} // namespace

namespace openblack::graphics::detail
{
glm::vec4 LandTexelsUniform(int blockTexels)
{
	const auto& sampling = land_detail::CoarseSamplingFor(blockTexels);
	const auto texels = static_cast<float>(sampling.blockTexels);
	return {texels, static_cast<float>(sampling.step), sampling.drawnTexels, texels / 255.0f};
}

const Texture2D* SnowDepth(std::unique_ptr<Texture2D>& texture, std::optional<uint32_t>& revision)
{
	if (!Locator::snowSystem::has_value())
	{
		return nullptr;
	}
	const auto& snow = Locator::snowSystem::value();
	if (!texture)
	{
		texture = std::make_unique<Texture2D>("SnowDepth");
		texture->Create(snow_cover::k_GridSize, snow_cover::k_GridSize, 1, TextureFormat::R32F, Wrapping::ClampEdge,
		                Filter::Nearest, nullptr);
		// After a restore of the graphics device the depths go to the texture again
		texture->SetTexelsGivenAgain();
		revision.reset();
	}
	if (revision != snow.GetRevision())
	{
		const auto depths = snow.GetDepths();
		texture->Update(depths.data(), static_cast<uint32_t>(depths.size_bytes()));
		revision = snow.GetRevision();
	}
	return texture.get();
}
} // namespace openblack::graphics::detail

void Renderer::PaintLandFootprints(const DrawSceneDesc& drawDesc) const
{
	auto section = Locator::profiler::value().BeginScoped(Profiler::Stage::FootprintPass);
	if (!drawDesc.drawIsland)
	{
		return;
	}
	// The footprints of the buildings, the features and the rivers are painted into the blocks' textures when they
	// change, as the game paints them
	if (!_landFootprints)
	{
		_landFootprints = std::make_unique<LandFootprints>();
	}
	_landFootprints->Update();
	// The game paints all of a block's texture again when its footprints change, so the block's snow is painted again too
	const auto repainted = _landFootprints->TakeRepainted();
	if (repainted.empty() || !Locator::terrainSystem::has_value())
	{
		return;
	}
	const auto& blocks = Locator::terrainSystem::value().GetBlocks();
	for (const auto block : repainted)
	{
		if (block < blocks.size())
		{
			_paintedSnow.Request(snow_cover::BlockIndex(blocks[block].GetBlockPosition()));
		}
	}
}

const Texture2D* Renderer::PaintLandSnow(std::span<const glm::ivec2> drawn) const
{
	if (!Locator::snowSystem::has_value())
	{
		return nullptr;
	}
	auto& snow = Locator::snowSystem::value();
	const auto requests = snow.TakePaintRequests();
	if (requests.all())
	{
		// Each block paints its snow again. On a new land, this paint is also the first paint of the block.
		_paintedSnow.RequestAll();
		_snowRepaints.set();
	}
	else if (requests.any())
	{
		for (uint16_t block = 0; block < snow_cover::k_Blocks; ++block)
		{
			if (requests.test(block))
			{
				_paintedSnow.Request(block);
				_snowRepaints.set(block);
			}
		}
	}
	// A paint for a change of the snow also paints the block's coarse texture
	for (const auto block : drawn)
	{
		if (block.x < 0 || block.y < 0 || block.x >= snow_cover::k_BlocksPerSide || block.y >= snow_cover::k_BlocksPerSide)
		{
			continue;
		}
		const auto index = snow_cover::BlockIndex(block);
		if (_paintedSnow.Waits(index) && _snowRepaints.test(index))
		{
			_snowRepaints.reset(index);
			_snowPaintedBlocks.push_back(block);
		}
	}
	const bool painted = _paintedSnow.PaintDrawn(drawn, snow.GetDepths());
	if (!_paintedSnowTexture)
	{
		_paintedSnowTexture = std::make_unique<Texture2D>("PaintedSnow");
		_paintedSnowTexture->Create(snow_cover::k_PaintedSide, snow_cover::k_PaintedSide, 1, TextureFormat::R32F,
		                            Wrapping::ClampEdge, Filter::Nearest, nullptr);
		// After a restore of the graphics device the painted snow goes to the texture again
		_paintedSnowTexture->SetTexelsGivenAgain();
	}
	else if (!painted)
	{
		return _paintedSnowTexture.get();
	}
	const auto texels = _paintedSnow.GetPainted();
	_paintedSnowTexture->Update(texels.data(), static_cast<uint32_t>(texels.size_bytes()));
	return _paintedSnowTexture.get();
}

void Renderer::SetSeaUniforms(const ShaderProgram& waterShader, const Camera& camera) const
{
	const auto& config = Locator::config::value();
	const float waterTiling = config.detail.waterTiling;
	const bool still = waterTiling == 0.0f;

	glm::vec4 u_seaParams {still ? sea_rows::k_StillPeriod : sea_rows::Period(waterTiling), 0.0f, 0.0f, 0.0f};
	glm::vec4 u_seaRows {0.0f};
	glm::vec4 u_seaMode {still ? 1.0f : 0.0f, 0.0f, 0.0f, 0.0f};
	auto driftDraw = still ? sea_rows::DriftDraw::Still : sea_rows::DriftDraw::NoRows;
	if (!still)
	{
		// The rows ripple along the camera's view across the ground, 0.9 units at most
		const auto forward = camera.GetForward();
		const glm::vec2 forwardAlongGround {forward.x, forward.z};
		if (forwardAlongGround != glm::vec2(0.0f))
		{
			const auto ripple = 0.9f * glm::normalize(forwardAlongGround);
			u_seaParams.z = ripple.x;
			u_seaParams.w = ripple.y;
		}
		const auto* stats = bgfx::getStats();
		const glm::vec2 viewport {stats->width, stats->height};
		u_seaMode.z = sea_rows::RowTextureSpan(viewport.x);
		if (const auto range = sea_rows::ComputeScreenRange(camera.GetViewProjectionMatrix(), viewport, config.cameraNearClip))
		{
			// The ripple steps on with the frames the game's time moves on in, at the screen refresh's pace
			// A warm-up does not step it
			const auto& time = Locator::time::value();
			if (!_warmingUp)
			{
				_seaRipple = sea_rows::AdvanceRipple(_seaRipple, time.GetFrameRealTime(), time.GetFrameGameTime().count() != 0);
			}
			driftDraw = sea_rows::DriftDraw::Rows;
			const auto rows = sea_rows::MakeRows(*range);
			u_seaRows = {static_cast<float>(rows.first), static_cast<float>(rows.count), rows.inverseDepth, rows.inverseStep};
			u_seaMode.y = rows.softTop ? 1.0f : 0.0f;
		}
		else
		{
			// No rows: only what lies under the sea shows
			u_seaRows = {viewport.y, 0.0f, 0.0f, 0.0f};
		}
		u_seaParams.y = static_cast<float>(_seaRipple.step);
	}
	// The texture drifts with the ambient wind by the frame's game time. A warm-up does not move it.
	if (!_warmingUp)
	{
		const auto wind =
		    Locator::weatherSystem::has_value() ? Locator::weatherSystem::value().GetAmbientWind() : glm::vec2(0.0f);
		_seaDrift = sea_rows::AdvanceDrift(_seaDrift, wind, Locator::time::value().GetFrameGameTime(),
		                                   sea_rows::Period(waterTiling), driftDraw);
	}
	const glm::vec4 u_seaDrift {_seaDrift.rows, _seaDrift.still};

	// The sea is coloured by the land's light at full luminosity
	glm::vec4 u_seaColour {1.0f};
	if (_landLightTable)
	{
		const auto texel = _landLightTable->GetTexels().back();
		u_seaColour = glm::vec4(static_cast<float>(texel & 0xFFu), static_cast<float>((texel >> 8) & 0xFFu),
		                        static_cast<float>((texel >> 16) & 0xFFu), 255.0f) /
		              255.0f;
	}
	// The eye of the view the sea is drawn with, a shake included, so that each pixel's line of sight meets the sea where
	// the view shows it
	const glm::vec4 u_seaCamera {glm::vec3(glm::inverse(camera.GetViewMatrix(Camera::Interpolation::Current))[3]), 0.0f};
	waterShader.SetUniformValue("u_seaParams", &u_seaParams);
	waterShader.SetUniformValue("u_seaRows", &u_seaRows);
	waterShader.SetUniformValue("u_seaMode", &u_seaMode);
	waterShader.SetUniformValue("u_seaColour", &u_seaColour);
	waterShader.SetUniformValue("u_seaCamera", &u_seaCamera);
	waterShader.SetUniformValue("u_seaDrift", &u_seaDrift);
}

void Renderer::DrawHandWaterGlow(const DrawSceneDesc& desc) const
{
	// The glow lies flat. It uses its own alpha and adds its colour to the colour under the sea.
	constexpr uint64_t k_State =
	    BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_A | BGFX_STATE_BLEND_FUNC(BGFX_STATE_BLEND_SRC_ALPHA, BGFX_STATE_BLEND_ONE);
	const auto* spriteShader = _shaderManager->GetShader("Sprite");
	// The warm-up draws the glow with no size, so that the renderer has its pipeline before the first night
	if (_warmingUp)
	{
		bgfx::setTransform(glm::value_ptr(glm::mat4(0.0f)));
		_plane->GetVertexBuffer().Bind();
		bgfx::setState(k_State);
		spriteShader->Submit(static_cast<bgfx::ViewId>(desc.viewId));
		return;
	}
	if (!_landLightTable || !Locator::handSystem::has_value() || !Locator::terrainSystem::has_value())
	{
		return;
	}
	const float strength = HandLight::GetStrength(_landLightTable->GetLandColour());
	const bool inTemple = Locator::temple::has_value() && Locator::temple::value().Active();
	if (!hand_water_glow::Shows(inTemple, strength))
	{
		return;
	}
	const auto handEntity =
	    Locator::handSystem::value().GetOwnHands()[static_cast<size_t>(ecs::systems::HandSystemInterface::Side::Left)];
	const auto* transform = Locator::entitiesRegistry::value().TryGet<ecs::components::Transform>(handEntity);
	if (transform == nullptr)
	{
		return;
	}
	const glm::vec2 centre {transform->position.x, transform->position.z};
	const auto& island = Locator::terrainSystem::value();
	const auto altitudeAt = [&island](int x, int z) -> std::optional<uint8_t> {
		if (x < 0 || z < 0 || x >= LandIslandInterface::k_MapCellsPerSide || z >= LandIslandInterface::k_MapCellsPerSide)
		{
			return std::nullopt;
		}
		const auto* cell = island.FindCell({static_cast<uint16_t>(x), static_cast<uint16_t>(z)});
		return cell != nullptr ? std::optional<uint8_t>(cell->altitude) : std::nullopt;
	};
	if (!hand_water_glow::NearLowLand(centre, altitudeAt))
	{
		return;
	}

	const auto& textures = Locator::resources::value().GetTextures();
	const auto atmos = entt::hashed_string("raw/ATMOS");
	const auto atmosAlpha = entt::hashed_string("raw/ATMOSA");
	if (!textures.Contains(atmos.value()) || !textures.Contains(atmosAlpha.value()))
	{
		return;
	}
	const auto colour = hand_water_glow::Colour(_landLightTable->GetWarmColour(), strength);
	const glm::vec4 u_tint = glm::vec4(static_cast<float>((colour >> 16) & 0xFFu), static_cast<float>((colour >> 8) & 0xFFu),
	                                   static_cast<float>(colour & 0xFFu), static_cast<float>(colour >> 24)) /
	                         255.0f;
	// The sprite's x runs along the world's x and its y against the world's z, so the texture's corners fall as the
	// game lays them
	const glm::vec2 uvSize = hand_water_glow::k_UvMaximum - hand_water_glow::k_UvMinimum;
	const glm::vec4 u_sampleRect {uvSize, hand_water_glow::k_UvMinimum};
	// Lying flat, with its own alpha, adding to what is under the sea
	const glm::vec4 u_spriteParams {0.0f, 1.0f, 1.0f, 0.0f};
	glm::mat4 model(0.0f);
	model[0] = glm::vec4(hand_water_glow::k_HalfSize, 0.0f, 0.0f, 0.0f);
	model[1] = glm::vec4(0.0f, 0.0f, -hand_water_glow::k_HalfSize, 0.0f);
	model[2] = glm::vec4(0.0f, 1.0f, 0.0f, 0.0f);
	model[3] = glm::vec4(centre.x, 0.0f, centre.y, 1.0f);

	bgfx::setTransform(glm::value_ptr(model));
	spriteShader->SetUniformValue("u_sampleRect", glm::value_ptr(u_sampleRect));
	spriteShader->SetUniformValue("u_spriteParams", glm::value_ptr(u_spriteParams));
	spriteShader->SetUniformValue("u_tint", glm::value_ptr(u_tint));
	spriteShader->SetTextureSampler("s_diffuse", 0, *textures.Handle(atmos));
	spriteShader->SetTextureSampler("s_alpha", 1, *textures.Handle(atmosAlpha));
	_plane->GetVertexBuffer().Bind();
	bgfx::setState(k_State);
	spriteShader->Submit(static_cast<bgfx::ViewId>(desc.viewId));
}

namespace
{
/// Whether a target is made and sized to the land's cell corners
bool FitsLand(const std::unique_ptr<FrameBuffer>& frameBuffer, glm::u16vec2 size)
{
	uint16_t width = 0;
	uint16_t height = 0;
	if (frameBuffer)
	{
		frameBuffer->GetSize(width, height);
	}
	return frameBuffer && width == size.x && height == size.y;
}
} // namespace

namespace openblack::graphics::detail
{
bgfx::ViewId SetUpLandView(RenderPass pass, const FrameBuffer& frameBuffer, glm::u16vec2 size)
{
	const auto w = static_cast<float>(size.x);
	const auto h = static_cast<float>(size.y);
	const auto projection = bgfx::getCaps()->originBottomLeft ? glm::ortho(0.0f, w, 0.0f, h) : glm::ortho(0.0f, w, h, 0.0f);
	const glm::mat4 identity(1.0f);
	const auto viewId = static_cast<bgfx::ViewId>(pass);
	frameBuffer.Bind(pass);
	bgfx::setViewRect(viewId, 0, 0, size.x, size.y);
	bgfx::setViewMode(viewId, bgfx::ViewMode::Sequential);
	bgfx::setViewTransform(viewId, glm::value_ptr(identity), glm::value_ptr(projection));
	bgfx::touch(viewId);
	return viewId;
}

void SubmitLandQuad(bgfx::ViewId viewId, const ShaderProgram& program, glm::vec2 from, glm::vec2 to, glm::vec2 uvFrom,
                    glm::vec2 uvTo, uint64_t state)
{
	struct Vertex
	{
		glm::vec2 position;
		glm::vec2 uv;
	};
	static const auto k_Layout = [] {
		bgfx::VertexLayout layout;
		layout.begin()
		    .add(bgfx::Attrib::Position, 2, bgfx::AttribType::Float)
		    .add(bgfx::Attrib::TexCoord0, 2, bgfx::AttribType::Float)
		    .end();
		return layout;
	}();
	if (bgfx::getAvailTransientVertexBuffer(6, k_Layout) < 6)
	{
		return;
	}
	bgfx::TransientVertexBuffer buffer;
	bgfx::allocTransientVertexBuffer(&buffer, 6, k_Layout);
	const auto vertices = std::span(reinterpret_cast<Vertex*>(buffer.data), 6);
	const std::array<Vertex, 4> corners {{
	    {{from.x, from.y}, {uvFrom.x, uvFrom.y}},
	    {{to.x, from.y}, {uvTo.x, uvFrom.y}},
	    {{to.x, to.y}, {uvTo.x, uvTo.y}},
	    {{from.x, to.y}, {uvFrom.x, uvTo.y}},
	}};
	constexpr std::array<size_t, 6> k_Indices {0, 1, 2, 2, 3, 0};
	for (size_t i = 0; i < vertices.size(); ++i)
	{
		vertices[i] = corners.at(k_Indices.at(i));
	}
	bgfx::setVertexBuffer(0, &buffer);
	bgfx::setState(state);
	program.Submit(viewId);
}
} // namespace openblack::graphics::detail

const Texture2D& Renderer::GetLandLuminosity() const
{
	if (_landLuminosity.at(_landLuminosityNow))
	{
		return *_landLuminosity.at(_landLuminosityNow);
	}
	return Locator::terrainSystem::value().GetLuminosityMap();
}

const Texture2D& Renderer::GetLandLuminosityBefore() const
{
	if (_landLuminosity.at(_landLuminosityNow ^ 1u))
	{
		return *_landLuminosity.at(_landLuminosityNow ^ 1u);
	}
	return GetLandLuminosity();
}

void Renderer::DrawLandLuminosityPass(const DrawSceneDesc& drawDesc) const
{
	if (!drawDesc.drawIsland || !Locator::terrainSystem::has_value() || !Locator::landShadeSystem::has_value())
	{
		return;
	}
	// What the frame shades the land with: the land light table is still the one of the frame before
	const auto& camera = *drawDesc.camera;
	const auto& detail = Locator::config::value().detail;
	ecs::systems::LandShadeFrame frame {
	    .viewProjection = camera.GetViewProjectionMatrix(),
	    .nearPlane = camera.GetNearClip(),
	    .landReflection = detail.landReflection,
	    .cloudShadows = detail.cloudShadows,
	    .skyClouds = detail.clouds,
	    .skyAlpha = clouds::Colour(Locator::alignmentSystem::value().GetSkyLand(), 0xFFFFFF) >> 24u,
	    .landColour = _landLightTable ? _landLightTable->GetLandColour() : 0xFFFFFFu,
	    .fullLightGreen =
	        _landLightTable ? static_cast<uint8_t>((_landLightTable->GetTexels().back() >> 8u) & 0xFFu) : uint8_t {0xFF},
	    .hand = std::nullopt,
	    .scriptHour = Locator::skyClockSystem::value().GetClock().GetScriptTime(),
	    .particleStamps = {},
	    .flashes = 0,
	};
	// The particles' light maps and shadows of the frame before, in the order they were drawn. A light map that cannot
	// stamp takes no place.
	frame.particleStamps.reserve(_landStamps.size());
	for (const auto& light : _landStamps)
	{
		const auto texels = ParticleLightMapTexels(light.bitmap, light.frame);
		if (texels.empty())
		{
			continue;
		}
		if (light.shadow)
		{
			frame.particleStamps.emplace_back(land_shade::ParticleShadow(light.centre, texels, light.pitch, light.strength));
		}
		else
		{
			frame.particleStamps.emplace_back(std::nullopt);
		}
	}
	// The hand's light while the hand shows
	if (drawDesc.drawHand && Locator::handSystem::has_value())
	{
		const auto hand =
		    Locator::handSystem::value().GetOwnHands()[static_cast<size_t>(ecs::systems::HandSystemInterface::Side::Left)];
		if (const auto* transform = drawDesc.entities.TryGet<ecs::components::Transform>(hand); transform != nullptr)
		{
			frame.hand = transform->position;
		}
	}
	// Each storm's flash of lightning takes a stamp while it shows
	drawDesc.entities.Each<const ecs::components::Storm>([&frame](const ecs::components::Storm& storm) {
		if (!storm.dead && storm.flash.active)
		{
			++frame.flashes;
		}
	});
	auto& shade = Locator::landShadeSystem::value();
	shade.Shade(frame);

	// The GPU's copy: a new land fills both textures, after that each frame changes the rows that this frame or the
	// frame before changed
	const auto map = shade.Map();
	if (map.size.x == 0 || map.size.y == 0 || map.texels.size() < static_cast<size_t>(map.size.x) * map.size.y)
	{
		return;
	}
	if (!_landLuminosity[0] || _landLuminosity[0]->GetResolution() != map.size)
	{
		for (auto& texture : _landLuminosity)
		{
			texture = std::make_unique<Texture2D>("LandLuminosity");
			texture->CreateWithinFrame(map.size.x, map.size.y, 1, TextureFormat::R8, Wrapping::Repeat, Filter::Linear, nullptr);
			// After a restore of the graphics device both textures are made again from the map
			texture->SetTexelsGivenAgain();
			texture->Update(map.texels.data(), static_cast<uint32_t>(map.texels.size()));
		}
		_landLuminosityNow = 0;
		_landLuminosityRowsBefore = {0, 0};
		return;
	}
	_landLuminosityNow ^= 1u;
	glm::u16vec2 rows {map.firstChangedRow, map.lastChangedRow};
	if (_landLuminosityRowsBefore.x < _landLuminosityRowsBefore.y)
	{
		rows = rows.x < rows.y
		           ? glm::u16vec2(std::min(rows.x, _landLuminosityRowsBefore.x), std::max(rows.y, _landLuminosityRowsBefore.y))
		           : _landLuminosityRowsBefore;
	}
	_landLuminosityRowsBefore = {map.firstChangedRow, map.lastChangedRow};
	if (rows.x < rows.y)
	{
		const auto first = static_cast<size_t>(rows.x) * map.size.x;
		const auto count = static_cast<uint16_t>(rows.y - rows.x);
		_landLuminosity.at(_landLuminosityNow)
		    ->UpdateLayerRegion(0, {0, rows.x}, {map.size.x, count}, map.texels.data() + first,
		                        static_cast<uint32_t>(static_cast<size_t>(count) * map.size.x));
	}
}

const Texture2D& Renderer::GetLandColour() const
{
	if (_landColourFrameBuffer)
	{
		return _landColourFrameBuffer->GetColorAttachment();
	}
	return Locator::terrainSystem::value().GetCellColourMap();
}

void Renderer::DrawLandColourPass(const DrawSceneDesc& drawDesc) const
{
	if (!drawDesc.drawIsland || !Locator::terrainSystem::has_value())
	{
		return;
	}
	const auto& island = Locator::terrainSystem::value();
	const auto& cellColours = island.GetCellColourMap();
	const auto size = cellColours.GetResolution();
	if (!FitsLand(_landColourFrameBuffer, size))
	{
		_landColourFrameBuffer = std::make_unique<FrameBuffer>("LandColour", size.x, size.y, graphics::TextureFormat::RGBA8);
	}
	const auto viewId = SetUpLandView(RenderPass::LandColour, *_landColourFrameBuffer, size);
	const auto& program = *_shaderManager->GetShader("LandColour");

	// The cells' colours as the land was laid
	const glm::vec4 u_copy {0.0f};
	program.SetTextureSampler("s_texture", 0, cellColours);
	program.SetUniformValue("u_landColourStamp", &u_copy);
	SubmitLandQuad(viewId, program, {0.0f, 0.0f}, glm::vec2(size), {0.0f, 0.0f}, {1.0f, 1.0f}, BGFX_STATE_WRITE_RGB);

	// The frame's stamps over them, in order, up to the game's most
	if (!_lightningGlowTexture)
	{
		const auto image = land_colour_stamps::LightningImage();
		constexpr auto k_Side = static_cast<uint16_t>(land_colour_stamps::k_LightningSide);
		_lightningGlowTexture = fromBgfx(bgfx::createTexture2D(k_Side, k_Side, false, 1, bgfx::TextureFormat::RGB8,
		                                                       BGFX_SAMPLER_U_CLAMP | BGFX_SAMPLER_V_CLAMP | BGFX_SAMPLER_POINT,
		                                                       bgfx::copy(image.data(), static_cast<uint32_t>(image.size()))));
		bgfx::setName(toBgfx(*_lightningGlowTexture), "Lightning Glow");
		// The renderer gives the picture again after a restore of the device, thus the device keeps no copy
		bgfx::setDeviceRestoreByApplication(toBgfx(*_lightningGlowTexture));
	}
	// An added stamp brightens the cells to at most white; another keeps the brighter colour
	const auto stampState = [](land_colour_stamps::Combine combine) {
		const auto equation =
		    combine == land_colour_stamps::Combine::Brighter ? BGFX_STATE_BLEND_EQUATION_MAX : BGFX_STATE_BLEND_EQUATION_ADD;
		return BGFX_STATE_WRITE_RGB | BGFX_STATE_BLEND_FUNC(BGFX_STATE_BLEND_ONE, BGFX_STATE_BLEND_ONE) |
		       BGFX_STATE_BLEND_EQUATION(equation);
	};
	// The warm-up draws a stamp of no size for each way to combine, so that the renderer has the pipelines before the
	// first light on the land
	if (_warmingUp)
	{
		const glm::vec4 u_noStamp {1.0f, 0.0f, 0.0f, 0.0f};
		for (const auto combine : {land_colour_stamps::Combine::Add, land_colour_stamps::Combine::Brighter})
		{
			program.SetTextureSampler("s_texture", 0, *_lightningGlowTexture);
			program.SetUniformValue("u_landColourStamp", &u_noStamp);
			SubmitLandQuad(viewId, program, {0.0f, 0.0f}, {0.0f, 0.0f}, {0.0f, 0.0f}, {0.0f, 0.0f}, stampState(combine));
		}
	}
	const glm::vec2 firstCell = island.GetExtent().minimum / LandIslandInterface::k_CellSize;
	size_t stamps = 0;
	// Each stamp takes a place in the game's list, also one with no strength
	const auto stamp = [&](TextureHandle image, int32_t side, glm::vec2 corner, uint8_t strength,
	                       land_colour_stamps::Combine combine) {
		if (stamps >= land_colour_stamps::k_MaxStamps)
		{
			return;
		}
		++stamps;
		if (strength == 0)
		{
			return;
		}
		const auto placement = land_colour_stamps::Place(corner);
		const auto origin = glm::vec2(placement.cell) - firstCell;
		constexpr float k_LastCell = static_cast<float>(LandIslandInterface::k_MapCellsPerSide - 1);
		const auto from = glm::max(origin, -firstCell);
		const auto to = glm::min(origin + static_cast<float>(side - 1), k_LastCell - firstCell);
		if (glm::any(glm::greaterThanEqual(from, to)))
		{
			return;
		}
		const glm::vec4 u_landColourStamp {1.0f, static_cast<float>(strength), static_cast<float>(side), 0.0f};
		const glm::vec4 u_landColourWeights {glm::vec2(placement.weight), 0.0f, 0.0f};
		program.SetTextureSampler("s_texture", 0, image);
		program.SetUniformValue("u_landColourStamp", &u_landColourStamp);
		program.SetUniformValue("u_landColourWeights", &u_landColourWeights);
		SubmitLandQuad(viewId, program, from, to, from - origin, to - origin, stampState(combine));
	};

	// The particles' light maps of the frame before light the ground under them. Their shadows darken the land's
	// luminosity (see DrawLandLuminosityPass) and only take a place here.
	for (const auto& light : _landStamps)
	{
		if (ParticleLightMapTexels(light.bitmap, light.frame).empty())
		{
			continue;
		}
		if (light.shadow)
		{
			stamps = std::min(stamps + 1, land_colour_stamps::k_MaxStamps);
			continue;
		}
		if (const auto* image = ParticleLightMap(light.bitmap, light.frame))
		{
			stamp(image->GetNativeHandle(), light.pitch, land_colour_stamps::CentredCorner(light.centre, light.pitch),
			      land_colour_stamps::Strength(light.strength), land_colour_stamps::Combine::Add);
		}
	}

	// Then each storm's lightning glows on the ground around where it struck, while its flash shows
	drawDesc.entities.Each<const ecs::components::Storm>([&](const ecs::components::Storm& storm) {
		if (storm.dead || !storm.flash.active)
		{
			return;
		}
		const auto strength = land_colour_stamps::Strength(lightning::GlowStrength(storm.flash));
		stamp(*_lightningGlowTexture, land_colour_stamps::k_LightningSide,
		      land_colour_stamps::CentredCorner(storm.flash.position, land_colour_stamps::k_LightningSide), strength,
		      land_colour_stamps::Combine::Add);
	});
}

void Renderer::KeepLightOfFrameBefore() const
{
	_before.haze.StartFrame(_landLightTable ? std::optional(_haze) : std::nullopt);
	_before.treeBrightness.StartFrame(_treeBrightnessMade ? std::optional(_treeBrightness) : std::nullopt);
	constexpr uint64_t k_Flags = BGFX_SAMPLER_U_CLAMP | BGFX_SAMPLER_V_CLAMP | BGFX_SAMPLER_POINT;
	// The table that the frame before built
	if (_landLightTable)
	{
		if (!_before.landLight.has_value())
		{
			_before.landLight =
			    fromBgfx(bgfx::createTexture2D(LandLightTable::k_Size, 1, false, 1, bgfx::TextureFormat::RGBA8, k_Flags));
			bgfx::setName(toBgfx(*_before.landLight), "Land Light Before");
			// Each frame fills the texture before it draws, thus the device keeps no copy after a restore
			bgfx::setDeviceRestoreByApplication(toBgfx(*_before.landLight));
		}
		const auto& texels = _landLightTable->GetTexels();
		bgfx::updateTexture2D(toBgfx(*_before.landLight), 0, 0, 0, 0, LandLightTable::k_Size, 1,
		                      bgfx::copy(texels.data(), static_cast<uint32_t>(texels.size() * sizeof(texels[0]))));
	}
	// The luminosity and the colours that the frame before drew, copied before the first pass of this frame draws over
	// them
	_before.texturesKept = false;
	if (!_landColourFrameBuffer)
	{
		return;
	}
	const auto copy = [](const FrameBuffer& from, std::optional<TextureHandle>& to, glm::u16vec2& size,
	                     bgfx::TextureFormat::Enum format, const char* name) {
		glm::u16vec2 fromSize {0};
		from.GetSize(fromSize.x, fromSize.y);
		if (!to.has_value() || size != fromSize)
		{
			if (to.has_value())
			{
				bgfx::destroy(toBgfx(*to));
			}
			to = fromBgfx(bgfx::createTexture2D(fromSize.x, fromSize.y, false, 1, format, BGFX_TEXTURE_BLIT_DST | k_Flags));
			bgfx::setName(toBgfx(*to), name);
			size = fromSize;
		}
		bgfx::TextureRegion dst;
		dst.init(toBgfx(*to));
		bgfx::TextureRegion src;
		src.init(toBgfx(from.GetColorAttachment().GetNativeHandle()));
		bgfx::blit(static_cast<bgfx::ViewId>(RenderPass::LandShade), dst, src);
	};
	copy(*_landColourFrameBuffer, _before.colour, _before.colourSize, bgfx::TextureFormat::RGBA8, "Land Colour Before");
	_before.texturesKept = true;
}

void Renderer::ReleaseLightOfFrameBefore() const
{
	for (auto* texture : {&_before.landLight, &_before.colour})
	{
		if (texture->has_value())
		{
			bgfx::destroy(toBgfx(**texture));
			texture->reset();
		}
	}
	_before.texturesKept = false;
}

TextureHandle Renderer::UpdateLandLight() const
{
	if (!_landLightTable)
	{
		_landLightTable = std::make_unique<LandLightTable>();
		_landLightTexture = fromBgfx(bgfx::createTexture2D(LandLightTable::k_Size, 1, false, 1, bgfx::TextureFormat::RGBA8,
		                                                   BGFX_SAMPLER_U_CLAMP | BGFX_SAMPLER_V_CLAMP | BGFX_SAMPLER_POINT));
		bgfx::setName(toBgfx(*_landLightTexture), "Land Light");
		// Each frame fills the texture before it draws, thus the device keeps no copy after a restore
		bgfx::setDeviceRestoreByApplication(toBgfx(*_landLightTexture));
	}
	const auto* dome =
	    Locator::skySystem::has_value()
	        ? Locator::entitiesRegistry::value().TryGet<const ecs::components::SkyDome>(Locator::skySystem::value().GetDome())
	        : nullptr;
	if (const auto& palettes = Locator::resources::value().GetLandLightPalettes();
	    dome != nullptr && palettes.Contains(LandLightPalette::k_Id.value()))
	{
		// The land's light as the sky worked it out for this frame: the clouds over the camera darken the land, and so
		// does a flash of lightning
		const auto [skyType, sky, overcast, flash] = dome->landLight;
		_landLightTable->Build(*palettes.Handle(LandLightPalette::k_Id.value()), skyType, sky, overcast, flash,
		                       FrameRampDivisor());
		const auto& haze = _landLightTable->GetHaze();
		const auto& detail = Locator::config::value().detail;
		// The lower detail levels draw no haze at all
		const float hazeOn = detail.fog ? 1.0f : 0.0f;
		_haze = {glm::vec4(haze.nearDistance, haze.farDistance, haze.k, hazeOn), glm::vec4(haze.colour, 0.0f)};
		_skyTint = sky_dome::TintOf({
		    .hazeColour = haze.colour,
		    .overcast = overcast,
		    .flash = flash,
		    .darkness = sky_dome::Darkness(sky),
		    .fog = detail.fog,
		    .weather = detail.weather,
		    .lowResolution = detail.lowResTextures,
		    .landColour = _landLightTable->GetLandColour(),
		    .skyType = skyType,
		});
		const auto& texels = _landLightTable->GetTexels();
		bgfx::updateTexture2D(toBgfx(*_landLightTexture), 0, 0, 0, 0, LandLightTable::k_Size, 1,
		                      bgfx::copy(texels.data(), static_cast<uint32_t>(texels.size() * sizeof(texels[0]))));
	}
	return *_landLightTexture;
}

glm::vec4 Renderer::GetModelLight() const
{
	// By day, or with no hand to carry it, the sun
	auto light = model_light::k_Sun;
	if (Locator::handSystem::has_value() && Locator::skyClockSystem::has_value() && Locator::camera::has_value())
	{
		const auto handEntity =
		    Locator::handSystem::value().GetOwnHands()[static_cast<size_t>(ecs::systems::HandSystemInterface::Side::Left)];
		if (const auto* transform = Locator::entitiesRegistry::value().TryGet<ecs::components::Transform>(handEntity);
		    transform != nullptr)
		{
			const auto ground =
			    Locator::terrainSystem::has_value()
			        ? Locator::terrainSystem::value().GetHeightAt(glm::vec2(transform->position.x, transform->position.z))
			        : 0.0f;
			const bool inTemple = Locator::temple::has_value() && Locator::temple::value().Active();
			light = model_light::FrameLight(transform->position, ground, Locator::camera::value().GetOrigin(),
			                                Locator::skyClockSystem::value().GetCurrentNightSkyType(),
			                                inTemple ? std::optional(Locator::temple::value().GetPosition()) : std::nullopt);
		}
	}
	return model_light::Uniform(light);
}

TextureHandle Renderer::GetLandLightTexture() const
{
	return _landLightTexture.value_or(_groundShadowData->GetNativeHandle());
}

void Renderer::DrawFilmReflections(const DrawSceneDesc& desc) const
{
	const auto& renderCtx = Locator::rendereringSystem::value().GetContext();
	if (desc.viewId != RenderPass::Reflection || renderCtx.filmReflections.empty())
	{
		return;
	}
	const auto& meshes = Locator::resources::value().GetMeshes();
	for (const auto& boat : renderCtx.filmReflections)
	{
		if (!meshes.Contains(boat.mesh))
		{
			continue;
		}
		L3DMeshSubmitDesc submitDesc = {};
		submitDesc.viewId = desc.viewId;
		submitDesc.program = _shaderManager->GetShader("Object");
		submitDesc.state = BGFX_STATE_WRITE_MASK | BGFX_STATE_DEPTH_TEST_GREATER | BGFX_STATE_MSAA;
		submitDesc.modelMatrices = &boat.model;
		submitDesc.matrixCount = 1;
		submitDesc.useMaterialBlending = true;
		submitDesc.useMaterialCulling = true;
		submitDesc.mirrored = desc.cullBack;
		submitDesc.detailLevel = boat.detailLevel;
		// In one flat colour of its own at every vertex, unshaded by the sun, and neither faded nor coloured by the
		// distance haze. The land's colour of the frame before at its place of the frame before is added over it, and
		// no land colour on its first frame.
		submitDesc.objectLook =
		    L3DMeshSubmitDesc::ObjectLook {.colour = ecs::boat_launch::SeaReflectionColour(), .alpha = 1.0f};
		submitDesc.landColourAdded = boat.landColourAt.has_value();
		submitDesc.landColourBefore = true;
		if (boat.landColourAt.has_value())
		{
			submitDesc.shadeAt = glm::vec4(*boat.landColourAt, 255.0f);
		}
		submitDesc.unlit = true;
		submitDesc.alwaysLit = true;
		submitDesc.hazed = false;
		submitDesc.snow = false;
		submitDesc.groundShadows = false;
		DrawMesh(*meshes.Handle(boat.mesh), submitDesc, std::numeric_limits<uint8_t>::max());
	}
}

void Renderer::UpdateLandDetail(const DrawSceneDesc& drawDesc) const
{
	if (!Locator::terrainSystem::has_value())
	{
		return;
	}
	const auto& config = Locator::config::value();
	auto& frame = _landDetail;
	// The classification of the frame that the draw list tested the blocks' triangles with, from the same camera
	if (Locator::landDetailSystem::has_value())
	{
		frame.detail = Locator::landDetailSystem::value().For(*drawDesc.camera);
	}
	else
	{
		std::vector<land_detail::BlockPlace> places;
		for (const auto& block : Locator::terrainSystem::value().GetBlocks())
		{
			places.push_back({.corner = block.GetMapPosition(), .at = block.GetBlockPosition()});
		}
		frame.detail =
		    land_detail::ClassifyFrame({.origin = drawDesc.camera->GetOrigin(),
		                                .forward = drawDesc.camera->GetForward(),
		                                .lines = config.landDetailLines.value_or(land_detail::For(config.detailLevel)),
		                                .coarseLand = config.detail.coarseLand},
		                               places);
	}

	const auto& blocks = Locator::terrainSystem::value().GetBlocks();
	// The blocks that the view can see: the same test as the list of the things that are drawn. With the land mirrored
	// in the sea, the box reaches as far under the water as it stands over it, so it holds the block's reflection too.
	const auto viewProjection = drawDesc.camera->GetViewProjectionMatrix();
	const auto nearPlane = drawDesc.camera->GetNearClip();
	const bool reflected = config.detail.landReflection;
	frame.inView.resize(blocks.size());
	for (size_t i = 0; i < blocks.size(); ++i)
	{
		frame.inView[i] = SharedBlockInView(
		    viewProjection, nearPlane,
		    {.corner = blocks[i].GetMapPosition(), .highestAltitude = blocks[i].GetHighestAltitude(), .reflected = reflected});
	}
	frame.neighbours.resize(frame.detail.neighbours.size());
	std::ranges::transform(frame.detail.neighbours, frame.neighbours.begin(), [](const std::array<int16_t, 4>& indices) {
		return glm::vec4(indices[0], indices[1], indices[2], indices[3]);
	});

	// The game works out the morph of the corners and the cross-fade of the texture on the CPU, in an order of float
	// sums that a vertex shader can not keep. Thus they are worked out here, for the blocks that morph or cross-fade,
	// and the vertex shader reads them.
	const auto& detail = frame.detail;
	const float meshWidth = 2.0f * detail.lines.meshBand;
	const float textureWidth = 2.0f * detail.lines.textureBand;
	const auto& corners = GetLandCorners();
	bool changed = false;
	const size_t squares = std::min(blocks.size(), land_detail::k_CornerSquaresPerSide * land_detail::k_CornerSquaresPerSide);
	for (size_t i = 0; i < squares && i < detail.blocks.size(); ++i)
	{
		const auto& block = detail.blocks[i];
		const bool morphs = block.morph != land_detail::Morph::None;
		const bool fades = block.texture == land_detail::Texture::CrossFade;
		if (!morphs && !fades)
		{
			continue;
		}
		const auto blockCorner = blocks[i].GetMapPosition();
		const auto& own = blocks[i].GetCornerAltitudes();
		const auto& meshLine = block.morph == land_detail::Morph::ToQuarter ? detail.meshFarGround : detail.meshNearGround;
		const auto altitudes = land_detail::MorphedCorners(own, block.morph, meshLine, meshWidth, blockCorner);
		const auto alphas =
		    fades ? land_detail::CrossFadeCorners(detail.textureGround, textureWidth, detail.lines.textureBand, blockCorner)
		          : std::array<uint8_t, land_detail::k_CornerCount> {};
		for (size_t x = 0; x < land_detail::k_CornersPerSide; ++x)
		{
			for (size_t z = 0; z < land_detail::k_CornersPerSide; ++z)
			{
				const size_t corner = x * land_detail::k_CornersPerSide + z;
				const size_t texel = 2 * land_detail::CornerTexel(i, x, z);
				const std::array<uint8_t, 2> drawn = {altitudes.at(corner), alphas.at(corner)};
				if (frame.corners.at(texel) != drawn[0] || frame.corners.at(texel + 1) != drawn[1])
				{
					frame.corners.at(texel) = drawn[0];
					frame.corners.at(texel + 1) = drawn[1];
					changed = true;
				}
			}
		}
	}
	if (changed)
	{
		corners.Update(frame.corners.data(), static_cast<uint32_t>(frame.corners.size()));
	}
}

const Texture2D& Renderer::GetLandCorners() const
{
	if (!_landCorners)
	{
		constexpr auto k_Side = static_cast<uint16_t>(land_detail::k_CornerTextureSide);
		_landDetail.corners.assign(2 * land_detail::k_CornerTextureSide * land_detail::k_CornerTextureSide, 0);
		_landCorners = std::make_unique<Texture2D>("LandCorners");
		_landCorners->CreateWithinFrame(k_Side, k_Side, 1, TextureFormat::RG8, Wrapping::ClampEdge, Filter::Nearest, nullptr);
		_landCorners->Update(_landDetail.corners.data(), static_cast<uint32_t>(_landDetail.corners.size()));
	}
	return *_landCorners;
}

const Texture2D* Renderer::GetCoarseTextures() const
{
	return _coarseEdges.textures ? &_coarseEdges.textures->GetColorAttachment() : nullptr;
}

bool Renderer::CoarseEdgesPainted(glm::ivec2 block) const
{
	return !_coarseEdges.lost && coarse_edges::OnMap(block) && _coarseEdges.painted.test(coarse_edges::PlaceOf(block));
}

void Renderer::PaintCoarseEdges(glm::vec3 camera, std::span<const size_t> drawn, const Texture2D* paintedSnow) const
{
	if (!Locator::terrainSystem::has_value())
	{
		return;
	}
	auto& island = Locator::terrainSystem::value();
	const auto& blocks = island.GetBlocks();
	auto& edges = _coarseEdges;
	auto repaints = island.TakeCoarseRepaints();
	const auto blockTexels = island.GetBlockTextureSide();
	const auto samples = static_cast<int32_t>(land_detail::CoarseSamplingFor(blockTexels).samples);
	// A new land, a new size of the textures or a restore of the device starts the edges again: every block gets its
	// first paint now
	const bool fresh = repaints.newLand || edges.lost || edges.samples != samples;
	if (fresh)
	{
		edges.painted.reset();
		edges.samples = samples;
		edges.lost = false;
		edges.waiting.assign(blocks.size(), true);
		_snowPaintedBlocks.clear();
	}
	edges.waiting.resize(blocks.size(), false);

	// The place of each block, and the block at each place
	std::vector<glm::ivec2> places(blocks.size());
	std::ranges::transform(blocks, places.begin(), [](const auto& block) { return block.GetBlockPosition(); });
	std::array<int32_t, coarse_edges::k_Blocks> blockAt {};
	blockAt.fill(-1);
	for (size_t i = 0; i < places.size(); ++i)
	{
		if (coarse_edges::OnMap(places[i]))
		{
			blockAt.at(coarse_edges::PlaceOf(places[i])) = static_cast<int32_t>(i);
		}
	}

	// The requests for a paint from the block textures, the snow and the shadows. The game paints all three into a block
	// at one time, thus a block that more than one of them asked for gets one paint.
	for (const auto index : repaints.blocks)
	{
		if (index < edges.waiting.size())
		{
			edges.waiting[index] = true;
		}
	}
	std::vector<glm::ivec2> otherPaints = std::exchange(_snowPaintedBlocks, {});
	if (_landShadowBake)
	{
		const auto baked = _landShadowBake->TakeNewlyBaked();
		otherPaints.insert(otherPaints.end(), baked.begin(), baked.end());
	}
	for (const auto at : otherPaints)
	{
		if (coarse_edges::OnMap(at) && blockAt.at(coarse_edges::PlaceOf(at)) >= 0)
		{
			edges.waiting[static_cast<size_t>(blockAt.at(coarse_edges::PlaceOf(at)))] = true;
		}
	}
	// A block paints when the land draws it with its own texture. The snow of these blocks is already painted in this
	// frame. A block whose shadows the other threads bake now waits for the bake, so that its paint takes the new shadows.
	const auto painted = [&](size_t index) {
		return coarse_edges::OnMap(places[index]) && edges.painted.test(coarse_edges::PlaceOf(places[index]));
	};
	const auto busy = [&](size_t index) { return _landShadowBake && _landShadowBake->Bakes(places[index]); };
	const auto due = coarse_edges::PaintsDue(edges.waiting, drawn, painted, busy);
	// Each pass of a frame needs views of its own. When the views of the frame are taken, the paints wait for the next
	// frame.
	if (due.empty() || edges.passes >= CoarseEdges::k_Views.size())
	{
		return;
	}
	std::vector<coarse_edges::PaintRequest> requests;
	requests.reserve(due.size());
	for (const auto index : due)
	{
		edges.waiting[index] = false;
		requests.push_back({.block = index, .centre = blocks[index].GetMapPosition() + glm::vec2(80.0f)});
	}
	const auto order = coarse_edges::PaintOrder(requests, camera);
	const auto paints = coarse_edges::MakeFramePaints(places, order, edges.painted);

	// The textures of the passes
	constexpr glm::u16vec2 k_Size {coarse_edges::k_TextureWidth, coarse_edges::k_TextureHeight};
	for (auto& target : edges.targets)
	{
		if (!target)
		{
			target = std::make_unique<FrameBuffer>("LandCoarseEdges", k_Size.x, k_Size.y, TextureFormat::RGBA8, std::nullopt, 1,
			                                       Wrapping::ClampEdge);
		}
	}
	constexpr auto k_TexturesSide = coarse_edges::k_CoarseTexturesSide;
	if (!edges.textures)
	{
		edges.textures = std::make_unique<FrameBuffer>("LandCoarseTextures", k_TexturesSide, k_TexturesSide,
		                                               TextureFormat::RGBA8, std::nullopt, 1, Wrapping::ClampEdge);
	}
	auto& paintsTexture = edges.paints.at(edges.passes);
	if (!paintsTexture)
	{
		constexpr auto k_Side = static_cast<uint16_t>(coarse_edges::k_BlocksPerSide);
		paintsTexture = std::make_unique<Texture2D>("LandCoarsePaints");
		paintsTexture->CreateWithinFrame(k_Side, k_Side, 1, TextureFormat::RGBA32F, Wrapping::ClampEdge, Filter::Nearest,
		                                 nullptr);
	}
	const auto texels = coarse_edges::FramePaintTexels(paints);
	paintsTexture->Update(texels.data(), static_cast<uint32_t>(texels.size() * sizeof(float)));

	// Both passes paint the blocks with the snow and the baked shadows, as the shader of the land paints them
	const auto& textures = Locator::resources::value().GetTextures();
	const bool snowed = paintedSnow != nullptr && textures.Contains(snow_cover::k_NoiseTextureId.value());
	const glm::vec4 u_snow {snowed ? 1.0f : 0.0f, 0.0f, 0.0f, 0.0f};
	// Each paint keeps the baked shadows. The land draws the moving shadows of the trees later, and the paint does not
	// keep them.
	const auto* baked = _landShadowsBaked && _landShadowBake ? _landShadowBake->GetTexture() : nullptr;
	const glm::vec4 u_objectShadows {0.0f, 0.0f, 0.0f, baked != nullptr ? 1.0f : 0.0f};
	const auto landTexels = LandTexelsUniform(blockTexels);
	const auto islandExtent = glm::vec4(island.GetExtent().minimum, island.GetExtent().maximum);
	const glm::vec4 blockSize {0.0f, 0.0f, 160.0f, 160.0f};
	const auto bindPaint = [&](const ShaderProgram& program) {
		program.SetTextureSampler("s2_coarsePaints", 2, *paintsTexture);
		program.SetTextureSampler("s0_blockTextures", 0, island.GetBlockTextures());
		program.SetUniformValue("u_snow", &u_snow);
		if (snowed)
		{
			program.SetTextureSampler("s3_paintedSnow", 3, *paintedSnow);
			program.SetTextureSampler("s12_snowNoise", 12, *textures.Handle(snow_cover::k_NoiseTextureId.value()));
		}
		program.SetUniformValue("u_objectShadows", &u_objectShadows);
		if (baked != nullptr)
		{
			program.SetTextureSampler("s14_landShadows", 14, *baked);
		}
		program.SetUniformValue("u_landTexels", &landTexels);
		program.SetUniformValue("u_islandExtent", &islandExtent);
		program.SetUniformValue("u_blockPositionAndSize", &blockSize);
	};

	// The pass of the edges reads the edges as they are and writes them after the paints into the other texture
	const auto& before = edges.targets.at(edges.current)->GetColorAttachment();
	const auto& after = *edges.targets.at(edges.current ^ 1u);
	const auto viewId = SetUpLandView(CoarseEdges::k_Views.at(edges.passes), after, k_Size);
	const auto& program = *_shaderManager->GetShader("LandCoarseEdges");
	program.SetTextureSampler("s1_coarseEdgesBefore", 1, before);
	bindPaint(program);
	const glm::vec2 corner {k_Size};
	SubmitLandQuad(viewId, program, {0.0f, 0.0f}, corner, {0.0f, 0.0f}, corner, BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_A);

	// The pass of the coarse textures writes the squares of the painted blocks and of the blocks round them. A paint
	// changes the cells round the edges of the neighbours, also at the corners.
	std::bitset<coarse_edges::k_Blocks> squares;
	for (const auto index : order)
	{
		const auto at = places[index];
		for (int32_t dx = -1; dx <= 1; ++dx)
		{
			for (int32_t dz = -1; dz <= 1; ++dz)
			{
				const glm::ivec2 near {at.x + dx, at.y + dz};
				if (coarse_edges::OnMap(near) && blockAt.at(coarse_edges::PlaceOf(near)) >= 0)
				{
					squares.set(coarse_edges::PlaceOf(near));
				}
			}
		}
	}
	const auto texturesView =
	    SetUpLandView(CoarseEdges::k_TextureViews.at(edges.passes), *edges.textures, {k_TexturesSide, k_TexturesSide});
	const auto& texturesProgram = *_shaderManager->GetShader("LandCoarseTextures");
	for (size_t place = 0; place < coarse_edges::k_Blocks; ++place)
	{
		if (!squares.test(place))
		{
			continue;
		}
		const glm::ivec2 block {static_cast<int32_t>(place) / coarse_edges::k_BlocksPerSide,
		                        static_cast<int32_t>(place) % coarse_edges::k_BlocksPerSide};
		const auto from = glm::vec2(coarse_edges::CoarseTexel(block, {0, 0}));
		const auto to = from + static_cast<float>(coarse_edges::k_SideTexels);
		texturesProgram.SetTextureSampler("s1_coarseEdgesAfter", 1, after.GetColorAttachment());
		bindPaint(texturesProgram);
		SubmitLandQuad(texturesView, texturesProgram, from, to, from, to, BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_A);
	}

	for (const auto index : order)
	{
		edges.painted.set(coarse_edges::PlaceOf(places[index]));
	}
	edges.current ^= 1u;
	++edges.passes;
}

void Renderer::BakeLandShadows(bool drawVegetation) const noexcept
{
	MainThreadQueue::CheckDirect(MainThreadApi::Renderer, "Renderer::BakeLandShadows");
	// The game bakes the shadows of the buildings, rocks and other things, and of the trees and big forests, into the
	// land as it paints it. The trees' shadows that move with them are drawn each frame, not baked.
	const bool movingTrees = drawVegetation && Locator::config::value().dynamicTreeShadows;
	if (!_landShadowBake)
	{
		_landShadowBake = std::make_unique<LandShadowBake>();
	}
	_landShadowBake->Update(!bgfx::getCaps()->originBottomLeft,
	                        LandShadowBake::Casters {.shapes = true, .leaves = drawVegetation && !movingTrees});
}
