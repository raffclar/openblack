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
#include <vector>

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

// Compassion for a town gives actions; compassion for a creature gives none
TEST(CreatureActivity, OnlyCompassionForATownGivesActions)
{
	EXPECT_TRUE(CompassionHasActions(Whole::Town));
	EXPECT_FALSE(CompassionHasActions(Whole::Creature));
}

// To impress, it acts on a villager that does not dance
TEST(CreatureActivity, ToImpressItActsOnAVillagerThatDoesNotDance)
{
	EXPECT_TRUE(ThingSuits(Desire::Impress, {.kind = Thing::Villager}));
	EXPECT_FALSE(ThingSuits(Desire::Impress, {.kind = Thing::Villager, .dancing = true}));
	EXPECT_FALSE(ThingSuits(Desire::Impress, {.kind = Thing::Building}));
	EXPECT_FALSE(ThingSuits(Desire::Impress, {.kind = Thing::Villager, .canActOn = false}));
	// A totem statue can be impressed, while its data lets a creature act on it
	EXPECT_TRUE(ThingSuits(Desire::Impress, {.kind = Thing::TotemStatue}));
	EXPECT_FALSE(ThingSuits(Desire::Impress, {.kind = Thing::TotemStatue, .canActOn = false}));
	EXPECT_TRUE(ThingSuits(Desire::Compassion, {.kind = Thing::TotemStatue}));
	EXPECT_TRUE(ThingSuits(Desire::Anger, {.kind = Thing::TotemStatue, .attackable = true}));
	EXPECT_FALSE(ThingSuits(Desire::Anger, {.kind = Thing::TotemStatue}));
}

// For anger, it acts on what its data lets a creature attack, and on every tree
TEST(CreatureActivity, ForAngerItActsOnWhatItCanAttack)
{
	EXPECT_TRUE(ThingSuits(Desire::Anger, {.kind = Thing::Villager, .attackable = true}));
	EXPECT_FALSE(ThingSuits(Desire::Anger, {.kind = Thing::Villager}));
	EXPECT_FALSE(ThingSuits(Desire::Anger, {.kind = Thing::Villager, .attackable = true, .dead = true}));
	EXPECT_FALSE(ThingSuits(Desire::Anger, {.kind = Thing::Animal, .attackable = true, .dead = true}));
	EXPECT_TRUE(ThingSuits(Desire::Anger, {.kind = Thing::Tree}));
	EXPECT_TRUE(ThingSuits(Desire::Anger, {.kind = Thing::Building, .attackable = true}));
	EXPECT_FALSE(ThingSuits(Desire::Anger, {.kind = Thing::Building}));
	EXPECT_FALSE(ThingSuits(Desire::Anger, {.kind = Thing::Tree, .canActOn = false}));
}

// For compassion, it acts on a villager that a hand can pick up, or on any other object
TEST(CreatureActivity, ForCompassionItActsOnVillagersAndBuildings)
{
	EXPECT_TRUE(ThingSuits(Desire::Compassion, {.kind = Thing::Villager}));
	EXPECT_FALSE(ThingSuits(Desire::Compassion, {.kind = Thing::Villager, .pickable = false}));
	EXPECT_TRUE(ThingSuits(Desire::Compassion, {.kind = Thing::Building}));
	EXPECT_TRUE(ThingSuits(Desire::Compassion, {.kind = Thing::Animal}));
	EXPECT_FALSE(ThingSuits(Desire::Compassion, {.kind = Thing::Spell}));
}

// To get high, it acts only on a magic mushroom
TEST(CreatureActivity, ToGetHighItActsOnlyOnAMagicMushroom)
{
	EXPECT_TRUE(ThingSuits(Desire::GetHigh, {.kind = Thing::Building, .mushroom = true}));
	EXPECT_TRUE(ThingSuits(Desire::GetHigh, {.kind = Thing::MobileObject, .mushroom = true}));
	EXPECT_FALSE(ThingSuits(Desire::GetHigh, {.kind = Thing::Villager}));
	EXPECT_FALSE(ThingSuits(Desire::GetHigh, {.kind = Thing::Animal}));
	EXPECT_FALSE(ThingSuits(Desire::GetHigh, {.kind = Thing::Rock}));
	EXPECT_FALSE(ThingSuits(Desire::GetHigh, {.kind = Thing::MobileObject}));
	EXPECT_FALSE(ThingSuits(Desire::GetHigh, {.kind = Thing::Building, .canActOn = false, .mushroom = true}));
}

// For anger, a rock always suits, a creature that is itself or a friend does not
TEST(CreatureActivity, ForAngerARockSuitsAndAFriendDoesNot)
{
	EXPECT_TRUE(ThingSuits(Desire::Anger, {.kind = Thing::Rock}));
	EXPECT_TRUE(ThingSuits(Desire::Anger, {.kind = Thing::Creature, .attackable = true}));
	EXPECT_FALSE(ThingSuits(Desire::Anger, {.kind = Thing::Creature, .attackable = true, .itself = true}));
	EXPECT_FALSE(ThingSuits(Desire::Anger, {.kind = Thing::Creature, .attackable = true, .friendOfCreature = true}));
	EXPECT_TRUE(ThingSuits(Desire::Anger, {.kind = Thing::MobileObject, .attackable = true}));
	EXPECT_FALSE(ThingSuits(Desire::Anger, {.kind = Thing::Spell, .attackable = true}));
}

// It plays with what lies about, statics, rocks, trees and creatures, and with living things whose data allows it
TEST(CreatureActivity, ItPlaysWithThingsThatLieAboutAndWithLivingThingsWhoseDataAllowsIt)
{
	EXPECT_TRUE(ThingSuits(Desire::Play, {.kind = Thing::MobileObject}));
	EXPECT_TRUE(ThingSuits(Desire::Play, {.kind = Thing::MobileStatic}));
	EXPECT_TRUE(ThingSuits(Desire::Play, {.kind = Thing::Creature}));
	EXPECT_TRUE(ThingSuits(Desire::Play, {.kind = Thing::Villager, .playable = true}));
	EXPECT_FALSE(ThingSuits(Desire::Play, {.kind = Thing::Villager}));
	EXPECT_FALSE(ThingSuits(Desire::Play, {.kind = Thing::Animal, .playable = true, .dead = true}));
	EXPECT_FALSE(ThingSuits(Desire::Play, {.kind = Thing::Building}));
	EXPECT_FALSE(ThingSuits(Desire::Play, {.kind = Thing::Other}));
}

// Fear is of creatures, miracles and the animals that frighten
TEST(CreatureActivity, FearIsOfCreaturesMiraclesAndAnimalsThatFrighten)
{
	EXPECT_TRUE(ThingSuits(Desire::Fear, {.kind = Thing::Creature}));
	EXPECT_TRUE(ThingSuits(Desire::Fear, {.kind = Thing::Spell}));
	EXPECT_TRUE(ThingSuits(Desire::Fear, {.kind = Thing::Animal, .frightening = true}));
	EXPECT_FALSE(ThingSuits(Desire::Fear, {.kind = Thing::Animal}));
	EXPECT_FALSE(ThingSuits(Desire::Fear, {.kind = Thing::Villager}));
}

// It looks over what its data allows and is not dead
TEST(CreatureActivity, ItLooksOverWhatItsDataAllows)
{
	EXPECT_TRUE(ThingSuits(Desire::Curiosity, {.kind = Thing::Building, .inspectable = true}));
	EXPECT_FALSE(ThingSuits(Desire::Curiosity, {.kind = Thing::Building}));
	EXPECT_FALSE(ThingSuits(Desire::Curiosity, {.kind = Thing::Villager, .inspectable = true, .dead = true}));
	EXPECT_FALSE(ThingSuits(Desire::Curiosity, {.kind = Thing::Spell, .inspectable = true}));
}

// It poos only on a tall thing that does not live
TEST(CreatureActivity, ItPoosOnlyOnATallThingThatDoesNotLive)
{
	EXPECT_TRUE(ThingSuits(Desire::Poo, {.kind = Thing::Building, .height = 1.6f}));
	EXPECT_FALSE(ThingSuits(Desire::Poo, {.kind = Thing::Building, .height = k_TallestThingNotToPooOn}));
	EXPECT_FALSE(ThingSuits(Desire::Poo, {.kind = Thing::Villager, .height = 2.0f}));
}

// It rests by a creature or by a thing set into the land, but not by a field or a fish farm
TEST(CreatureActivity, ItRestsByACreatureOrAThingSetIntoTheLand)
{
	for (const auto desire : {Desire::Tiredness, Desire::Rest})
	{
		EXPECT_TRUE(ThingSuits(desire, {.kind = Thing::Creature}));
		EXPECT_TRUE(ThingSuits(desire, {.kind = Thing::Building}));
		EXPECT_TRUE(ThingSuits(desire, {.kind = Thing::Tree}));
		EXPECT_FALSE(ThingSuits(desire, {.kind = Thing::Field}));
		EXPECT_FALSE(ThingSuits(desire, {.kind = Thing::FishFarm}));
		EXPECT_FALSE(ThingSuits(desire, {.kind = Thing::MobileObject}));
		EXPECT_FALSE(ThingSuits(desire, {.kind = Thing::Villager}));
	}
}

// It brings home what it can pick up, and the desires about other creatures act only on creatures
TEST(CreatureActivity, ItBringsHomeWhatItCanPickUpAndBefriendsOnlyCreatures)
{
	EXPECT_TRUE(ThingSuits(Desire::BringStuffHome, {.kind = Thing::MobileObject, .creatureCanPickUp = true}));
	EXPECT_FALSE(ThingSuits(Desire::BringStuffHome, {.kind = Thing::MobileObject}));
	EXPECT_TRUE(ThingSuits(Desire::BeFriends, {.kind = Thing::Creature}));
	EXPECT_FALSE(ThingSuits(Desire::MissFriend, {.kind = Thing::Villager}));
	// A desire without a rule of its own acts on any thing that lets a creature act on it
	EXPECT_TRUE(ThingSuits(Desire::Hunger, {.kind = Thing::Villager}));
	EXPECT_FALSE(ThingSuits(Desire::Hunger, {.kind = Thing::Villager, .canActOn = false}));
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

// "CREATURE_FORCE_FRIENDS true Gremlin Ogre": each creature goes in the list of the other, once
TEST(CreatureActivity, AScriptMakesTwoCreaturesFriendsBothWays)
{
	std::vector<uint32_t> ofGremlin;
	std::vector<uint32_t> ofOgre;
	EXPECT_FALSE(creature_activity::AreFriends(ofGremlin, 3, ofOgre, 7));
	creature_activity::MakeFriends(ofGremlin, 3, ofOgre, 7);
	creature_activity::MakeFriends(ofOgre, 7, ofGremlin, 3);
	EXPECT_EQ(ofGremlin, std::vector<uint32_t> {7});
	EXPECT_EQ(ofOgre, std::vector<uint32_t> {3});
	EXPECT_TRUE(creature_activity::AreFriends(ofGremlin, 3, ofOgre, 7));
	EXPECT_TRUE(creature_activity::AreFriends(ofOgre, 7, ofGremlin, 3));
	// Either list is enough
	EXPECT_TRUE(creature_activity::AreFriends({}, 3, ofOgre, 7));
	EXPECT_FALSE(creature_activity::AreFriends(ofGremlin, 3, {}, 9));
}
