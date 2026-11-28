/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#define LOCATOR_IMPLEMENTATIONS 1

#include "HandGrabSystemFixture.h"

using namespace openblack;
using namespace openblack::ecs;
using namespace openblack::ecs::components;
using namespace openblack::ecs::systems;
using namespace hand_grab_test;

TEST_F(HandGrabSystemWithWorld, ThingsOutOfTheInfluenceOrHeldByAScriptAreLeft)
{
	const auto rock = _world->AddRock({0.0f, 0.0f, 0.0f});
	_world->underCursor = rock;
	_world->influence = false;
	EXPECT_FALSE(Press());
	_world->influence = true;
	_world->registry.Assign<CannotBePickedUp>(rock);
	EXPECT_FALSE(Press());
	_world->registry.Remove<CannotBePickedUp>(rock);
	// A rock wider than the hand can lift
	const auto boulder = _world->AddRock({5.0f, 0.0f, 0.0f}, 4.0f);
	_world->tappable.insert(boulder);
	_world->underCursor = boulder;
	EXPECT_FALSE(Press());
	// Out of the influence or held by a script nothing is tapped; a boulder the hand can't lift is tapped at once
	ASSERT_EQ(_world->tapped.size(), 1u);
	EXPECT_EQ(_world->tapped.front(), boulder);
}

// A hand that holds a thing does not tap the thing under it, as a temple's entrance: the press makes ready to throw
TEST_F(HandGrabSystemWithWorld, AHandThatHoldsAThingTapsNothing)
{
	const auto rock = _world->AddRock({0.0f, 0.0f, 0.0f});
	_world->underCursor = rock;
	EXPECT_TRUE(Press());
	for (int i = 0; i < 20 && !_system->GetHeld().has_value(); ++i)
	{
		Frame(10);
	}
	ASSERT_TRUE(_system->GetHeld().has_value());
	Release();
	_world->tapped.clear();
	const auto boulder = _world->AddRock({5.0f, 0.0f, 0.0f}, 4.0f);
	_world->underCursor = boulder;
	EXPECT_TRUE(Press());
	EXPECT_TRUE(_world->tapped.empty());
}

// The influence is asked where the thing under the cursor stands, not where the hand rests on the land behind it
TEST_F(HandGrabSystemWithWorld, ATakeAsksTheInfluenceWhereThePickedThingStands)
{
	_world->borderX = 10.0f;
	// Standing just inside, with the land the hand rests on outside: taken
	const auto inside = _world->AddRock({8.0f, 0.0f, 0.0f});
	_world->underCursor = inside;
	_world->pickedPoint = glm::vec3(8.0f, 0.0f, 0.0f);
	Frame(10, {12.0f, 0.0f, 0.0f}, {12.0f, 0.0f, 0.0f});
	EXPECT_TRUE(Press());
	Release();
	// Standing just outside, with the land the hand rests on inside: left, and not tapped
	const auto outside = _world->AddRock({12.0f, 0.0f, 0.0f});
	_world->underCursor = outside;
	_world->pickedPoint = glm::vec3(12.0f, 0.0f, 0.0f);
	Frame(10, {8.0f, 0.0f, 0.0f}, {8.0f, 0.0f, 0.0f});
	EXPECT_FALSE(Press());
	EXPECT_EQ(std::ranges::count(_world->tapped, outside), 0);
}

// The turn asks the influence at the point of the last report of the hand's player, never at the cursor of this computer
TEST_F(HandGrabSystemWithWorld, ATakeAtTheTurnAsksTheInfluenceAtTheReportedPoint)
{
	_world->borderX = 10.0f;
	const auto rock = _world->AddRock({8.0f, 0.0f, 0.0f});
	// The cursor of this computer is outside, the report of the player inside: taken
	_world->pickedPoint = glm::vec3(12.0f, 0.0f, 0.0f);
	_world->reportedCollide = glm::vec3(8.0f, 0.0f, 0.0f);
	_system->DoTake(_world->HandPlayer(), rock);
	EXPECT_EQ(_system->GetHeld(), std::optional(rock));
	_system->Reset();
	// The cursor inside, the report outside: left
	const auto other = _world->AddRock({9.0f, 0.0f, 0.0f});
	_world->pickedPoint = glm::vec3(8.0f, 0.0f, 0.0f);
	_world->reportedCollide = glm::vec3(12.0f, 0.0f, 0.0f);
	_system->DoTake(_world->HandPlayer(), other);
	EXPECT_NE(_system->GetHeld(), std::optional(other));
}

TEST_F(HandGrabSystemWithWorld, ATakeThatTheTurnCannotDoEndsTheInterfaceLock)
{
	_world->borderX = 10.0f;
	const auto outside = _world->AddRock({12.0f, 0.0f, 0.0f});
	const auto inside = _world->AddRock({8.0f, 0.0f, 0.0f});
	_world->reportedCollide = glm::vec3(12.0f, 0.0f, 0.0f);
	// The thing cannot go into the hand outside the influence
	_system->DoTake(_world->HandPlayer(), outside);
	EXPECT_EQ(_world->lockEnds, 1);
	// A take that the turn does keeps the lock
	_world->reportedCollide = glm::vec3(8.0f, 0.0f, 0.0f);
	_system->DoTake(_world->HandPlayer(), inside);
	ASSERT_EQ(_system->GetHeld(), std::optional(inside));
	EXPECT_EQ(_world->lockEnds, 1);
	// The hand holds something already
	_system->DoTake(_world->HandPlayer(), outside);
	EXPECT_EQ(_world->lockEnds, 2);
	// The thing went
	_system->Reset();
	_world->registry.Destroy(outside);
	_system->DoTake(_world->HandPlayer(), outside);
	EXPECT_EQ(_world->lockEnds, 3);
}

// A crop is never held: the take puts a handful of food in the hand in its place, the crop goes, and the interface lock
// ends
TEST_F(HandGrabSystemWithWorld, ACropGivesAHandfulOfFoodInItsPlace)
{
	const auto crop = _world->AddCrop({4.0f, 0.0f, 0.0f});
	_world->reportedCollide = glm::vec3(4.0f, 0.0f, 0.0f);
	_system->DoTake(_world->HandPlayer(), crop);
	EXPECT_FALSE(_world->registry.Valid(crop));
	const auto held = _system->GetHeld();
	ASSERT_TRUE(held.has_value());
	const auto* pot = _world->registry.TryGet<const Pot>(*held);
	ASSERT_NE(pot, nullptr);
	EXPECT_EQ(pot->type, PotInfo::HandFood);
	EXPECT_EQ(pot->amount, 16u);
	EXPECT_EQ(_world->lockEnds, 1);
	// The hand picks the handful up as a thing of the world, with a pick-up reaction
	EXPECT_EQ(
	    std::ranges::count(_world->reactions, Reaction::ReactToHandPickUp, &decltype(_world->reactions)::value_type::type), 1);
}

// The steps of a computer god take a thing also outside the influence of its player
TEST_F(HandGrabSystemWithWorld, AComputerGodTakesAThingOutsideItsInfluence)
{
	_world->borderX = 10.0f;
	const auto rock = _world->AddRock({12.0f, 0.0f, 0.0f});
	_world->pickedPoint = glm::vec3(12.0f, 0.0f, 0.0f);
	_world->reportedCollide = glm::vec3(12.0f, 0.0f, 0.0f);
	// The take of a player outside the influence leaves the rock
	_system->DoTake(_world->HandPlayer(), rock);
	EXPECT_NE(_system->GetHeld(), std::optional(rock));
	// The take of a computer god takes it
	_system->DoComputerTake(_world->HandPlayer(), rock);
	EXPECT_EQ(_system->GetHeld(), std::optional(rock));
	// The pick-up reaction of a computer god's hand has the same time as that of the player's hand
	const auto pickUp =
	    std::ranges::find(_world->reactions, Reaction::ReactToHandPickUp, &decltype(_world->reactions)::value_type::type);
	ASSERT_NE(pickUp, _world->reactions.end());
	EXPECT_TRUE(pickUp->timedFromStart);
}

// The other rules of a take apply to the steps of a computer god
TEST_F(HandGrabSystemWithWorld, AComputerGodLeavesAThingThatCannotBePickedUp)
{
	_world->borderX = 10.0f;
	const auto rock = _world->AddRock({8.0f, 0.0f, 0.0f});
	_world->pickedPoint = glm::vec3(8.0f, 0.0f, 0.0f);
	_world->reportedCollide = glm::vec3(8.0f, 0.0f, 0.0f);
	_world->registry.Assign<CannotBePickedUp>(rock);
	_system->DoComputerTake(_world->HandPlayer(), rock);
	EXPECT_NE(_system->GetHeld(), std::optional(rock));
}

TEST_F(HandGrabSystemWithWorld, AScoopAsksTheInfluenceWhereThePileStands)
{
	_world->borderX = 10.0f;
	const auto addPile = [this](float x) {
		const auto pile = _world->registry.Create();
		_world->registry.Assign<Transform>(pile, glm::vec3(x, 0.0f, 0.0f), glm::mat3(1.0f), glm::vec3(1.0f));
		_world->registry.Assign<Pot>(pile, Pot {.amount = 1000, .maxAmount = 2000, .type = PotInfo::FoodPile});
		_world->sizes[pile] = {.radius = 2.0f, .height = 3.0f};
		return pile;
	};
	// A pile just outside isn't scooped from, though the hand rests inside
	const auto outside = addPile(12.0f);
	_world->underCursor = outside;
	_world->pickedPoint = glm::vec3(12.0f, 0.0f, 0.0f);
	Frame(10, {8.0f, 0.0f, 0.0f}, {8.0f, 0.0f, 0.0f});
	EXPECT_FALSE(Press());
	EXPECT_EQ(_world->registry.Get<const Pot>(outside).amount, 1000u);
	// A pile just inside is, though the hand rests outside
	const auto inside = addPile(8.0f);
	_world->underCursor = inside;
	_world->pickedPoint = glm::vec3(8.0f, 0.0f, 0.0f);
	Frame(10, {12.0f, 0.0f, 0.0f}, {12.0f, 0.0f, 0.0f});
	EXPECT_TRUE(Press());
	EXPECT_EQ(_world->registry.Get<const Pot>(inside).amount, 975u);
}

TEST_F(HandGrabSystemWithWorld, TheEndOfAScoopAtTheTurnEndsTheInterfaceLock)
{
	const auto pile = _world->registry.Create();
	_world->registry.Assign<Transform>(pile, glm::vec3(0.0f), glm::mat3(1.0f), glm::vec3(1.0f));
	_world->registry.Assign<Pot>(pile, Pot {.amount = 1000, .maxAmount = 2000, .type = PotInfo::FoodPile});
	_world->sizes[pile] = {.radius = 2.0f, .height = 3.0f};
	_world->underCursor = pile;
	_world->pickedPoint = glm::vec3(0.0f);
	Frame(10);
	ASSERT_TRUE(Press());
	ASSERT_EQ(_system->GetLockedOn(), std::optional(pile));
	// A turn that scoops more keeps the lock
	Turn();
	EXPECT_EQ(_world->lockEnds, 0);
	// The pile goes: the turn ends the scoop, and with it the interface lock of the hand
	_world->registry.Destroy(pile);
	Turn();
	EXPECT_EQ(_system->GetLockedOn(), std::nullopt);
	EXPECT_EQ(_world->lockEnds, 1);
}

TEST_F(HandGrabSystemWithWorld, LettingGoAsksTheInfluenceAtThePointPicked)
{
	const auto rock = _world->AddRock({0.0f, 0.0f, 0.0f});
	_world->underCursor = rock;
	Press();
	for (int i = 0; i < 20 && !_system->GetHeld().has_value(); ++i)
	{
		Frame(10);
	}
	Release();
	ASSERT_TRUE(_system->GetHeld().has_value());
	_world->underCursor.reset();
	_world->borderX = 10.0f;
	// Over a thing standing just outside, with the land the hand rests on inside: the press does nothing
	_world->pickedPoint = glm::vec3(12.0f, 0.0f, 0.0f);
	Frame(10, {8.0f, 0.0f, 0.0f}, {8.0f, 0.0f, 0.0f});
	Press();
	Frame(10, {8.0f, 0.0f, 0.0f}, {8.0f, 0.0f, 0.0f});
	Release();
	EXPECT_TRUE(_system->GetHeld().has_value());
	EXPECT_TRUE(_world->released.empty());
	// Over a thing standing just inside, with the land the hand rests on outside: thrown
	_world->pickedPoint = glm::vec3(8.0f, 0.0f, 0.0f);
	Frame(10, {12.0f, 0.0f, 0.0f}, {12.0f, 0.0f, 0.0f});
	Press();
	Frame(10, {12.0f, 0.0f, 0.0f}, {12.0f, 0.0f, 0.0f});
	Release();
	EXPECT_EQ(_world->released.size(), 1u);
}

// While the hand waits for its last send, a let go keeps hold of the thing and sends nothing
TEST_F(HandGrabSystemWithWorld, WhileTheHandWaitsForItsLastSendALetGoKeepsHold)
{
	const auto rock = _world->AddRock({0.0f, 0.0f, 0.0f});
	_world->underCursor = rock;
	Press();
	for (int i = 0; i < 20 && !_system->GetHeld().has_value(); ++i)
	{
		Frame(10);
	}
	Release();
	ASSERT_TRUE(_system->GetHeld().has_value());
	_world->underCursor.reset();
	_world->pickedPoint = glm::vec3(8.0f, 0.0f, 0.0f);
	_world->sendWaits = true;
	const auto sentBefore = _world->sent.size();
	Frame(10, {8.0f, 0.0f, 0.0f}, {8.0f, 0.0f, 0.0f});
	Press();
	Frame(10, {8.0f, 0.0f, 0.0f}, {8.0f, 0.0f, 0.0f});
	Release();
	EXPECT_TRUE(_system->GetHeld().has_value());
	EXPECT_TRUE(_world->released.empty());
	EXPECT_TRUE(std::ranges::none_of(_world->sent | std::views::drop(sentBefore), [](const auto& action) {
		return std::holds_alternative<player_commands::HandLetGo>(action);
	}));
	EXPECT_EQ(_world->sendMarks, 0);
	// After the wait, the let go throws the thing and starts a wait
	_world->sendWaits = false;
	Press();
	Frame(10, {8.0f, 0.0f, 0.0f}, {8.0f, 0.0f, 0.0f});
	Release();
	EXPECT_EQ(_world->released.size(), 1u);
	EXPECT_EQ(_world->sendMarks, 1);
}

// The help counts each thing that the hand takes and each thing that it lets go of. A let-go that the influence
// refuses does not count.
TEST_F(HandGrabSystemWithWorld, TheHelpCountsATakeAndALetGo)
{
	const auto rock = _world->AddRock({0.0f, 0.0f, 0.0f});
	_world->underCursor = rock;
	Press();
	for (int i = 0; i < 20 && !_system->GetHeld().has_value(); ++i)
	{
		Frame(10);
	}
	Release();
	ASSERT_TRUE(_system->GetHeld().has_value());
	EXPECT_EQ(_world->helpEvents, std::vector<uint32_t> {help::profile::k_PickUp});
	_world->underCursor.reset();
	_world->borderX = 10.0f;
	_world->pickedPoint = glm::vec3(12.0f, 0.0f, 0.0f);
	Frame(10, {8.0f, 0.0f, 0.0f}, {8.0f, 0.0f, 0.0f});
	Press();
	Frame(10, {8.0f, 0.0f, 0.0f}, {8.0f, 0.0f, 0.0f});
	Release();
	EXPECT_EQ(_world->helpEvents.size(), 1u);
	_world->pickedPoint = glm::vec3(8.0f, 0.0f, 0.0f);
	Frame(10, {12.0f, 0.0f, 0.0f}, {12.0f, 0.0f, 0.0f});
	Press();
	Frame(10, {12.0f, 0.0f, 0.0f}, {12.0f, 0.0f, 0.0f});
	Release();
	EXPECT_EQ(_world->helpEvents, (std::vector<uint32_t> {help::profile::k_PickUp, help::profile::k_Throw}));
}

// The hand starts the path of its gesture again when it takes a thing and when it lets go of it. A let-go that the
// influence refuses does not.
TEST_F(HandGrabSystemWithWorld, TheGesturePathStartsAgainOnATakeAndALetGo)
{
	const auto rock = _world->AddRock({0.0f, 0.0f, 0.0f});
	_world->underCursor = rock;
	Press();
	for (int i = 0; i < 20 && !_system->GetHeld().has_value(); ++i)
	{
		Frame(10);
	}
	Release();
	ASSERT_TRUE(_system->GetHeld().has_value());
	EXPECT_EQ(_world->pathsForgotten, 1);
	_world->underCursor.reset();
	_world->borderX = 10.0f;
	_world->pickedPoint = glm::vec3(12.0f, 0.0f, 0.0f);
	Frame(10, {8.0f, 0.0f, 0.0f}, {8.0f, 0.0f, 0.0f});
	Press();
	Frame(10, {8.0f, 0.0f, 0.0f}, {8.0f, 0.0f, 0.0f});
	Release();
	EXPECT_EQ(_world->pathsForgotten, 1);
	_world->pickedPoint = glm::vec3(8.0f, 0.0f, 0.0f);
	Frame(10, {12.0f, 0.0f, 0.0f}, {12.0f, 0.0f, 0.0f});
	Press();
	Frame(10, {12.0f, 0.0f, 0.0f}, {12.0f, 0.0f, 0.0f});
	Release();
	EXPECT_EQ(_world->pathsForgotten, 2);
}

// The hand of a computer god or of another player takes and lets go of things without wiping the local player's path
TEST_F(HandGrabSystemWithWorld, TheGesturePathIsLeftAloneByAHandThatIsNotTheLocalOne)
{
	_world->localHand = false;
	const auto rock = _world->AddRock({0.0f, 0.0f, 0.0f});
	_world->underCursor = rock;
	Press();
	for (int i = 0; i < 20 && !_system->GetHeld().has_value(); ++i)
	{
		Frame(10);
	}
	Release();
	ASSERT_TRUE(_system->GetHeld().has_value());
	EXPECT_EQ(_world->pathsForgotten, 0);
	_world->underCursor.reset();
	_world->borderX = 10.0f;
	_world->pickedPoint = glm::vec3(8.0f, 0.0f, 0.0f);
	Frame(10, {12.0f, 0.0f, 0.0f}, {12.0f, 0.0f, 0.0f});
	Press();
	Frame(10, {12.0f, 0.0f, 0.0f}, {12.0f, 0.0f, 0.0f});
	Release();
	EXPECT_FALSE(_system->GetHeld().has_value());
	EXPECT_EQ(_world->pathsForgotten, 0);
}

// Any player's hand that takes a thing catches a firefly that hides where it stood, the local hand or not
TEST_F(HandGrabSystemWithWorld, AHandThatIsNotTheLocalOneAlsoCatchesAFirefly)
{
	_world->localHand = false;
	const auto rock = _world->AddRock({0.0f, 0.0f, 0.0f});
	_world->underCursor = rock;
	Press();
	for (int i = 0; i < 20 && !_system->GetHeld().has_value(); ++i)
	{
		Frame(10);
	}
	ASSERT_TRUE(_system->GetHeld().has_value());
	EXPECT_EQ(_world->fireflyCatches, std::vector<entt::entity> {rock});
}

// The help counts only the events of the local hand: the hand of a computer god or of another player takes and lets go
// of things with no count
TEST_F(HandGrabSystemWithWorld, TheHelpCountsNothingForAHandThatIsNotTheLocalOne)
{
	_world->localHand = false;
	const auto rock = _world->AddRock({0.0f, 0.0f, 0.0f});
	_world->underCursor = rock;
	Press();
	for (int i = 0; i < 20 && !_system->GetHeld().has_value(); ++i)
	{
		Frame(10);
	}
	Release();
	ASSERT_TRUE(_system->GetHeld().has_value());
	_world->underCursor.reset();
	_world->borderX = 10.0f;
	_world->pickedPoint = glm::vec3(8.0f, 0.0f, 0.0f);
	Frame(10, {12.0f, 0.0f, 0.0f}, {12.0f, 0.0f, 0.0f});
	Press();
	Frame(10, {12.0f, 0.0f, 0.0f}, {12.0f, 0.0f, 0.0f});
	Release();
	EXPECT_FALSE(_system->GetHeld().has_value());
	EXPECT_TRUE(_world->helpEvents.empty());
}

// The hand says why a press on a thing wouldn't take it, as the inspector shows it
// The hand stops the spread of the reactions of a thing that it takes. They spread again when the hand lets go.
TEST_F(HandGrabSystemWithWorld, TheReactionsOfAHeldThingSpreadAgainOnALetGo)
{
	const auto rock = _world->AddRock({0.0f, 0.0f, 0.0f});
	_world->underCursor = rock;
	Press();
	for (int i = 0; i < 20 && !_system->GetHeld().has_value(); ++i)
	{
		Frame(10);
	}
	Release();
	ASSERT_TRUE(_system->GetHeld().has_value());
	EXPECT_EQ(_world->spreadStopped, std::vector<entt::entity> {rock});
	EXPECT_TRUE(_world->spreadAgain.empty());
	_world->underCursor.reset();
	_world->borderX = 10.0f;
	_world->pickedPoint = glm::vec3(8.0f, 0.0f, 0.0f);
	Frame(10, {12.0f, 0.0f, 0.0f}, {12.0f, 0.0f, 0.0f});
	Press();
	Frame(10, {12.0f, 0.0f, 0.0f}, {12.0f, 0.0f, 0.0f});
	Release();
	EXPECT_FALSE(_system->GetHeld().has_value());
	EXPECT_EQ(_world->spreadAgain, std::vector<entt::entity> {rock});
}

TEST_F(HandGrabSystemWithWorld, ItSaysWhyAThingWouldntBeTaken)
{
	const auto rock = _world->AddRock({0.0f, 0.0f, 0.0f});
	_world->underCursor = rock;
	EXPECT_EQ(_system->WhyNotTake(rock), "");
	_world->influence = false;
	EXPECT_NE(_system->WhyNotTake(rock).find("influence"), std::string::npos);
	_world->influence = true;
	_world->registry.Assign<CannotBePickedUp>(rock);
	EXPECT_NE(_system->WhyNotTake(rock).find("script"), std::string::npos);
	_world->registry.Remove<CannotBePickedUp>(rock);
	EXPECT_NE(_system->WhyNotTake(_world->AddRock({5.0f, 0.0f, 0.0f}, 4.0f)).find("can't hold"), std::string::npos);
	EXPECT_NE(_system->WhyNotTake(_world->registry.Create()).find("can't hold"), std::string::npos);
	// Busy taking the rock, it takes nothing else
	EXPECT_TRUE(Press());
	EXPECT_NE(_system->WhyNotTake(rock).find("busy"), std::string::npos);
}

TEST_F(HandGrabSystemWithWorld, ItSaysThatAPressOnAPileScoopsFromIt)
{
	const auto pile = _world->registry.Create();
	_world->registry.Assign<Transform>(pile, glm::vec3(0.0f), glm::mat3(1.0f), glm::vec3(1.0f));
	_world->registry.Assign<Pot>(pile, Pot {.amount = 1000, .maxAmount = 2000, .type = PotInfo::FoodPile});
	_world->sizes[pile] = {.radius = 2.0f, .height = 3.0f};
	EXPECT_TRUE(_system->WouldScoop(pile));
	EXPECT_FALSE(_system->WouldScoop(_world->AddRock({5.0f, 0.0f, 0.0f})));
	// Not out of the influence
	_world->influence = false;
	EXPECT_FALSE(_system->WouldScoop(pile));
	_world->influence = true;
	// Not from an empty pile
	_world->registry.Get<Pot>(pile).amount = 0;
	EXPECT_FALSE(_system->WouldScoop(pile));
}

TEST_F(HandGrabSystemWithWorld, APressTheHandCantTakeClicksTheThing)
{
	const auto rock = _world->AddRock({0.0f, 0.0f, 0.0f});
	_world->underCursor = rock;
	// Even out of the influence, where it isn't tapped
	_world->influence = false;
	_commandTurn = _now / 100;
	EXPECT_FALSE(Press());
	const auto* clicked = _world->registry.TryGet<const HandClicked>(_world->hand);
	ASSERT_NE(clicked, nullptr);
	EXPECT_EQ(clicked->thing, rock);
	EXPECT_EQ(clicked->thingTurn, _now / 100);
	EXPECT_TRUE(_world->tapped.empty());
}
