/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#define LOCATOR_IMPLEMENTATIONS

#include "RewardSystem.h"

#include <cmath>

#include <limits>
#include <vector>

#include <PackFile.h>
#include <entt/core/hashed_string.hpp>
#include <glm/gtc/constants.hpp>
#include <glm/gtx/euler_angles.hpp>

#include "3D/L3DAnim.h"
#include "3D/L3DMesh.h"
#include "3D/LandIslandInterface.h"
#include "Animals/AnimalAnimation.h"
#include "Audio/AudioManagerInterface.h"
#include "Audio/GameSoundEffects.h"
#include "Common/GameRandom.h"
#include "Common/PortableRotations.h"
#include "ECS/Components/AnimatedStatic.h"
#include "ECS/Components/BlendedByMaterial.h"
#include "ECS/Components/Mesh.h"
#include "ECS/Components/Reward.h"
#include "ECS/Components/Transform.h"
#include "ECS/Registry.h"
#include "ECS/RewardOpening.h"
#include "ECS/RewardRules.h"
#include "ECS/Systems/ExplosionSystemInterface.h"
#include "ECS/Systems/HandSystemInterface.h"
#include "ECS/Systems/ObjectDrawListSystemInterface.h"
#include "ECS/Systems/PlayerSystemInterface.h"
#include "GameRewardOpeningWorld.h"
#include "InfoConstants.h"
#include "Locator.h"
#include "Magic/DispenserRules.h"
#include "Magic/MagicTables.h"
#include "Magic/MiracleVisuals.h"
#include "Particles/ParticleBlast.h"
#include "Particles/ParticleDrawFrame.h"
#include "Resources/ResourcesInterface.h"

using namespace openblack;
using namespace openblack::ecs::components;
using namespace openblack::ecs::systems;

namespace
{
/// The closed chest's model, and the thump of one landing, heard alike wherever the camera is
constexpr entt::hashed_string k_ChestMesh = entt::hashed_string("misc/chest0");
constexpr entt::hashed_string k_LandingSound = entt::hashed_string("InGame.sad/174");
/// The opened chest, whose lid lifts as its clip plays
constexpr entt::hashed_string k_OpenedMesh = entt::hashed_string("coffre");
constexpr entt::hashed_string k_OpenedClip = entt::hashed_string("coffre");
/// A tap on a chest, heard by the player tapping; and the two sounds of it opening, heard alike wherever the camera is
constexpr entt::hashed_string k_TapSound = entt::hashed_string("InGame.sad/173");
constexpr entt::hashed_string k_OpeningSound = entt::hashed_string("InGame.sad/40");
constexpr entt::hashed_string k_LidSound = entt::hashed_string("InGame.sad/41");
/// The smoke sheet its dust is drawn from
constexpr entt::hashed_string k_SmokeSheet = entt::hashed_string("raw/smoke");
constexpr entt::hashed_string k_SmokeSheetAlpha = entt::hashed_string("raw/smokea");
/// The landing shakes a camera only near the middle of the world's first corner: the game shakes about the world's
/// origin rather than the chest
constexpr glm::vec3 k_ShakeCentre {0.0f};

float LocalRandom(float a, float b)
{
	return Locator::gameRandom::has_value() ? a + Locator::gameRandom::value().LocalFloatRand(b - a) : (a + b) * 0.5f;
}

float LandHeight(glm::vec3 point)
{
	return Locator::terrainSystem::has_value() ? Locator::terrainSystem::value().GetHeightAt({point.x, point.z}) : 0.0f;
}

/// The player of this computer, who hears the tap on a chest
PlayerNames OwnPlayer()
{
	return Locator::handSystem::has_value() ? Locator::handSystem::value().GetOwnPlayer() : PlayerNames::PLAYER_ONE;
}

/// What a kind of chest holds, from the game's reward table
ecs::reward::Contents ContentsOf(RewardObjectInfo type)
{
	if (!Locator::infoConstants::has_value())
	{
		return {};
	}
	const auto& table = Locator::infoConstants::value().reward;
	const auto index = static_cast<size_t>(type);
	if (index >= table.size())
	{
		return {};
	}
	const auto& row = table.at(index);
	return {.food = row.foodValue,
	        .wood = row.woodValue,
	        .belief = row.beliefValue,
	        .object = row.mobileStatic,
	        .scaffold = row.scaffold != 0,
	        .nextMiracle = type == RewardObjectInfo::Increment,
	        .seed = row.seed,
	        .powerUp = row.powerUp};
}

/// A town's new miracle above its chest spins as a globe's seed does, and a creature spell's phial pulses
void StepSeed(RewardSeed& shown, float seconds)
{
	shown.spin = std::fmod(shown.spin + (magic::k_OrbSeedSpin * seconds), glm::two_pi<float>());
	if (!magic::visuals::IsPhialSeed(shown.seed) || !Locator::infoConstants::has_value())
	{
		return;
	}
	const auto& info = Locator::infoConstants::value();
	const auto* spell =
	    magic::GetMagicInfoAs<GMagicCreatureSpellInfo>(info, magic::GetSpellSeedInfo(info, shown.seed).magicTypes[0]);
	const int receiveType = spell != nullptr ? static_cast<int>(spell->creatureReceiveSpellType) : 0;
	shown.phialFrame = magic::visuals::StepPhialFrame(shown.phialFrame, seconds);
	shown.phialPhase = std::fmod(shown.phialPhase + (magic::visuals::PhialPulseSpeed(receiveType) * seconds), 1.0f);
}

/// The opened chest's model posed at a place in its clip; its resting pose when the clip doesn't fit it
void PoseOpened(uint32_t place, std::vector<glm::mat4>& bones)
{
	if (!Locator::resources::has_value())
	{
		return;
	}
	const auto& meshes = Locator::resources::value().GetMeshes();
	const auto& animations = Locator::resources::value().GetAnimations();
	if (!meshes.Contains(k_OpenedMesh.value()) || !animations.Contains(k_OpenedClip.value()))
	{
		bones.clear();
		return;
	}
	const auto clip = animations.Handle(k_OpenedClip.value());
	const auto model = meshes.Handle(k_OpenedMesh.value());
	const auto& frames = clip->GetFrames();
	const auto& parents = model->GetBoneParents();
	const animals::ClipTiming timing {
	    .playTime = clip->GetPlayTime(), .frameCount = frames.size(), .looping = clip->IsLooping(), .playedByTime = true};
	const auto span = animals::SpanAt(timing, place);
	if (!model->IsBoned() || span.from >= frames.size() || span.to >= frames.size() ||
	    frames[span.from].bones.size() < parents.size() || frames[span.to].bones.size() < parents.size())
	{
		bones.clear();
		return;
	}
	bones.resize(parents.size());
	animals::PoseBetween(frames[span.from].bones, frames[span.to].bones, span.t, parents, bones);
}

particles::Creator MakeDustCreator()
{
	particles::Creator dust;
	dust.kind = particles::Creator::Kind::Sprite;
	dust.className = "RewardDust";
	dust.texture = "smoke";
	dust.spritesPerRow = 8;
	dust.numFrames = 64;
	dust.additive = false;
	return dust;
}
} // namespace

entt::entity RewardSystem::Create(glm::vec3 position, RewardObjectInfo type, std::optional<PlayerNames> player,
                                  entt::entity town, bool fromSky)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto entity = registry.Create();
	// Given on the land it stands at the height asked for; from the sky it falls to the land itself
	const glm::vec3 ground {position.x, LandHeight(position), position.z};
	registry.Assign<Transform>(entity, fromSky ? ground + glm::vec3(0.0f, reward::k_FallFrom, 0.0f) : position, glm::mat3(1.0f),
	                           glm::vec3(1.0f));
	registry.Assign<Mesh>(entity, k_ChestMesh.value(), static_cast<int8_t>(0), static_cast<int8_t>(1));
	auto& chest = registry.Assign<Reward>(entity);
	chest.type = type;
	chest.player = player;
	chest.town = town;
	chest.landingPoint = ground;
	chest.madeAt = position;
	// Its dust is made with it, and only shown once it has fallen
	chest.dust = reward::MakeDust(LocalRandom);
	if (fromSky)
	{
		chest.state = Reward::State::Falling;
		chest.dustMilliseconds = reward::k_DustMilliseconds;
	}
	else
	{
		// Put on the land, it stands in the map's cells at once
		chest.state = Reward::State::Landed;
		registry.Assign<RewardOnLand>(entity);
	}
	// Make the land's draw list again when a player gets the chest
	if (player.has_value() && Locator::objectDrawListSystem::has_value())
	{
		Locator::objectDrawListSystem::value().RequestRebuild(1);
	}
	return entity;
}

bool RewardSystem::Tap(entt::entity chest, PlayerNames player)
{
	auto& registry = Locator::entitiesRegistry::value();
	auto* reward = registry.Valid(chest) ? registry.TryGet<Reward>(chest) : nullptr;
	// Only its own player may tap it, and only until it is open
	if (reward == nullptr || reward->player != player || reward->state == Reward::State::Opened)
	{
		return false;
	}
	if (player == OwnPlayer() && Locator::audio::has_value())
	{
		audio::PlayGameSoundEffect(k_TapSound.value(), std::nullopt);
	}
	// It opens once it is on the land
	if (reward->state != Reward::State::Landed || reward->opened)
	{
		return true;
	}
	reward->opened = true;
	reward->state = Reward::State::Opened;
	reward->turnsLeft = reward::k_OpenedTurns;
	reward->clipPlace = 0;
	// The closed chest gives way to the opened one, where it stood, its lid lifting as its clip plays
	registry.AssignOrReplace<Mesh>(chest, k_OpenedMesh.value(), static_cast<int8_t>(0), static_cast<int8_t>(0));
	// The light shining out of it blends by its texture
	// Its light shines in a colour of its own, the same for every chest, whatever the land's light
	const auto& [red, green, blue] = reward::k_OpenedLightColour;
	registry.AssignOrReplace<BlendedByMaterial>(
	    chest, BlendedByMaterial {.shineColour =
	                                  glm::vec3(static_cast<float>(red), static_cast<float>(green), static_cast<float>(blue))});
	auto& pose = registry.AssignOrReplace<AnimatedStaticPose>(chest);
	PoseOpened(0, pose.bones);
	registry.SetDirty();
	if (Locator::audio::has_value())
	{
		// The opening sounds do not start again while they play
		audio::StartGameSoundEffect(k_OpeningSound.value(), {.voicePlayType = pack::AudioBankLoop::Once});
		audio::StartGameSoundEffect(k_LidSound.value(), {.voicePlayType = pack::AudioBankLoop::Once});
	}
	const auto contents = ContentsOf(reward->type);
	GameRewardOpeningWorld world(chest);
	reward::Open(contents, world);
	return true;
}

bool RewardSystem::IsOpened(entt::entity chest) const
{
	const auto& registry = Locator::entitiesRegistry::value();
	const auto* reward = registry.Valid(chest) ? registry.TryGet<const Reward>(chest) : nullptr;
	return reward != nullptr && reward->opened;
}

void RewardSystem::Update(float milliseconds, float turnFraction)
{
	if (!Locator::entitiesRegistry::has_value())
	{
		return;
	}
	auto& registry = Locator::entitiesRegistry::value();
	bool lidMoved = false;
	registry.Each<Reward>([&](entt::entity entity, Reward& chest) {
		if (chest.state == Reward::State::Opened)
		{
			if (auto* seed = registry.TryGet<RewardSeed>(entity))
			{
				StepSeed(*seed, milliseconds * 0.001f);
			}
			// The lid lifts by the frame's game time, holding once the clip has played
			if (!Locator::resources::has_value() || !Locator::resources::value().GetAnimations().Contains(k_OpenedClip.value()))
			{
				return;
			}
			const auto clip = Locator::resources::value().GetAnimations().Handle(k_OpenedClip.value());
			const auto place = reward::StepOpenedClip(chest.clipPlace, static_cast<uint32_t>(milliseconds), clip->GetPlayTime(),
			                                          clip->IsLooping());
			if (place != chest.clipPlace)
			{
				chest.clipPlace = place;
				if (auto* pose = registry.TryGet<AnimatedStaticPose>(entity))
				{
					pose->place = place;
					PoseOpened(place, pose->bones);
					lidMoved = true;
				}
			}
			return;
		}
		if (chest.state == Reward::State::Landed)
		{
			// Its dust fades once it has fallen
			chest.dustMilliseconds = std::max(chest.dustMilliseconds - milliseconds, 0.0f);
			return;
		}
		// The frame draws a chest that falls between the places of its last two turns
		const float clock = reward::DrawnFallClock(chest.seconds, turnFraction);
		chest.drawn = Reward::Drawn {
		    .position = chest.landingPoint + glm::vec3(0.0f, reward::DrawnFallHeight(clock), 0.0f),
		    .rotation = glm::mat3(portable_math::EulerAngleY(-reward::FallYaw(clock))),
		};
	});
	if (lidMoved)
	{
		registry.SetDirty();
	}
}

void RewardSystem::ProcessTurn()
{
	if (!Locator::entitiesRegistry::has_value())
	{
		return;
	}
	auto& registry = Locator::entitiesRegistry::value();
	std::vector<entt::entity> landed;
	std::vector<entt::entity> gone;
	registry.Each<Reward>([&landed, &gone](entt::entity entity, Reward& chest) {
		if (chest.goesIntoMap)
		{
			chest.goesIntoMap = false;
			landed.push_back(entity);
		}
		// An opened chest goes once its turns are up
		if (chest.state == Reward::State::Opened && --chest.turnsLeft <= 0)
		{
			gone.push_back(entity);
		}
	});
	// A chest that landed goes into the map's cells the turn after
	for (const auto entity : landed)
	{
		registry.AssignOrReplace<RewardOnLand>(entity);
	}
	for (const auto entity : gone)
	{
		registry.Destroy(entity);
	}
	// A chest from the sky falls and turns by the turn's game time, and thumps down on the land
	registry.Each<Reward, Transform>([](entt::entity /*unused*/, Reward& chest, Transform& transform) {
		if (chest.state != Reward::State::Falling)
		{
			return;
		}
		const auto step = reward::StepFall(chest.seconds);
		chest.seconds = step.seconds;
		transform.rotation = glm::mat3(portable_math::EulerAngleY(-step.yaw));
		if (!step.lands)
		{
			transform.position = chest.landingPoint + glm::vec3(0.0f, step.height, 0.0f);
			return;
		}
		// It thumps down on the land, shaking a camera near the world's origin
		transform.position = chest.landingPoint;
		chest.state = Reward::State::Landed;
		chest.goesIntoMap = true;
		chest.drawn.reset();
		if (Locator::audio::has_value())
		{
			audio::PlayGameSoundEffect(k_LandingSound.value(), std::nullopt);
		}
		if (Locator::explosionSystem::has_value())
		{
			Locator::explosionSystem::value().AddShake(k_ShakeCentre, reward::k_ShakeRadius, reward::k_ShakeStrength,
			                                           reward::k_ShakeMilliseconds, false);
		}
	});
}

void RewardSystem::CollectDrawFrame(particles::draw::Frame& frame) const
{
	if (!Locator::entitiesRegistry::has_value())
	{
		return;
	}
	const auto creator = MakeDustCreator();
	const particles::draw::Sources sources {
	    .textures = [](std::string_view /*texture*/) -> std::optional<std::pair<entt::id_type, entt::id_type>> {
		    return std::pair {k_SmokeSheet.value(), k_SmokeSheetAlpha.value()};
	    },
	    .playerColour = {},
	    .random = {},
	};
	particles::Effect::DrawWalk walk;
	const auto& registry = Locator::entitiesRegistry::value();
	registry.Each<const Reward>([&](const Reward& chest) {
		// Only a chest that fell shows its dust, while its time lasts
		if (chest.state != Reward::State::Landed || chest.dustMilliseconds <= 0.0f)
		{
			return;
		}
		const auto look = reward::LookOfDust(chest.dustMilliseconds);
		walk.Clear();
		for (const auto& sprite : chest.dust)
		{
			walk.steps.push_back({.chain = false, .index = static_cast<uint32_t>(walk.atoms.size())});
			walk.atoms.push_back({
			    .creator = &creator,
			    .position = chest.madeAt + sprite.offset,
			    .rotation = glm::mat3(1.0f),
			    .scale = look.size * 0.5f,
			    .stretch = 1.0f,
			    .alpha = static_cast<float>(look.alpha),
			    .frame = static_cast<float>(sprite.frame),
			    .rgb = sprite.rgb,
			});
		}
		particles::draw::AddEffect(frame, walk, particles::draw::DrawPath::Sorted, chest.madeAt,
		                           particles::draw::k_NeutralPlayer, sources);
	});
}
