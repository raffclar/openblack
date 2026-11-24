/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "Registry.h"

#include <utility>

#include "Components/Abode.h"
#include "Components/Animal.h"
#include "Components/Creature.h"
#include "Components/Disciple.h"
#include "Components/FixedOutline.h"
#include "Components/Flock.h"
#include "Components/LivingListPlace.h"
#include "Components/LivingReaction.h"
#include "Components/MapCellResident.h"
#include "Components/MapPlace.h"
#include "Components/ScriptControl.h"
#include "Components/SpellDispenser.h"
#include "Components/Town.h"
#include "Components/Villager.h"
#include "KeptStores.h"
#include "LivingTurnOrder.h"
#include "Locator.h"
#include "ReactionGroups.h"
#include "ScriptFlocks.h"
#include "Systems/RenderingSystemInterface.h"
#include "TownCensus.h"

namespace openblack::ecs
{

Registry::Registry()
    : _townCaches(std::make_unique<town_census::Caches>())
    , _livingList(std::make_unique<living_turn::Kept<entt::entity>>())
    , _reactionGroups(std::make_unique<reaction_groups::Counts>())
{
	_registry.ctx().emplace<RegistryContext>();
	// Each entity made is counted as it is made, however it is made
	_registry.on_construct<entt::entity>().connect<&Registry::EntityMade>(*this);
	// The registry also counts each entity that goes. A reader can then know if entities went since it last looked.
	_registry.on_destroy<entt::entity>().connect<&Registry::EntityGone>(*this);
	// Every living that goes leaves its flock first, as each living does when the game deletes it. Thus a flock never
	// keeps a member that is gone, and its leader is always a living that is there. The stores of the flock components
	// exist from the start, so that the removal of a living never adds a store while the registry removes components. The
	// store of the script hold exists from the start for the same reason.
	_registry.storage<components::Flock>();
	_registry.storage<components::InScript>();
	_registry.on_destroy<components::Animal>().connect<&Registry::LivingGone>(*this);
	_registry.on_destroy<components::FlockMember>().connect<&Registry::LivingGone>(*this);
	// The lists of the towns and their members are made again after a change to the things that they list
	ConnectTownMember<components::Villager>();
	ConnectTownMember<components::Abode>();
	ConnectTownMember<components::SpellDispenser>();
	_registry.on_construct<components::Town>().connect<&Registry::TownChanged>(*this);
	_registry.on_update<components::Town>().connect<&Registry::TownChanged>(*this);
	_registry.on_destroy<components::Town>().connect<&Registry::TownChanged>(*this);
	// The counts of the people of the towns follow each villager and each disciple that comes, changes or goes
	_registry.on_construct<components::Villager>().connect<&Registry::CountedVillagerChanged>(*this);
	_registry.on_update<components::Villager>().connect<&Registry::CountedVillagerChanged>(*this);
	_registry.on_destroy<components::Villager>().connect<&Registry::CountedVillagerGoing>(*this);
	_registry.on_construct<components::Disciple>().connect<&Registry::CountedVillagerChanged>(*this);
	_registry.on_update<components::Disciple>().connect<&Registry::CountedVillagerChanged>(*this);
	_registry.on_destroy<components::Disciple>().connect<&Registry::CountedDiscipleGoing>(*this);
	// The list of the living things in the order of their turn follows each living thing that comes or goes
	ConnectLiving<components::Villager>();
	ConnectLiving<components::Animal>();
	ConnectLiving<components::Creature>();
	ConnectLiving<components::LivingListPlace>();
	_registry.on_update<components::LivingListPlace>().connect<&Registry::LivingChanged>(*this);
	// The counts of the members of the reactions follow each reaction component that comes, changes or goes
	_registry.on_construct<components::LivingReaction>().connect<&Registry::ReactionChanged>(*this);
	_registry.on_update<components::LivingReaction>().connect<&Registry::ReactionChanged>(*this);
	_registry.on_destroy<components::LivingReaction>().connect<&Registry::ReactionGoing>(*this);
	// The hash of the world saves again only the rows of these components that changed
	MarkHashRows<components::FixedOutline>();
	MarkHashRows<components::MapCellResident>();
	MarkHashRows<components::MapPlace>();
	// The stores that the rules of life and the walks test most exist from the start, and the registry keeps them
	KeptStores::MakeStores(_registry);
	_kept = std::make_unique<KeptStores>(*this);
}

Registry::~Registry() = default;

Registry::Registry(Drawn drawn)
    : Registry()
{
	_drawn = drawn;
}

void Registry::LivingGone(entt::registry& /*registry*/, entt::entity living)
{
	script_flocks::RemoveAtDeath(*this, living);
}

void Registry::TownMembersChanged() noexcept
{
	++_townMemberChanges;
	town_census::RecountVillagers(*this);
}

void Registry::TownMembersChanged(entt::entity villager)
{
	++_townMemberChanges;
	town_census::VillagerChanged(*this, villager);
}

void Registry::CountedVillagerChanged(entt::registry& /*registry*/, entt::entity entity)
{
	town_census::VillagerChanged(*this, entity);
}

void Registry::CountedVillagerGoing(entt::registry& /*registry*/, entt::entity entity)
{
	town_census::VillagerGoing(*this, entity);
}

void Registry::CountedDiscipleGoing(entt::registry& /*registry*/, entt::entity entity)
{
	town_census::DiscipleGoing(*this, entity);
}

void Registry::LivingChanged(entt::registry& /*registry*/, entt::entity entity)
{
	_livingList->changed.insert(entity);
}

void Registry::ReactionChanged(entt::registry& /*registry*/, entt::entity entity)
{
	reaction_groups::Changed(*this, entity);
}

void Registry::ReactionGoing(entt::registry& /*registry*/, entt::entity entity)
{
	reaction_groups::Going(*this, entity);
}

void Registry::StoresWritten() noexcept
{
	// A direct write to the stores does not mark the rows that it changes
	_rowMarks.Forget();
	_livingList->rebuild = true;
	reaction_groups::Recount(*this);
}

void Registry::Release(entt::entity entity)
{
	ENTT_ASSERT(_registry.orphan(entity), "Non-orphan entity");
	_registry.storage<entt::entity>().erase(entity);
}

void Registry::Destroy(entt::entity entity)
{
	_destroying.publish(entity);
	SetLayoutDirty();
	_registry.destroy(entity);
}

RegistryContext& Registry::Context()
{
	return _registry.ctx().get<RegistryContext>();
}

const RegistryContext& Registry::Context() const
{
	return _registry.ctx().get<const RegistryContext>();
}

void Registry::Reset()
{
	SetLayoutDirty();
	_registry.clear();
	// The store of the ids forgets the ids of the entities that went, so that the next ids are those of a new registry.
	// The components' stores, their signals and the context stay.
	if (std::exchange(_freshIdsAtNextReset, false))
	{
		_registry.storage<entt::entity>().clear();
	}
	_registry.ctx().erase<RegistryContext>();
	_registry.ctx().emplace<RegistryContext>();
	// The next entities can have the ids and the versions of the entities that went
	_rowMarks.Forget();
	_entitiesMade = 0;
	++_resets;
	++_townMemberChanges;
	++_townChanges;
	++_entitiesGone;
}

void Registry::SetDirty()
{
	if (_drawn == Drawn::Yes && Locator::rendereringSystem::has_value())
	{
		Locator::rendereringSystem::value().SetDirty();
	}
}

void Registry::SetLayoutDirty()
{
	// The draw lists of the renderer do not change when an entity of a registry that it does not draw changes
	if (_drawn == Drawn::Yes && Locator::rendereringSystem::has_value())
	{
		Locator::rendereringSystem::value().SetLayoutDirty();
	}
}
} // namespace openblack::ecs
