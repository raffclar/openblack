/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cstdint>

#include <string_view>
#include <vector>

#include "Creature/CreatureDesires.h"
#include "TestbedScenarioRegistry.h"

using namespace openblack;
using namespace openblack::testbed_scenarios;

namespace
{
using Kind = Command::Kind;

/// The game turns in a second
constexpr uint32_t k_Turns = 10;

/// The camera looks at the creature at the middle of the map from its side, with the thing in front of it in view
const Framing k_SideView {.shot = Shot::Placed, .eye = {40.0f, 14.0f, -10.0f}, .look = {0.0f, 6.0f, -10.0f}};

/// The player's tiger at the middle of the map, facing south towards the thing in front of it
CreatureSetup Watcher()
{
	return {.label = "watcher", .offset = {0.0f, 0.0f}, .facingDegrees = 0.0f};
}

/// A small evil tiger of the player that wants only to scratch, so that it stays where it stands, facing east
CreatureSetup EvilAtRest(glm::vec2 offset)
{
	std::vector<DesireOverride> desires;
	for (size_t i = 0; i < creature_desires::k_DesireCount; ++i)
	{
		const auto desire = static_cast<creature_desires::Desire>(i);
		desires.push_back({.desire = desire, .fraction = desire == creature_desires::Desire::Scratch ? 1.0f : 0.0f});
	}
	return {.label = "evil",
	        .offset = offset,
	        .facingDegrees = 90.0f,
	        .alignment = -1.0f,
	        .size = 0.3f,
	        .needs = {.energy = 1.0f, .exhaustion = 0.0f, .dehydration = 0.0f, .poo = 0.0f, .life = 1.0f, .warmth = 0.0f},
	        .desires = std::move(desires),
	        .hold = true};
}

/// A check that the creature keeps the kind of reaction at some read from a turn to a later turn
checks::Check Keeps(std::string_view label, size_t creature, Reaction kind, uint32_t from, uint32_t by)
{
	const auto value = static_cast<double>(kind);
	return {.label = label,
	        .quantity = checks::Quantity::CreatureReaction,
	        .index = creature,
	        .afterTurns = from,
	        .byTurn = by,
	        .atLeast = value,
	        .atMost = value};
}

/// A check that the creature keeps no reaction at a turn
checks::Check KeepsNone(std::string_view label, size_t creature, uint32_t turn)
{
	return {.label = label,
	        .quantity = checks::Quantity::CreatureReaction,
	        .index = creature,
	        .afterTurns = turn,
	        .atLeast = 0.0,
	        .atMost = 0.0};
}

/// What changes in the scenario where a script has told the creature to take no notice of reactions
struct Ignored
{
	std::string_view id;
	std::string_view name;
	std::string_view expected;
	/// The creature that takes no notice
	size_t creature {0};
	/// The turns at which it keeps no reaction
	std::vector<uint32_t> turns;
};

/// Adds the scenario, and the same scenario where a script tells the creature to take no notice of reactions as it
/// starts
void AddWithNoticeOnAndOff(std::vector<Scenario>& all, Scenario scenario, const Ignored& ignored)
{
	all.push_back(scenario);
	scenario.id = ignored.id;
	scenario.name = ignored.name;
	scenario.expected = ignored.expected;
	scenario.commands.insert(scenario.commands.begin(),
	                         {.kind = Kind::ScriptReaction, .creature = ignored.creature, .delaySeconds = 0.0f, .value = 0});
	// The repeat starts from the same command as before
	if (scenario.repeatFrom.has_value())
	{
		++*scenario.repeatFrom;
	}
	scenario.checks.clear();
	for (const auto turn : ignored.turns)
	{
		scenario.checks.push_back(KeepsNone("it keeps no reaction", ignored.creature, turn));
	}
	all.push_back(std::move(scenario));
}

} // namespace

void openblack::testbed_scenarios::AddCreatureReactionKindScenarios(std::vector<Scenario>& all)
{
	AddWithNoticeOnAndOff(
	    all,
	    {
	        .id = "creatures.reaction_food",
	        .name = "Reactions: a creature sees food put down",
	        .facet = Facet::Senses,
	        .description = "The player's tiger stands facing south. A pile of food 20 metres in front of it makes the "
	                       "reaction of food, by the player.",
	        .expected = "The tiger sees the food and takes up its reaction. It learns about the food, but makes no plan "
	                    "for it: it carries on with what it does.",
	        .environment = {.dispenserGrid = false},
	        .framing = k_SideView,
	        .creatures = {Watcher()},
	        .objects = {{.type = PotInfo::HandFood, .offset = {0.0f, -20.0f}, .reaction = Reaction::ReactToFood}},
	        .checks = {Keeps("it keeps the reaction of food", 0, Reaction::ReactToFood, 1, 10 * k_Turns)},
	    },
	    {.id = "creatures.reaction_food_ignored",
	     .name = "Reactions: a creature that takes no notice sees food put down",
	     .expected = "A script tells the tiger to take no notice of reactions. It takes no notice of the food.",
	     .turns = {3 * k_Turns, 10 * k_Turns}});

	AddWithNoticeOnAndOff(
	    all,
	    {
	        .id = "creatures.reaction_magic_tree",
	        .name = "Reactions: a creature near a magic tree",
	        .facet = Facet::Senses,
	        .description = "The player's tiger stands facing south. An oak 20 metres in front of it makes the reaction of "
	                       "a magic tree, by the player.",
	        .expected = "The tiger takes up the reaction but does nothing about it and does not keep it: the thing is not "
	                    "a miracle. It carries on with what it does.",
	        .environment = {.dispenserGrid = false},
	        .framing = k_SideView,
	        .creatures = {Watcher()},
	        .objects = {{.type = TreeInfo::Oak, .offset = {0.0f, -20.0f}, .reaction = Reaction::ReactToMagicTree}},
	        .checks = {KeepsNone("it keeps no reaction", 0, 3 * k_Turns), KeepsNone("it keeps no reaction", 0, 10 * k_Turns)},
	    },
	    {.id = "creatures.reaction_magic_tree_ignored",
	     .name = "Reactions: a creature that takes no notice near a magic tree",
	     .expected = "A script tells the tiger to take no notice of reactions. It takes no notice of the tree.",
	     .turns = {3 * k_Turns, 10 * k_Turns}});

	AddWithNoticeOnAndOff(
	    all,
	    {
	        .id = "creatures.reaction_storage_pit",
	        .name = "Reactions: a creature sees the hand fill a storage pit",
	        .facet = Facet::Senses,
	        .description = "The player's tiger stands facing south. A storage pit 30 metres in front of it makes the "
	                       "reaction of the hand putting things in it, by the player.",
	        .expected = "The tiger takes up the reaction and keeps it. It learns about the pit and watches the skill of "
	                    "using a storage pit, which it can then learn. It makes no plan for it.",
	        .environment = {.dispenserGrid = false},
	        .framing = k_SideView,
	        .creatures = {Watcher()},
	        .objects = {{.type = AbodeInfo::CelticStoragePit,
	                     .offset = {0.0f, -30.0f},
	                     .reaction = Reaction::ReactToHandPuttingStuffInStoragePit}},
	        .checks = {Keeps("it keeps the reaction of the storage pit", 0, Reaction::ReactToHandPuttingStuffInStoragePit, 1,
	                         10 * k_Turns)},
	    },
	    {.id = "creatures.reaction_storage_pit_ignored",
	     .name = "Reactions: a creature that takes no notice sees the hand fill a storage pit",
	     .expected = "A script tells the tiger to take no notice of reactions. It takes no notice of the pit.",
	     .turns = {3 * k_Turns, 10 * k_Turns}});

	AddWithNoticeOnAndOff(
	    all,
	    {
	        .id = "creatures.reaction_dropped_by_hand",
	        .name = "Reactions: a creature near a thing the hand puts down",
	        .facet = Facet::Senses,
	        .description = "The player's tiger stands facing south. A cart 20 metres in front of it makes the reaction of "
	                       "a thing put down by the hand, by the player.",
	        .expected = "The tiger takes no notice: only villagers that can follow a disciple heed this reaction.",
	        .environment = {.dispenserGrid = false},
	        .framing = k_SideView,
	        .creatures = {Watcher()},
	        .objects = {{.type = MobileObjectInfo::Cart, .offset = {0.0f, -20.0f}, .reaction = Reaction::ReactToDroppedByHand}},
	        .checks = {KeepsNone("it keeps no reaction", 0, 3 * k_Turns), KeepsNone("it keeps no reaction", 0, 10 * k_Turns)},
	    },
	    {.id = "creatures.reaction_dropped_by_hand_ignored",
	     .name = "Reactions: a creature that takes no notice near a thing the hand puts down",
	     .expected = "A script tells the tiger to take no notice of reactions. It takes no notice of the cart.",
	     .turns = {3 * k_Turns, 10 * k_Turns}});

	AddWithNoticeOnAndOff(
	    all,
	    {
	        .id = "creatures.reaction_confused",
	        .name = "Reactions: a creature near a thing a script makes confusing",
	        .facet = Facet::Senses,
	        .description = "The player's tiger stands facing south. A cart stands 6 metres in front of it, in the same "
	                       "cell of the map. Two seconds in, a script makes the cart the cause of the confused reaction. "
	                       "The reaction belongs to no player. It starts small and grows slowly, so at first it reaches "
	                       "only the cell of the cart.",
	        .expected = "The tiger takes up the reaction and keeps it. It stops what it does and goes to examine the cart "
	                    "by looking at it.",
	        .environment = {.dispenserGrid = false},
	        .framing = k_SideView,
	        .creatures = {{.label = "watcher", .offset = {5.0f, 8.0f}, .facingDegrees = 0.0f}},
	        .objects = {{.type = MobileObjectInfo::Cart, .offset = {5.0f, 2.0f}}},
	        .commands = {{.kind = Kind::CreateReaction,
	                      .delaySeconds = 2.0f,
	                      .value = static_cast<size_t>(Reaction::Confused),
	                      .object = 0}},
	        .checks = {Keeps("it keeps the confused reaction", 0, Reaction::Confused, 2 * k_Turns, 12 * k_Turns)},
	    },
	    {.id = "creatures.reaction_confused_ignored",
	     .name = "Reactions: a creature that takes no notice near a thing a script makes confusing",
	     .expected = "A script tells the tiger to take no notice of reactions. It takes no notice of the cart.",
	     .turns = {5 * k_Turns, 12 * k_Turns}});

	// Two creatures: the second looks the first over first, which is over long before the first faints
	AddWithNoticeOnAndOff(
	    all,
	    {
	        .id = "creatures.reaction_death",
	        .name = "Reactions: a creature sees another creature faint",
	        .facet = Facet::Senses,
	        .description = "Two tigers of the player stand 30 metres apart and face each other. Thirty seconds in, the "
	                       "first tiger faints.",
	        .expected = "The second tiger takes up the reaction of a death and keeps it. It is sadder, but makes no plan "
	                    "for it.",
	        .environment = {.dispenserGrid = false},
	        .framing = {.shot = Shot::Overview, .include = {{0.0f, 0.0f}, {30.0f, 0.0f}}, .distance = 0.7f},
	        .creatures = {{.label = "fainting", .offset = {0.0f, 0.0f}, .facingDegrees = 90.0f},
	                      {.label = "watcher", .offset = {30.0f, 0.0f}, .facingDegrees = 270.0f}},
	        .commands = {{.kind = Kind::Faint, .creature = 0, .delaySeconds = 30.0f}},
	        .checks = {Keeps("it keeps the reaction of a death", 1, Reaction::ReactToDeath, 30 * k_Turns, 40 * k_Turns)},
	    },
	    {.id = "creatures.reaction_death_ignored",
	     .name = "Reactions: a creature that takes no notice sees another creature faint",
	     .expected = "A script tells the second tiger to take no notice of reactions. It takes no notice of the first "
	                 "tiger, before or after it faints.",
	     .creature = 1,
	     .turns = {10 * k_Turns, 35 * k_Turns, 40 * k_Turns}});

	// The evil tiger farts only after the other has looked it over and the look is over
	AddWithNoticeOnAndOff(
	    all,
	    {
	        .id = "creatures.reaction_fainting",
	        .name = "Reactions: a creature near the fart of an evil creature goes to sleep",
	        .facet = Facet::Senses,
	        .description = "Two small tigers of the player stand 6 metres apart, in the same cell of the map, and face each "
	                       "other. The first tiger is evil and wants only to scratch, so it stays where it is. Thirty "
	                       "seconds in, it farts, and again each 20 seconds after it is free. Each cloud of the fart makes a "
	                       "fainting reaction that starts small and grows "
	                       "slowly, so at first it reaches only the cell of the evil tiger.",
	        .expected = "The fart of the evil tiger makes the fainting reaction. The second tiger takes it up and keeps "
	                    "it. It stops what it does and goes to sleep at its home, the place where it was made.",
	        .environment = {.dispenserGrid = false},
	        .framing = {.shot = Shot::Overview, .include = {{-5.0f, 0.0f}, {15.0f, 10.0f}}, .distance = 0.7f},
	        .creatures = {EvilAtRest({2.0f, 5.0f}),
	                      {.label = "near", .offset = {8.0f, 5.0f}, .facingDegrees = 270.0f, .size = 0.3f}},
	        .commands = {{.kind = Kind::Fart, .creature = 0, .delaySeconds = 30.0f},
	                     {.kind = Kind::Fart, .creature = 0, .delaySeconds = 20.0f, .waitUntilFree = true}},
	        .repeatFrom = 1,
	        .checks = {Keeps("it keeps the fainting reaction", 1, Reaction::Fainting, 30 * k_Turns, 60 * k_Turns)},
	    },
	    {.id = "creatures.reaction_fainting_ignored",
	     .name = "Reactions: a creature that takes no notice near the fart of an evil creature",
	     .expected = "A script tells the second tiger to take no notice of reactions. It takes no notice of the farts.",
	     .creature = 1,
	     .turns = {40 * k_Turns, 60 * k_Turns}});

	// The arena of a fight makes the reaction of the fight. The arena is not a creature, so no creature watches.
	AddWithNoticeOnAndOff(
	    all,
	    {
	        .id = "creatures.reaction_fight",
	        .name = "Reactions: a creature near a fight of two other creatures",
	        .facet = Facet::Senses,
	        .description = "Two tigers of two players stand 20 metres apart. A third tiger stands 50 metres north of them "
	                       "and faces them. Thirty seconds in, the two tigers fight by themselves.",
	        .expected = "The arena of the fight makes the reaction of a fight. The third tiger takes no notice of it, "
	                    "because the arena is not a creature: it does not go to watch the fight and does not keep the "
	                    "reaction.",
	        .environment = {.dispenserGrid = false},
	        .framing = {.shot = Shot::Overview, .include = {{0.0f, 0.0f}, {20.0f, 0.0f}, {10.0f, 50.0f}}, .distance = 0.7f},
	        .creatures = {{.label = "red", .offset = {0.0f, 0.0f}, .facingDegrees = 90.0f},
	                      {.label = "blue", .offset = {20.0f, 0.0f}, .facingDegrees = 270.0f, .owner = PlayerNames::PLAYER_TWO},
	                      {.label = "watcher", .offset = {10.0f, 50.0f}, .facingDegrees = 0.0f}},
	        .commands = {{.kind = Kind::StartFight, .creature = 0, .delaySeconds = 30.0f, .value = 1},
	                     {.kind = Kind::FightAuto, .creature = 0, .delaySeconds = 0.2f, .value = 1}},
	    },
	    {.id = "creatures.reaction_fight_ignored",
	     .name = "Reactions: a creature that takes no notice near a fight of two other creatures",
	     .expected = "A script tells the third tiger to take no notice of reactions. It takes no notice of the other "
	                 "tigers or of their fight.",
	     .creature = 2,
	     .turns = {10 * k_Turns, 40 * k_Turns}});
	// Of the miracles, only the flying flock makes the reaction of an impressive miracle
	AddWithNoticeOnAndOff(
	    all,
	    {
	        .id = "creatures.reaction_impressive_miracle",
	        .name = "Reactions: a creature sees its player cast the flying flock",
	        .facet = Facet::Senses,
	        .description = "The player's tiger stands facing south. Two seconds in, the player casts the flying flock 30 "
	                       "metres in front of it.",
	        .expected = "The flock makes the reaction of an impressive miracle. The tiger takes it up as a miracle of its "
	                    "own player and keeps it. It goes to look at the miracle and points at it, or turns and points, or "
	                    "waits there puzzled.",
	        .environment = {.dispenserGrid = false},
	        .framing = {.shot = Shot::Overview, .include = {{0.0f, 0.0f}, {0.0f, -30.0f}}, .distance = 0.8f},
	        .creatures = {Watcher()},
	        .miracles = {{.type = MagicType::FlockFlying,
	                      .point = {0.0f, -30.0f},
	                      .handOffset = {0.0f, -20.0f},
	                      .handHeight = 12.0f,
	                      .delaySeconds = 2.0f,
	                      .holdSeconds = 6.0f}},
	        .checks = {Keeps("it keeps the reaction of an impressive miracle", 0, Reaction::ReactToImpressiveSpell, 2 * k_Turns,
	                         15 * k_Turns)},
	    },
	    {.id = "creatures.reaction_impressive_miracle_ignored",
	     .name = "Reactions: a creature that takes no notice sees its player cast the flying flock",
	     .expected = "A script tells the tiger to take no notice of reactions. It takes no notice of the flock.",
	     .turns = {5 * k_Turns, 15 * k_Turns}});
}
