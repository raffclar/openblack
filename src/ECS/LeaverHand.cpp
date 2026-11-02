/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "LeaverHand.h"

#include "ECS/Components/LivingAction.h"
#include "ECS/Components/Villager.h"
#include "ECS/Registry.h"
#include "ECS/Systems/HandGrabSystemInterface.h"
#include "ECS/Systems/LeashSystemInterface.h"
#include "ECS/Systems/LivingActionSystemInterface.h"
#include "ECS/Systems/MagicSystemInterface.h"
#include "ECS/Systems/SpellIconSystemInterface.h"
#include "ECS/Systems/WorshipSiteSystemInterface.h"
#include "ECS/WorldObjects.h"
#include "Locator.h"

namespace openblack::ecs::leaver_hand
{

Release ReleaseHand(LeaverHandWorldInterface& world, PlayerNames player)
{
	Release release;
	release.leashOff = world.TakeOffLeash(player);
	world.CancelCharges(player);
	// The hand holds one thing at a time: a miracle seed or another thing
	if (const auto seed = world.HeldSeed(player); seed.has_value() && world.IsAvailable(*seed))
	{
		world.DeleteSeed(*seed);
		release.deleted = seed;
	}
	else if (const auto thing = world.HeldThing(player); thing.has_value() && world.IsAvailable(*thing))
	{
		world.DeleteThing(*thing);
		release.deleted = thing;
	}
	return release;
}

namespace
{

class GameLeaverHandWorld final: public LeaverHandWorldInterface
{
public:
	bool TakeOffLeash(PlayerNames player) override
	{
		return Locator::leashSystem::has_value() && Locator::leashSystem::value().TakeOffAtLeave(player);
	}

	void CancelCharges(PlayerNames player) override
	{
		if (!Locator::worshipSiteSystem::has_value() || !Locator::spellIconSystem::has_value())
		{
			return;
		}
		if (const auto temple = Locator::worshipSiteSystem::value().TempleOf(player); temple != entt::null)
		{
			Locator::spellIconSystem::value().CancelChargesFor(temple, player);
		}
	}

	[[nodiscard]] std::optional<entt::entity> HeldSeed(PlayerNames player) const override
	{
		return Locator::magicSystem::has_value() ? Locator::magicSystem::value().GetHeldSeedOf(player) : std::nullopt;
	}

	[[nodiscard]] std::optional<entt::entity> HeldThing(PlayerNames player) const override
	{
		return Locator::handGrabSystem::has_value() ? Locator::handGrabSystem::value().GetHeldOf(player) : std::nullopt;
	}

	[[nodiscard]] bool IsAvailable(entt::entity thing) const override
	{
		const auto& registry = Locator::entitiesRegistry::value();
		if (!registry.Valid(thing))
		{
			return false;
		}
		// The world cannot deal with a villager that is about to die
		const auto* action =
		    registry.AllOf<components::Villager>(thing) ? registry.TryGet<const components::LivingAction>(thing) : nullptr;
		return action == nullptr || !Locator::livingActionSystem::has_value() ||
		       Locator::livingActionSystem::value().VillagerGetState(*action, components::LivingAction::Index::Final) !=
		           VillagerStates::Dying;
	}

	void DeleteSeed(entt::entity seed) override
	{
		if (Locator::magicSystem::has_value())
		{
			Locator::magicSystem::value().DeleteSeed(seed);
		}
	}

	void DeleteThing(entt::entity thing) override
	{
		// The hand sees at its next turn that the thing went, and then holds nothing
		world_objects::Remove(thing);
	}
};

} // namespace

Release ReleaseHandInGame(PlayerNames player)
{
	GameLeaverHandWorld world;
	return ReleaseHand(world, player);
}

} // namespace openblack::ecs::leaver_hand
