/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The renderer is one of the locator's implementations
#define LOCATOR_IMPLEMENTATIONS 1

#include <cstdint>

#include <algorithm>
#include <array>
#include <iterator>
#include <memory>
#include <optional>
#include <tuple>
#include <utility>
#include <vector>

#include <LNDFile.h>
#include <bgfx/bgfx.h>
#include <fmt/format.h>
#include <spdlog/spdlog.h>

#include "3D/LandBlock.h"
#include "3D/LandIslandInterface.h"
#include "3D/OceanInterface.h"
#include "Camera/Camera.h"
#include "EngineConfig.h"
#include "Graphics/FrameBuffer.h"
#include "Graphics/GraphicsHandleBgfx.h"
#include "Graphics/IndexBuffer.h"
#include "Graphics/LakeMirrors.h"
#include "Graphics/LakeSurface.h"
#include "Graphics/Mesh.h"
#include "Graphics/RendererDetail.h"
#include "Graphics/ShaderManager.h"
#include "Graphics/VertexBuffer.h"
#include "Graphics/ViewFrustum.h"
#include "Locator.h"
#include "Profiler.h"
#include "Renderer.h"
#include "Resources/ResourcesInterface.h"
#include "Scenery/ObjectDrawList.h"

using namespace openblack;
using namespace openblack::graphics;
using namespace openblack::graphics::detail;

namespace
{
/// A corner outside a lake counts as land at least one altitude above the lake's level
constexpr float k_ShoreReach = 0.67f;
/// The colour that a mirror is cleared to before its sky is drawn, as the sea's reflection is
constexpr uint32_t k_MirrorClearColour = 0x274659ff;
/// The pixels round the rectangle of a lake's water that its mirror also draws: the move of the pixels' centres and the
/// rounding of the rectangle
constexpr uint16_t k_MirrorMargin = 2;

/// The water surface of the lakes of the land, as a mesh that gives its data again after a restore of the device, and the
/// triangles of each level of the water in the mesh
std::pair<std::unique_ptr<Mesh>, std::vector<lake_mirrors::Level>> MakeLakeMesh(const LandIslandInterface& island,
                                                                                const land_lakes::LakeMap& lakes)
{
	const auto cellAt = [&island](int x, int z) -> const lnd::LNDCell* {
		if (x < 0 || z < 0 || x >= LandIslandInterface::k_MapCellsPerSide || z >= LandIslandInterface::k_MapCellsPerSide)
		{
			return nullptr;
		}
		return island.FindCell({static_cast<uint16_t>(x), static_cast<uint16_t>(z)});
	};
	auto surface = lake_surface::Build(
	    lakes,
	    [&cellAt](int x, int z) -> std::optional<float> {
		    const auto* cell = cellAt(x, z);
		    return cell != nullptr ? std::optional<float>(LandIslandInterface::GetDrawnAltitude(cell->altitude)) : std::nullopt;
	    },
	    [&cellAt](int x, int z) {
		    const auto* cell = cellAt(x, z);
		    return cell != nullptr && cell->properties.split != 0;
	    },
	    k_ShoreReach);
	if (surface.indices.empty())
	{
		return {nullptr, {}};
	}
	// Each level draws its own triangles, with its own mirror
	auto levels = lake_mirrors::GroupByLevel(surface);

	VertexDecl decl;
	decl.reserve(2);
	decl.emplace_back(VertexAttrib::Attribute::Position, static_cast<uint8_t>(3), VertexAttrib::Type::Float);
	decl.emplace_back(VertexAttrib::Attribute::TexCoord0, static_cast<uint8_t>(2), VertexAttrib::Type::Float);
	const auto bytes = [](const auto& values) {
		const auto* first = reinterpret_cast<const uint8_t*>(values.data());
		return std::vector<uint8_t>(first, first + (values.size() * sizeof(values[0])));
	};
	// The device takes where each vertex is, the height that its water fades by and the level of its lake
	std::vector<std::array<float, 5>> drawn;
	drawn.reserve(surface.vertices.size());
	std::ranges::transform(surface.vertices, std::back_inserter(drawn), [](const lake_surface::Vertex& vertex) {
		return std::array<float, 5> {vertex.x, vertex.y, vertex.z, vertex.fade, vertex.level};
	});
	auto vertices = bytes(drawn);
	auto indices = bytes(surface.indices);
	const auto* vertexMemory = bgfx::copy(vertices.data(), static_cast<uint32_t>(vertices.size()));
	auto* vertexBuffer = new VertexBuffer("Lakes", vertexMemory, decl);
	// The buffers take copies of the bytes, because the device reads them after this function
	const auto* indexMemory = bgfx::copy(indices.data(), static_cast<uint32_t>(indices.size()));
	auto* indexBuffer = new IndexBuffer("Lakes", indexMemory, IndexBuffer::Type::Uint32);
	auto mesh = std::make_unique<Mesh>(vertexBuffer, indexBuffer);
	mesh->KeepContentsForRestore(std::move(vertices), std::move(indices));
	return {std::move(mesh), std::move(levels)};
}
} // namespace

bool Renderer::PrepareLakeMesh() const
{
	if (!Locator::terrainSystem::has_value())
	{
		return false;
	}
	const auto& island = Locator::terrainSystem::value();
	const auto* lakes = island.GetLakeMap();
	if (lakes == nullptr)
	{
		if (_lakeMesh)
		{
			ReleaseLakes();
		}
		return false;
	}
	if (_lakeMeshDigest != lakes->Digest())
	{
		ReleaseLakes();
		std::tie(_lakeMesh, _lakeLevels) = MakeLakeMesh(island, *lakes);
		_lakeMeshDigest = lakes->Digest();
		_lakeQueries.assign(_lakeLevels.size(), BGFX_INVALID_HANDLE);
		_lakeHidden.assign(_lakeLevels.size(), 0);
		SPDLOG_LOGGER_INFO(spdlog::get("graphics"), "Lakes: {} lakes at {} levels, {} indices of water",
		                   lakes->GetLakes().size(), _lakeLevels.size(),
		                   _lakeMesh ? _lakeMesh->GetIndexBuffer().GetCount() : 0u);
	}
	return _lakeMesh != nullptr;
}

void Renderer::DrawLakeMirrors(const DrawSceneDesc& drawDesc) const
{
	const auto before = std::exchange(_lakePasses, {});
	_lakeSlotOfLevel.clear();
	if (!drawDesc.drawWater || !drawDesc.drawIsland || _warmingUp || !Locator::oceanSystem::has_value() || !PrepareLakeMesh())
	{
		return;
	}
	_lakeSlotOfLevel.assign(_lakeLevels.size(), k_LakeMirrorSlots);
	const auto& options = Locator::config::value().lakeMirrors;
	if (!options.mirrors)
	{
		return;
	}
	auto section = Locator::profiler::value().BeginScoped(Profiler::Stage::LakeMirrorPass);
	glm::u16vec2 size {0};
	Locator::oceanSystem::value().GetReflectionFramebuffer().GetSize(size.x, size.y);

	// The answers of the drawing of the last frames: a level whose water did not show gets no mirror
	for (size_t i = 0; i < _lakeQueries.size(); ++i)
	{
		if (!options.occlusion)
		{
			_lakeHidden[i] = 0;
		}
		else if (bgfx::isValid(_lakeQueries[i]))
		{
			const auto answer = bgfx::getResult(_lakeQueries[i]);
			if (answer != bgfx::OcclusionQueryResult::NoResult)
			{
				_lakeHidden[i] = answer == bgfx::OcclusionQueryResult::Invisible ? 1 : 0;
			}
		}
	}
	const auto& camera = *drawDesc.camera;
	const lake_mirrors::PlanSettings settings {
	    .slots = k_LakeMirrorSlots,
	    .margin = k_MirrorMargin,
	    .rects = options.rects,
	    .shareLayers = options.shareLayers,
	    .halfSizeBelow = options.halfSizeBelow,
	    .joinWithin = options.joinWithin,
	    .everySecondFrameBelow = options.everySecondFrameBelow,
	    .drawsAll = (_lakeMirrorFrames % 2) == 0,
	};
	++_lakeMirrorFrames;
	auto plan = lake_mirrors::Plan(_lakeLevels, camera.GetViewProjectionMatrix(), camera.GetOrigin(), size, _lakeHidden, before,
	                               settings);
	_lakePasses = std::move(plan.passes);
	for (size_t level = 0; level < plan.passOfLevel.size(); ++level)
	{
		if (plan.passOfLevel[level].has_value())
		{
			_lakeSlotOfLevel[level] = *plan.passOfLevel[level];
		}
	}

	// The layers of the target, each the size of the sea's reflection
	size_t layerCount = 0;
	for (const auto& pass : _lakePasses)
	{
		layerCount = std::max(layerCount, static_cast<size_t>(pass.layer) + 1);
	}
	if (!_lakeLayers.empty())
	{
		glm::u16vec2 layerSize {0};
		_lakeLayers.front()->GetSize(layerSize.x, layerSize.y);
		if (layerSize != size)
		{
			_lakeLayers.clear();
			for (auto& pass : _lakePasses)
			{
				pass.kept = false;
			}
		}
	}
	while (_lakeLayers.size() < layerCount)
	{
		_lakeLayers.push_back(std::make_unique<FrameBuffer>(fmt::format("Lake Mirror {}", _lakeLayers.size()), size.x, size.y,
		                                                    TextureFormat::RGBA8, TextureFormat::Depth24Stencil8));
	}

	const auto& blocks = Locator::terrainSystem::value().GetBlocks();
	for (uint8_t slot = 0; slot < _lakePasses.size(); ++slot)
	{
		const auto& pass = _lakePasses[slot];
		const float level = _lakeLevels[pass.level].level;
		_lakeMirrorLevels.at(slot) = level;
		_lakeMirrorRects.at(slot) = pass.drawn;
		if (pass.kept)
		{
			continue;
		}
		const auto scene = LakeMirrorPass(slot);
		const auto sky = SkyPassOf(scene);
		const auto translucent = TranslucentPassOf(scene);
		const auto mirrored = MirroredPassOf(scene);
		// The mirror is cleared and drawn as the sea's reflection is, in its rectangle of its layer
		bgfx::setViewClear(static_cast<bgfx::ViewId>(sky), BGFX_CLEAR_COLOR | BGFX_CLEAR_DEPTH, k_MirrorClearColour, 0.0f, 0);
		bgfx::setViewClear(static_cast<bgfx::ViewId>(scene), BGFX_CLEAR_NONE);
		bgfx::setViewClear(static_cast<bgfx::ViewId>(translucent), BGFX_CLEAR_NONE);
		bgfx::setViewClear(static_cast<bgfx::ViewId>(mirrored), BGFX_CLEAR_DEPTH, 0, 0.0f, 0);
		for (const auto view : {sky, scene, translucent, mirrored})
		{
			bgfx::setViewRect(static_cast<bgfx::ViewId>(view), pass.drawn.x, pass.drawn.y, pass.drawn.width, pass.drawn.height);
		}
		const auto mirrorCamera = camera.Reflect(level, lake_mirrors::RectTransform(pass.rect, size));

		// The blocks of the land that the mirror sees. With the floor, only the blocks that stand over the level.
		const auto viewProjection = mirrorCamera->GetViewProjectionMatrix();
		auto& inView = _lakeBlocksInView.at(slot);
		inView.resize(blocks.size());
		for (size_t i = 0; i < blocks.size(); ++i)
		{
			const auto highest = static_cast<float>(blocks[i].GetHighestAltitude()) * view_frustum::k_AltitudeUnit;
			const bool seen = (!options.floor || highest >= level) &&
			                  object_draw_list::BlockInView(viewProjection, mirrorCamera->GetNearClip(),
			                                                {.corner = blocks[i].GetMapPosition(),
			                                                 .highestAltitude = blocks[i].GetHighestAltitude(),
			                                                 .reflected = false});
			inView[i] = seen ? 1 : 0;
		}

		DrawSceneDesc drawPassDesc = drawDesc;
		drawPassDesc.viewId = scene;
		drawPassDesc.camera = mirrorCamera.get();
		drawPassDesc.frameBuffer = _lakeLayers.at(pass.layer).get();
		drawPassDesc.drawWater = false;
		drawPassDesc.drawBoundingBoxes = false;
		drawPassDesc.cullBack = true;
		drawPassDesc.smallBumpMapStrength = 0.0f;
		DrawPass(drawPassDesc);
	}
}

void Renderer::ReleaseLakes() const
{
	_lakeMesh.reset();
	_lakeMeshDigest = 0;
	_lakeLevels.clear();
	_lakePasses.clear();
	_lakeSlotOfLevel.clear();
	_lakeLayers.clear();
	for (auto& query : _lakeQueries)
	{
		if (bgfx::isValid(query))
		{
			bgfx::destroy(query);
		}
	}
	_lakeQueries.clear();
	_lakeHidden.clear();
}

float Renderer::MirrorLevelOf(RenderPass viewId) const
{
	const auto slot = LakeMirrorSlotOf(viewId);
	return slot.has_value() ? _lakeMirrorLevels.at(*slot) : land_lakes::k_SeaLevel;
}

view_frustum::Frustum Renderer::SceneFrustum(const DrawSceneDesc& desc) const
{
	const auto frustum = view_frustum::FromViewProjection(desc.camera->GetProjectionMatrix() *
	                                                      desc.camera->GetViewMatrix(Camera::Interpolation::Current));
	// A lake's mirror shows only what stands over the lake's water
	if (LakeMirrorSlotOf(desc.viewId).has_value() && Locator::config::value().lakeMirrors.floor)
	{
		return view_frustum::WithFloor(frustum, MirrorLevelOf(desc.viewId));
	}
	return frustum;
}

glm::vec2 Renderer::SceneTargetOf(const DrawSceneDesc& desc) const
{
	if (const auto slot = LakeMirrorSlotOf(desc.viewId))
	{
		const auto& rect = _lakeMirrorRects.at(*slot);
		return {rect.width, rect.height};
	}
	return detail::SceneTarget(desc.frameBuffer);
}

void Renderer::SetLakeUniforms(const ShaderProgram& lakeShader) const
{
	lakeShader.SetUniformValue("u_seaParams", _seaUniforms.data());
	lakeShader.SetUniformValue("u_seaRows", &_seaUniforms[1]);
	lakeShader.SetUniformValue("u_seaMode", &_seaUniforms[2]);
	lakeShader.SetUniformValue("u_seaColour", &_seaUniforms[3]);
	lakeShader.SetUniformValue("u_seaCamera", &_seaUniforms[4]);
	lakeShader.SetUniformValue("u_seaDrift", &_seaUniforms[5]);
}

void Renderer::DrawPassLakes(const DrawSceneDesc& desc) const
{
	// Only the view itself has the lakes' water
	if (desc.viewId != RenderPass::Main || !desc.drawWater || !desc.drawIsland || _warmingUp || !PrepareLakeMesh())
	{
		return;
	}
	const auto* lakeShader = _shaderManager->GetShader("Lake");
	const auto& ocean = Locator::oceanSystem::value();
	const auto& seaMirror = ocean.GetReflectionFramebuffer();
	glm::u16vec2 size {0};
	seaMirror.GetSize(size.x, size.y);
	const bool asks = Locator::config::value().lakeMirrors.occlusion;
	auto texture = Locator::resources::value().GetTextures().Handle(ocean.GetTexture());
	// The water is tested against the depth of the land and writes no depth, as the sea. It is blended over the land by
	// the land's coast alpha under it, so the land in front of a lake hides it.
	constexpr auto k_LakeState = (k_BgfxDefaultStateInvertedZ & ~(BGFX_STATE_WRITE_Z | BGFX_STATE_CULL_MASK)) |
	                             BGFX_STATE_BLEND_FUNC(BGFX_STATE_BLEND_SRC_ALPHA, BGFX_STATE_BLEND_INV_SRC_ALPHA);
	for (size_t level = 0; level < _lakeLevels.size(); ++level)
	{
		const auto& water = _lakeLevels[level];
		_lakeMesh->GetIndexBuffer().Bind(water.indexCount, water.firstIndex);
		_lakeMesh->GetVertexBuffer().Bind();
		bgfx::setState(k_LakeState);
		lakeShader->SetTextureSampler("s_diffuse", 0, *texture);
		// Each level shows its own mirror: the pixel of the mirror's rectangle in its layer under the water's pixel. A
		// level with no mirror shows the sea's reflection, whose sky is the same at every height.
		const uint8_t slot = level < _lakeSlotOfLevel.size() ? _lakeSlotOfLevel[level] : k_LakeMirrorSlots;
		glm::vec4 u_lakeMirrorRect {0.0f};
		glm::vec4 u_lakeMirrorScale {1.0f, 1.0f, size.x, size.y};
		if (slot < _lakePasses.size() && _lakePasses[slot].layer < _lakeLayers.size())
		{
			const auto& pass = _lakePasses[slot];
			u_lakeMirrorRect = {pass.rect.x, pass.rect.y, pass.drawn.x, pass.drawn.y};
			u_lakeMirrorScale.x = static_cast<float>(pass.drawn.width) / static_cast<float>(pass.rect.width);
			u_lakeMirrorScale.y = static_cast<float>(pass.drawn.height) / static_cast<float>(pass.rect.height);
			lakeShader->SetTextureSampler("s_reflection", 1, _lakeLayers[pass.layer]->GetColorAttachment());
		}
		else
		{
			lakeShader->SetTextureSampler("s_reflection", 1, seaMirror.GetColorAttachment());
		}
		lakeShader->SetUniformValue("u_lakeMirrorRect", &u_lakeMirrorRect);
		lakeShader->SetUniformValue("u_lakeMirrorScale", &u_lakeMirrorScale);
		SetLakeUniforms(*lakeShader);
		// The view itself comes after the land's view, whose blocks the workers can submit late, so the land is all drawn
		// before the water. With the occlusion option, the drawing answers whether any of the water showed.
		if (asks && level < _lakeQueries.size())
		{
			auto& query = _lakeQueries[level];
			if (!bgfx::isValid(query))
			{
				query = bgfx::createOcclusionQuery();
			}
			bgfx::submit(static_cast<bgfx::ViewId>(desc.viewId), toBgfx(lakeShader->GetRawHandle()), query);
		}
		else
		{
			lakeShader->Submit(static_cast<bgfx::ViewId>(desc.viewId));
		}
	}
}
