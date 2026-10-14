/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The testbed's scenarios of the sounds of the land: a village with its houses, its foresters and its animals, by day
// and by night

#include <vector>

#include "TestbedScenarioRegistry.h"

using namespace openblack;
using namespace openblack::testbed_scenarios;

namespace
{
/// A small Norse village: five huts and a storage pit, its people, two forester disciples by a wood of tall oaks, and cows and
/// sheep that graze beside it
std::vector<ObjectSetup> SoundVillage()
{
	std::vector<ObjectSetup> village {
	    {.type = AbodeInfo::NorseStoragePit, .offset = {0.0f, 40.0f}},
	    {.type = AbodeInfo::NorseHut, .offset = {-18.0f, 30.0f}, .yawDegrees = 180.0f},
	    {.type = AbodeInfo::NorseHut, .offset = {-9.0f, 52.0f}, .yawDegrees = 180.0f},
	    {.type = AbodeInfo::NorseHut, .offset = {9.0f, 52.0f}, .yawDegrees = 180.0f},
	    {.type = AbodeInfo::NorseHut, .offset = {18.0f, 30.0f}, .yawDegrees = 180.0f},
	    {.type = AbodeInfo::NorseHut, .offset = {0.0f, 62.0f}, .yawDegrees = 180.0f},
	    {.type = VillagerInfo::NorseFarmerMale, .offset = {-8.0f, 32.0f}, .joinTown = true},
	    {.type = VillagerInfo::NorseHousewifeFemale, .offset = {-4.0f, 30.0f}, .joinTown = true},
	    {.type = VillagerInfo::NorseFarmerMale, .offset = {4.0f, 30.0f}, .joinTown = true},
	    {.type = VillagerInfo::NorseHousewifeFemale, .offset = {8.0f, 30.0f}, .joinTown = true},
	    {.type = VillagerInfo::NorseForesterMale,
	     .offset = {-12.0f, 14.0f},
	     .joinTown = true,
	     .disciple = VillagerDisciple::Forester},
	    {.type = VillagerInfo::NorseForesterMale,
	     .offset = {-12.0f, 20.0f},
	     .joinTown = true,
	     .disciple = VillagerDisciple::Forester},
	    {.type = AnimalInfo::Cow, .offset = {14.0f, 12.0f}, .living = true},
	    {.type = AnimalInfo::Cow, .offset = {20.0f, 16.0f}, .living = true},
	    {.type = AnimalInfo::Sheep, .offset = {16.0f, 22.0f}, .living = true},
	    {.type = AnimalInfo::Sheep, .offset = {22.0f, 24.0f}, .living = true},
	    {.type = AnimalInfo::Horse, .offset = {24.0f, 8.0f}, .living = true},
	};
	// The wood that the foresters fell
	for (int row = 0; row < 3; ++row)
	{
		for (int column = 0; column < 3; ++column)
		{
			village.push_back({.type = TreeInfo::Oak,
			                   .offset = {-34.0f + static_cast<float>(column) * 8.0f, 8.0f + static_cast<float>(row) * 8.0f},
			                   .scale = 1.5f,
			                   .forest = 1});
		}
	}
	return village;
}
} // namespace

void testbed_scenarios::AddWorldSoundScenarios(std::vector<Scenario>& all)
{
	all.push_back({
	    .id = "audio.village_day",
	    .name = "The sounds of a village by day",
	    .facet = Facet::Audio,
	    .description = "A small Norse village at noon: five huts and a storage pit with their people, two foresters by a "
	                   "wood of tall oaks, and cows, sheep and a horse beside it. The camera stands low in the middle of it.",
	    .expected = "Each hut and the storage pit make the day sound of a building, about one time in 80 seconds each. "
	                "The inspector query living.abode_sounds counts them. The foresters chop the oaks with the sounds "
	                "of the axe. An oak that a forester fells makes the sound of a tree that falls as it tips over, "
	                "then thuds on the ground. The people walk with the sounds of their feet. The animals call and "
	                "walk with their sounds.",
	    .environment = {.hour = 12.0f, .clockRuns = false, .dispenserGrid = false},
	    .framing = {.shot = Shot::Placed, .eye = {0.0f, 12.0f, 2.0f}, .look = {0.0f, 0.0f, 30.0f}},
	    .objects = SoundVillage(),
	    .townBeliefInPlayer = 1000.0f,
	});
	all.push_back({
	    .id = "audio.village_night",
	    .name = "The sounds of a village by night",
	    .facet = Facet::Audio,
	    .description = "The village of audio.village_day at eleven at night, the clock standing still.",
	    .expected = "The huts and the storage pit roll for their night sound about one time in 80 seconds each. The "
	                "game's bank has no night sound for a building, so they stay silent. The animals and the "
	                "foresters still make their sounds.",
	    .environment = {.hour = 23.0f, .clockRuns = false, .dispenserGrid = false},
	    .framing = {.shot = Shot::Placed, .eye = {0.0f, 12.0f, 2.0f}, .look = {0.0f, 0.0f, 30.0f}},
	    .objects = SoundVillage(),
	    .townBeliefInPlayer = 1000.0f,
	});
	all.push_back({
	    .id = "audio.village_dawn",
	    .name = "The sounds of a village at dawn",
	    .facet = Facet::Audio,
	    .description = "The village of audio.village_day at half past six in the morning, the clock standing still.",
	    .expected = "The huts and the storage pit sometimes make the dawn sound of a building: a rooster or a pigeon. "
	                "Most of the dawn sounds are silent. The inspector query living.abode_sounds counts them.",
	    .environment = {.hour = 6.5f, .clockRuns = false, .dispenserGrid = false},
	    .framing = {.shot = Shot::Placed, .eye = {0.0f, 12.0f, 2.0f}, .look = {0.0f, 0.0f, 30.0f}},
	    .objects = SoundVillage(),
	    .townBeliefInPlayer = 1000.0f,
	});
}
