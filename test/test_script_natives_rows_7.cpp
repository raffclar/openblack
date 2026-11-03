/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <glm/vec2.hpp>
#include <gtest/gtest.h>

#include "ECS/BuildingSiteList.h"
#include "ECS/Components/Animal.h"
#include "ECS/Components/Construction.h"
#include "ECS/Components/CreatureLocomotion.h"
#include "ECS/Components/Spell.h"
#include "ECS/Components/Villager.h"
#include "ECS/Components/WallHug.h"
#include "ECS/Registry.h"
#include "ECS/ScriptDestination.h"
#include "ECS/SpellCreator.h"

using namespace openblack;
using namespace openblack::ecs::components;

namespace
{

/// A town for the site lists: only its number counts
entt::entity TownNumbered(uint32_t number)
{
	return static_cast<entt::entity>(number);
}

} // namespace

TEST(ScriptNativesRows7, APointOfAWalkIsItsMapPositionWithNoHeight)
{
	const auto place = ecs::script_destination::OfPoint({1.0f, -1.0f});
	EXPECT_EQ(place.x, map_coords::ToFixed(1.0f));
	EXPECT_EQ(place.z, map_coords::ToFixed(-1.0f));
	EXPECT_EQ(place.x, 6553);
	EXPECT_EQ(place.z, -6553);
	EXPECT_EQ(place.altitude, 0.0f);
}

TEST(ScriptNativesRows7, TheDestinationOfEachLivingKind)
{
	ecs::Registry registry;
	const auto creature = registry.Create();
	auto& locomotion = registry.Assign<CreatureLocomotion>(creature);
	locomotion.scriptDestination = {.x = 100, .z = 200, .altitude = 3.0f};
	const auto animal = registry.Create();
	auto& beast = registry.Assign<Animal>(animal);
	beast.move.goal = {300, 400};
	beast.goalHeight = 5.0f;
	const auto villager = registry.Create();
	registry.Assign<Villager>(villager);
	auto& walk = registry.Assign<WallHug>(villager);
	walk.goalPlace = {500, 600};
	const auto rock = registry.Create();
	registry.Assign<WallHug>(rock);

	EXPECT_EQ(ecs::script_destination::Of(registry, creature), (map_coords::MapCoords {100, 200, 3.0f}));
	EXPECT_EQ(ecs::script_destination::Of(registry, animal), (map_coords::MapCoords {300, 400, 5.0f}));
	EXPECT_EQ(ecs::script_destination::Of(registry, villager), (map_coords::MapCoords {500, 600, 0.0f}));
	EXPECT_FALSE(ecs::script_destination::Of(registry, rock).has_value());
	EXPECT_FALSE(ecs::script_destination::Of(registry, entt::null).has_value());
}

TEST(ScriptNativesRows7, TheCreatorOfAMiracleIsTheThingOrTheIconThatCastIt)
{
	const auto creature = static_cast<entt::entity>(7);
	const auto icon = static_cast<entt::entity>(9);
	using Kind = SpellCaster::Kind;
	EXPECT_EQ(ecs::spell_creator::CreatorOf({.kind = Kind::Creature, .entity = creature}), creature);
	EXPECT_EQ(ecs::spell_creator::CreatorOf({.kind = Kind::Object, .entity = creature}), creature);
	EXPECT_EQ(ecs::spell_creator::CreatorOf({.kind = Kind::Player, .madeBySpellIcon = true, .icon = icon}), icon);
	// A player's own miracle, and a miracle of a script for the neutral player, have the player as the creator
	EXPECT_EQ(ecs::spell_creator::CreatorOf({.kind = Kind::Player, .withoutIcon = true}), entt::entity {entt::null});
	EXPECT_EQ(ecs::spell_creator::CreatorOf({.kind = Kind::Player, .player = PlayerNames::NEUTRAL}), entt::entity {entt::null});
	EXPECT_EQ(ecs::spell_creator::CreatorOf({.kind = Kind::None}), entt::entity {entt::null});
}

TEST(ScriptNativesRows7, OnlyTheCreatorCastTheMiracle)
{
	const auto creature = static_cast<entt::entity>(7);
	Spell spell;
	spell.creator = creature;
	EXPECT_TRUE(ecs::spell_creator::CastBy(spell, creature));
	EXPECT_FALSE(ecs::spell_creator::CastBy(spell, static_cast<entt::entity>(8)));
	EXPECT_FALSE(ecs::spell_creator::CastBy(spell, entt::null));
	// A miracle whose caster is gone has no creator, so no thing cast it
	spell.creator = entt::null;
	EXPECT_FALSE(ecs::spell_creator::CastBy(spell, entt::null));
}

TEST(ScriptNativesRows7, ASharedSiteIsInTheListOfTheOtherTown)
{
	BuildingSites sites;
	auto& site = ecs::building_site_list::Open(sites, TownNumbered(1));
	site.number = 4;
	EXPECT_EQ(ecs::building_site_list::Find(sites, TownNumbered(2)), nullptr);
	EXPECT_TRUE(ecs::building_site_list::Share(site, TownNumbered(2), 9));
	// No town gets the site twice, and the own town does not get it as a shared site
	EXPECT_FALSE(ecs::building_site_list::Share(site, TownNumbered(2), 10));
	EXPECT_FALSE(ecs::building_site_list::Share(site, TownNumbered(1), 11));
	EXPECT_EQ(ecs::building_site_list::Find(sites, TownNumbered(2)), &sites.sites.front());
	EXPECT_EQ(ecs::building_site_list::Find(sites, TownNumbered(1)), &sites.sites.front());
	EXPECT_EQ(ecs::building_site_list::NumberFor(sites.sites.front(), TownNumbered(1)), 4u);
	EXPECT_EQ(ecs::building_site_list::NumberFor(sites.sites.front(), TownNumbered(2)), 9u);
	EXPECT_EQ(ecs::building_site_list::NumberFor(sites.sites.front(), TownNumbered(3)), 0u);
	EXPECT_TRUE(ecs::building_site_list::IsSharedWith(sites.sites.front(), TownNumbered(2)));
	EXPECT_FALSE(ecs::building_site_list::IsSharedWith(sites.sites.front(), TownNumbered(1)));
}

TEST(ScriptNativesRows7, ATownFindsItsOwnSiteBeforeASharedSite)
{
	BuildingSites sites;
	ecs::building_site_list::Open(sites, TownNumbered(2)).number = 1;
	auto& newest = ecs::building_site_list::Open(sites, TownNumbered(1));
	EXPECT_TRUE(ecs::building_site_list::Share(newest, TownNumbered(2), 2));
	const auto* found = ecs::building_site_list::Find(sites, TownNumbered(2));
	ASSERT_NE(found, nullptr);
	EXPECT_EQ(found->town, TownNumbered(2));
}

TEST(ScriptNativesRows7, ATownThatRemovesASharedSiteClosesItForAllTowns)
{
	BuildingSites sites;
	auto& site = ecs::building_site_list::Open(sites, TownNumbered(1));
	site.workers = {static_cast<entt::entity>(20), static_cast<entt::entity>(21)};
	EXPECT_TRUE(ecs::building_site_list::Share(site, TownNumbered(2), 1));
	const auto closed = ecs::building_site_list::Close(sites, TownNumbered(2));
	ASSERT_TRUE(closed.has_value());
	EXPECT_EQ(closed->town, TownNumbered(1));
	EXPECT_EQ(closed->workers.size(), 2u);
	EXPECT_TRUE(sites.sites.empty());
	EXPECT_EQ(ecs::building_site_list::Find(sites, TownNumbered(1)), nullptr);
}
