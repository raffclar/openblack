/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The testbed's scenario of a creature that dances with the villagers of a town to impress them

#include <array>
#include <vector>

#include "TestbedScenarioRegistry.h"

using namespace openblack;
using namespace openblack::testbed_scenarios;

namespace
{
/// The town's two huts, west of the open ground where its people stand
constexpr std::array<glm::vec2, 2> k_Huts {{{-45.0f, 20.0f}, {-45.0f, 50.0f}}};
/// Ten grown people of the town in two rows, five men and five women
constexpr size_t k_People = 10;
constexpr glm::vec2 k_FirstPerson {-10.0f, 25.0f};
constexpr float k_PersonSpacing = 5.0f;
} // namespace

void openblack::testbed_scenarios::AddCreatureDanceScenarios(std::vector<Scenario>& all)
{
	std::vector<ObjectSetup> objects;
	for (const auto hut : k_Huts)
	{
		objects.push_back({.type = AbodeInfo::CelticHut, .offset = hut, .yawDegrees = 90.0f});
	}
	for (size_t i = 0; i < k_People; ++i)
	{
		const auto row = static_cast<float>(i / 5);
		const auto column = static_cast<float>(i % 5);
		objects.push_back({.type = i % 2 == 0 ? VillagerInfo::CelticFarmerMale : VillagerInfo::CelticHousewifeFemale,
		                   .offset = k_FirstPerson + glm::vec2(column * k_PersonSpacing, row * k_PersonSpacing),
		                   .yawDegrees = 0.0f,
		                   .joinTown = true,
		                   .livesIn = i % 2});
	}
	// The villager the dance is for: the first of the people
	constexpr size_t k_DanceVillager = k_Huts.size();
	all.push_back({
	    .id = "creatures.dance_with_villagers",
	    .name = "A creature dancing with villagers",
	    .facet = Facet::Objects,
	    .description = "Two huts of the player's town, with ten grown people of the town who live in them, standing in two "
	                   "rows on open ground. The player's tiger stands forty metres to the east. One second in, it is told "
	                   "to dance with the villagers, with the first man of the rows as the villager it dances for.",
	    .expected = "The tiger goes near the man and faces him, and plays its summon. The town's people stop what they do: "
	                "eight of the ten come under its control (it calls an even number, and stops when it has more than two "
	                "thirds of the town's adults). The tiger walks to a clear place by the man, without a turn of its head, "
	                "and points at it with a smile. The eight people run to their places in a circle round it and stand "
	                "still. Then the dance starts: the people dance and worship the tiger, and the tiger dances in the "
	                "middle, playing its wave, impress, happy and summon actions and turning as the dance says. While "
	                "the people worship, the town's belief in the player goes up, with the belief symbols above them. A "
	                "camera near the dance hears the celtic chant. After 45 seconds the dance stops and the people go home.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Overview, .include = {{-50.0f, 0.0f}, {50.0f, 70.0f}}, .distance = 0.8f},
	    .creatures = {{.species = CreatureType::Tiger, .offset = {40.0f, 30.0f}, .facingDegrees = 270.0f}},
	    .objects = std::move(objects),
	    .commands = {{.kind = Command::Kind::DoAction,
	                  .delaySeconds = 1.0f,
	                  .object = k_DanceVillager,
	                  .action = "DanceWithVillagers"}},
	});
}
