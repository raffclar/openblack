/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "VillagerFightWatch.h"

#include <algorithm>
#include <optional>

#include "3D/MapCoords.h"
#include "Common/GameRandom.h"
#include "ECS/Components/CreatureArena.h"
#include "ECS/Components/CreatureFight.h"
#include "ECS/Components/LivingAction.h"
#include "ECS/Components/LivingReaction.h"
#include "ECS/Components/Town.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/Villager.h"
#include "ECS/FightWatchRules.h"
#include "ECS/Registry.h"
#include "ECS/Systems/ReactionSystemInterface.h"
#include "ECS/Systems/TimeSystemInterface.h"
#include "ECS/VillagerAnimation.h"
#include "Locator.h"
#include "VillagerCreatureReactions.h"
#include "VillagerHome.h"
#include "VillagerReactions.h"

using namespace openblack;
using namespace openblack::ecs;
using namespace openblack::ecs::components;
using namespace openblack::ecs::systems;

namespace
{
/// Game turns in a second
constexpr uint32_t k_TurnsPerSecond = 10;

ecs::Registry& EntityRegistry()
{
	return Locator::entitiesRegistry::value();
}

uint32_t Random(uint32_t range)
{
	return Locator::gameRandom::has_value() ? Locator::gameRandom::value().GameRand(range) : 0;
}

/// The arena the villager watches, while it is there
std::optional<entt::entity> ArenaOf(entt::entity villager)
{
	const auto thing = villager_reactions::ReactingTo(villager);
	if (!thing.has_value() || !EntityRegistry().AllOf<CreatureArena, Transform>(*thing))
	{
		return std::nullopt;
	}
	return thing;
}

entt::entity TownOf(entt::entity villager)
{
	auto& registry = EntityRegistry();
	const auto* component = registry.TryGet<const Villager>(villager);
	if (component == nullptr || component->town == entt::null || !registry.Valid(component->town) ||
	    !registry.AllOf<Town>(component->town))
	{
		return entt::null;
	}
	return component->town;
}

/// How many of the villagers holding the same reaction joined it after this one
std::optional<uint32_t> PlaceIndexOf(entt::entity villager)
{
	auto& registry = EntityRegistry();
	const auto* mine = registry.TryGet<const LivingReaction>(villager);
	if (mine == nullptr || mine->reaction == 0)
	{
		return std::nullopt;
	}
	uint32_t later = 0;
	registry.Each<const LivingReaction>([&](entt::entity other, const LivingReaction& state) {
		if (other != villager && state.reaction == mine->reaction && state.joined > mine->joined &&
		    registry.AllOf<Villager>(other))
		{
			++later;
		}
	});
	return later;
}

/// The game turn a creature was last struck by a blow in a fight; the start of the game before any
uint32_t StruckTurnOf(entt::entity creature)
{
	const auto* record = EntityRegistry().TryGet<const CreatureFightRecord>(creature);
	return record != nullptr ? record->struckTurn : 0;
}
} // namespace

void ecs::villager_fight_watch::WatchCreaturesFight(LivingAction& action, entt::entity creature)
{
	villager_reactions::StopReacting(action);
	auto& registry = EntityRegistry();
	const auto* fighting = registry.TryGet<const CreatureFighting>(creature);
	if (fighting == nullptr || fighting->arenaEntity == entt::null || !Locator::reactionSystem::has_value())
	{
		return;
	}
	auto& reactions = Locator::reactionSystem::value();
	const auto all = reactions.GetReactions();
	const auto found =
	    std::ranges::find(all, fighting->arenaEntity, [](const auto& reaction) { return reaction.source.initiator; });
	if (found != all.end())
	{
		reactions.TakeUp(found->id, registry.ToEntity(action));
	}
}

uint32_t ecs::villager_fight_watch::InitialiseFightReaction(LivingAction& action)
{
	auto& registry = EntityRegistry();
	const auto villager = registry.ToEntity(action);
	const auto arena = ArenaOf(villager);
	if (!arena.has_value())
	{
		villager_reactions::StopReacting(action);
		return 0;
	}
	const auto index = PlaceIndexOf(villager);
	if (!index.has_value())
	{
		return 0;
	}
	const auto& taken = registry.Get<const CreatureArena>(*arena);
	const auto place = fight_watch::PlaceOnRing(taken.place, taken.radius, fight_watch::PlaceAngle(*index));
	villager_home::SetupMoveTo(action, {map_coords::ToMetres(place.x), map_coords::ToMetres(place.y)},
	                           VillagerStates::PerformFightReaction);
	return 1;
}

uint32_t ecs::villager_fight_watch::PerformFightReaction(LivingAction& action)
{
	auto& registry = EntityRegistry();
	const auto villager = registry.ToEntity(action);
	// The arena gone, the villager stops reacting when its reaction goes with it
	if (const auto arena = ArenaOf(villager); arena.has_value())
	{
		villager_reactions::LookAt(villager, registry.Get<const Transform>(*arena).position,
		                           villager_reactions::LookSpeed::Quick);
	}
	return 1;
}

AnimId ecs::villager_fight_watch::WatchClip(entt::entity villager)
{
	auto& registry = EntityRegistry();
	const auto arena = ArenaOf(villager);
	if (!arena.has_value())
	{
		return AnimId::PStand;
	}
	const auto& taken = registry.Get<const CreatureArena>(*arena);
	if (taken.first == entt::null || taken.second == entt::null || !registry.Valid(taken.first) ||
	    !registry.Valid(taken.second))
	{
		return AnimId::PStand;
	}
	std::optional<fight_watch::TownFeelings> feelings;
	if (const auto town = TownOf(villager); town != entt::null)
	{
		feelings = fight_watch::TownFeelings {.first = villager_creature::AttitudeOf(town, taken.first),
		                                      .second = villager_creature::AttitudeOf(town, taken.second)};
	}
	const auto side = fight_watch::Supports(feelings, Random);
	const auto supported = side == fight_watch::Side::First ? taken.first : taken.second;
	const auto other = side == fight_watch::Side::First ? taken.second : taken.first;
	const auto now = Locator::time::has_value() ? Locator::time::value().GetTurn() : 0u;
	return villager_animation::WatchFightClip(fight_watch::SecondsSince(now, StruckTurnOf(supported), k_TurnsPerSecond),
	                                          fight_watch::SecondsSince(now, StruckTurnOf(other), k_TurnsPerSecond), Random);
}
