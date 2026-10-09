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

#include "Audio/AudioManagerInterface.h"
#include "Common/GameRandom.h"
#include "ECS/Registry.h"
#include "ECS/Systems/ParticleSystemInterface.h"
#include "ECS/Systems/PlayerSystemInterface.h"
#include "ECS/Systems/TimeSystemInterface.h"
#include "ECS/WorldObjects.h"
#include "Locator.h"

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

std::optional<PlayerNames> GameTempleDestructionWorld::LocalPlayer() const
{
	if (!Locator::playerSystem::has_value())
	{
		return std::nullopt;
	}
	return Locator::playerSystem::value().GetLocalPlayer();
}

entt::entity GameTempleDestructionWorld::StartLoop(entt::id_type sound, glm::vec3 position, entt::entity owner)
{
	if (!Locator::audio::has_value())
	{
		return entt::null;
	}
	return Locator::audio::value().StartSoundEffect(
	    sound, {.position = position, .playType = audio::PlayType::Repeat, .owner = owner});
}

void GameTempleDestructionWorld::PlayOnce(entt::id_type sound, glm::vec3 position, entt::entity owner)
{
	if (Locator::audio::has_value())
	{
		Locator::audio::value().StartSoundEffect(sound, {.position = position, .owner = owner});
	}
}

void GameTempleDestructionWorld::StopSound(entt::entity emitter)
{
	if (Locator::audio::has_value())
	{
		Locator::audio::value().StopEmitter(emitter);
	}
}

std::optional<uint32_t> GameTempleDestructionWorld::StartSpotVisual(SpotVisualType type, glm::vec3 position, int32_t turns,
                                                                    PlayerNames player)
{
	if (!Locator::particleSystem::has_value())
	{
		return std::nullopt;
	}
	auto& particles = Locator::particleSystem::value();
	const auto id = particles.StartSpotVisual(type, position, turns, entt::null, 1.0f);
	particles.SetPlayer(id, static_cast<int>(player));
	return id;
}

void GameTempleDestructionWorld::FollowWithSpotVisual(uint32_t visual, entt::entity target)
{
	if (Locator::particleSystem::has_value())
	{
		Locator::particleSystem::value().AddTarget(visual, target);
	}
}

float GameTempleDestructionWorld::RandomShare(float spread)
{
	return Locator::gameRandom::has_value() ? Locator::gameRandom::value().GameFloatRand(spread) : 0.0f;
}

void GameTempleDestructionWorld::StartScript(std::string_view name)
{
	if (Locator::vm::has_value())
	{
		Locator::vm::value().StartScript(std::string(name), lhvm::ScriptType::All);
	}
}

void GameTempleDestructionWorld::Remove(entt::entity object)
{
	world_objects::Remove(object);
}
