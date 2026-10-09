/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#define LOCATOR_IMPLEMENTATIONS

#include "TempleDestructionSystem.h"

#include <vector>

#include <LHVM.h>
#include <entt/core/hashed_string.hpp>

#include "Audio/AudioManagerInterface.h"
#include "Audio/Sound.h"
#include "Common/GameRandom.h"
#include "ECS/Components/Temple.h"
#include "ECS/Components/Transform.h"
#include "ECS/Registry.h"
#include "ECS/Systems/ParticleSystemInterface.h"
#include "ECS/Systems/PlayerSystemInterface.h"
#include "ECS/Systems/TimeSystemInterface.h"
#include "ECS/WorldObjects.h"
#include "Locator.h"
#include "Temple/TempleDestruction.h"

using namespace openblack;
using namespace openblack::ecs;
using namespace openblack::ecs::components;
using namespace openblack::ecs::systems;

namespace
{
/// The sound that loops over the temple until the explosion, and the explosion's
constexpr entt::hashed_string k_LoopSound = entt::hashed_string("InGame.sad/171");
constexpr entt::hashed_string k_ExplosionSound = entt::hashed_string("InGame.sad/168");
/// The script the game starts when the local player's temple is destroyed
constexpr const char* k_GameOverScript = "GameOver";

uint32_t MillisecondsPerTurn()
{
	return static_cast<uint32_t>(TimeSystemInterface::k_TurnDuration.count());
}

/// A spot visual at the temple for some seconds, given its player
std::optional<ParticleSystemInterface::EffectId> SpotVisualAt(entt::entity temple, PlayerNames owner, SpotVisualType type,
                                                              int32_t turns)
{
	if (!Locator::particleSystem::has_value())
	{
		return std::nullopt;
	}
	auto& particles = Locator::particleSystem::value();
	const auto& position = Locator::entitiesRegistry::value().Get<const Transform>(temple).position;
	const auto id = particles.StartSpotVisual(type, position, turns, entt::null, 1.0f);
	particles.SetPlayer(id, static_cast<int>(owner));
	return id;
}

/// The temple goes, with its way in
void RemoveTemple(entt::entity temple)
{
	auto& registry = Locator::entitiesRegistry::value();
	std::vector<entt::entity> entrances;
	registry.Each<const TempleEntrance>([temple, &entrances](entt::entity entity, const TempleEntrance& entrance) {
		if (entrance.temple == temple)
		{
			entrances.push_back(entity);
		}
	});
	for (const auto entrance : entrances)
	{
		world_objects::Remove(entrance);
	}
	world_objects::Remove(temple);
}

/// One turn of a temple being destroyed, in the order the game takes its steps
void Step(entt::entity entity, Temple& temple)
{
	namespace td = temple_destruction;
	const float before = temple.destructionClock;
	constexpr float k_SecondsPerMillisecond = 0.001f;
	temple.destructionClock += static_cast<float>(MillisecondsPerTurn()) * k_SecondsPerMillisecond;
	const auto events = td::Between(before, temple.destructionClock);
	// TODO(physics): the local player's heartbeat quickens with the clock, and the heart fades out from fourteen seconds
	// under a shell drawn over it; openblack has no heartbeat, and draws no heart fade or shell yet
	// TODO(physics): the beams across the heart, between random points of its own model, wait on picking points of the
	// heart's model as the targetless beam does
	// A clock started again doesn't start a second loop over the one still playing
	if (events.loopStarts && temple.destructionLoop == entt::null && Locator::audio::has_value())
	{
		const auto& position = Locator::entitiesRegistry::value().Get<const Transform>(entity).position;
		temple.destructionLoop = Locator::audio::value().StartSoundEffect(
		    k_LoopSound.value(), {.position = position, .playType = audio::PlayType::Repeat, .owner = entity});
	}
	if (events.glow)
	{
		temple.destructionGlow = SpotVisualAt(entity, temple.owner, SpotVisualType::MagicFxOnCitadel,
		                                      td::TurnsFor(MillisecondsPerTurn(), td::k_GlowSeconds));
		if (temple.destructionGlow.has_value())
		{
			// The glow is over the heart
			Locator::particleSystem::value().AddTarget(*temple.destructionGlow, entity);
		}
	}
	if (events.explosion)
	{
		if (Locator::audio::has_value())
		{
			auto& audio = Locator::audio::value();
			if (temple.destructionLoop != entt::null)
			{
				audio.StopEmitter(temple.destructionLoop);
				temple.destructionLoop = entt::null;
			}
			const auto& position = Locator::entitiesRegistry::value().Get<const Transform>(entity).position;
			audio.StartSoundEffect(k_ExplosionSound.value(), {.position = position, .owner = entity});
		}
		SpotVisualAt(entity, temple.owner, SpotVisualType::ExplosionCitadel,
		             td::TurnsFor(MillisecondsPerTurn(), td::k_ExplosionSeconds));
	}
	if (events.smoke)
	{
		const float share =
		    Locator::gameRandom::has_value() ? Locator::gameRandom::value().GameFloatRand(td::k_SmokeShareSpread) : 0.0f;
		// TODO(physics): the smoke is shown ten times its size; openblack's spot visuals have no scale of their own yet
		SpotVisualAt(entity, temple.owner, SpotVisualType::EvilSmoke, td::SmokeTurns(MillisecondsPerTurn(), share));
	}
	if (events.end)
	{
		temple.destroying = false;
		temple.destructionClock = 0.0f;
		RemoveTemple(entity);
	}
}
} // namespace

void TempleDestructionSystem::Start(entt::entity temple)
{
	auto& registry = Locator::entitiesRegistry::value();
	auto* component = registry.Valid(temple) ? registry.TryGet<Temple>(temple) : nullptr;
	if (component == nullptr)
	{
		return;
	}
	// Started again, as by a heart healed and destroyed anew while it goes, its clock starts again from nothing
	component->destroying = true;
	component->destructionClock = 0.0f;
	// TODO(physics): the temple's other parts, its worship sites among them, go at once; openblack's temple has no parts
}

void TempleDestructionSystem::ProcessTurn()
{
	if (!Locator::entitiesRegistry::has_value())
	{
		return;
	}
	auto& registry = Locator::entitiesRegistry::value();
	// The local player whose temple is being destroyed has lost: the game's end is played out by its script, once
	// TODO(physics): not in a skirmish or a multiplayer game, which openblack doesn't tell apart yet
	auto& context = registry.Context();
	if (!context.gameOver && Locator::playerSystem::has_value())
	{
		const auto local = Locator::playerSystem::value().GetLocalPlayer();
		bool lost = false;
		registry.Each<const Temple>([local, &lost](entt::entity, const Temple& temple) {
			lost = lost || (temple.owner == local && temple.destroying);
		});
		if (lost)
		{
			context.gameOver = true;
			if (Locator::vm::has_value())
			{
				Locator::vm::value().StartScript(k_GameOverScript, lhvm::ScriptType::All);
			}
		}
	}
	std::vector<entt::entity> destroying;
	registry.Each<const Temple>([&destroying](entt::entity entity, const Temple& temple) {
		if (temple.destroying)
		{
			destroying.push_back(entity);
		}
	});
	for (const auto entity : destroying)
	{
		if (registry.Valid(entity))
		{
			Step(entity, registry.Get<Temple>(entity));
		}
	}
}
