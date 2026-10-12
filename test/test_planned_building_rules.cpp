/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <array>
#include <numbers>
#include <optional>
#include <vector>

#include <glm/gtx/euler_angles.hpp>
#include <gtest/gtest.h>

#include "ECS/PlannedBuildingRules.h"

using namespace openblack;
using namespace openblack::ecs::planned_building;

namespace
{
/// A house of the first kind, wanted a quarter, two adults to it
PlannedKind House(uint32_t sites = 0)
{
	return {.type = AbodeType::LivingQuarters,
	        .number = AbodeNumber::A,
	        .desireToBeBuilt = 0.25f,
	        .maxAdults = 2,
	        .sitesOfSameType = sites};
}

TownForPlans Town(int32_t adultRoom, uint32_t homeless, uint32_t adults, uint32_t children)
{
	return {.adultRoom = adultRoom, .homeless = homeless, .adults = adults, .children = children};
}

PlannedKind Civic(AbodeType type, float desire, uint32_t sites = 0)
{
	return {.type = type, .desireToBeBuilt = desire, .sitesOfSameType = sites};
}

TownForPlans With(std::initializer_list<AbodeNumber> built)
{
	TownForPlans town;
	for (const auto number : built)
	{
		town.built.at(static_cast<size_t>(number)) = 1;
	}
	return town;
}
} // namespace

TEST(PlannedBuildingRules, AHouseIsNotWantedWhileThereIsRoomForATenthMore)
{
	// Forty people want room for five more; ten is plenty
	EXPECT_FLOAT_EQ(DesireToBeBuilt(House(), Town(10, 0, 30, 10)), 0.0f);
	// Five is just enough to keep wanting it as much as the kind is wanted
	EXPECT_FLOAT_EQ(DesireToBeBuilt(House(), Town(5, 0, 30, 10)), 0.25f);
	// The homeless take up room
	EXPECT_FLOAT_EQ(DesireToBeBuilt(House(), Town(10, 5, 30, 10)), 0.25f);
	// A kind wanted not at all is never wanted
	auto unwanted = House();
	unwanted.desireToBeBuilt = 0.0f;
	EXPECT_FLOAT_EQ(DesireToBeBuilt(unwanted, Town(-5, 3, 30, 10)), 0.0f);
}

TEST(PlannedBuildingRules, AHouseIsWantedMoreTheShorterOfRoomTheTownIs)
{
	// Short by one of three needed: scale 0.25 + 1/3, and one short is half a house of two: 0.3 + 0.6
	EXPECT_NEAR(DesireToBeBuilt(House(), Town(0, 1, 18, 2)), 0.9f * (0.25f + 1.0f / 3.0f), 1e-6f);
	// Short by three: the scale is held at 0.8, and three short is more than a house: 2/3 of 0.2, plus 0.6
	EXPECT_NEAR(DesireToBeBuilt(House(), Town(0, 3, 18, 2)), (2.0f / 3.0f * 0.2f + 0.6f) * 0.8f, 1e-6f);
	// Shared over its sites
	EXPECT_NEAR(DesireToBeBuilt(House(2), Town(0, 3, 18, 2)), (2.0f / 3.0f * 0.2f + 0.6f) * 0.8f / 2.0f, 1e-6f);
	// Short by twenty-five counts as short by ten
	EXPECT_NEAR(DesireToBeBuilt(House(), Town(0, 25, 100, 0)), (2.0f / 10.0f * 0.2f + 0.6f) * 0.8f, 1e-6f);
}

TEST(PlannedBuildingRules, AHouseIsWantedLessForEachOfItsKindStanding)
{
	auto town = Town(3, 0, 30, 10);
	town.built.at(static_cast<size_t>(AbodeNumber::A)) = 4;
	// A tenth off for each of four
	EXPECT_NEAR(DesireToBeBuilt(House(), town), 0.25f - 0.025f * 4.0f, 1e-6f);
	// Past nine, all but one share in as many again
	town.built.at(static_cast<size_t>(AbodeNumber::A)) = 12;
	EXPECT_NEAR(DesireToBeBuilt(House(), town), 0.25f / 13.0f, 1e-6f);
	// Houses of another kind don't count
	town.built.at(static_cast<size_t>(AbodeNumber::A)) = 0;
	town.built.at(static_cast<size_t>(AbodeNumber::B)) = 12;
	EXPECT_FLOAT_EQ(DesireToBeBuilt(House(), town), 0.25f);
}

TEST(PlannedBuildingRules, CivicBuildingsAreWantedUntilTheTownHasOne)
{
	EXPECT_FLOAT_EQ(DesireToBeBuilt(Civic(AbodeType::StoragePit, 0.9f), With({})), 0.9f);
	EXPECT_FLOAT_EQ(DesireToBeBuilt(Civic(AbodeType::StoragePit, 0.9f), With({AbodeNumber::StoragePit})), 0.0f);
	EXPECT_FLOAT_EQ(DesireToBeBuilt(Civic(AbodeType::StoragePit, 0.9f, 1), With({})), 0.0f);
	EXPECT_FLOAT_EQ(DesireToBeBuilt(Civic(AbodeType::Creche, 0.35f), With({AbodeNumber::Creche})), 0.0f);
	EXPECT_FLOAT_EQ(DesireToBeBuilt(Civic(AbodeType::Creche, 0.35f), With({AbodeNumber::Graveyard})), 0.35f);
	// A graveyard going up still leaves the want of another, shared over the sites
	EXPECT_FLOAT_EQ(DesireToBeBuilt(Civic(AbodeType::Graveyard, 0.3f, 2), With({})), 0.15f);
	EXPECT_FLOAT_EQ(DesireToBeBuilt(Civic(AbodeType::Graveyard, 0.3f), With({AbodeNumber::Graveyard})), 0.0f);
	// Football is never played
	EXPECT_FLOAT_EQ(DesireToBeBuilt(Civic(AbodeType::FootballPitch, 1.0f), With({})), 0.0f);
}

TEST(PlannedBuildingRules, AVillageCentreAndATotemShutEachOtherOut)
{
	EXPECT_FLOAT_EQ(DesireToBeBuilt(Civic(AbodeType::TownCentre, 1.0f), With({})), 1.0f);
	EXPECT_FLOAT_EQ(DesireToBeBuilt(Civic(AbodeType::TownCentre, 1.0f), With({AbodeNumber::Totem})), 0.0f);
	EXPECT_FLOAT_EQ(DesireToBeBuilt(Civic(AbodeType::Totem, 1.0f), With({})), 1.0f);
	EXPECT_FLOAT_EQ(DesireToBeBuilt(Civic(AbodeType::Totem, 1.0f), With({AbodeNumber::TownCentre})), 0.0f);
	// The neutral player's town wants no totem
	auto neutral = With({});
	neutral.neutral = true;
	EXPECT_FLOAT_EQ(DesireToBeBuilt(Civic(AbodeType::Totem, 1.0f), neutral), 0.0f);
	EXPECT_FLOAT_EQ(DesireToBeBuilt(Civic(AbodeType::TownCentre, 1.0f), neutral), 1.0f);
}

TEST(PlannedBuildingRules, AnotherWorkshopIsWantedHalfAsMuch)
{
	EXPECT_FLOAT_EQ(DesireToBeBuilt(Civic(AbodeType::Workshop, 0.5f), With({})), 0.5f);
	EXPECT_FLOAT_EQ(DesireToBeBuilt(Civic(AbodeType::Workshop, 0.5f), With({AbodeNumber::Workshop})), 0.25f);
	EXPECT_FLOAT_EQ(DesireToBeBuilt(Civic(AbodeType::Workshop, 0.5f, 2), With({AbodeNumber::Workshop})), 0.125f);
	auto neutral = With({AbodeNumber::Workshop});
	neutral.neutral = true;
	EXPECT_FLOAT_EQ(DesireToBeBuilt(Civic(AbodeType::Workshop, 0.5f), neutral), 0.0f);
}

TEST(PlannedBuildingRules, ASpellDispenserWaitsForEveryOtherCivicBuilding)
{
	EXPECT_FLOAT_EQ(
	    DesireToBeBuilt(Civic(AbodeType::SpellDispenser, 0.3f), With({AbodeNumber::TownCentre, AbodeNumber::StoragePit,
	                                                                  AbodeNumber::Graveyard, AbodeNumber::Workshop})),
	    0.0f);
	EXPECT_FLOAT_EQ(DesireToBeBuilt(Civic(AbodeType::SpellDispenser, 0.3f),
	                                With({AbodeNumber::TownCentre, AbodeNumber::StoragePit, AbodeNumber::Graveyard,
	                                      AbodeNumber::Creche, AbodeNumber::Workshop})),
	                0.3f);
}

TEST(PlannedBuildingRules, OtherBuildingsAreWantedAsTheirKindSharedOverTheirSites)
{
	// A temple is a citadel
	EXPECT_FLOAT_EQ(DesireToBeBuilt(Civic(AbodeType::Citadel, 1.0f), With({})), 1.0f);
	EXPECT_FLOAT_EQ(DesireToBeBuilt(Civic(AbodeType::Citadel, 1.0f, 2), With({})), 0.5f);
	// A wonder by the town's want of one, once that sets its villagers to it
	auto town = With({});
	town.wonderDesire = 0.5f;
	town.wonderTrigger = 0.6f;
	EXPECT_FLOAT_EQ(DesireToBeBuilt(Civic(AbodeType::Wonder, 0.4f), town), 0.0f);
	town.wonderDesire = 0.7f;
	EXPECT_FLOAT_EQ(DesireToBeBuilt(Civic(AbodeType::Wonder, 0.4f), town), 0.28f);
}

TEST(PlannedBuildingRules, TheTownPicksThePlanOfTheWantedTypeItWantsMost)
{
	const std::array<PlannedCandidate, 6> plans {
	    PlannedCandidate {.type = AbodeType::LivingQuarters, .desire = 0.2f},
	    PlannedCandidate {.type = AbodeType::StoragePit, .desire = 0.9f},
	    PlannedCandidate {.type = AbodeType::LivingQuarters, .desire = 0.3f},
	    PlannedCandidate {.type = AbodeType::LivingQuarters, .desire = 0.3f},
	    PlannedCandidate {.type = AbodeType::Citadel, .desire = 1.0f},
	    PlannedCandidate {.type = AbodeType::Wonder, .desire = 5.0f},
	};
	// The first planned on a tie; a wonder is neither a house nor civic
	EXPECT_EQ(BestPlanned(plans, AbodeType::LivingQuarters), 2u);
	EXPECT_EQ(BestPlanned(plans, AbodeType::Civic), 4u);
	// A windmill counts as a house
	const std::array<PlannedCandidate, 2> windmill {
	    PlannedCandidate {.type = AbodeType::LivingQuarters, .desire = 0.2f},
	    PlannedCandidate {.type = AbodeType::Windmill, .desire = 0.5f},
	};
	EXPECT_EQ(BestPlanned(windmill, AbodeType::LivingQuarters), 1u);
	// None wanted at all, none picked
	const std::array<PlannedCandidate, 1> unwanted {PlannedCandidate {.type = AbodeType::LivingQuarters, .desire = 0.0f}};
	EXPECT_EQ(BestPlanned(unwanted, AbodeType::LivingQuarters), std::nullopt);
	EXPECT_EQ(BestPlanned({}, AbodeType::Civic), std::nullopt);
}

TEST(PlannedBuildingRules, APlaceWithinTheNearestOtherTownPasses)
{
	const std::vector<OtherTown> towns {
	    OtherTown {.cell = {100, 100}, .areaMin = glm::ivec2 {90, 90}, .areaMax = glm::ivec2 {110, 110}},
	    OtherTown {.cell = {200, 200}, .areaMin = glm::ivec2 {0, 0}, .areaMax = glm::ivec2 {300, 300}},
	};
	// Four cells beyond its box still count, five don't
	EXPECT_TRUE(WithinNearestOtherTown({114, 100}, towns));
	EXPECT_FALSE(WithinNearestOtherTown({115, 100}, towns));
	EXPECT_TRUE(WithinNearestOtherTown({100, 86}, towns));
	EXPECT_FALSE(WithinNearestOtherTown({100, 85}, towns));
	// Only the nearest counts, the first on a tie, the diagonal at half again
	EXPECT_FALSE(WithinNearestOtherTown({150, 150}, towns));
	const std::vector<OtherTown> swapped {towns[1], towns[0]};
	EXPECT_TRUE(WithinNearestOtherTown({150, 150}, swapped));
	// Near the map's edge the margin wraps round and the cell lies outside every box
	const std::vector<OtherTown> wide {
	    OtherTown {.cell = {150, 150}, .areaMin = glm::ivec2 {0, 0}, .areaMax = glm::ivec2 {300, 300}}};
	EXPECT_TRUE(WithinNearestOtherTown({4, 150}, wide));
	EXPECT_FALSE(WithinNearestOtherTown({2, 150}, wide));
	// A town with no buildings or fields covers nothing, and with no other town there is none to be in
	const std::vector<OtherTown> bare {OtherTown {.cell = {100, 100}}};
	EXPECT_FALSE(WithinNearestOtherTown({100, 100}, bare));
	EXPECT_FALSE(WithinNearestOtherTown({100, 100}, {}));
}

TEST(PlannedBuildingRules, ANewHouseNeedsDryLandAndRoomFromTheBuildingsAboutIt)
{
	// A house with no footprint passes, wet or not
	EXPECT_TRUE(IsClearToBuild({}, false, 6.0f));
	const std::array<BuildingNearby, 1> clear {BuildingNearby {.distance = 10.0f, .radius = 4.0f}};
	const std::array<BuildingNearby, 1> inTheWay {BuildingNearby {.distance = 9.9f, .radius = 4.0f}};
	const std::array<FootprintCell, 2> open {FootprintCell {}, FootprintCell {.buildings = clear}};
	const std::array<FootprintCell, 2> blocked {FootprintCell {.buildings = clear}, FootprintCell {.buildings = inTheWay}};
	EXPECT_TRUE(IsClearToBuild(open, true, 6.0f));
	EXPECT_FALSE(IsClearToBuild(open, false, 6.0f));
	EXPECT_FALSE(IsClearToBuild(blocked, true, 6.0f));
	EXPECT_TRUE(IsClearToBuild(blocked, true, 5.9f));
}

TEST(PlannedBuildingRules, AScriptNamingAPlaceHasEachTownStartItsOwnNearestPlan)
{
	using building_construction::PlannedCandidate;
	constexpr glm::vec2 k_Place {100.0f, 100.0f};
	const std::vector<std::vector<PlannedCandidate>> towns {
	    // A house 3 m off with a 2.5 m reach is within a metre beyond its reach; the one further in is nearer still
	    {PlannedCandidate {.position = {103.0f, 100.0f}, .reach = 2.5f},
	     PlannedCandidate {.position = {101.0f, 100.0f}, .reach = 2.5f}},
	    // Too far for its reach
	    {PlannedCandidate {.position = {110.0f, 100.0f}, .reach = 2.5f}},
	    // No plans at all
	    {},
	    // Another town's plan at the same place is started too; the later of two equally near
	    {PlannedCandidate {.position = {100.0f, 102.0f}, .reach = 2.0f},
	     PlannedCandidate {.position = {102.0f, 100.0f}, .reach = 2.0f}},
	};
	const auto started = StartedByScript(towns, k_Place);
	ASSERT_EQ(started.size(), towns.size());
	EXPECT_EQ(started[0], 1u);
	EXPECT_EQ(started[1], std::nullopt);
	EXPECT_EQ(started[2], std::nullopt);
	EXPECT_EQ(started[3], 1u);
}

TEST(PlannedBuildingRules, ABuildingPlannedAgainKeepsTheTurnItWasPlacedWith)
{
	constexpr float k_FullTurn = 2.0f * std::numbers::pi_v<float>;
	for (const float angle : {0.0f, 0.5f, 1.5f, 3.0f, 4.776f, 6.2f})
	{
		const auto rotation = glm::mat3(glm::eulerAngleY(-angle));
		EXPECT_NEAR(YAngleOf(rotation), angle, 1.0e-4f) << angle;
	}
	// A turn past a full one, or backwards, reads as the same turn within one full turn
	EXPECT_NEAR(YAngleOf(glm::mat3(glm::eulerAngleY(-(k_FullTurn + 1.0f)))), 1.0f, 1.0e-4f);
	EXPECT_NEAR(YAngleOf(glm::mat3(glm::eulerAngleY(1.0f))), k_FullTurn - 1.0f, 1.0e-4f);
}
