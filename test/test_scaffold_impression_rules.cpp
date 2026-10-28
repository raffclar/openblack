/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cstdint>

#include <array>
#include <optional>
#include <vector>

#include <entt/entity/entity.hpp>
#include <gtest/gtest.h>

#include "ECS/ScaffoldImpressionRules.h"
#include "Magic/Impressiveness.h"

using namespace openblack;
using namespace openblack::ecs::scaffold_impression;

namespace
{

/// A person with a number
entt::entity Person(uint32_t number)
{
	return static_cast<entt::entity>(number);
}

} // namespace

// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables): external macro
TEST(ScaffoldImpression, TheValueOfTheScaffoldsAndTheWantOfTheSiteMultiplyTheThing)
{
	EXPECT_FLOAT_EQ(ImpressiveValue(3, 2.0f, std::nullopt), 6.0f);
	EXPECT_FLOAT_EQ(ImpressiveValue(3, 2.0f, 0.5f), 3.0f);
	// A town that does not want the building makes the scaffold not impressive
	EXPECT_FLOAT_EQ(ImpressiveValue(3, 2.0f, 0.0f), 0.0f);
	EXPECT_FLOAT_EQ(ImpressiveValue(0, 2.0f, 1.0f), 0.0f);
	EXPECT_EQ(SiteWant(true), TownDesireInfo::ForAbodes);
	EXPECT_EQ(SiteWant(false), TownDesireInfo::ForCivicBuilding);
}

// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables): external macro
TEST(ScaffoldImpression, AThingImpressesByAllItsPartsAndLessAtTheEdgeOfTheReach)
{
	const magic::ObjectImpressionInputs close {
	    .landBalance = 2.0f,
	    .impressiveValue = 3.0f,
	    .reactionMultiplier = 0.5f,
	    .distance = 0.0f,
	    .maxDistance = 100.0f,
	    .boredom = 0.5f,
	    .artefact = 2.0f,
	};
	// Close by, all of the impression reaches the watcher
	EXPECT_FLOAT_EQ(magic::ObjectImpressiveValue(close), 3.0f);
	auto far = close;
	far.distance = 200.0f;
	EXPECT_LT(magic::ObjectImpressiveValue(far), magic::ObjectImpressiveValue(close) / 4.0f);
	auto bored = close;
	bored.boredom = 0.0f;
	EXPECT_FLOAT_EQ(magic::ObjectImpressiveValue(bored), 0.0f);
}

// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables): external macro
TEST(ScaffoldImpression, ASiteImpressesAPartOfATownAndAtLeastOnePerson)
{
	EXPECT_EQ(PeopleToImpress(40, 0.25f), 10u);
	// Truncated
	EXPECT_EQ(PeopleToImpress(7, 0.5f), 3u);
	EXPECT_EQ(PeopleToImpress(0, 0.5f), 1u);
	EXPECT_EQ(PeopleToImpress(10, 0.0f), 1u);
	EXPECT_EQ(PeopleToImpress(3, 1.0f), 3u);
}

// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables): external macro
TEST(ScaffoldImpression, TheFirstPeopleComeAbodeByAbode)
{
	const std::vector<std::vector<entt::entity>> abodes {
	    {Person(1), Person(2)},
	    {},
	    {Person(3), Person(4), Person(5)},
	};
	EXPECT_EQ(FirstPeople(abodes, 4), (std::vector<entt::entity> {Person(1), Person(2), Person(3), Person(4)}));
	EXPECT_EQ(FirstPeople(abodes, 1), (std::vector<entt::entity> {Person(1)}));
	EXPECT_EQ(FirstPeople(abodes, 10).size(), 5u);
	EXPECT_TRUE(FirstPeople({}, 3).empty());
}

// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables): external macro
TEST(ScaffoldImpression, TheTownMadeLastAndThoseItsPlayerGainedAfterItCanBeImpressed)
{
	const std::array<TownEntry, 5> towns {
	    TownEntry {.made = 0, .owner = PlayerNames::PLAYER_ONE, .gained = 0},
	    TownEntry {.made = 1, .owner = PlayerNames::PLAYER_TWO, .gained = 1},
	    // Made last, but its player gained it before the town that comes next
	    TownEntry {.made = 4, .owner = PlayerNames::PLAYER_ONE, .gained = 2},
	    TownEntry {.made = 3, .owner = PlayerNames::PLAYER_ONE, .gained = 6},
	    TownEntry {.made = 2, .owner = PlayerNames::PLAYER_ONE, .gained = 5},
	};
	// The first town of the player, gained before it, and the town of the other player are not in the list
	EXPECT_EQ(TownsToImpress(towns), (std::vector<size_t> {2, 4, 3}));
	EXPECT_TRUE(TownsToImpress({}).empty());
	const std::array<TownEntry, 1> one {TownEntry {.made = 7, .owner = PlayerNames::NEUTRAL, .gained = 0}};
	EXPECT_EQ(TownsToImpress(one), (std::vector<size_t> {0}));
}
