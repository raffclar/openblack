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

#include <array>
#include <limits>
#include <memory>
#include <optional>

#include <bgfx/bgfx.h>
#include <glm/gtc/type_ptr.hpp>

#include "3D/L3DMesh.h"
#include "Camera/Camera.h"
#include "Creature/TempleLeashes.h"
#include "ECS/Components/CreatureLeash.h"
#include "ECS/Components/OneOffSpellSeed.h"
#include "ECS/Components/Player.h"
#include "ECS/PlayerColour.h"
#include "ECS/Systems/AlignmentSystemInterface.h"
#include "ECS/Systems/GestureSystemInterface.h"
#include "ECS/Systems/HandSystemInterface.h"
#include "ECS/Systems/ParticleSystemInterface.h"
#include "ECS/Systems/TimeSystemInterface.h"
#include "Gestures/GestureHints.h"
#include "Graphics/FrameBuffer.h"
#include "Graphics/ShaderManager.h"
#include "Graphics/Texture2D.h"
#include "Locator.h"
#include "Magic/MiracleVisuals.h"
#include "Renderer.h"
#include "Resources/ResourcesInterface.h"
#include "Windowing/WindowingInterface.h"

using namespace openblack;
using namespace openblack::graphics;

namespace
{
/// The depth runs from 1 at the near clip to 0 at infinity, as the screen's camera has it
constexpr auto k_ReverseZ =
    glm::mat4(1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, -0.5f, 0.0f, 0.0f, 0.0f, 0.5f, 1.0f);

/// Drawn solid, the nearest in front, culling the backs of the collar's faces as the game's objects are
constexpr uint64_t k_CollarState = BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_A | BGFX_STATE_WRITE_Z | BGFX_STATE_CULL_CW |
                                   BGFX_STATE_DEPTH_TEST_GREATER | BGFX_STATE_MSAA;

/// The views the collars are drawn in, one for each, as many as there are leashes
constexpr std::array k_CollarPasses = {RenderPass::GestureHintCollar0, RenderPass::GestureHintCollar1,
                                       RenderPass::GestureHintCollar2};
/// The views the seeds and the globe are drawn in, one for each picture
constexpr std::array k_SeedPasses = {RenderPass::GestureHintSeed0, RenderPass::GestureHintSeed1, RenderPass::GestureHintSeed2,
                                     RenderPass::GestureHintSeed3, RenderPass::GestureHintSeed4};

/// Tested against the depth, added over what is behind by the texture's alpha times the tint's alpha
constexpr uint64_t k_AddedState = BGFX_STATE_WRITE_RGB | BGFX_STATE_DEPTH_TEST_GREATER | BGFX_STATE_MSAA |
                                  BGFX_STATE_BLEND_FUNC(BGFX_STATE_BLEND_SRC_ALPHA, BGFX_STATE_BLEND_ONE);

constexpr float k_ByteMax = 255.0f;

glm::vec3 ColourOf(uint32_t rgb)
{
	return glm::vec3(static_cast<float>((rgb >> 16u) & 0xFFu), static_cast<float>((rgb >> 8u) & 0xFFu),
	                 static_cast<float>(rgb & 0xFFu)) /
	       k_ByteMax;
}

/// Puts a view over the square of a picture. The view clears the depth and keeps its draws in the order they come.
void PlacePictureView(RenderPass pass, const FrameBuffer* frameBuffer, const gesture_hints::PixelRect& rect,
                      const glm::mat4& view, const glm::mat4& projection)
{
	const auto viewId = static_cast<bgfx::ViewId>(pass);
	if (frameBuffer != nullptr)
	{
		frameBuffer->Bind(pass);
	}
	bgfx::setViewRect(viewId, static_cast<uint16_t>(rect.x), static_cast<uint16_t>(rect.y), static_cast<uint16_t>(rect.width),
	                  static_cast<uint16_t>(rect.height));
	bgfx::setViewClear(viewId, BGFX_CLEAR_DEPTH, 0, 0.0f, 0);
	bgfx::setViewMode(viewId, bgfx::ViewMode::Sequential);
	bgfx::setViewTransform(viewId, glm::value_ptr(view), glm::value_ptr(projection));
	bgfx::touch(viewId);
}
} // namespace

void Renderer::DrawGestureHintCollars(const DrawSceneDesc& desc) const
{
	namespace hints = gesture_hints;
	using ecs::components::LeashPost;
	if (!Locator::gestureSystem::has_value() || !Locator::windowing::has_value() || desc.camera == nullptr)
	{
		return;
	}
	const auto& row = Locator::gestureSystem::value().GetHints();
	if (!_warmingUp && (row.hints.empty() || row.alpha <= 0.0f))
	{
		return;
	}
	const auto& meshes = Locator::resources::value().GetMeshes();
	const auto& textures = Locator::resources::value().GetTextures();
	if (!meshes.Contains(LeashPost::k_MeshId) || !textures.Contains(LeashPost::k_TextureId))
	{
		return;
	}
	const auto& mesh = *meshes.Handle(LeashPost::k_MeshId);
	const auto skin = textures.Handle(LeashPost::k_TextureId)->GetNativeHandle();
	const auto drawCollar = [&](RenderPass pass, const glm::mat4& model, LeashType leash) {
		L3DMeshSubmitDesc submitDesc = {};
		submitDesc.viewId = pass;
		submitDesc.program = _shaderManager->GetShader("Object");
		submitDesc.state = k_CollarState;
		submitDesc.modelMatrices = &model;
		submitDesc.matrixCount = 1;
		// The leash's band of the leash texture, not slid round it
		submitDesc.skinTexture = &skin;
		submitDesc.uvOffset = glm::vec2(0.0f, temple_leashes::Band(leash));
		submitDesc.useMaterialBlending = true;
		submitDesc.useMaterialCulling = true;
		DrawMesh(mesh, submitDesc, std::numeric_limits<uint8_t>::max());
	};
	// The warm-up draws a collar of no size in the view of each collar, so that the renderer has the pipelines before
	// the first hint shows
	if (_warmingUp)
	{
		for (const auto pass : k_CollarPasses)
		{
			const auto viewId = static_cast<bgfx::ViewId>(pass);
			if (desc.frameBuffer != nullptr)
			{
				desc.frameBuffer->Bind(pass);
			}
			bgfx::setViewRect(viewId, 0, 0, 1, 1);
			bgfx::touch(viewId);
			drawCollar(pass, glm::mat4(0.0f), LeashType::Rope);
		}
		return;
	}
	const auto screen = static_cast<glm::vec2>(Locator::windowing::value().GetSize());
	const float focal = hints::FocalPixels(hints::RowWidth(screen), desc.camera->GetHorizontalFieldOfView());
	const auto view = hints::CollarView();

	size_t used = 0;
	for (size_t slot = 0; slot < row.hints.size() && used < k_CollarPasses.size(); ++slot)
	{
		const auto& hint = row.hints.at(slot);
		if (hint.path.empty() || hint.leash == LeashType::None)
		{
			continue;
		}
		const auto picture = hints::PictureSquare(hints::Square(slot, screen));
		const auto rect = hints::PixelsOf(picture);
		if (rect.width <= 0 || rect.height <= 0 || rect.x < 0 || rect.y < 0)
		{
			continue;
		}
		const auto pass = k_CollarPasses.at(used++);
		const auto viewId = static_cast<bgfx::ViewId>(pass);
		if (desc.frameBuffer != nullptr)
		{
			desc.frameBuffer->Bind(pass);
		}
		// The picture's square has its depth cleared, so the collar shows over the world behind it
		bgfx::setViewRect(viewId, static_cast<uint16_t>(rect.x), static_cast<uint16_t>(rect.y),
		                  static_cast<uint16_t>(rect.width), static_cast<uint16_t>(rect.height));
		bgfx::setViewClear(viewId, BGFX_CLEAR_DEPTH, 0, 0.0f, 0);
		const auto projection =
		    k_ReverseZ * hints::CollarProjection(rect, (picture.min + picture.max) * 0.5f, focal, desc.camera->GetNearClip());
		bgfx::setViewTransform(viewId, glm::value_ptr(view), glm::value_ptr(projection));
		bgfx::touch(viewId);

		drawCollar(pass, hints::CollarModel(row.collars.at(static_cast<size_t>(hint.leash))), hint.leash);
	}
}

void Renderer::DrawGestureHintSeeds(const DrawSceneDesc& desc) const
{
	namespace hints = gesture_hints;
	using ecs::components::OneOffSpellSeed;
	if (!Locator::gestureSystem::has_value() || !Locator::windowing::has_value() || !Locator::infoConstants::has_value() ||
	    desc.camera == nullptr)
	{
		return;
	}
	const auto& row = Locator::gestureSystem::value().GetHints();
	if (!_warmingUp && (row.hints.empty() || row.alpha <= 0.0f))
	{
		return;
	}
	if (_hintCamera == nullptr)
	{
		_hintCamera = std::make_unique<Camera>(hints::k_CollarAt);
		_hintCamera->SetOrigin(hints::k_CollarEye);
	}
	const auto& meshes = Locator::resources::value().GetMeshes();
	const auto* objectProgram = _shaderManager->GetShader("Object");
	const auto eye = hints::k_CollarEye;
	const auto sceneOf = [&](RenderPass pass) {
		return DrawSceneDesc {.camera = _hintCamera.get(),
		                      .frameBuffer = desc.frameBuffer,
		                      .entities = desc.entities,
		                      .time = desc.time,
		                      .timeOfDay = desc.timeOfDay,
		                      .smallBumpMapStrength = desc.smallBumpMapStrength,
		                      .viewId = pass,
		                      .drawSky = false,
		                      .drawWater = false,
		                      .drawIsland = false,
		                      .drawEntities = true,
		                      .drawSprites = false,
		                      .drawVegetation = false,
		                      .drawBoundingBoxes = false,
		                      .cullBack = desc.cullBack,
		                      .wireframe = desc.wireframe};
	};
	// The globe adds its mesh over what is behind by the texture's alpha, at the row's alpha. It writes its depth.
	const auto drawGlobe = [&](RenderPass pass, const glm::mat4& model, uint8_t alpha) {
		if (!meshes.Contains(OneOffSpellSeed::k_MeshId.value()))
		{
			return;
		}
		L3DMeshSubmitDesc submitDesc = {};
		submitDesc.viewId = pass;
		submitDesc.program = objectProgram;
		submitDesc.modelMatrices = &model;
		submitDesc.matrixCount = 1;
		submitDesc.useMaterialCulling = true;
		submitDesc.state = k_AddedState | BGFX_STATE_WRITE_Z;
		submitDesc.tint = glm::vec4(1.0f, 1.0f, 1.0f, static_cast<float>(alpha) / k_ByteMax);
		DrawMesh(*meshes.Handle(OneOffSpellSeed::k_MeshId.value()), submitDesc, std::numeric_limits<uint8_t>::max());
	};
	// The seeds are the seeds of the player at this computer. The alignment of this player makes the seed of the flock
	// a bat or a dove. The rings have the colour of this player.
	const auto own = Locator::handSystem::has_value() ? Locator::handSystem::value().GetOwnPlayer() : PlayerNames::PLAYER_ONE;
	const float alignment =
	    Locator::alignmentSystem::has_value() ? Locator::alignmentSystem::value().GetPlayerAlignment(own) : 0.0f;
	const auto ringColour = ColourOf(player_colour::Of(own));

	// The warm-up draws a globe of no size in the view of each picture, and each seed of no size in the first view.
	// Thus the renderer has the pipelines before the first picture shows.
	if (_warmingUp)
	{
		for (const auto pass : k_SeedPasses)
		{
			PlacePictureView(pass, desc.frameBuffer, {.x = 0, .y = 0, .width = 1, .height = 1}, glm::mat4(1.0f),
			                 glm::mat4(1.0f));
			drawGlobe(pass, glm::mat4(0.0f), std::numeric_limits<uint8_t>::max());
		}
		const auto scene = sceneOf(k_SeedPasses.front());
		for (int32_t seed = 0; seed <= static_cast<int32_t>(SpellSeedType::BeamExplosion); ++seed)
		{
			DrawSeedModel(scene, {.seed = static_cast<SpellSeedType>(seed),
			                      .middle = hints::SeedMiddle(),
			                      .scale = 0.0f,
			                      .spin = 0.0f,
			                      .phialPhase = 0.0f,
			                      .phialFrame = 0.0f,
			                      .phialAlpha = std::numeric_limits<uint8_t>::max(),
			                      .ownerAlignment = alignment,
			                      .drawnAtOnce = true});
		}
		return;
	}
	const auto screen = static_cast<glm::vec2>(Locator::windowing::value().GetSize());
	const float focal = hints::FocalPixels(hints::RowWidth(screen), desc.camera->GetHorizontalFieldOfView());
	const auto view = hints::CollarView();
	const auto alpha = hints::PictureAlpha(row.alpha);
	const auto* particles = Locator::particleSystem::has_value() ? &Locator::particleSystem::value() : nullptr;
	const float turnFraction = Locator::time::has_value() ? Locator::time::value().GetTurnFraction() : 0.0f;

	size_t used = 0;
	for (size_t slot = 0; slot < row.hints.size() && used < k_SeedPasses.size(); ++slot)
	{
		const auto& hint = row.hints.at(slot);
		const auto picture = hint.seed != SpellSeedType::None ? row.seeds.find(hint.magic) : row.seeds.end();
		const bool seedShown = picture != row.seeds.end();
		if (hint.path.empty() || hint.leash != LeashType::None || (!seedShown && !hint.globe))
		{
			continue;
		}
		const auto square = hints::PictureSquare(hints::Square(slot, screen));
		const auto rect = hints::PixelsOf(square);
		if (rect.width <= 0 || rect.height <= 0 || rect.x < 0 || rect.y < 0)
		{
			continue;
		}
		const auto pass = k_SeedPasses.at(used++);
		const auto projection =
		    k_ReverseZ * hints::CollarProjection(rect, (square.min + square.max) * 0.5f, focal, desc.camera->GetNearClip());
		PlacePictureView(pass, desc.frameBuffer, rect, view, projection);
		if (!seedShown)
		{
			drawGlobe(pass, hints::GlobeModel(), alpha);
			continue;
		}

		// First the model of the seed, drawn at once. It spins above the middle at its share of its size.
		const auto& shown = picture->second;
		const auto scene = sceneOf(pass);
		DrawSeedModel(scene, {.seed = shown.seed,
		                      .middle = hints::SeedMiddle(),
		                      .scale = hints::k_SeedScale,
		                      .spin = shown.spin,
		                      .phialPhase = shown.phialPhase,
		                      .phialFrame = shown.phialFrame,
		                      .phialAlpha = alpha,
		                      .ownerAlignment = alignment,
		                      .drawnAtOnce = true});

		// Then the effect of its holder, over the model
		if (particles != nullptr && shown.holderEffect.has_value())
		{
			particles->CollectEffectDrawFrame(*shown.holderEffect, turnFraction, _hintParticleFrame);
			if (!_hintParticleFrame.items.empty())
			{
				particles::draw::Order(_hintParticleFrame, eye, std::nullopt, _hintParticleCommands, _hintParticleSpriteOrder);
				SubmitParticleFrame(scene, _hintParticleFrame, _hintParticleCommands, _hintParticleSpriteOrder, {}, eye);
			}
		}

		// Then a ring for each power-up level. The renderer draws each ring two times, added over what is behind, with no
		// depth written, in the colour of the player and with a grey specular.
		const int rings = magic::visuals::RingCount(shown.powerUp);
		if (rings <= 0 || !meshes.Contains(OneOffSpellSeed::k_RingMeshId.value()))
		{
			continue;
		}
		const auto ring = meshes.Handle(OneOffSpellSeed::k_RingMeshId.value());
		const float ringAlpha = static_cast<float>(hints::SeedRingAlpha(alpha)) / k_ByteMax;
		for (int i = 0; i < rings; ++i)
		{
			const auto model = magic::visuals::RingModel(i, shown.ringSpin, hints::SeedMiddle(),
			                                             hints::k_SeedScale * hints::k_SeedRingShare, eye);
			L3DMeshSubmitDesc submitDesc = {};
			submitDesc.viewId = pass;
			submitDesc.program = objectProgram;
			submitDesc.modelMatrices = &model;
			submitDesc.matrixCount = 1;
			submitDesc.state = k_AddedState;
			submitDesc.tint = glm::vec4(ringColour, ringAlpha);
			submitDesc.lightAdd = ColourOf(magic::visuals::k_RingSpecular);
			DrawMesh(*ring, submitDesc, std::numeric_limits<uint8_t>::max());
			DrawMesh(*ring, submitDesc, std::numeric_limits<uint8_t>::max());
		}
	}
}
