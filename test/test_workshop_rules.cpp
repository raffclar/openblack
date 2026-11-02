/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cstdint>
#include <cstring>

#include <array>
#include <bit>

#include <glm/mat4x4.hpp>
#include <glm/vec2.hpp>
#include <gtest/gtest.h>

#include "3D/MapCoords.h"
#include "ECS/Components/Scaffold.h"
#include "ECS/Components/Workshop.h"
#include "ECS/Registry.h"
#include "ECS/WorkshopRules.h"

using namespace openblack::ecs;
using namespace openblack::ecs::components;
using Place = Workshop::Place;

namespace
{
uint32_t BitsOf(float value)
{
	uint32_t bits = 0;
	std::memcpy(&bits, &value, sizeof(bits));
	return bits;
}

workshop::TurnFacts Facts(bool functional, int32_t room, uint32_t wood)
{
	return {.functional = functional, .room = room, .wood = wood, .scaffoldWood = 100};
}
} // namespace

// The scale of a scaffold of each worth, as the game's floats give it
TEST(WorkshopRules, ScaleOfValueMatchesTheGameFloats)
{
	constexpr std::array<uint32_t, 7> k_Bits {0x3F000000, 0x3F333333, 0x3F666666, 0x3F8CCCCD,
	                                          0x3FA66666, 0x3FC00000, 0x3FD9999A};
	for (uint32_t value = 0; value < k_Bits.size(); ++value)
	{
		EXPECT_EQ(BitsOf(workshop::ScaleOfValue(value)), k_Bits.at(value)) << value;
	}
}

// The worth of a scaffold comes back from its scale, also from the scale that a script gives
TEST(WorkshopRules, ValueOfScaleRoundsToTheNearestWorth)
{
	for (uint32_t value = 0; value < 7; ++value)
	{
		EXPECT_EQ(workshop::ValueOfScale(workshop::ScaleOfValue(value)), value) << value;
	}
	EXPECT_EQ(workshop::ValueOfScale(1.0f), 3u);
	EXPECT_EQ(workshop::ValueOfScale(0.79f), 1u);
	EXPECT_EQ(workshop::ValueOfScale(0.81f), 2u);
}

// A script's size is cut towards nought and taken as a worth; a negative size wraps round to a very large worth
TEST(WorkshopRules, AScriptSizeIsCutToAWholeWorth)
{
	EXPECT_EQ(BitsOf(workshop::ScaleOfScriptSize(5.0f)), BitsOf(workshop::ScaleOfValue(5)));
	EXPECT_EQ(BitsOf(workshop::ScaleOfScriptSize(3.9f)), BitsOf(workshop::ScaleOfValue(3)));
	EXPECT_EQ(BitsOf(workshop::ScaleOfScriptSize(0.5f)), BitsOf(workshop::ScaleOfValue(0)));
	EXPECT_EQ(BitsOf(workshop::ScaleOfScriptSize(-0.5f)), BitsOf(workshop::ScaleOfValue(0)));
	EXPECT_EQ(BitsOf(workshop::ScaleOfScriptSize(-1.0f)), BitsOf(workshop::ScaleOfValue(0xFFFFFFFFu)));
	EXPECT_EQ(workshop::ValueOfScale(workshop::ScaleOfScriptSize(5.0f)), 5u);
}

// A storage pit gets the wood of one scaffold for each scaffold that the given scaffold is worth
TEST(WorkshopRules, AScaffoldIsWorthTheWoodOfEachScaffold)
{
	EXPECT_EQ(workshop::WoodOfScaffold(1, 2500), 2500u);
	EXPECT_EQ(workshop::WoodOfScaffold(7, 2500), 17500u);
	EXPECT_EQ(workshop::WoodOfScaffold(0, 2500), 0u);
}

// Only the worth that the building does not need stays in the hand
TEST(WorkshopRules, TheRestOfTheWorthStaysInTheHand)
{
	EXPECT_EQ(workshop::RestInHand(5, 3), 2u);
	EXPECT_EQ(workshop::RestInHand(7, 1), 6u);
	EXPECT_FALSE(workshop::RestInHand(3, 3).has_value());
	EXPECT_FALSE(workshop::RestInHand(2, 3).has_value());
	// The game takes the two counts as signed numbers: a very large worth is a negative number
	EXPECT_FALSE(workshop::RestInHand(0xFFFFFFFFu, 3).has_value());
}

// A tap breaks only a scaffold worth more than one that lies still, has no script building, and can leave its site
TEST(WorkshopRules, ATapBreaksOnlyAScaffoldThatTheHandCouldTake)
{
	const workshop::TapFacts loose {.value = 2};
	EXPECT_TRUE(workshop::CanTapToBreak(loose));
	EXPECT_FALSE(workshop::CanTapToBreak({.value = 1}));
	EXPECT_FALSE(workshop::CanTapToBreak({.value = 3, .inPhysics = true}));
	EXPECT_FALSE(workshop::CanTapToBreak({.value = 3, .scriptBuilding = true}));
	EXPECT_FALSE(workshop::CanTapToBreak({.value = 3, .onSite = true, .kept = false}));
	EXPECT_TRUE(workshop::CanTapToBreak({.value = 3, .onSite = true, .kept = true}));
	// The keep test does not matter for a scaffold on no site
	EXPECT_TRUE(workshop::CanTapToBreak({.value = 3, .onSite = false, .kept = false}));
}

// The parts move apart along a quarter turn, the float of pi / 2 times the draw, added to the scaffold's angle
TEST(WorkshopRules, TheBreakAngleIsAQuarterTurnFromTheScaffold)
{
	EXPECT_EQ(BitsOf(workshop::BreakAngle(0, 0.25f)), BitsOf(0.25f));
	EXPECT_EQ(BitsOf(workshop::BreakAngle(1, 0.0f)), 0x3FC90FDBu);
	EXPECT_EQ(BitsOf(workshop::BreakAngle(2, 0.0f)), 0x40490FDBu);
	const float three = 3.0f * std::bit_cast<float>(0x3FC90FDBu);
	EXPECT_EQ(BitsOf(workshop::BreakAngle(3, 1.0f)), BitsOf(1.0f + three));
}

// The abode number -1 gives a scaffold no building of a script
TEST(WorkshopRules, AScriptBuildingIsAnAbodeNumber)
{
	EXPECT_EQ(workshop::ScriptBuildingOf(12), openblack::AbodeNumber::TownCentre);
	EXPECT_EQ(workshop::ScriptBuildingOf(7), openblack::AbodeNumber::StoragePit);
	EXPECT_FALSE(workshop::ScriptBuildingOf(-1).has_value());
}

// The game cuts the fraction off the time to make a scaffold
TEST(WorkshopRules, MakingTurnsCutsTheFractionOff)
{
	EXPECT_EQ(workshop::MakingTurns(40.9f), 40);
	EXPECT_EQ(workshop::MakingTurns(40.0f), 40);
	EXPECT_EQ(workshop::MakingTurns(0.5f), 0);
}

// A workshop starts a scaffold only when it works, has room and has the wood for it
TEST(WorkshopRules, StartsWithRoomAndWood)
{
	int32_t turns = 0;
	EXPECT_EQ(workshop::NextEvent(turns, Facts(true, 1, 100)), workshop::TurnEvent::Starts);
	EXPECT_EQ(workshop::NextEvent(turns, Facts(true, 1, 99)), workshop::TurnEvent::None);
	EXPECT_EQ(workshop::NextEvent(turns, Facts(true, 0, 500)), workshop::TurnEvent::None);
	EXPECT_EQ(workshop::NextEvent(turns, Facts(false, 3, 500)), workshop::TurnEvent::None);
	// The room can be less than 0, which counts as room
	EXPECT_EQ(workshop::NextEvent(turns, Facts(true, -1, 100)), workshop::TurnEvent::Starts);
	EXPECT_EQ(turns, 0);
}

// A workshop counts its turns down and finishes the scaffold at 0; it does not count while it does not work
TEST(WorkshopRules, CountsDownAndFinishes)
{
	int32_t turns = 3;
	EXPECT_EQ(workshop::NextEvent(turns, Facts(true, 0, 0)), workshop::TurnEvent::None);
	EXPECT_EQ(turns, 2);
	EXPECT_EQ(workshop::NextEvent(turns, Facts(false, 0, 0)), workshop::TurnEvent::None);
	EXPECT_EQ(turns, 2);
	EXPECT_EQ(workshop::NextEvent(turns, Facts(true, 0, 0)), workshop::TurnEvent::None);
	EXPECT_EQ(workshop::NextEvent(turns, Facts(true, 0, 0)), workshop::TurnEvent::Finishes);
	EXPECT_EQ(turns, 0);
}

TEST(WorkshopRules, FirstFreePlaceIsTheFirstPlaceWhenNoneIsFree)
{
	EXPECT_EQ(workshop::FirstFreePlace({Place::Free, Place::Free, Place::Free}), 0);
	EXPECT_EQ(workshop::FirstFreePlace({Place::Holds, Place::Away, Place::Free}), 2);
	EXPECT_EQ(workshop::FirstFreePlace({Place::Holds, Place::Free, Place::Free}), 1);
	EXPECT_EQ(workshop::FirstFreePlace({Place::Holds, Place::Away, Place::Holds}), 0);
}

// A workshop lets a scaffold go when it lay for the turns after a hand put it down, unless a script gave its building
TEST(WorkshopRules, KeepsScaffoldUntilItLayForTheTurns)
{
	// A scaffold that no hand put down lay for no turns
	EXPECT_TRUE(workshop::KeepsScaffold(0, 5000, 150, false));
	EXPECT_FALSE(workshop::KeepsScaffold(0, 5000, 0, false));
	EXPECT_TRUE(workshop::KeepsScaffold(1000, 1149, 150, false));
	EXPECT_FALSE(workshop::KeepsScaffold(1000, 1150, 150, false));
	EXPECT_TRUE(workshop::KeepsScaffold(1000, 1150, 150, true));
}

TEST(WorkshopRules, AddAndRemoveKeepTheCount)
{
	Registry registry;
	const auto a = registry.Create();
	const auto b = registry.Create();
	const auto c = registry.Create();
	Workshop data;
	workshop::AddScaffold(data, a);
	workshop::AddScaffold(data, b);
	workshop::AddScaffold(data, c);
	ASSERT_EQ(data.scaffolds.size(), 3u);
	EXPECT_EQ(data.scaffolds.front(), c);
	EXPECT_EQ(data.counted, 3u);
	data.places = {Place::Holds, Place::Holds, Place::Holds};
	// The yard is not full now: the town wants again what it needs
	EXPECT_TRUE(workshop::RemoveScaffold(data, b, 1));
	EXPECT_EQ(data.counted, 2u);
	EXPECT_EQ(data.places.at(1), Place::Free);
	// A scaffold that the workshop does not have changes nothing
	EXPECT_FALSE(workshop::RemoveScaffold(data, b, 0));
	EXPECT_EQ(data.places.at(0), Place::Holds);
	EXPECT_FALSE(workshop::RemoveScaffold(data, a, 0));
	EXPECT_EQ(data.counted, 1u);
	// None counts, but its count goes up
	workshop::AddScaffold(data, entt::null);
	EXPECT_EQ(data.scaffolds.size(), 1u);
	EXPECT_EQ(data.counted, 2u);
}

TEST(WorkshopRules, StandsInYardOnlyInAPlaceThatHoldsIt)
{
	Registry registry;
	const auto owner = registry.Create();
	auto& data = registry.Assign<Workshop>(owner);
	data.places = {Place::Free, Place::Holds, Place::Away};
	const auto held = registry.Create();
	registry.Assign<Scaffold>(held, Scaffold {.workshop = owner, .place = 1});
	const auto away = registry.Create();
	registry.Assign<Scaffold>(away, Scaffold {.workshop = owner, .place = 2});
	const auto loose = registry.Create();
	registry.Assign<Scaffold>(loose, Scaffold {});
	const auto other = registry.Create();
	EXPECT_TRUE(workshop::StandsInYard(registry, held));
	EXPECT_FALSE(workshop::StandsInYard(registry, away));
	EXPECT_FALSE(workshop::StandsInYard(registry, loose));
	EXPECT_FALSE(workshop::StandsInYard(registry, other));
}

TEST(WorkshopRules, OnlyTheWoodPileOfAWorkshopIsItsWoodPile)
{
	Registry registry;
	const auto owner = registry.Create();
	const auto pile = registry.Create();
	const auto other = registry.Create();
	registry.Assign<Workshop>(owner).woodPile = pile;
	registry.Assign<Workshop>(other);
	EXPECT_TRUE(workshop::IsWoodPile(registry, pile));
	EXPECT_FALSE(workshop::IsWoodPile(registry, other));
	EXPECT_FALSE(workshop::IsWoodPile(registry, entt::null));
}

TEST(WorkshopRules, TheSpecialPointIsThePlaceOfTheSpecialPlaceWithItsHeight)
{
	glm::mat4 point {1.0f};
	point[3] = glm::vec4(1.5f, 2.0f, -3.25f, 1.0f);
	const auto building =
	    workshop::BuildingMatrix(glm::vec3(0.0f, 0.75f, 0.0f), false, 1.2f, glm::vec3(2000.0f, 40.0f, 2100.0f));
	const auto world = workshop::SpecialPointOf(point, building);
	const auto place = workshop::SpecialPlaceOf(point, building);
	const auto coords = openblack::map_coords::FromMetres(glm::vec2(world.x, world.z));
	EXPECT_EQ(coords.x, place.coords.x);
	EXPECT_EQ(coords.z, place.coords.z);
	// The height is the building's height and the point's height, by the building's scale
	EXPECT_NEAR(world.y, 40.0f + (2.0f * 1.2f), 1e-4f);
}

TEST(WorkshopRules, TheHandJoinsScaffoldsOnlyWhenTheirSumIsBelowTheMost)
{
	const workshop::JoinFacts facts {.heldValue = 2, .targetValue = 3, .mostForCombining = 6};
	EXPECT_TRUE(workshop::CanJoin(facts));
	// The two together must be worth less than the most, or as much with the football feature
	EXPECT_FALSE(workshop::CanJoin({.heldValue = 3, .targetValue = 3, .mostForCombining = 6}));
	EXPECT_TRUE(workshop::CanJoin({.heldValue = 3, .targetValue = 3, .mostForCombining = 6, .football = true}));
	// A table row with no most lets any sum through
	EXPECT_TRUE(workshop::CanJoin({.heldValue = 40, .targetValue = 50, .mostForCombining = 0}));
}

TEST(WorkshopRules, TheHandDoesNotJoinScaffoldsOfScriptsOrInThePhysicsOrOnAStartedSite)
{
	const workshop::JoinFacts facts {.heldValue = 1, .targetValue = 1, .mostForCombining = 6};
	auto script = facts;
	script.heldScriptBuilding = true;
	EXPECT_FALSE(workshop::CanJoin(script));
	script = facts;
	script.targetScriptBuilding = true;
	EXPECT_FALSE(workshop::CanJoin(script));
	auto physics = facts;
	physics.targetInPhysics = true;
	EXPECT_FALSE(workshop::CanJoin(physics));
	auto site = facts;
	site.targetOnSite = true;
	EXPECT_TRUE(workshop::CanJoin(site));
	site.targetSiteBuilt = 0.01f;
	EXPECT_FALSE(workshop::CanJoin(site));
}

TEST(WorkshopRules, AJoinedScaffoldBuildsForTheTownOfTheScaffoldWorthMore)
{
	using workshop::JoinedTown;
	EXPECT_EQ(workshop::TownOfJoined({.heldHasTown = false, .targetHasTown = true}), JoinedTown::Target);
	EXPECT_EQ(workshop::TownOfJoined({.heldHasTown = false, .targetHasTown = false}), JoinedTown::Target);
	EXPECT_EQ(workshop::TownOfJoined({.heldHasTown = true, .targetHasTown = false}), JoinedTown::Held);
	EXPECT_EQ(workshop::TownOfJoined({.heldHasTown = true, .targetHasTown = true, .heldValue = 3, .targetValue = 2}),
	          JoinedTown::Held);
	EXPECT_EQ(workshop::TownOfJoined({.heldHasTown = true, .targetHasTown = true, .heldValue = 2, .targetValue = 3}),
	          JoinedTown::Target);
	// When the two are worth the same, the held town only when the player who holds it has that town
	EXPECT_EQ(workshop::TownOfJoined(
	              {.heldHasTown = true, .targetHasTown = true, .heldValue = 2, .targetValue = 2, .holderOwnsHeldTown = true}),
	          JoinedTown::Held);
	EXPECT_EQ(workshop::TownOfJoined({.heldHasTown = true, .targetHasTown = true, .heldValue = 2, .targetValue = 2}),
	          JoinedTown::Target);
}

TEST(WorkshopRules, AFoundedTownTakesTheGameIdOrTheFirstFreeIdAfterIt)
{
	EXPECT_EQ(workshop::FoundedTownId({}), workshop::k_FoundedTownId);
	const std::array<uint32_t, 3> land {0, 1, 2};
	EXPECT_EQ(workshop::FoundedTownId(land), 703058u);
	// A second founded town takes the next free id
	const std::array<uint32_t, 4> oneFounded {0, 1, 703058, 2};
	EXPECT_EQ(workshop::FoundedTownId(oneFounded), 703059u);
	const std::array<uint32_t, 3> twoFounded {703059, 703058, 4};
	EXPECT_EQ(workshop::FoundedTownId(twoFounded), 703060u);
}

TEST(WorkshopRules, AThingIsInTheClearingReachWhenItsEdgeIsWithinTheReach)
{
	EXPECT_TRUE(workshop::InClearingReach(3.0f, 1.0f, 2.0f));
	EXPECT_TRUE(workshop::InClearingReach(2.5f, 1.0f, 2.0f));
	EXPECT_FALSE(workshop::InClearingReach(3.5f, 1.0f, 2.0f));
}

TEST(WorkshopRules, AClearingScaffoldPushesAThingTwiceAsFarAsItsEdge)
{
	const auto push = workshop::PushOutOfWay({10.0f, 20.0f}, {11.0f, 20.5f});
	EXPECT_EQ(push.offset, glm::vec2(2.0f, 1.0f));
	EXPECT_EQ(push.velocity, glm::vec3(2.0f, 0.2f, 1.0f));
	// The larger part keeps its sign: a thing at low x and low z flies the other way
	const auto low = workshop::PushOutOfWay({0.0f, 0.0f}, {-1.0f, -2.0f});
	EXPECT_EQ(low.offset, glm::vec2(-2.0f, -4.0f));
	EXPECT_EQ(low.velocity, glm::vec3(2.0f, 0.2f, 4.0f));
	// A small larger part with its sign gives a very high speed
	const auto fast = workshop::PushOutOfWay({0.0f, 0.0f}, {-0.0005f, -2.0f});
	EXPECT_NEAR(fast.velocity.z, 8000.0f, 1.0f);
}

TEST(WorkshopRules, APushedThingStartsAtTheHeightOfTheScaffoldAboveTheLandPlusHalfAMetre)
{
	const auto push = workshop::PushOutOfWay({10.0f, 20.0f}, {11.0f, 20.5f});
	const auto start = workshop::PushStart({10.0f, 20.0f}, push);
	EXPECT_EQ(start, openblack::map_coords::FromMetres({12.0f, 21.0f}));
	// The start is a height of the world, not a height above the land: on high land the thing starts under the land
	EXPECT_EQ(workshop::PushStartHeight(0.0f), 0.5f);
	EXPECT_EQ(workshop::PushStartHeight(2.25f), 2.75f);
}

TEST(WorkshopRules, AClearingScaffoldPushesOrRemovesThingsAndLeavesCreaturesAlone)
{
	using workshop::ClearingAction;
	EXPECT_EQ(workshop::ClearingActionFor({.canFly = true}), ClearingAction::Push);
	EXPECT_EQ(workshop::ClearingActionFor({.held = true, .canFly = true}), ClearingAction::Remove);
	EXPECT_EQ(workshop::ClearingActionFor({}), ClearingAction::Remove);
	// A thing with the fixed mark (a rock, a dead tree, a piece of a building) goes, also when it can fly
	EXPECT_EQ(workshop::ClearingActionFor({.fixed = true, .canFly = true}), ClearingAction::Remove);
	EXPECT_EQ(workshop::ClearingActionFor({.fixed = true}), ClearingAction::Remove);
	// A creature stays, also one that a hand holds
	EXPECT_EQ(workshop::ClearingActionFor({.creature = true}), ClearingAction::LeaveAlone);
	EXPECT_EQ(workshop::ClearingActionFor({.creature = true, .held = true}), ClearingAction::LeaveAlone);
}

TEST(WorkshopRules, AJoinPlaysTheFourSoundsInTurnAtTheHandForTheHeldScaffold)
{
	const auto held = static_cast<entt::entity>(42);
	const glm::vec3 hand {10.0f, 2.0f, -3.0f};
	uint32_t step = 0;
	for (const uint32_t sample : {201u, 202u, 203u, 204u, 201u})
	{
		const auto sound = workshop::JoinSoundOf(step, held, hand);
		EXPECT_EQ(sound.sample, sample);
		ASSERT_TRUE(sound.options.position.has_value());
		EXPECT_EQ(*sound.options.position, hand);
		EXPECT_EQ(sound.options.owner, held);
		EXPECT_EQ(sound.options.follows, openblack::audio::Follows::Nothing);
		EXPECT_FALSE(sound.options.voicePlayType.has_value());
		step = workshop::NextJoinSound(step);
	}
}

TEST(WorkshopRules, ThePuffOfAJoinShowsTheColourOfTheNeutralPlayer)
{
	EXPECT_EQ(workshop::k_JoinPuffPlayer, openblack::PlayerNames::NEUTRAL);
}
