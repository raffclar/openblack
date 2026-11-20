/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// Testbed scenarios that measure the frame time with a crowd.

#include "TestbedScenarioBuilders.h"
#include "TestbedScenarioRegistry.h"

using namespace openblack;
using namespace openblack::testbed_scenarios;

namespace
{
/// A benchmark of a crowd of the size, to compare runs of several sizes and see which costs grow faster than the crowd
/// does
void AddCrowd(std::vector<Scenario>& all, Crowd::Kind kind, size_t count, std::string_view id, std::string_view name)
{
	const bool creatures = kind == Crowd::Kind::Creatures;
	all.push_back({
	    .id = id,
	    .name = name,
	    .facet = Facet::Benchmark,
	    .description = creatures ? "Creatures of every species and of four players, spread evenly over the land round the "
	                               "middle and clear of the lake, the same way every run. Nothing holds them: their minds, "
	                               "needs, walking and route finding all run as in the game."
	                             : "Villagers of every tribe and role in towns of fifty, each a ring of huts about a storage "
	                               "pit, spread evenly over the land round the middle and clear of the lake, the same way "
	                               "every run, and left to go about their lives.",
	    .expected = "They spawn a batch a frame, then the frames are measured once they have settled: the mean, 95th "
	                "percentile and slowest frame and the costliest profiler stages show as it runs, and Save results "
	                "writes them out to compare with the other sizes.",
	    // The crowd is spread over the middle, where the miracle dispensers would stand
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Testbed},
	    .crowd = Crowd {.kind = kind, .count = count, .seed = k_BenchmarkSeed, .perFrame = creatures ? 100u : 250u},
	});
}

/// A benchmark of creatures of the count that live their usual lives among towns. The one of 100 creatures is the most
/// creatures a frame must hold at 100 frames a second. The smaller counts show the count where a frame goes past 10 ms.
void AddCreaturesAtWork(std::vector<Scenario>& all, size_t count, std::string_view id, std::string_view name)
{
	all.push_back({
	    .id = id,
	    .name = name,
	    .facet = Facet::Benchmark,
	    .description = "Creatures of every species and of four players, spread evenly round four towns of 50 villagers "
	                   "each, clear of the homes and the lake, the same way every run. Nothing holds them: they eat, play, "
	                   "rest, look at the villagers and the buildings, walk and fight as in the game.",
	    .expected = "The towns spawn first, then the creatures, a batch a frame. Then the frames are measured once they "
	                "have settled. Every frame must be under 10 ms on the processor.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Testbed},
	    .crowd = Crowd {.kind = Crowd::Kind::Creatures,
	                    .count = count,
	                    .seed = k_BenchmarkSeed,
	                    .perFrame = 100,
	                    .villagers = 4 * k_VillagersPerTown},
	});
}
} // namespace

void testbed_scenarios::AddBenchmark(std::vector<Scenario>& all)
{
	// Steps close enough together that a cost growing faster than the crowd shows between them
	AddCrowd(all, Crowd::Kind::Creatures, 1, "benchmark.creatures_1", "1 active creature");
	AddCrowd(all, Crowd::Kind::Creatures, 4, "benchmark.creatures_4", "4 active creatures");
	AddCrowd(all, Crowd::Kind::Creatures, 8, "benchmark.creatures_8", "8 active creatures");
	AddCrowd(all, Crowd::Kind::Creatures, 10, "benchmark.creatures_10", "10 active creatures");
	AddCrowd(all, Crowd::Kind::Creatures, 16, "benchmark.creatures_16", "16 active creatures");
	AddCrowd(all, Crowd::Kind::Creatures, 25, "benchmark.creatures_25", "25 active creatures");
	AddCrowd(all, Crowd::Kind::Creatures, 32, "benchmark.creatures_32", "32 active creatures");
	AddCrowd(all, Crowd::Kind::Creatures, 50, "benchmark.creatures_50", "50 active creatures");
	AddCrowd(all, Crowd::Kind::Creatures, 64, "benchmark.creatures_64", "64 active creatures");
	AddCrowd(all, Crowd::Kind::Creatures, 100, "benchmark.creatures_100", "100 active creatures");
	AddCrowd(all, Crowd::Kind::Villagers, 100, "benchmark.villagers_100", "100 active villagers");
	AddCrowd(all, Crowd::Kind::Villagers, 250, "benchmark.villagers_250", "250 active villagers");
	AddCrowd(all, Crowd::Kind::Villagers, 500, "benchmark.villagers_500", "500 active villagers");
	AddCrowd(all, Crowd::Kind::Villagers, 1'000, "benchmark.villagers_1000", "1,000 active villagers");
	AddCreaturesAtWork(all, 100, "benchmark.creatures_at_work", "100 creatures among towns");
	AddCreaturesAtWork(all, 8, "benchmark.town_creatures_8", "8 creatures among towns");
	AddCreaturesAtWork(all, 16, "benchmark.town_creatures_16", "16 creatures among towns");
	AddCreaturesAtWork(all, 32, "benchmark.town_creatures_32", "32 creatures among towns");
	AddCreaturesAtWork(all, 64, "benchmark.town_creatures_64", "64 creatures among towns");
}
