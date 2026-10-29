/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#define LOCATOR_IMPLEMENTATIONS

#include "GameTempleDestructionWorld.h"

#include <string>

#include <LHVM.h>

#include "Common/GameRandom.h"
#include "ECS/Components/LivingAction.h"
#include "ECS/Registry.h"
#include "ECS/Systems/EffectRequestSystemInterface.h"
#include "ECS/Systems/HandSystemInterface.h"
#include "ECS/Systems/HeartbeatSystemInterface.h"
#include "ECS/Systems/LivingActionSystemInterface.h"
#include "ECS/Systems/PlayerSystemInterface.h"
#include "ECS/Systems/TimeSystemInterface.h"
#include "ECS/Systems/WorshipSiteSystemInterface.h"
#include "ECS/TempleHeartTowns.h"
#include "ECS/WorldObjects.h"
#include "InfoConstants.h"
#include "Locator.h"
#include "ScriptHeaders/ScriptStart.h"
#include "Sound/GameSoundEffects.h"
#include "Sound/SoundRequestInterface.h"

using namespace openblack;
using namespace openblack::ecs;
using namespace openblack::ecs::systems;

Registry* GameTempleDestructionWorld::Entities()
{
	return Locator::entitiesRegistry::has_value() ? &Locator::entitiesRegistry::value() : nullptr;
}

uint32_t GameTempleDestructionWorld::MillisecondsPerTurn() const
{
	return static_cast<uint32_t>(TimeSystemInterface::k_TurnDuration.count());
}

uint32_t GameTempleDestructionWorld::Turn() const
{
	return Locator::time::has_value() ? Locator::time::value().GetTurn() : 0;
}

std::optional<PlayerNames> GameTempleDestructionWorld::LocalPlayer() const
{
	if (!Locator::playerSystem::has_value())
	{
		return std::nullopt;
	}
	return (Locator::handSystem::has_value() ? Locator::handSystem::value().GetOwnPlayer() : PlayerNames::PLAYER_ONE);
}

bool GameTempleDestructionWorld::NetworkGame() const
{
	return Locator::time::has_value() && Locator::time::value().IsNetworkGame();
}

entt::entity GameTempleDestructionWorld::StartLoop(entt::id_type sound, glm::vec3 position, entt::entity owner)
{
	if (!Locator::sounds::has_value())
	{
		return entt::null;
	}
	// The loop stays with the temple, and it stops when the temple goes
	return audio::StartGameSoundEffect(sound, {.position = position,
	                                           .playType = audio::PlayType::Repeat,
	                                           .owner = owner,
	                                           .voiceOwner = audio::VoiceOwner::OwnSound,
	                                           .follows = audio::Follows::Owner});
}

void GameTempleDestructionWorld::PlayOnce(entt::id_type sound, glm::vec3 position, entt::entity owner, audio::Follows follows)
{
	if (Locator::sounds::has_value())
	{
		audio::StartGameSoundEffect(
		    sound, {.position = position, .owner = owner, .voiceOwner = audio::VoiceOwner::OwnSound, .follows = follows});
	}
}

void GameTempleDestructionWorld::StopSound(entt::entity emitter)
{
	// The audio can end a loop before the temple stops it, as when the camera goes too far away
	if (Locator::sounds::has_value() && Locator::sounds::value().EmitterExists(emitter))
	{
		Locator::sounds::value().StopEmitter(emitter);
	}
}

std::optional<uint32_t> GameTempleDestructionWorld::StartSpotVisual(SpotVisualType type, glm::vec3 position, int32_t turns,
                                                                    PlayerNames player)
{
	if (!Locator::effectRequestSystem::has_value())
	{
		return std::nullopt;
	}
	auto& effects = Locator::effectRequestSystem::value();
	const auto handle = effects.Start({.type = type, .position = position, .turns = turns, .player = static_cast<int>(player)});
	// The turn goes on only with a visual that started
	if (!effects.IsRunning(handle))
	{
		return std::nullopt;
	}
	return handle;
}

void GameTempleDestructionWorld::FollowWithSpotVisual(uint32_t visual, entt::entity target)
{
	if (Locator::effectRequestSystem::has_value())
	{
		Locator::effectRequestSystem::value().AddTarget(visual, target);
	}
}

void GameTempleDestructionWorld::SetSpotVisualMagnitude(uint32_t visual, float magnitude)
{
	if (Locator::effectRequestSystem::has_value())
	{
		Locator::effectRequestSystem::value().SetMagnitude(visual, magnitude);
	}
}

bool GameTempleDestructionWorld::SpotVisualRunning(uint32_t visual) const
{
	return Locator::effectRequestSystem::has_value() && Locator::effectRequestSystem::value().IsRunning(visual);
}

void GameTempleDestructionWorld::AddPlasma(uint32_t source, const particles::PlasmaCommand& command)
{
	if (Locator::effectRequestSystem::has_value())
	{
		Locator::effectRequestSystem::value().AddPlasma(source, command);
	}
}

std::vector<model_surface::Triangle> GameTempleDestructionWorld::DrawnTrianglesOf(entt::entity object) const
{
	return world_objects::DrawnTrianglesOf(object);
}

glm::mat4 GameTempleDestructionWorld::PlacementOf(entt::entity object) const
{
	return world_objects::PlacementOf(object);
}

std::vector<physics::temple_heart::Town> GameTempleDestructionWorld::HeartTowns(PlayerNames owner)
{
	auto& registry = Locator::entitiesRegistry::value();
	return temple_heart_towns::Gather(
	    registry, owner,
	    {
	        .lifeOf = [](entt::entity abode) { return world_objects::LifeOf(abode); },
	        .footballPitch =
	            [](entt::entity abode) {
		            const auto* info = world_objects::AbodeInfoOf(abode);
		            return info != nullptr && info->abodeType == AbodeType::FootballPitch;
	            },
	        .dying =
	            [&registry](entt::entity villager) {
		            const auto* action = registry.TryGet<const components::LivingAction>(villager);
		            return action != nullptr && Locator::livingActionSystem::has_value() &&
		                   Locator::livingActionSystem::value().VillagerGetState(
		                       *action, components::LivingAction::Index::Final) == VillagerStates::Dying;
	            },
	    });
}

GameRandomInterface* GameTempleDestructionWorld::Random()
{
	return Locator::gameRandom::has_value() ? &Locator::gameRandom::value() : nullptr;
}

float GameTempleDestructionWorld::RandomShare(float spread)
{
	return Locator::gameRandom::has_value() ? Locator::gameRandom::value().GameFloatRand(spread) : 0.0f;
}

void GameTempleDestructionWorld::StartScript(std::string_view name)
{
	if (Locator::vm::has_value())
	{
		Locator::vm::value().StartScript(std::string(name), script::start::StartableScripts());
	}
}

void GameTempleDestructionWorld::Remove(entt::entity object)
{
	world_objects::Remove(object);
}

void GameTempleDestructionWorld::RemoveWorshipSites(entt::entity temple)
{
	if (Locator::worshipSiteSystem::has_value())
	{
		Locator::worshipSiteSystem::value().RemoveSites(temple);
	}
}

void GameTempleDestructionWorld::EndHeartbeatHold(PlayerNames player)
{
	if (Locator::heartbeatSystem::has_value())
	{
		Locator::heartbeatSystem::value().EndDestruction(player);
	}
}
