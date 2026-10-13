/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <array>
#include <optional>

#include <gtest/gtest.h>

#include "Creature/CreatureActivity.h"
#include "Creature/CreatureDecisionTree.h"
#include "Creature/CreaturePlanner.h"

using namespace openblack;
using namespace openblack::creature_activity;
using creature_desires::Desire;

// Only towns, forests, temples, creatures and flocks are whole things
TEST(CreatureActivity, TheWholeThingsAreTownsForestsTemplesCreaturesAndFlocks)
{
	namespace types = creature_tree::belief_types;
	EXPECT_EQ(WholeOf(types::k_Town), Whole::Town);
	EXPECT_EQ(WholeOf(types::k_Forest), Whole::Forest);
	EXPECT_EQ(WholeOf(types::k_Citadel), Whole::Citadel);
	EXPECT_EQ(WholeOf(types::k_Creature), Whole::Creature);
	EXPECT_EQ(WholeOf(types::k_Flock), Whole::Flock);
	EXPECT_EQ(WholeOf(types::k_Villager), std::nullopt);
	EXPECT_EQ(WholeOf(types::k_Abode), std::nullopt);
}

// The desire to impress is about a town, but not a town of its own player that is loyal to that player
TEST(CreatureActivity, ItImpressesTownsThatAreNotLoyalToItsPlayer)
{
	EXPECT_TRUE(CanBeAbout(Desire::Impress, {.kind = Whole::Town}, false));
	EXPECT_TRUE(CanBeAbout(Desire::Impress, {.kind = Whole::Town, .samePlayer = true}, false));
	EXPECT_TRUE(CanBeAbout(Desire::Impress, {.kind = Whole::Town, .believesInPlayer = true}, false));
	EXPECT_FALSE(CanBeAbout(Desire::Impress, {.kind = Whole::Town, .samePlayer = true, .believesInPlayer = true}, false));
	EXPECT_FALSE(CanBeAbout(Desire::Impress, {.kind = Whole::Creature}, false));
	EXPECT_FALSE(CanBeAbout(Desire::Impress, {.kind = Whole::Forest}, false));
}

// Anger is about the things of other players, the town of its own player only while the player looks at it
TEST(CreatureActivity, AngerIsAboutTheThingsOfOtherPlayers)
{
	EXPECT_TRUE(CanBeAbout(Desire::Anger, {.kind = Whole::Town}, false));
	EXPECT_TRUE(CanBeAbout(Desire::Anger, {.kind = Whole::Forest}, false));
	EXPECT_TRUE(CanBeAbout(Desire::Anger, {.kind = Whole::Flock}, false));
	EXPECT_TRUE(CanBeAbout(Desire::Anger, {.kind = Whole::Citadel}, false));
	EXPECT_FALSE(CanBeAbout(Desire::Anger, {.kind = Whole::Creature}, false));
	EXPECT_TRUE(CanBeAbout(Desire::Anger, {.kind = Whole::Creature, .attackable = true}, false));
	// Its own player's things: never until it may attack them
	EXPECT_FALSE(CanBeAbout(Desire::Anger, {.kind = Whole::Town, .samePlayer = true, .lookedAt = true}, false));
	EXPECT_FALSE(CanBeAbout(Desire::Anger, {.kind = Whole::Forest, .samePlayer = true}, false));
	// Then a town only while the player looks, and never its player's temple
	EXPECT_TRUE(CanBeAbout(Desire::Anger, {.kind = Whole::Town, .samePlayer = true, .lookedAt = true}, true));
	EXPECT_FALSE(CanBeAbout(Desire::Anger, {.kind = Whole::Town, .samePlayer = true}, true));
	EXPECT_TRUE(CanBeAbout(Desire::Anger, {.kind = Whole::Forest, .samePlayer = true}, true));
	EXPECT_FALSE(CanBeAbout(Desire::Anger, {.kind = Whole::Citadel, .samePlayer = true}, true));
}

// Compassion is about towns and creatures; the desires about creatures only about creatures
TEST(CreatureActivity, CompassionAndTheDesiresAboutCreatures)
{
	EXPECT_TRUE(CanBeAbout(Desire::Compassion, {.kind = Whole::Town}, false));
	EXPECT_TRUE(CanBeAbout(Desire::Compassion, {.kind = Whole::Creature}, false));
	EXPECT_FALSE(CanBeAbout(Desire::Compassion, {.kind = Whole::Flock}, false));
	EXPECT_TRUE(CanBeAbout(Desire::BeFriends, {.kind = Whole::Creature}, false));
	EXPECT_FALSE(CanBeAbout(Desire::BeFriends, {.kind = Whole::Town}, false));
	EXPECT_FALSE(CanBeAbout(Desire::Hunger, {.kind = Whole::Creature}, false));
}

// To impress, it acts on a villager that does not dance
TEST(CreatureActivity, ToImpressItActsOnAVillagerThatDoesNotDance)
{
	EXPECT_TRUE(PartSuits(Desire::Impress, {.kind = Part::Villager}));
	EXPECT_FALSE(PartSuits(Desire::Impress, {.kind = Part::Villager, .dancing = true}));
	EXPECT_FALSE(PartSuits(Desire::Impress, {.kind = Part::Building}));
	EXPECT_FALSE(PartSuits(Desire::Impress, {.kind = Part::Villager, .canActOn = false}));
}

// For anger, it acts on what its data lets a creature attack, and on every tree
TEST(CreatureActivity, ForAngerItActsOnWhatItCanAttack)
{
	EXPECT_TRUE(PartSuits(Desire::Anger, {.kind = Part::Villager, .attackable = true}));
	EXPECT_FALSE(PartSuits(Desire::Anger, {.kind = Part::Villager}));
	EXPECT_FALSE(PartSuits(Desire::Anger, {.kind = Part::Villager, .attackable = true, .dead = true}));
	EXPECT_FALSE(PartSuits(Desire::Anger, {.kind = Part::Animal, .attackable = true, .dead = true}));
	EXPECT_TRUE(PartSuits(Desire::Anger, {.kind = Part::Tree}));
	EXPECT_TRUE(PartSuits(Desire::Anger, {.kind = Part::Building, .attackable = true}));
	EXPECT_FALSE(PartSuits(Desire::Anger, {.kind = Part::Building}));
	EXPECT_FALSE(PartSuits(Desire::Anger, {.kind = Part::Tree, .canActOn = false}));
}

// For compassion, it acts on a villager that a hand can pick up, or on any other part
TEST(CreatureActivity, ForCompassionItActsOnVillagersAndBuildings)
{
	EXPECT_TRUE(PartSuits(Desire::Compassion, {.kind = Part::Villager}));
	EXPECT_FALSE(PartSuits(Desire::Compassion, {.kind = Part::Villager, .pickable = false}));
	EXPECT_TRUE(PartSuits(Desire::Compassion, {.kind = Part::Building}));
}

// The first of the most useful wins, and nothing that is worth nothing
TEST(CreatureActivity, TheFirstOfTheMostUsefulWins)
{
	EXPECT_EQ(MostUseful(std::array {0.2f, 0.5f, 0.5f, 0.1f}), 1u);
	EXPECT_EQ(MostUseful(std::array {0.0f, 0.0f}), std::nullopt);
	EXPECT_EQ(MostUseful(std::array<float, 0> {}), std::nullopt);
}

// Anger counts what it acts on; every other desire counts the default
TEST(CreatureActivity, OnlyAngerCountsThePartItActsOn)
{
	EXPECT_FLOAT_EQ(ActedOnUsefulness(Desire::Anger, 0.4f), 0.4f);
	EXPECT_FLOAT_EQ(ActedOnUsefulness(Desire::Impress, 0.4f), creature_planner::k_DefaultUsefulness);
	EXPECT_FLOAT_EQ(ActedOnUsefulness(Desire::BeFriends, 0.4f), creature_planner::k_DefaultUsefulness);
}
