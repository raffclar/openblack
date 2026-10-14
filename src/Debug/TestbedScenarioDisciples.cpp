/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The testbed scenarios for disciples. One shows the symbol of the work. One shows the work that a villager keeps when
// the hand releases it. One shows the villagers that follow a disciple to its work.

#include <vector>

#include "TestbedScenarioRegistry.h"

using namespace openblack;
using namespace openblack::testbed_scenarios;

void testbed_scenarios::AddDiscipleScenarios(std::vector<Scenario>& all)
{
	all.push_back({
	    .id = "villagers.disciple_symbol",
	    .name = "A disciple's symbol under the hand",
	    .facet = Facet::Hand,
	    .description = "A grown villager of the player's town, made a farming disciple and held still by a script, stands "
	                   "in the middle of the screen with the player's hand over it.",
	    .expected = "The villager glows orange, its puffs turning, and the yellow wheat symbol of a farmer stands over the "
	                "glow, the same size on the screen wherever the camera is. On every screen it takes the same share "
	                "of the screen's height (of its 4:3 middle) as on the game's default 800x600 screen.",
	    .environment = {.dispenserGrid = false, .cursor = glm::vec2 {0.5f, 0.5f}},
	    .framing = {.shot = Shot::Placed, .eye = {0.0f, 8.0f, -14.0f}, .look = {0.0f, 1.0f, 0.0f}},
	    .objects = {{.type = VillagerInfo::CelticFarmerMale,
	                 .offset = {0.0f, 0.0f},
	                 .joinTown = true,
	                 .inScript = true,
	                 .disciple = VillagerDisciple::Farmer}},
	});

	using Kind = Command::Kind;
	all.push_back({
	    .id = "villagers.disciple_letgo",
	    .name = "What a villager keeps of the work the hand showed it",
	    .facet = Facet::Hand,
	    .description = "A wheat field of the player's town (which believes in the player from the start) lies in the middle of "
	                   "the screen under the hand, with three "
	                   "grown villagers of the town about it, none a disciple; a script has made the third immovable. The "
	                   "hand takes each in turn over the field: the first is put down, the second thrown to the east, the "
	                   "third (which the physics won't move) let go.",
	    .expected = "Over the field each glows and shows the farmer's wheat symbol. The first, put down, stays a farmer: "
	                "the farming disciple's line plays and it goes to work the field. The second flies, and from the "
	                "moment it leaves the hand it is an ordinary villager again: no glow or symbol, and it decides for "
	                "itself what to do once it is up. The third is set straight down on the land under the hand and is "
	                "an ordinary villager again too.",
	    .environment = {.dispenserGrid = false, .cursor = glm::vec2 {0.5f, 0.5f}},
	    .framing = {.shot = Shot::Placed, .eye = {0.0f, 22.0f, -30.0f}, .look = {0.0f, 0.0f, 0.0f}},
	    .objects =
	        {{.type = FieldTypeInfo::Wheat, .offset = {0.0f, 0.0f}},
	         {.type = VillagerInfo::CelticFarmerMale, .offset = {-10.0f, -8.0f}, .joinTown = true},
	         {.type = VillagerInfo::CelticFarmerMale, .offset = {-12.0f, -8.0f}, .joinTown = true},
	         {.type = VillagerInfo::CelticHousewifeFemale, .offset = {10.0f, -8.0f}, .joinTown = true, .immovable = true}},
	    .commands = {{.kind = Kind::HandTake, .delaySeconds = 1.0f, .object = 1},
	                 {.kind = Kind::HandRelease, .delaySeconds = 3.0f},
	                 {.kind = Kind::HandTake, .delaySeconds = 3.0f, .object = 2},
	                 {.kind = Kind::HandRelease, .delaySeconds = 3.0f, .velocity = {12.0f, 6.0f, 0.0f}},
	                 {.kind = Kind::HandTake, .delaySeconds = 6.0f, .object = 3},
	                 {.kind = Kind::HandRelease, .delaySeconds = 3.0f}},
	    // The town belongs to the player at the start. Thus the field gives its work to the villagers of the player.
	    .townBeliefInPlayer = 1000.0f,
	});

	// A farming disciple starts its work. The villagers of its town near it follow it to the field one time.
	all.push_back({
	    .id = "villagers.disciple_followers",
	    .name = "Villagers that follow a disciple to its work",
	    .facet = Facet::Objects,
	    .description = "A wheat field of the player's town lies in the middle of the screen. The town has a storage pit "
	                   "full of food and wood to the west, thus it does not want more food. A farming disciple stands "
	                   "south of the field. Four grown villagers of the town stand within 10 metres of the disciple, and "
	                   "a fifth stands 20 metres from it. Best watched at game speed 4.",
	    .expected = "The disciple glows and walks to the field to farm it. Each time the disciple starts its work, the "
	                "villagers within 10 metres of it that are free to change what they do follow it. They decide again "
	                "and walk to the field to farm it one time, without a glow. A villager that has not decided since "
	                "it was made cannot follow. The fifth villager does not follow. When a follower has farmed, it "
	                "decides for itself again.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Placed, .eye = {0.0f, 26.0f, -40.0f}, .look = {0.0f, 0.0f, -8.0f}},
	    .objects = {{.type = FieldTypeInfo::Wheat, .offset = {0.0f, 0.0f}},
	                {.type = AbodeInfo::CelticStoragePit, .offset = {-30.0f, -20.0f}, .storedFood = 5000, .storedWood = 5000},
	                {.type = VillagerInfo::CelticFarmerMale,
	                 .offset = {0.0f, -16.0f},
	                 .joinTown = true,
	                 .disciple = VillagerDisciple::Farmer},
	                {.type = VillagerInfo::CelticForesterMale, .offset = {-4.0f, -18.0f}, .joinTown = true},
	                {.type = VillagerInfo::CelticHousewifeFemale, .offset = {4.0f, -18.0f}, .joinTown = true},
	                {.type = VillagerInfo::CelticFishermanMale, .offset = {-3.0f, -22.0f}, .joinTown = true},
	                {.type = VillagerInfo::CelticHousewifeFemale, .offset = {3.0f, -22.0f}, .joinTown = true},
	                {.type = VillagerInfo::CelticForesterMale, .offset = {20.0f, -16.0f}, .joinTown = true}},
	    .townBeliefInPlayer = 1000.0f,
	});
}
