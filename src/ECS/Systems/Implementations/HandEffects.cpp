/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#define LOCATOR_IMPLEMENTATIONS

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include <algorithm>
#include <fstream>
#include <tuple>

#include <L3DFile.h>
#include <LNDFile.h>
#include <fmt/format.h>
#include <glm/geometric.hpp>
#include <glm/gtc/constants.hpp>
#include <glm/gtc/matrix_inverse.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/rotate_vector.hpp>
#include <spdlog/spdlog.h>

#include "3D/AllMeshes.h"
#include "3D/FrameAnim.h"
#include "3D/L3DMesh.h"
#include "3D/L3DSubMesh.h"
#include "3D/LandIslandInterface.h"
#include "3D/ObjectMatrix.h"
#include "Audio/Audio.h"
#include "Camera/Camera.h"
#include "Camera/CameraModel.h"
#include "Common/GameRandom.h"
#include "Debug/DebugEnv.h"
#include "ECS/Archetypes/AbodeArchetype.h"
#include "ECS/Archetypes/HandArchetype.h"
#include "ECS/Archetypes/MobileStaticArchetype.h"
#include "ECS/Archetypes/PotArchetype.h"
#include "ECS/Archetypes/TreeArchetype.h"
#include "ECS/Components/Abode.h"
#include "ECS/Components/Alpha.h"
#include "ECS/Components/Fixed.h"
#include "ECS/Components/Hand.h"
#include "ECS/Components/Mesh.h"
#include "ECS/Components/Mobile.h"
#include "ECS/Components/MorphWithTerrain.h"
#include "ECS/Components/Pot.h"
#include "ECS/Components/Sprite.h"
#include "ECS/Components/StoragePit.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/Tree.h"
#include "ECS/Components/Villager.h"
#include "ECS/Registry.h"
#include "ECS/Systems/DebugHooksInterface.h"
#include "FileSystem/FileSystemInterface.h"
#include "Graphics/Texture2D.h"
#include "HandSystem.h"
#include "HandSystemDetail.h"
#include "InfoConstants.h"
#include "Locator.h"
#include "Resources/Loaders.h"
#include "Resources/ResourceManager.h"
#include "Resources/ResourcesInterface.h"
#include "Resources/SharedAssets.h"
#include "Windowing/WindowingInterface.h"

using namespace openblack;
using namespace openblack::ecs::archetypes;
using namespace openblack::ecs::components;
using namespace openblack::ecs::systems;
using namespace openblack::ecs::systems::hand_detail;

namespace
{
// Data/Spells/ZSpellFiles/SF_GripLandscape_txt.zzz
constexpr uint32_t k_DustAtoms = 8;         // CreateRuleSphere.NumAtoms
constexpr float k_DustRadius = 1.41371f;    // CreateRuleSphere.Radius
constexpr float k_DustFrameRate = 20.7611f; // ParticleSpriteCreator.FrameRate
constexpr uint32_t k_DustFrames = 32;       // ParticleSpriteCreator.NumFrames (4 rows of the 8x8 S_SpriteSheet3)
constexpr float k_DustDieAge = 2.65752f;    // RemoveRuleOldAgeOnly.DieAge
constexpr float k_DustStartScale = 0.0f;    // UR_ChangeScale
constexpr float k_DustStopScale = 1.43009f; //
constexpr float k_DustStartAlpha = 78.0f;   // AR_FadeAlpha
constexpr float k_DustStopAlpha = 2.0f;     //
constexpr glm::vec3 k_DustColour {255.0f / 255.0f, 182.0f / 255.0f, 198.0f / 255.0f}; // ColorR/G/B
constexpr float k_DustColourAlpha = 154.0f / 255.0f;                                  // ColorA

/// The cell of a looped PSys sprite (frame_anim::ParticleFrameIndex), FileOffset + f
glm::vec2 ParticleSpriteUv(int fileOffset, float frame, uint32_t frames)
{
	const int f = graphics::frame_anim::ParticleFrameIndex(frame, static_cast<int>(frames), true);
	return graphics::frame_anim::SpriteCellUv(fileOffset + f, 8)[0];
}

/// S_SpriteSheet3 is an 8x8 grid; the dust animation is the first 4 rows (the rest are other effects). FileOffset 0
glm::vec2 DustFrameUv(float frame)
{
	return ParticleSpriteUv(0, frame, k_DustFrames);
}

/// The frame step of a PSys atom (frame_anim::ParticleFrameAdvance); PlayAnim is 1 in these .zzz files
void StepFrame(float& frame, float seconds, float rate, uint32_t frames)
{
	float previous = frame;
	graphics::frame_anim::ParticleFrameAdvance(previous, frame, seconds, rate, static_cast<int>(frames), true);
}

/// What the hand effects' trace keeps between calls, in the debug hooks' store (Locator::debugHooks)
struct HandEffectsDebugHooksState
{
	float traceTime {0.0f}; // OPENBLACK_HAND_TRACE: seconds since the last pick-up particles line
};

HandEffectsDebugHooksState& HandEffectsDebugHooksData()
{
	if (!Locator::debugHooks::has_value())
	{
		std::fputs("ecs::systems::HandSystem: no debug hooks in the locator (Locator::debugHooks)\n", stderr);
		std::abort();
	}
	return Locator::debugHooks::value().Get<HandEffectsDebugHooksState>();
}
} // namespace

void HandSystem::EmitGripDust(glm::vec3 point) noexcept
{
	auto& resources = Locator::resources::value();
	auto& textures = resources.GetTextures();
	const auto textureId = openblack::resources::shared_assets::k_SpriteSheet3a; // alpha of the sheet: the puff shape
	switch (openblack::resources::shared_assets::LoadSpriteSheet3a("Grip dust"))
	{
	case openblack::resources::shared_assets::LoadResult::Failed:
		return;
	case openblack::resources::shared_assets::LoadResult::LoadedNow:
		SPDLOG_LOGGER_INFO(spdlog::get("game"), "Grip dust: loaded S_SpriteSheet3a.raw (present now: {})",
		                   textures.Contains(textureId));
		break;
	case openblack::resources::shared_assets::LoadResult::AlreadyLoaded:
		break;
	}
	// Dust only on land: HandPlacement calls this only when the gripped cell is land (not over the sea).
	if (debug_env::HandTrace() && Locator::terrainSystem::has_value())
	{
		SPDLOG_LOGGER_INFO(spdlog::get("game"), "Grip dust at ({:.1f},{:.1f},{:.1f}) terrain height {:.2f}", point.x, point.y,
		                   point.z, Locator::terrainSystem::value().GetHeightAt(glm::vec2(point.x, point.z)));
	}
	// No height test: the caller picks the land or the water branch by the cell's water bit (the landscape grip)
	const auto texture = textures.Handle(textureId)->GetNativeHandle();
	auto& registry = Locator::entitiesRegistry::value();
	// The hand's effects are local PSys effects: their atoms draw psys::FloatRand / psys::Rand on the local seed
	// (LocalRand). (approximate) The emitter is hand-rolled here, not the
	// .zzz's: the stream and the PSys formulas are the original's, the number and the order of the position draws are not
	const game_random::psys::StepScope scope(game_random::psys::NetGameType::Local);
	const auto random = []() { return game_random::psys::FloatRand(1.0f); };
	for (uint32_t i = 0; i < k_DustAtoms; ++i)
	{
		// Random point in the upper half of the sphere around the grab point.
		const float a = random() * glm::two_pi<float>();
		const float u = random();
		const float r = std::sqrt(1.0f - u * u);
		const float d = k_DustRadius * std::cbrt(random());
		const glm::vec3 offset(std::cos(a) * r * d, u * d * 0.5f, std::sin(a) * r * d);
		const auto entity = registry.Create();
		// RandomiseInitFrame: psys::Rand(NumFrames), as the 3D sprite atoms are made
		const auto frame = static_cast<float>(game_random::psys::Rand(static_cast<int32_t>(k_DustFrames)));
		// UseAdditiveAlpha 0 in SF_GripLandscape: normal blending (tint premultiplied by alpha each frame).
		registry.Assign<Sprite>(entity, texture, DustFrameUv(frame), glm::vec2(1.0f / 8.0f), glm::vec4(k_DustColour, 0.0f),
		                        false);
		registry.Assign<Transform>(entity, point + offset, glm::mat3(1.0f), glm::vec3(k_DustStartScale));
		_dust.push_back({entity, 0.0f, frame});
	}
}

void HandSystem::UpdateGripDust(float seconds) noexcept
{
	if (_dust.empty())
	{
		return;
	}
	auto& registry = Locator::entitiesRegistry::value();
	for (auto& particle : _dust)
	{
		particle.age += seconds;
		if (particle.age >= k_DustDieAge || !registry.Valid(particle.entity))
		{
			if (registry.Valid(particle.entity))
			{
				registry.Destroy(particle.entity);
			}
			particle.entity = entt::null;
			continue;
		}
		const float t = particle.age / k_DustDieAge;
		auto& sprite = registry.Get<Sprite>(particle.entity);
		StepFrame(particle.frame, seconds, k_DustFrameRate, k_DustFrames);
		sprite.uvMin = DustFrameUv(particle.frame);
		sprite.tint.a = (k_DustStartAlpha + (k_DustStopAlpha - k_DustStartAlpha) * t) / 255.0f * k_DustColourAlpha;
		sprite.tint = glm::vec4(k_DustColour * sprite.tint.a, sprite.tint.a); // premultiplied
		registry.Get<Transform>(particle.entity).scale = glm::vec3(k_DustStartScale + (k_DustStopScale - k_DustStartScale) * t);
	}
	std::erase_if(_dust, [](const DustParticle& particle) { return particle.entity == entt::null; });
}

namespace
{
// Data/Spells/ZSpellFiles/SF_MultiPickUp{Wood,Food}_txt.zzz
constexpr float k_PickupEmitRate = 8.0f;        // ER_MultiPickup.EmitRate (atoms per second)
constexpr float k_PickupRaiseTime = 1.0f;       // ER_MultiPickup.RaiseTime
constexpr float k_PickupWoodScale = 0.35f;      // ParticleMeshCreator_Wood.InitialScale (MSH_I_OFFERING_WOOD)
constexpr float k_TumbleSpeed = 0.849115f;      // AppearanceRuleTumble.TumbleSpeed
constexpr float k_MaxTumbleSpeed = 6.20088f;    // AppearanceRuleTumble.MaxTumbleSpeed (RestrictMaxRotation 1)
constexpr float k_PickupGrainScale = 0.5f;      // ParticleSpriteCreator_Grain.InitialScale
constexpr float k_PickupGrainFrameRate = 20.0f; // FrameRate, 32 frames, 8 per row of S_SpriteSheet1, looped
constexpr uint32_t k_PickupGrainFrames = 32;
// UseLandscapeColor: the original tints by the landscape light; the sprite shader only has the sheet's alpha, so
// the grains take the mean colour of S_SpriteSheet1's first 32 frames instead.
constexpr glm::vec3 k_PickupGrainColour {229.0f / 255.0f, 208.0f / 255.0f, 148.0f / 255.0f};
// SF_MultiPickUpFoodFish_txt.zzz: ParticleSpriteCreator_Fish, S_Spangle_A.raw cells 48..63 at 40 fps, looped, from a
// random frame in a random direction, InitialScale 1 with RandomiseScale, colour 200 x the landscape colour (here the
// mean colour of those cells, 96 142 133, x 200 / 255).
constexpr float k_PickupFishScale = 1.0f;
constexpr float k_PickupFishFrameRate = 40.0f;
constexpr uint32_t k_PickupFishFirstCell = 48;
constexpr uint32_t k_PickupFishFrames = 16;
constexpr glm::vec3 k_PickupFishColour {96.0f * 200.0f / 65025.0f, 142.0f * 200.0f / 65025.0f, 133.0f * 200.0f / 65025.0f};
} // namespace

void HandSystem::UpdatePickupParticles(float seconds, bool emitting) noexcept
{
	auto& registry = Locator::entitiesRegistry::value();
	if (!emitting)
	{
		// stopping the pick-up deletes the particle system with its atoms.
		for (const auto& particle : _pickupParticles)
		{
			if (registry.Valid(particle.entity))
			{
				registry.Destroy(particle.entity);
			}
		}
		if (!_pickupParticles.empty())
		{
			registry.SetDirty();
		}
		_pickupParticles.clear();
		_pickupOwed = 0.0f;
		_pickupEmitted = 0;
		return;
	}
	const bool wood = PotInfoOf(*_held) == PotInfo::HandWood;
	const bool fish = _pickFish;
	// the gesture position: the hand.
	const auto hand = registry.Get<Transform>(_hands[static_cast<size_t>(Side::Left)]).position;

	// owed += dt * EmitRate; while emitted < owed, create an atom at
	// the ground under the hand (x, z of the gesture position, y = the land's height).
	_pickupOwed += seconds * k_PickupEmitRate;
	while (static_cast<float>(_pickupEmitted) < _pickupOwed)
	{
		++_pickupEmitted;
		const glm::vec3 start(hand.x, Locator::terrainSystem::value().GetHeightAt(glm::vec2(hand.x, hand.z)), hand.z);
		const auto entity = registry.Create();
		// A fish atom's draws, as the 3D sprite atoms are made (local stream, see EmitGripDust): RandomiseScale
		// (psys::FloatRand(0.7) + 0.3) x InitialScale, RandomiseInitFrame psys::Rand(NumFrames),
		// RandomiseFrameDirection psys::Rand(0x100) > 0x80 reverses the rate
		float fishScale = k_PickupFishScale;
		float fishFrame = 0.0f;
		float fishFrameRate = k_PickupFishFrameRate;
		if (fish)
		{
			const game_random::psys::StepScope scope(game_random::psys::NetGameType::Local);
			const float r = game_random::psys::FloatRand(0.7f);
			const float s = r + 0.3f;
			fishScale = s * k_PickupFishScale;
			fishFrame = static_cast<float>(game_random::psys::Rand(static_cast<int32_t>(k_PickupFishFrames)));
			if (game_random::psys::Rand(0x100) > 0x80)
			{
				fishFrameRate = -fishFrameRate;
			}
		}
		if (wood)
		{
			// RandomiseOrientations 0: Y angle = DefaultOrientation = 0.
			const auto& pots = Locator::infoConstants::value().pot;
			const auto meshId = resources::HashIdentifier(pots[static_cast<size_t>(PotInfo::HandWood)].meshId);
			registry.Assign<Transform>(entity, start, glm::mat3(1.0f), glm::vec3(k_PickupWoodScale));
			registry.Assign<Mesh>(entity, meshId, static_cast<int8_t>(0), static_cast<int8_t>(1));
		}
		else
		{
			auto& textures = Locator::resources::value().GetTextures();
			const auto* sheet = fish ? "S_Spangle_Aa" : "S_SpriteSheet1a";
			const auto textureId = fish ? entt::hashed_string("raw/S_Spangle_Aa") : entt::hashed_string("raw/S_SpriteSheet1a");
			if (!textures.Contains(textureId))
			{
				try
				{
					auto& fileSystem = Locator::filesystem::value();
					textures.Load(
					    textureId, resources::Texture2DLoader::FromDiskTag {},
					    fileSystem.FindPath(fileSystem.GetPath<filesystem::Path::Textures>() / fmt::format("{}.raw", sheet)));
				}
				catch (const std::exception& e)
				{
					SPDLOG_LOGGER_WARN(spdlog::get("game"), "Pick-up grain: cannot load S_SpriteSheet1a.raw: {}", e.what());
					registry.Destroy(entity);
					continue;
				}
			}
			const auto texture = textures.Handle(textureId)->GetNativeHandle();
			// UseAdditiveAlpha 0: normal blending with a premultiplied tint. InitFrame 0, not randomised.
			registry.Assign<Sprite>(entity, texture, glm::vec2(0.0f), glm::vec2(1.0f / 8.0f),
			                        glm::vec4(fish ? k_PickupFishColour : k_PickupGrainColour, 1.0f), false);
			registry.Assign<Transform>(entity, start, glm::mat3(1.0f), glm::vec3(fish ? fishScale : k_PickupGrainScale));
		}
		PickupParticle particle {entity, 0.0f, start, start, wood};
		if (fish)
		{
			particle.frame = fishFrame;
			particle.frameRate = fishFrameRate;
		}
		else
		{
			particle.frameRate = k_PickupGrainFrameRate;
		}
		_pickupParticles.push_back(particle);
	}

	// Each atom: removed once its age passes RaiseTime; otherwise pos = start + (hand - start) * age / RaiseTime (the
	// current hand, so the pieces follow it) and velocity = (pos - previous) / dt.
	for (auto& particle : _pickupParticles)
	{
		particle.age += seconds;
		if (particle.age > k_PickupRaiseTime || !registry.Valid(particle.entity))
		{
			if (registry.Valid(particle.entity))
			{
				registry.Destroy(particle.entity);
			}
			particle.entity = entt::null;
			continue;
		}
		auto& transform = registry.Get<Transform>(particle.entity);
		const auto position = particle.start + (hand - particle.start) * (particle.age / k_PickupRaiseTime);
		const auto velocity = seconds > 0.0f ? (position - particle.previous) / seconds : glm::vec3(0.0f);
		particle.previous = position;
		transform.position = position;
		if (particle.mesh)
		{
			// AppearanceRuleTumble: turn by clamp(|v| * TumbleSpeed, +-Max) * dt, every row's
			// (x, y) about the world Z when |vz| < |vx|, else its (y, z) about X:
			// R(-a) on the left, as Particles/Rules/Sprinkle.cpp (it was R(+a) on the right, the atom's own axes)
			const float rate = std::clamp(glm::length(velocity) * k_TumbleSpeed, -k_MaxTumbleSpeed, k_MaxTumbleSpeed);
			const float angle = rate * seconds;
			affine::TurnRows(transform.rotation, std::abs(velocity.z) < std::abs(velocity.x) ? 2 : 0, angle);
		}
		else
		{
			auto& sprite = registry.Get<Sprite>(particle.entity);
			const uint32_t frames = fish ? k_PickupFishFrames : k_PickupGrainFrames;
			StepFrame(particle.frame, seconds, particle.frameRate, frames);
			sprite.uvMin = ParticleSpriteUv(fish ? static_cast<int>(k_PickupFishFirstCell) : 0, particle.frame, frames);
		}
	}
	std::erase_if(_pickupParticles, [](const PickupParticle& particle) { return particle.entity == entt::null; });
	registry.SetDirty();
	if (debug_env::HandTrace() && !_pickupParticles.empty())
	{
		auto& traceTime = HandEffectsDebugHooksData().traceTime;
		traceTime += seconds;
		if (traceTime > 0.5f)
		{
			traceTime = 0.0f;
			const auto& first = _pickupParticles.front();
			const auto& p = registry.Get<Transform>(first.entity).position;
			const auto& handTransform = registry.Get<Transform>(_hands[static_cast<size_t>(Side::Left)]);
			const auto* bones = GetBoneMatrices();
			SPDLOG_LOGGER_INFO(
			    spdlog::get("game"),
			    "Pick-up psys: {} atoms, oldest age {:.2f} at ({:.1f},{:.2f},{:.1f}), hand ({:.1f},{:.2f},{:.1f}) det {:.3f} "
			    "scale {:.3f} bone0 {:.2f},{:.2f},{:.2f} entities {}",
			    _pickupParticles.size(), first.age, p.x, p.y, p.z, hand.x, hand.y, hand.z,
			    glm::determinant(handTransform.rotation), handTransform.scale.x,
			    bones && !bones->empty() ? (*bones)[0][3][0] : -1.0f, bones && !bones->empty() ? (*bones)[0][3][1] : -1.0f,
			    bones && !bones->empty() ? (*bones)[0][3][2] : -1.0f, static_cast<uint32_t>(entt::to_integral(first.entity)));
		}
	}
}

void HandSystem::UpdatePickupSound(bool active) noexcept
{
	// the pick-up plays 98 G_PickUpWood for a wood pile and 44 G_PickUpFood for everything else
	// (food piles, fields, fish farms) every turn: bank InGame, 3D, not
	// moved afterwards, no object, at the hand's position as the interface status last received it (the hand sends it
	// every turn, before the pick-up is updated).
	// The .sad (flags 0x7E0) makes it loop (-1) in mode 2, so
	// after the first turn playing again leaves the playing channel (and its position) alone and only
	// the pitch (60 + 180 t^2, truncated) changes it (rate * p / 100, from 60 % to 240 %).
	// Stopping the pick-up stops both samples, 44 and 98.
	constexpr int k_Food = 44; // G_PickUpFood
	constexpr int k_Wood = 98; // G_PickUpWood
	const auto none = audio::Owner::None();
	if (!active)
	{
		if (_pickupSound)
		{
			// stopping the pick-up stops both samples
			audio::StopSoundEffect(k_Food, none, audio::SfxBank::InGame);
			audio::StopSoundEffect(k_Wood, none, audio::SfxBank::InGame);
			_pickupSound.reset();
		}
		_pickupSoundFraction = 0.0f;
		return;
	}
	const int sample = PotInfoOf(*_held) == PotInfo::HandWood ? k_Wood : k_Food;
	audio::PlayOptions options;
	options.sample = {audio::Bank(audio::SfxBank::InGame), sample};
	options.is3D = true;
	options.track = false;
	options.position = Locator::entitiesRegistry::value().Get<Transform>(_hands[static_cast<size_t>(Side::Left)]).position;
	const auto channel = audio::PlaySoundEffect(options);
	if (channel != audio::k_NoChannel)
	{
		_pickupSound = channel;
	}
	// the pitch: 60 + 180 x the stored fraction (t^2), truncated
	audio::SetPitch(audio::Bank(audio::SfxBank::InGame), none, sample, static_cast<int>(60.0f + 180.0f * _pickupSoundFraction));
}
