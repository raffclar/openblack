/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The testbed's scenarios of a creature that learns a skill by watching villagers: building, and dancing at a worship
// site

#include <array>
#include <vector>

#include "TestbedScenarioRegistry.h"

using namespace openblack;
using namespace openblack::testbed_scenarios;
using creature_desires::Desire;

namespace
{
/// The building town: its village centre, a storage pit full of wood, and a hut that stands a fifth built, its site open
constexpr glm::vec2 k_Centre {0.0f, 60.0f};
constexpr glm::vec2 k_Pit {-30.0f, 45.0f};
constexpr glm::vec2 k_Site {0.0f, 25.0f};
constexpr float k_SiteBuilt = 0.2f;
constexpr uint32_t k_PitStock = 5000;
/// Its people: grown couples, more than its one hut holds, so that they want the new hut
constexpr size_t k_Couples = 4;
constexpr float k_Spacing = 3.0f;
/// The dancing town: its village centre north of the player's temple, with the temple's worship site between them
constexpr glm::vec2 k_DanceCentre {0.0f, 40.0f};
constexpr glm::vec2 k_Temple {0.0f, -30.0f};
/// The towns' belief in the player, so that they stay the player's
constexpr float k_TownBelief = 1000.0f;

/// A creature kept content, that only wants to find out about things, so that nothing else takes it away
CreatureSetup Curious(glm::vec2 offset, float facing)
{
	std::vector<DesireOverride> desires;
	for (size_t i = 0; i < creature_desires::k_DesireCount; ++i)
	{
		const auto desire = static_cast<Desire>(i);
		desires.push_back({.desire = desire, .fraction = desire == Desire::Curiosity ? 1.0f : 0.0f});
	}
	return {.species = CreatureType::Lion,
	        .offset = offset,
	        .facingDegrees = facing,
	        .needs = {.energy = 1.0f, .exhaustion = 0.0f, .dehydration = 0.0f, .poo = 0.0f, .life = 1.0f, .warmth = 0.0f},
	        .desires = std::move(desires),
	        .hold = true};
}

std::vector<ObjectSetup> People(glm::vec2 from, size_t couples)
{
	std::vector<ObjectSetup> people;
	for (size_t i = 0; i < couples; ++i)
	{
		const float x = from.x + ((static_cast<float>(i) - (static_cast<float>(couples) * 0.5f)) * k_Spacing);
		people.push_back({.type = VillagerInfo::CelticForesterMale, .offset = {x, from.y}, .joinTown = true});
		people.push_back({.type = VillagerInfo::CelticHousewifeFemale, .offset = {x, from.y - k_Spacing}, .joinTown = true});
	}
	return people;
}
} // namespace

void openblack::testbed_scenarios::AddCreatureLearningScenarios(std::vector<Scenario>& all)
{
	std::vector<ObjectSetup> building {
	    {.type = AbodeInfo::CelticTownCentre, .offset = k_Centre},
	    {.type = AbodeInfo::CelticStoragePit, .offset = k_Pit, .storedFood = k_PitStock, .storedWood = k_PitStock},
	    {.type = AbodeInfo::CelticHut, .offset = {25.0f, 45.0f}, .yawDegrees = 180.0f},
	    {.type = AbodeInfo::CelticHut, .offset = k_Site, .yawDegrees = 180.0f, .built = k_SiteBuilt},
	};
	for (auto& person : People({0.0f, 40.0f}, k_Couples))
	{
		building.push_back(std::move(person));
	}
	all.push_back({
	    .id = "creatures.learns_build",
	    .name = "A creature learning to build by watching builders",
	    .facet = Facet::Mind,
	    .description = "A Celtic town of the player's with a village centre, a storage pit full of wood and food, one hut, "
	                   "a second hut a fifth built with its site open, and four couples. A grown-up lion, content and "
	                   "wanting only to find out about things, stands thirty metres south of the site, facing it. Best "
	                   "watched at game speed 2.",
	    .expected = "Builders fetch wood and build the hut. The lion sees a builder at work again: a light bulb with "
	                "\"50%\" in yellow floats up from its head, and it goes after the builder, amazed, looking at it "
	                "and keeping within fifteen metres of it. Seven or more seconds after it first saw the building it "
	                "knows the skill: its thoughts say it has learnt to build and the Mind panel marks Build known (no "
	                "\"100%\" shows when it learns within ten seconds of the last bulb). It stops following, holds one "
	                "to four seconds and scratches its head, amazed. The advisor says nothing of building.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Follow},
	    .creatures = {Curious({0.0f, -5.0f}, 180.0f)},
	    .objects = std::move(building),
	    .townBeliefInPlayer = k_TownBelief,
	});

	std::vector<ObjectSetup> dancing {
	    {.type = AbodeInfo::CelticTownCentre, .offset = k_DanceCentre},
	    {.type = AbodeInfo::CelticHut, .offset = k_DanceCentre + glm::vec2(-16.0f, 6.0f), .yawDegrees = 160.0f},
	    {.type = AbodeInfo::CelticHut, .offset = k_DanceCentre + glm::vec2(16.0f, 6.0f), .yawDegrees = 200.0f},
	};
	for (auto& person : People(k_DanceCentre + glm::vec2(0.0f, -10.0f), k_Couples))
	{
		dancing.push_back(std::move(person));
	}
	all.push_back({
	    .id = "creatures.learns_dance",
	    .name = "A creature learning to dance by watching worshippers",
	    .facet = Facet::Mind,
	    .description = "The player's temple with a Celtic town of the player's north of it, the town's worship site put up "
	                   "built and its totem raised to all its people. A grown-up lion, content and wanting only to find "
	                   "out about things, stands east of the worship site, facing it.",
	    .expected = "The town's people go to the worship site and dance round its altar. Seeing a dancer again, the lion "
	                "shows a light bulb with \"50%\" over its head and follows the dancer, amazed. Six or more seconds "
	                "after it first saw the dance it knows the skill: its thoughts say it has learnt to dance and the "
	                "Mind panel marks Dance known; then it holds a moment and scratches its head. From then on it may "
	                "dance with villagers to impress them. A fully grown creature is told by the advisor that it is "
	                "learning to dance and that it has learnt it.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Follow},
	    .creatures = {Curious({35.0f, -20.0f}, 270.0f)},
	    .objects = std::move(dancing),
	    .temples = {{.offset = k_Temple}},
	    .townBeliefInPlayer = k_TownBelief,
	    .worshipShare = 1.0f,
	});
}
