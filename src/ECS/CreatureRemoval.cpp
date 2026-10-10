/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "CreatureRemoval.h"

#include <algorithm>
#include <string>
#include <vector>

#include <spdlog/spdlog.h>

#include "Audio/AudioManagerInterface.h"
#include "Creature/PrimaryCreature.h"
#include "ECS/Components/Creature.h"
#include "ECS/Components/CreatureLeash.h"
#include "ECS/Components/CreatureLocomotion.h"
#include "ECS/Components/CreatureObjectAction.h"
#include "ECS/Components/HandOnCreature.h"
#include "ECS/Components/Player.h"
#include "ECS/Components/PlayerCreatures.h"
#include "ECS/Registry.h"
#include "ECS/Systems/CreatureFightSystemInterface.h"
#include "ECS/Systems/CreatureHandSystemInterface.h"
#include "ECS/Systems/CreatureLocomotionSystemInterface.h"
#include "ECS/Systems/CreatureModeSystemInterface.h"
#include "ECS/Systems/CreatureObjectActionSystemInterface.h"
#include "ECS/Systems/DynamicsSystemInterface.h"
#include "ECS/Systems/HandGrabSystemInterface.h"
#include "ECS/Systems/LeashSystemInterface.h"
#include "ECS/Systems/MagicSystemInterface.h"
#include "ECS/Systems/ReactionSystemInterface.h"
#include "ECS/WorldObjects.h"
#include "Locator.h"

using namespace openblack;
using namespace openblack::ecs;
using namespace openblack::ecs::components;

namespace
{

/// The earliest creature the player got that is still theirs
std::optional<entt::entity> PrimaryOf(const Registry& registry, const PlayerCreatures& list, PlayerNames owner,
                                      std::optional<entt::entity> leaving = std::nullopt)
{
	return primary_creature::Primary(list.acquired, [&registry, owner, leaving](entt::entity candidate) {
		if (candidate == leaving || !registry.Valid(candidate))
		{
			return false;
		}
		const auto* body = registry.TryGet<const Creature>(candidate);
		return body != nullptr && body->owner == owner;
	});
}

/// The game's steps, through its systems; each is skipped when its system isn't there
class GameWorld final: public creature_removal::WorldInterface
{
public:
	void LetGoFromHand(entt::entity creature) override
	{
		if (Locator::creatureHandSystem::has_value() && Locator::creatureHandSystem::value().GetCreature() == creature)
		{
			Locator::creatureHandSystem::value().Release();
		}
		if (Locator::handGrabSystem::has_value() && Locator::handGrabSystem::value().GetHeld() == creature)
		{
			Locator::handGrabSystem::value().ForceDrop();
		}
	}

	void EndFight(entt::entity creature) override
	{
		if (Locator::creatureFightSystem::has_value())
		{
			Locator::creatureFightSystem::value().Withdraw(creature);
		}
	}

	void TakeOffLeash(entt::entity creature) override
	{
		if (Locator::leashSystem::has_value())
		{
			Locator::leashSystem::value().SetLeashable(creature, false);
		}
	}

	void ReleaseEffects(entt::entity creature) override
	{
		if (Locator::magicSystem::has_value())
		{
			Locator::magicSystem::value().ReleaseCreatureCast(creature);
		}
		if (Locator::audio::has_value())
		{
			Locator::audio::value().StopOwnedSounds(creature);
		}
	}

	void RemoveReactions(entt::entity creature) override
	{
		if (Locator::reactionSystem::has_value())
		{
			Locator::reactionSystem::value().RemoveFrom(creature);
		}
	}

	void StopActing(entt::entity creature) override
	{
		if (Locator::creatureObjectActionSystem::has_value())
		{
			auto& actions = Locator::creatureObjectActionSystem::value();
			actions.Cancel(creature);
			actions.Drop(creature);
		}
		if (Locator::creatureLocomotionSystem::has_value())
		{
			Locator::creatureLocomotionSystem::value().Stop(creature);
		}
	}

	void LeaveView(entt::entity creature) override
	{
		if (Locator::creatureModeSystem::has_value() && Locator::creatureModeSystem::value().GetCreature() == creature)
		{
			Locator::creatureModeSystem::value().Leave();
		}
	}

	void RemoveFromWorld(entt::entity creature) override
	{
		if (Locator::dynamicsSystem::has_value())
		{
			Locator::dynamicsSystem::value().RemoveObject(creature, false, false);
		}
		// Its fire and its place on the map go with it
		world_objects::Remove(creature);
	}

	void ClaimLeash(entt::entity creature) override
	{
		if (Locator::leashSystem::has_value())
		{
			Locator::leashSystem::value().ClaimOnArrival(creature);
		}
	}
};

} // namespace

std::optional<entt::entity> creature_removal::ForgetInPlayerLists(Registry& registry, entt::entity creature, PlayerNames owner)
{
	std::optional<entt::entity> primary;
	registry.Each<const Player, PlayerCreatures>(
	    [&registry, &primary, creature, owner](entt::entity, const Player& player, PlayerCreatures& list) {
		    std::erase(list.acquired, creature);
		    if (player.name == owner)
		    {
			    primary = PrimaryOf(registry, list, owner);
		    }
	    });
	return primary;
}

size_t creature_removal::ForgetReferences(Registry& registry, entt::entity creature)
{
	size_t cleared = 0;
	// The markers of where its leash sent it
	std::vector<entt::entity> markers;
	registry.Each<const LeashMarker>([&markers, creature](entt::entity entity, const LeashMarker& marker) {
		if (marker.creature == creature)
		{
			markers.push_back(entity);
		}
	});
	cleared += markers.size();
	registry.Destroy(markers.begin(), markers.end());

	// What it holds, should it still hold anything
	std::vector<entt::entity> held;
	registry.Each<const HeldByCreature>([&held, creature](entt::entity entity, const HeldByCreature& holder) {
		if (holder.creature == creature)
		{
			held.push_back(entity);
		}
	});
	for (const auto entity : held)
	{
		registry.Remove<HeldByCreature>(entity);
	}
	cleared += held.size();

	// Creatures that follow it stop following
	registry.Each<CreatureLocomotion>([&cleared, creature](entt::entity, CreatureLocomotion& locomotion) {
		if (locomotion.following == creature)
		{
			locomotion.following.reset();
			++cleared;
		}
	});

	// A hand resting on it
	std::vector<entt::entity> hands;
	registry.Each<const HandOnCreature>([&hands, creature](entt::entity entity, const HandOnCreature& on) {
		if (on.creature == creature)
		{
			hands.push_back(entity);
		}
	});
	for (const auto entity : hands)
	{
		registry.Remove<HandOnCreature>(entity);
	}
	cleared += hands.size();
	return cleared;
}

std::optional<creature_removal::Removed> creature_removal::Remove(Registry& registry, entt::entity creature,
                                                                  WorldInterface& world)
{
	if (!registry.Valid(creature) || !registry.AllOf<Creature>(creature))
	{
		return std::nullopt;
	}
	Removed removed {.owner = registry.Get<const Creature>(creature).owner};
	registry.Each<const Player, const PlayerCreatures>([&](entt::entity, const Player& player, const PlayerCreatures& list) {
		if (player.name == removed.owner)
		{
			removed.wasPrimary = PrimaryOf(registry, list, removed.owner) == creature;
		}
	});

	// The hand and a fight let go of it before the game's own deletion starts
	world.LetGoFromHand(creature);
	world.EndFight(creature);
	// Then as the game deletes a creature: its leash comes off, its miracles and sounds stop, and its player forgets it
	world.TakeOffLeash(creature);
	world.ReleaseEffects(creature);
	removed.newPrimary = ForgetInPlayerLists(registry, creature, removed.owner);
	// The reactions it set off, then what it does as a living thing: its actions, its walk and what it carries
	world.RemoveReactions(creature);
	world.StopActing(creature);
	world.LeaveView(creature);
	removed.referencesCleared = ForgetReferences(registry, creature);
	// Last its place in the world
	world.RemoveFromWorld(creature);

	if (removed.wasPrimary && removed.newPrimary.has_value())
	{
		world.ClaimLeash(*removed.newPrimary);
	}
	if (const auto logger = spdlog::get("game"); logger != nullptr)
	{
		SPDLOG_LOGGER_INFO(logger, "Creature {} removed from the game{}", entt::to_integral(creature),
		                   removed.newPrimary.has_value() ? fmt::format("; creature {} is its player's primary one",
		                                                                entt::to_integral(*removed.newPrimary))
		                                                  : std::string {});
	}
	return removed;
}

std::unique_ptr<creature_removal::WorldInterface> creature_removal::MakeGameWorld()
{
	return std::make_unique<GameWorld>();
}

std::optional<creature_removal::Removed> creature_removal::RemoveFromGame(entt::entity creature)
{
	if (!Locator::entitiesRegistry::has_value())
	{
		return std::nullopt;
	}
	auto world = MakeGameWorld();
	return Remove(Locator::entitiesRegistry::value(), creature, *world);
}
