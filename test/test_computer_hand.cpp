/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cstdint>

#include <map>
#include <optional>
#include <vector>

#include <gtest/gtest.h>

#include "3D/MapCoords.h"
#include "Common/GUtilsDistance.h"
#include "ECS/ComputerHand.h"
#include "ECS/ComputerMindRules.h"

using namespace openblack;
using namespace openblack::computer_hand;
using namespace openblack::computer_mind_rules;

namespace
{

constexpr auto k_Icon = static_cast<entt::entity>(5);
constexpr auto k_Store = static_cast<entt::entity>(6);

/// A flat land with things at places that the test sets
class FakeWorld final: public World
{
public:
	[[nodiscard]] std::optional<map_coords::MapCoords> CoordsOf(entt::entity thing) const override
	{
		const auto found = places.find(thing);
		if (found == places.end())
		{
			return std::nullopt;
		}
		return map_coords::MapCoords {
		    .x = map_coords::ToFixed(found->second.x), .z = map_coords::ToFixed(found->second.y), .altitude = 0.0f};
	}
	[[nodiscard]] float HeightOf(entt::entity /*thing*/) const override { return 4.0f; }
	[[nodiscard]] float LandHeight(const map_coords::MapCoords& /*coords*/) const override { return 20.0f; }
	[[nodiscard]] bool IconHasSite(entt::entity icon) const override { return icon == k_Icon; }
	[[nodiscard]] bool IconCharging(entt::entity /*icon*/) const override { return charging; }
	bool StartChant(entt::entity /*icon*/, MagicType /*type*/) override { return startWorks; }
	[[nodiscard]] float ChantNeeded(entt::entity /*icon*/) const override { return needed; }
	[[nodiscard]] bool Angry(MagicType type) const override { return type == MagicType::Fireball; }
	bool Cast(MagicType type, entt::entity /*target*/, entt::entity /*icon*/, glm::vec3 /*hand*/) override
	{
		cast.push_back(type);
		return castWorks;
	}
	[[nodiscard]] float CastSeconds(MagicType /*type*/) const override { return 0.5f; }
	[[nodiscard]] uint32_t SeedGesture(MagicType /*type*/, int32_t stage) const override { return stage == 0 ? 1 : 9; }
	[[nodiscard]] float GestureLength(uint32_t gesture) const override { return gesture == k_GestureCircle ? 2.0f : 1.0f; }
	[[nodiscard]] glm::vec3 GesturePoint(uint32_t /*gesture*/, float share) const override
	{
		// A line from the bottom left of the square to its top right
		return {share, 0.0f, share};
	}
	void ShowGesture(uint32_t gesture, glm::vec3 corner, float size) override
	{
		shown.push_back(gesture);
		shownAt = corner;
		shownSize = size;
	}
	[[nodiscard]] bool HandHasRoom() const override { return held == entt::null; }
	void PutDownHeld(glm::vec3 /*hand*/) override
	{
		held = entt::null;
		++putDown;
	}
	[[nodiscard]] bool CanPickUp(entt::entity thing) const override { return places.contains(thing); }
	void PickUp(entt::entity thing, glm::vec3 /*hand*/) override { held = thing; }
	void UseHeldOn(entt::entity target, glm::vec3 /*hand*/) override
	{
		usedOn.push_back(target);
		held = entt::null;
	}
	void DropHeld(glm::vec3 hand) override
	{
		held = entt::null;
		droppedAt.push_back(hand);
	}

	std::map<entt::entity, glm::vec2> places;
	bool charging {false};
	bool startWorks {false};
	float needed {0.0f};
	bool castWorks {true};
	std::vector<MagicType> cast;
	std::vector<uint32_t> shown;
	glm::vec3 shownAt {0.0f};
	float shownSize {0.0f};
	entt::entity held {entt::null};
	int putDown {0};
	std::vector<entt::entity> usedOn;
	std::vector<glm::vec3> droppedAt;
};

} // namespace

TEST(ComputerHand, StepGetsTheMoveThatBringsTheHandToIt)
{
	FakeWorld world;
	world.places[k_Store] = {50.0f, 0.0f};
	HandAction action;
	Add(action, StepKind::CastSpellAtObject,
	    {Argument {.object = k_Store}, Argument {.number = static_cast<int32_t>(MagicType::Food)},
	     Argument {.object = k_Icon}});
	EXPECT_TRUE(Process(action, world));
	ASSERT_EQ(action.steps.size(), 2u);
	EXPECT_EQ(action.steps[0].kind, StepKind::MoveHandToObject);
	EXPECT_EQ(action.steps[0].arguments[0].object, k_Store);
	// An angry miracle is cast from near the thing
	HandAction angry;
	Add(angry, StepKind::CastSpellAtObject,
	    {Argument {.object = k_Store}, Argument {.number = static_cast<int32_t>(MagicType::Fireball)}});
	Process(angry, world);
	EXPECT_EQ(angry.steps[0].kind, StepKind::MoveHandNearObject);
}

TEST(ComputerHand, HandMovesFarStepsThenItsSpeed)
{
	FakeWorld world;
	world.places[k_Icon] = {1000.0f, 0.0f};
	HandAction action;
	Add(action, StepKind::MoveHandToObject, {Argument {.object = k_Icon}});
	Process(action, world); // the start finds the target
	Process(action, world);
	EXPECT_FLOAT_EQ(action.hand.x, 100.0f);
	EXPECT_FLOAT_EQ(action.hand.y, 30.0f);
	// Between 100 and 300 metres away the hand moves 10 metres a turn
	action.hand.x = 750.0f;
	Process(action, world);
	EXPECT_FLOAT_EQ(action.hand.x, 760.0f);
	// Near the target it moves at its speed, 90 metres a second
	action.hand.x = 950.0f;
	Process(action, world);
	EXPECT_FLOAT_EQ(action.hand.x, 959.0f);
	// It arrives when it is no more than a metre away, and the step ends
	action.hand.x = 999.5f;
	Process(action, world);
	Process(action, world);
	EXPECT_FALSE(Busy(action));
}

TEST(ComputerHand, ChantStopsWhenTheWorshippersCannotStart)
{
	FakeWorld world;
	HandAction action;
	Add(action, StepKind::MakeWorshippersChantForSpellNoWait,
	    {Argument {.number = static_cast<int32_t>(MagicType::Food)}, Argument {.object = k_Icon}});
	EXPECT_TRUE(Process(action, world)); // the start
	// It tries for 6 whole seconds, then the action ends badly
	for (int turn = 0; turn < 59; ++turn)
	{
		ASSERT_TRUE(Process(action, world)) << turn;
	}
	EXPECT_FALSE(Process(action, world));
	EXPECT_FALSE(Busy(action));
}

TEST(ComputerHand, ChantWaitsForTheFullCharge)
{
	FakeWorld world;
	world.startWorks = true;
	world.needed = 10.0f;
	HandAction action;
	Add(action, StepKind::MakeWorshippersChantForSpell,
	    {Argument {.number = static_cast<int32_t>(MagicType::Food)}, Argument {.object = k_Icon}});
	Process(action, world);
	Process(action, world);
	EXPECT_EQ(action.part, 2);
	Process(action, world);
	EXPECT_TRUE(Busy(action));
	world.needed = 0.0f;
	Process(action, world);
	EXPECT_FALSE(Busy(action));
}

TEST(ComputerHand, CastKeepsTheHandOverTheThing)
{
	FakeWorld world;
	world.places[k_Store] = {0.0f, 0.0f};
	HandAction action;
	action.moveFirst = false;
	Add(action, StepKind::CastSpellAtObject,
	    {Argument {.object = k_Store}, Argument {.number = static_cast<int32_t>(MagicType::Food)},
	     Argument {.object = k_Icon}});
	Process(action, world);
	Process(action, world);
	ASSERT_EQ(world.cast.size(), 1u);
	// The hand floats over the thing at two and a half times its height
	EXPECT_FLOAT_EQ(action.hand.y, 30.0f);
	EXPECT_EQ(action.counter, 5u);
	for (int turn = 0; turn < 4; ++turn)
	{
		Process(action, world);
	}
	EXPECT_TRUE(Busy(action));
	Process(action, world);
	EXPECT_FALSE(Busy(action));
	// A miracle that is not made ends the action badly
	world.castWorks = false;
	Add(action, StepKind::CastSpellAtObject, {Argument {.object = k_Store}, Argument {.number = 14}});
	action.moveFirst = false;
	Process(action, world);
	EXPECT_FALSE(Process(action, world));
}

TEST(ComputerHand, MapPositionFollowsTheHandEachTurn)
{
	FakeWorld world;
	HandAction action;
	EXPECT_EQ(action.coords.x, 0x00808000);
	action.hand = {100.0f, 0.0f, 200.0f};
	Process(action, world);
	EXPECT_EQ(action.coords.x, map_coords::ToFixed(100.0f));
	EXPECT_EQ(action.coords.z, map_coords::ToFixed(200.0f));
}

TEST(ComputerHand, GestureDrawsTheShapeInTheAirThenShowsItOnTheLand)
{
	FakeWorld world;
	HandAction action;
	action.hand = {100.0f, 30.0f, 200.0f};
	Add(action, StepKind::MakeGesture, {Argument {.number = static_cast<int32_t>(MagicType::Food)}, Argument {.number = 0}});
	// The start: the hand goes to the start of the shape, which is the corner here
	EXPECT_TRUE(Process(action, world));
	EXPECT_EQ(action.hand, glm::vec3(100.0f, 30.0f, 200.0f));
	// Half as long as the circle: 15 turns. The shape's x goes east and its z goes up.
	const float share = GestureSharePerTurn(1.0f, 2.0f);
	EXPECT_FLOAT_EQ(share, 1.0f / 15.0f);
	EXPECT_TRUE(Process(action, world));
	EXPECT_FLOAT_EQ(action.progress, share);
	EXPECT_FLOAT_EQ(action.hand.x, share * k_GestureSize + 100.0f);
	EXPECT_FLOAT_EQ(action.hand.y, share * k_GestureSize + 30.0f);
	EXPECT_FLOAT_EQ(action.hand.z, 200.0f);
	int turns = 1;
	while (action.part == 1)
	{
		ASSERT_TRUE(Process(action, world));
		++turns;
		ASSERT_LT(turns, 40);
	}
	EXPECT_EQ(turns, 15);
	EXPECT_FLOAT_EQ(action.progress, 1.0f);
	EXPECT_TRUE(world.shown.empty());
	// The end: the hand goes back, and the shape of the gesture that opens the choice shows on the land there
	EXPECT_TRUE(Process(action, world));
	EXPECT_FALSE(Busy(action));
	EXPECT_EQ(action.hand, glm::vec3(100.0f, 30.0f, 200.0f));
	ASSERT_EQ(world.shown.size(), 1u);
	EXPECT_EQ(world.shown[0], 1u);
	EXPECT_EQ(world.shownAt, action.hand);
	EXPECT_FLOAT_EQ(world.shownSize, k_GestureSize);
}

TEST(ComputerHand, GestureWithNoShapeTakesOneTurn)
{
	EXPECT_FLOAT_EQ(GestureSharePerTurn(0.0f, 2.0f), 1.0f);
	EXPECT_FLOAT_EQ(GestureSharePerTurn(3.0f, 0.0f), 1.0f);
	EXPECT_FLOAT_EQ(GestureSharePerTurn(4.0f, 2.0f), 1.0f / 60.0f);
}

TEST(ComputerHand, CameraFollowsTheHandButNotOnTheTurnOfTheInsertedMove)
{
	FakeWorld world;
	world.places[k_Icon] = {1000.0f, 1000.0f};
	HandAction action;
	action.hand = {900.0f, 0.0f, 1000.0f};
	Add(action, StepKind::CastSpellAtObject,
	    {Argument {.object = k_Icon}, Argument {.number = static_cast<int32_t>(MagicType::Food)}, Argument {.object = k_Icon}});
	// The turn that puts the move in front leaves the camera where it was
	Process(action, world);
	EXPECT_EQ(action.camera, glm::vec3(0.0f));
	Process(action, world); // the start of the move
	Process(action, world); // the first step of the move
	EXPECT_EQ(action.camera, action.hand);
	EXPECT_GT(action.camera.x, 900.0f);
}

TEST(ComputerHand, HandTakesAThingAndDropsItOnTheStore)
{
	constexpr auto k_Field = static_cast<entt::entity>(7);
	FakeWorld world;
	world.places[k_Field] = {100.0f, 100.0f};
	world.places[k_Store] = {100.0f, 102.0f};
	HandAction action;
	action.hand = {100.0f, 0.0f, 100.0f};
	Add(action, StepKind::PickUpObject, {Argument {.object = k_Field}, Argument {.value = 90.0f}});
	Add(action, StepKind::DropObjectInHandOnObject, {Argument {.object = k_Store}});
	// Each step gets a move to its thing first: three parts of the move, then three of the step
	for (int turn = 0; turn < 20 && Busy(action); ++turn)
	{
		ASSERT_TRUE(Process(action, world));
	}
	EXPECT_FALSE(Busy(action));
	ASSERT_EQ(world.usedOn.size(), 1u);
	EXPECT_EQ(world.usedOn[0], k_Store);
	EXPECT_TRUE(world.held == entt::null);
}

TEST(ComputerHand, FullHandPutsDownFirstAndAMissingThingEndsBadly)
{
	constexpr auto k_Field = static_cast<entt::entity>(7);
	constexpr auto k_Other = static_cast<entt::entity>(8);
	FakeWorld world;
	world.places[k_Field] = {100.0f, 100.0f};
	world.held = k_Other;
	HandAction action;
	action.hand = {100.0f, 0.0f, 100.0f};
	action.moveFirst = false;
	Add(action, StepKind::PickUpObject, {Argument {.object = k_Field}});
	Process(action, world); // the start
	Process(action, world); // the hand is full: it puts down what it holds
	EXPECT_EQ(world.putDown, 1);
	EXPECT_EQ(action.part, 1);
	Process(action, world);
	EXPECT_EQ(world.held, k_Field);
	// A thing that is gone ends the action badly
	Clear(action);
	world.held = entt::null;
	action.moveFirst = false;
	Add(action, StepKind::PickUpObject, {Argument {.object = static_cast<entt::entity>(99)}});
	Process(action, world);
	EXPECT_FALSE(Process(action, world));
}

TEST(ComputerHand, VillagerGoesToThePlaceWaitsAndIsDropped)
{
	constexpr auto k_Villager = static_cast<entt::entity>(9);
	FakeWorld world;
	world.places[k_Villager] = {100.0f, 100.0f};
	HandAction action;
	action.hand = {100.0f, 0.0f, 100.0f};
	action.moveFirst = false;
	const glm::vec3 gate {100.0f, 20.0f, 130.0f};
	Add(action, StepKind::PickUpObject, {Argument {.object = k_Villager}});
	Add(action, StepKind::MoveHandToPos, {Argument {.place = gate}, Argument {.value = 900.0f}});
	Add(action, StepKind::Wait, {Argument {.value = 1.0f}});
	Add(action, StepKind::DropObjectInHandAtPos, {Argument {.place = gate}});
	// The pick up has no move in front of it here; its three parts take three turns
	for (int turn = 0; turn < 3; ++turn)
	{
		Process(action, world);
	}
	EXPECT_EQ(world.held, k_Villager);
	ASSERT_EQ(action.steps.front().kind, StepKind::MoveHandToPos);
	// The move gets no move of its own. At 90 metres a second the hand covers 30 metres in four steps of 9 and the rest.
	Process(action, world); // the start
	for (int turn = 0; turn < 3; ++turn)
	{
		Process(action, world);
		EXPECT_EQ(action.part, 1);
	}
	EXPECT_FLOAT_EQ(action.hand.z, 127.0f);
	Process(action, world);
	EXPECT_EQ(action.part, 2);
	EXPECT_FLOAT_EQ(action.hand.z, 130.0f);
	// The hand floats over the land at the place
	EXPECT_FLOAT_EQ(action.hand.y, 30.0f);
	Process(action, world); // the end of the move
	ASSERT_EQ(action.steps.front().kind, StepKind::Wait);
	// The wait takes ten turns of the second, after its start
	Process(action, world);
	EXPECT_EQ(action.counter, 10u);
	for (int turn = 0; turn < 9; ++turn)
	{
		Process(action, world);
		EXPECT_EQ(action.part, 1);
	}
	Process(action, world);
	EXPECT_EQ(action.part, 2);
	Process(action, world);
	// The drop gets the move to the place first, which ends at once
	ASSERT_EQ(action.steps.front().kind, StepKind::DropObjectInHandAtPos);
	for (int turn = 0; turn < 8 && Busy(action); ++turn)
	{
		Process(action, world);
	}
	EXPECT_FALSE(Busy(action));
	ASSERT_EQ(world.droppedAt.size(), 1u);
	EXPECT_TRUE(world.held == entt::null);
}

TEST(ComputerMindRules, WorshipNeeds)
{
	// A town with no one at worship needs worshippers fully, and one with most of its people there needs none
	EXPECT_FLOAT_EQ(TownWorshipNeed(10, 0, 0), 1.0f);
	EXPECT_FLOAT_EQ(TownWorshipNeed(0, 0, 0), 0.0f);
	EXPECT_FLOAT_EQ(TownWorshipNeed(10, 5, 1), 0.0f);
	// A third at worship leaves the cube of two thirds
	const float missing = 1.0f - 1.0f / 3.0f;
	EXPECT_FLOAT_EQ(TownWorshipNeed(3, 1, 0), (missing * missing) * missing);
	// More than all counts as all
	EXPECT_FLOAT_EQ(TownWorshipNeed(2, 3, 1), 0.0f);
	// A site with no icons needs no dancers; four dancers for each icon fill it
	EXPECT_FLOAT_EQ(SiteDancerNeed(0, 0), 0.0f);
	EXPECT_FLOAT_EQ(SiteDancerNeed(0, 3), 1.0f);
	EXPECT_FLOAT_EQ(SiteDancerNeed(12, 3), 0.0f);
	EXPECT_FLOAT_EQ(SiteDancerNeed(6, 3), 1.0f - 0.0625f);
}

TEST(ComputerMindRules, FieldWishAndChoice)
{
	// A full field at the hand with a full pull is wanted almost fully
	const float full = FieldWish(100.0f, 100.0f, 0.0f, 1.0f);
	EXPECT_FLOAT_EQ(full, gutils::GetDistanceModifier(0.0f, k_FieldReach) * ((100.0f + 0.001f) / (100.0f + 0.001f)));
	// An empty field is wanted little, and a field outside the influence that does not pull is not wanted
	EXPECT_LT(FieldWish(0.0f, 100.0f, 0.0f, 1.0f), 0.001f);
	EXPECT_FLOAT_EQ(FieldWish(100.0f, 100.0f, 0.0f, 0.0f), 0.0f);
	// The first of the largest over a tenth wins; none at a tenth or less
	EXPECT_EQ(MostWantedField(std::vector<float> {0.1f, 0.05f}), std::nullopt);
	EXPECT_EQ(MostWantedField(std::vector<float> {0.2f, 0.5f, 0.5f}), std::optional<size_t> {1});
}

TEST(ComputerMindRules, ChargeShareCountsTheSiteAndTheIcon)
{
	const IconFacts icon {
	    .site = true, .store = 1.0f, .cost = 10.0f, .battery = 2.0f, .available = 5.0f, .used = 1.0f, .capacity = 3.0f};
	EXPECT_FLOAT_EQ(ChargeShare(icon), (4.0f + 2.0f + 1.001f) / 10.001f);
	// With no dancers and too little, nothing
	auto still = icon;
	still.capacity = 0.0f;
	EXPECT_EQ(ChargeShare(still), 0.0f);
	// Without a site only the store counts
	EXPECT_FLOAT_EQ(ChargeShare({.store = 1.0f, .cost = 10.0f}), 1.001f / 10.001f);
}

TEST(ComputerMindRules, TurnsToChargeTheRest)
{
	const IconFacts icon {.site = true, .store = 1.0f, .cost = 10.0f, .available = 3.0f, .capacity = 2.0f};
	EXPECT_FLOAT_EQ(TurnsToCharge(icon), 3.0f);
	auto enough = icon;
	enough.available = 9.0f;
	EXPECT_EQ(TurnsToCharge(enough), 0.0f);
	auto still = icon;
	still.capacity = 0.0f;
	EXPECT_EQ(TurnsToCharge(still), -1.0f);
	EXPECT_EQ(TurnsToCharge({.store = 1.0f, .cost = 10.0f}), -1.0f);
}

TEST(ComputerMindRules, WishesAndChoices)
{
	EXPECT_FLOAT_EQ(RequestShare(30), 0.3f);
	EXPECT_EQ(RequestShare(500), 1.0f);
	EXPECT_EQ(SpellValue(2.0f, 0.0f, 1.0f), 1.0f);
	EXPECT_EQ(SpellValue(0.5f, 0.0f, 0.0f), 0.0f);
	const std::vector<float> desires {0.2f, 0.5f, 0.5f};
	EXPECT_EQ(MostWanting(desires), 1u);
	EXPECT_FALSE(MostWanting(std::vector<float> {0.0f, -1.0f}).has_value());
	const std::vector<ChargeCandidate> icons {{.charging = true, .requests = 90},
	                                          {.store = 1.0f, .requests = 90},
	                                          {.requests = 20},
	                                          {.requests = 40},
	                                          {.requests = 40}};
	const auto choice = ChooseCharge(icons);
	ASSERT_TRUE(choice.has_value());
	EXPECT_EQ(choice->index, 3u);
	EXPECT_FLOAT_EQ(choice->value, 0.4f);
}

TEST(ComputerMindRules, NearestEdgeOfTheInfluence)
{
	const auto at = [](float x, float z) {
		return map_coords::MapCoords {.x = map_coords::ToFixed(x), .z = map_coords::ToFixed(z), .altitude = 0.0f};
	};
	EXPECT_FALSE(NearestInfluenceEdge(at(0.0f, 0.0f), {}).has_value());
	// The citadel at (1000, 1000) reaches 100 m; a town at (1500, 1000) reaches 300 m. The place is east of both.
	const std::vector<InfluenceCircle> circles {{.centre = at(1000.0f, 1000.0f), .radius = 100.0f},
	                                            {.centre = at(1500.0f, 1000.0f), .radius = 300.0f}};
	const auto place = at(1900.0f, 1000.0f);
	const auto edge = NearestInfluenceEdge(place, circles);
	ASSERT_TRUE(edge.has_value());
	// The town's edge is nearer: 100 m from the place, east of the town
	EXPECT_NEAR(map_coords::ToMetres(edge->x), 1800.0f, 0.5f);
	EXPECT_NEAR(map_coords::ToMetres(edge->z), 1000.0f, 0.5f);
	// Of two circles at the same distance the first wins
	const std::vector<InfluenceCircle> same {{.centre = at(1000.0f, 1000.0f), .radius = 100.0f},
	                                         {.centre = at(1000.0f, 1000.0f), .radius = 100.0f}};
	EXPECT_TRUE(NearestInfluenceEdge(place, same).has_value());
}

TEST(ComputerMindRules, PlaceOutsideTheInfluencePullsLessFurtherOut)
{
	// The pull is the square of the distance modifier, nearly full at the edge
	const float edge = gutils::GetDistanceModifier(0.0f, 200.0f);
	EXPECT_FLOAT_EQ(OutsidePlacePull(0.0f, 200.0f), edge * edge);
	EXPECT_GT(OutsidePlacePull(0.0f, 200.0f), 0.99f);
	const float modifier = gutils::GetDistanceModifier(50.0f, 200.0f);
	EXPECT_FLOAT_EQ(OutsidePlacePull(50.0f, 200.0f), modifier * modifier);
	// Past the reach it is nothing
	EXPECT_FLOAT_EQ(OutsidePlacePull(400.0f, 200.0f), 0.0f);
}
