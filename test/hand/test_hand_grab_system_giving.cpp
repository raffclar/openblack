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

TEST_F(HandGrabSystemGiving, OverACreatureThatWouldTakeItTheHandOffersAndTurnsItOver)
{
	const auto rock = HoldRock();
	ASSERT_EQ(_system->GetHeld(), rock);
	const auto creature = AddCreature(creature_giving::Verdict::Give);
	_world->underCursor = creature;
	Frame(16);
	EXPECT_EQ(_system->GetOfferedCreature(), creature);
	// The palm turns over, eased towards the turn afresh every frame over 0.4 s, so it settles rather than arriving
	for (int i = 0; i < 120; ++i)
	{
		Frame(16);
	}
	EXPECT_NEAR(_system->GetOfferTurn(), std::numbers::pi_v<float>, 1e-3f);
	// Off the creature the offer lasts 1.1 s, turning back once less than 0.3 s is left
	_world->underCursor.reset();
	Frame(700);
	EXPECT_EQ(_system->GetOfferedCreature(), creature);
	Frame(200);
	Frame(200);
	EXPECT_FALSE(_system->GetOfferedCreature().has_value());
}

TEST_F(HandGrabSystemGiving, TheFrameThatRenewsTheOfferAlsoRunsItDown)
{
	HoldRock();
	const auto creature = AddCreature(creature_giving::Verdict::Give);
	_world->underCursor = creature;
	// The frame over the creature sets 1.1 s and then takes its own 0.5 s off, so 0.6 s is left
	Frame(500);
	EXPECT_EQ(_system->GetOfferedCreature(), creature);
	_world->underCursor.reset();
	Frame(599);
	EXPECT_EQ(_system->GetOfferedCreature(), creature);
	Frame(2);
	EXPECT_FALSE(_system->GetOfferedCreature().has_value());
}

TEST_F(HandGrabSystemGiving, ThePressWhileOfferingGivesItToTheCreatureWithTheMatchingHand)
{
	const auto rock = HoldRock();
	const auto creature = AddCreature(creature_giving::Verdict::Give);
	_world->underCursor = creature;
	Frame(16);
	// The cursor has moved off it, but the offer stands
	_world->underCursor.reset();
	Frame(16);
	_world->helpEvents.clear();
	// Given at the press, with no wait for the button to be held
	EXPECT_TRUE(Press());
	ASSERT_EQ(_world->given.size(), 1u);
	// The help counts the gift once
	EXPECT_EQ(std::ranges::count(_world->helpEvents, help::profile::k_Give), 1);
	EXPECT_EQ(_world->given.front().creature, creature);
	EXPECT_EQ(_world->given.front().held, rock);
	EXPECT_TRUE(_world->given.front().leftHand);
	// Still held until the creature takes it, and not made ready to throw
	EXPECT_EQ(_system->GetHeld(), rock);
	Release();
	EXPECT_TRUE(_world->released.empty());
	// Taken, the hand holds nothing and offers nothing
	EXPECT_TRUE(_system->HandOver(rock));
	EXPECT_FALSE(_system->GetHeld().has_value());
	EXPECT_FALSE(_world->registry.AllOf<InHand>(rock));
	EXPECT_FALSE(_system->GetOfferedCreature().has_value());
	EXPECT_FALSE(_system->HandOver(rock));
}

// The gift of a hand that is not the local one gives the thing with no count for the help
TEST_F(HandGrabSystemGiving, TheGiftOfAHandThatIsNotTheLocalOneCountsNothing)
{
	const auto rock = HoldRock();
	const auto creature = AddCreature(creature_giving::Verdict::Give);
	_world->underCursor = creature;
	Frame(16);
	_world->helpEvents.clear();
	_world->localHand = false;
	EXPECT_TRUE(Press());
	ASSERT_EQ(_world->given.size(), 1u);
	EXPECT_EQ(_world->given.front().held, rock);
	EXPECT_TRUE(_world->helpEvents.empty());
}

TEST_F(HandGrabSystemGiving, TheGiveOfThePressIsACommandThatWaitsForItsTurn)
{
	const auto rock = HoldRock();
	const auto creature = AddCreature(creature_giving::Verdict::Give);
	_world->underCursor = creature;
	Frame(16);
	_deferCommands = true;
	EXPECT_TRUE(Press());
	// The press sends the give at once; the creature gets the thing at the turn that does the command
	ASSERT_EQ(_waiting.size(), 1u);
	ASSERT_TRUE(std::holds_alternative<player_commands::HandGive>(_waiting.front()));
	EXPECT_EQ(std::get<player_commands::HandGive>(_waiting.front()).creature, creature);
	EXPECT_EQ(std::get<player_commands::HandGive>(_waiting.front()).object, rock);
	EXPECT_TRUE(_world->given.empty());
	// Held down, the button sends no second give
	Frame(300);
	EXPECT_EQ(_waiting.size(), 1u);
	RunWaiting();
	ASSERT_EQ(_world->given.size(), 1u);
	EXPECT_EQ(_world->given.front().creature, creature);
}

// A sent gift makes the interface wait until its lock ends, also after the turn did the gift. The wait keeps the thing
// that was under the hand at the press. A press that sends no gift makes no wait.
TEST_F(HandGrabSystemGiving, TheSentGiftWaitsUntilTheEndOfTheInterfaceLock)
{
	HoldRock();
	const auto creature = AddCreature(creature_giving::Verdict::Give);
	_world->underCursor = creature;
	Frame(16);
	_deferCommands = true;
	_world->sendWaits = true;
	EXPECT_TRUE(Press());
	EXPECT_FALSE(_system->GetGiveWait().has_value());
	Release();
	_world->sendWaits = false;
	EXPECT_TRUE(Press());
	EXPECT_EQ(_system->GetGiveWait(), std::optional(creature));
	EXPECT_FALSE(_system->WaitsForUseLetGo());
	Release();
	RunWaiting();
	EXPECT_EQ(_system->GetGiveWait(), std::optional(creature));
	_system->StopActionAtLockEnd();
	EXPECT_FALSE(_system->GetGiveWait().has_value());
}

TEST_F(HandGrabSystemGiving, TheTurnGivesWithTheSideOfTheHandOfTheComputerThatSentTheGive)
{
	const auto rock = HoldRock();
	const auto creature = AddCreature(creature_giving::Verdict::Give);
	_world->underCursor = creature;
	Frame(16);
	_deferCommands = true;
	// The giver has a right hand
	_world->rightHanded = true;
	EXPECT_TRUE(Press());
	ASSERT_EQ(_waiting.size(), 1u);
	EXPECT_FALSE(std::get<player_commands::HandGive>(_waiting.front()).leftHand);
	// Another computer, with a left hand, does the turn: the creature takes it with the giver's side
	_world->rightHanded = false;
	RunWaiting();
	ASSERT_EQ(_world->given.size(), 1u);
	EXPECT_EQ(_world->given.front().held, rock);
	EXPECT_FALSE(_world->given.front().leftHand);
}

TEST_F(HandGrabSystemGiving, WhileTheCreatureReachesTheHandMeetsItsHandAtTheMomentOfTaking)
{
	const auto rock = HoldRock();
	const auto creature = AddCreature(creature_giving::Verdict::Give);
	_world->underCursor = creature;
	Frame(16);
	Press();
	Frame(230);
	_world->takePoint = glm::vec3(5.0f, 3.0f, 0.0f);
	_world->taking = hand_grab::HandGrabWorldInterface::Taking {.object = rock, .msFromTake = 0.0f};
	const auto atTake = Frame(16, {0.0f, 0.0f, 0.0f});
	// At the creature's hand, the held thing's hang below and the hand's rise above cancelling for a palm hold
	EXPECT_NEAR(atTake.x, 5.0f, 1e-4f);
	// Half a second from the take it is where it should be
	_world->taking->msFromTake = -500.0f;
	const auto before = Frame(16, {0.0f, 0.0f, 0.0f});
	EXPECT_NEAR(before.x, 0.0f, 1e-4f);
}

TEST_F(HandGrabSystemGiving, MovingTheHandFifteenSizesFromTheReachingCreatureCallsItOff)
{
	HoldRock();
	const auto creature = AddCreature(creature_giving::Verdict::Give);
	_world->underCursor = creature;
	Frame(16);
	Press();
	Frame(230);
	_world->underCursor.reset();
	_world->takePoint = glm::vec3(0.0f, 0.0f, 0.0f);
	// It is measured from where the hand was drawn the frame before
	Frame(16, {29.0f, 0.0f, 0.0f});
	Frame(16, {29.0f, 0.0f, 0.0f});
	EXPECT_TRUE(_world->calledOff.empty());
	Frame(16, {40.0f, 0.0f, 0.0f});
	Frame(16, {40.0f, 0.0f, 0.0f});
	EXPECT_EQ(_world->calledOff, std::vector<entt::entity> {creature});
	EXPECT_FALSE(_system->GetOfferedCreature().has_value());
}

TEST_F(HandGrabSystemGiving, AFrameThatCallsTheGiftOffStillMovesTheHandToTheCreaturesHand)
{
	const auto rock = HoldRock();
	const auto creature = AddCreature(creature_giving::Verdict::Give);
	_world->underCursor = creature;
	Frame(16);
	Press();
	Frame(230);
	_world->underCursor.reset();
	_world->takePoint = glm::vec3(0.0f, 0.0f, 0.0f);
	// The creature takes the thing in 400 ms: the hand goes a fifth of the way from where it should be to the creature's
	// hand
	_world->taking = hand_grab::HandGrabWorldInterface::Taking {.object = rock, .msFromTake = -400.0f};
	_deferCommands = true;
	Frame(16, {40.0f, 0.0f, 0.0f});
	const auto drawn = Frame(16, {40.0f, 0.0f, 0.0f});
	const auto callOffs = std::ranges::count_if(_waiting, [](const player_commands::Action& action) {
		return std::holds_alternative<player_commands::GiveCallOff>(action);
	});
	EXPECT_EQ(callOffs, 1);
	EXPECT_NEAR(drawn.x, 32.0f, 1e-3f);
}

TEST_F(HandGrabSystemGiving, TheHandSendsTheCallOffInEachFrameUntilTheTurnCallsItOff)
{
	HoldRock();
	const auto creature = AddCreature(creature_giving::Verdict::Give);
	_world->underCursor = creature;
	Frame(16);
	Press();
	Frame(230);
	_world->underCursor.reset();
	_world->takePoint = glm::vec3(0.0f, 0.0f, 0.0f);
	_deferCommands = true;
	Frame(16, {40.0f, 0.0f, 0.0f});
	Frame(16, {40.0f, 0.0f, 0.0f});
	Frame(16, {40.0f, 0.0f, 0.0f});
	const auto callOffs = [this] {
		return std::ranges::count_if(_waiting, [](const player_commands::Action& action) {
			return std::holds_alternative<player_commands::GiveCallOff>(action);
		});
	};
	// The frames send the call off each time, and do not call off the creature or forget the gift
	EXPECT_EQ(callOffs(), 2);
	EXPECT_TRUE(_world->calledOff.empty());
	EXPECT_TRUE(_system->GetOfferedCreature().has_value());
	// The turn does each call off, and the hand forgets the gift
	RunWaiting();
	EXPECT_EQ(_world->calledOff, (std::vector<entt::entity> {creature, creature}));
	EXPECT_FALSE(_system->GetOfferedCreature().has_value());
	// Then the frames send no more
	Frame(16, {40.0f, 0.0f, 0.0f});
	EXPECT_EQ(callOffs(), 0);
}

// A call off stops what the creature does, unless it does nothing or already took the thing
TEST(CreatureGivingCallOff, TheCallOffStopsOnlyACreatureThatActsAndDidNotTakeTheThing)
{
	EXPECT_TRUE(creature_giving::CallOffStops(true, false));
	EXPECT_FALSE(creature_giving::CallOffStops(true, true));
	EXPECT_FALSE(creature_giving::CallOffStops(false, false));
	EXPECT_FALSE(creature_giving::CallOffStops(false, true));
}

TEST_F(HandGrabSystemGiving, ACreatureThatCantPickItUpSaysSoAndIsNotOffered)
{
	HoldRock();
	const auto creature = AddCreature(creature_giving::Verdict::CantPickUp);
	_world->underCursor = creature;
	Frame(16);
	EXPECT_EQ(_world->cantPickUp, std::vector<entt::entity> {creature});
	EXPECT_FALSE(_system->GetOfferedCreature().has_value());
	Press();
	Frame(230);
	EXPECT_TRUE(_world->given.empty());
}

// The hand forgets the last thing it took and the last thing it let go of once that thing is gone
TEST_F(HandGrabSystemWithWorld, TheLastThingsTakenAndLetGoAreForgottenWhenTheyGo)
{
	const auto rock = _world->AddRock({0.0f, 0.0f, 0.0f});
	_world->underCursor = rock;
	EXPECT_TRUE(Press());
	Release();
	auto& grab = _world->registry.Get<HandGrab>(_world->hand);
	const auto taken = _world->AddRock({2.0f, 0.0f, 0.0f});
	const auto dropped = _world->AddRock({4.0f, 0.0f, 0.0f});
	grab.lastPickedUp = taken;
	grab.lastDropped = dropped;
	Turn();
	EXPECT_EQ(_world->registry.Get<HandGrab>(_world->hand).lastPickedUp, taken);
	EXPECT_EQ(_world->registry.Get<HandGrab>(_world->hand).lastDropped, dropped);
	_world->registry.Destroy(taken);
	_world->registry.Destroy(dropped);
	Turn();
	EXPECT_EQ(_world->registry.Get<HandGrab>(_world->hand).lastPickedUp, entt::entity {entt::null});
	EXPECT_EQ(_world->registry.Get<HandGrab>(_world->hand).lastDropped, entt::entity {entt::null});
}

TEST_F(HandGrabSystemWithWorld, TheHandShowsWhatItHoldsAfterEachTurnOfAScoopAndAtTheLetGoOfAPileOrAFishFarm)
{
	const auto pile = _world->registry.Create();
	_world->registry.Assign<Transform>(pile, glm::vec3(0.0f), glm::mat3(1.0f), glm::vec3(1.0f));
	_world->registry.Assign<Pot>(pile, Pot {.amount = 1000, .maxAmount = 2000, .type = PotInfo::FoodPile});
	_world->underCursor = pile;
	// Nothing as the scoop starts
	EXPECT_TRUE(Press());
	EXPECT_FALSE(_system->TakeAmountToolTip().has_value());
	const auto handful = _system->GetHeld();
	ASSERT_TRUE(handful.has_value());
	// Each turn that it takes some, what it holds now, once
	Turn();
	EXPECT_EQ(_system->TakeAmountToolTip(), std::optional(_world->registry.Get<const Pot>(*handful).amount));
	EXPECT_FALSE(_system->TakeAmountToolTip().has_value());
	// At the let go too
	Release();
	EXPECT_EQ(_system->TakeAmountToolTip(), std::optional(_world->registry.Get<const Pot>(*handful).amount));
}

TEST_F(HandGrabSystemWithWorld, AFieldAndAFishFarmShowWhatTheHandHoldsEachTurnButOnlyAFishFarmAtTheLetGo)
{
	// The field of another player
	const auto field = _world->registry.Create();
	_world->registry.Assign<Transform>(field, glm::vec3(0.0f), glm::mat3(1.0f), glm::vec3(1.0f));
	_world->fields[field] = {.food = 1000, .ripe = false};
	_world->owners[field] = PlayerNames::PLAYER_TWO;
	_world->underCursor = field;
	EXPECT_TRUE(Press());
	EXPECT_FALSE(_system->TakeAmountToolTip().has_value());
	const auto fromField = _system->GetHeld();
	ASSERT_TRUE(fromField.has_value());
	Turn();
	EXPECT_EQ(_system->TakeAmountToolTip(), std::optional(_world->registry.Get<const Pot>(*fromField).amount));
	Release();
	EXPECT_FALSE(_system->TakeAmountToolTip().has_value());
	_system->HandOver(*fromField);

	const auto farm = _world->registry.Create();
	_world->registry.Assign<Transform>(farm, glm::vec3(5.0f, 0.0f, 5.0f), glm::mat3(1.0f), glm::vec3(1.0f));
	_world->fishFarms[farm] = 100.0f;
	_world->underCursor = farm;
	EXPECT_TRUE(Press());
	EXPECT_FALSE(_system->TakeAmountToolTip().has_value());
	const auto fromFarm = _system->GetHeld();
	ASSERT_TRUE(fromFarm.has_value());
	Turn();
	EXPECT_EQ(_system->TakeAmountToolTip(), std::optional(_world->registry.Get<const Pot>(*fromFarm).amount));
	Release();
	EXPECT_EQ(_system->TakeAmountToolTip(), std::optional(_world->registry.Get<const Pot>(*fromFarm).amount));
}

// The let-go of a scoop from a field of the local player shows what the hand holds, as the hand lets go. A field of no
// town does not.
TEST_F(HandGrabSystemWithWorld, AFieldOfTheLocalPlayerShowsWhatTheHandHoldsAtTheLetGo)
{
	static_assert(hand_grab::FieldShowsHeldAtLetGo(true, PlayerNames::PLAYER_ONE, PlayerNames::PLAYER_ONE));
	static_assert(!hand_grab::FieldShowsHeldAtLetGo(true, PlayerNames::PLAYER_TWO, PlayerNames::PLAYER_ONE));
	static_assert(!hand_grab::FieldShowsHeldAtLetGo(true, std::nullopt, PlayerNames::PLAYER_ONE));
	static_assert(!hand_grab::FieldShowsHeldAtLetGo(false, PlayerNames::PLAYER_ONE, PlayerNames::PLAYER_ONE));
	const auto field = _world->registry.Create();
	_world->registry.Assign<Transform>(field, glm::vec3(0.0f), glm::mat3(1.0f), glm::vec3(1.0f));
	_world->fields[field] = {.food = 1000, .ripe = false};
	_world->underCursor = field;
	EXPECT_TRUE(Press());
	const auto fromField = _system->GetHeld();
	ASSERT_TRUE(fromField.has_value());
	Turn();
	EXPECT_TRUE(_system->TakeAmountToolTip().has_value());
	Release();
	EXPECT_EQ(_system->TakeAmountToolTip(), std::optional(_world->registry.Get<const Pot>(*fromField).amount));
	EXPECT_FALSE(_system->TakeAmountToolTip().has_value());
	_system->HandOver(*fromField);

	const auto noTown = _world->registry.Create();
	_world->registry.Assign<Transform>(noTown, glm::vec3(0.0f), glm::mat3(1.0f), glm::vec3(1.0f));
	_world->fields[noTown] = {.food = 1000, .ripe = false};
	_world->owners[noTown] = std::nullopt;
	_world->underCursor = noTown;
	EXPECT_TRUE(Press());
	ASSERT_TRUE(_system->GetHeld().has_value());
	Turn();
	EXPECT_TRUE(_system->TakeAmountToolTip().has_value());
	Release();
	EXPECT_FALSE(_system->TakeAmountToolTip().has_value());
}

TEST_F(HandGrabSystemWithWorld, AHeldThingThatIsDestroyedLeavesTheHandAtTheNextTurn)
{
	const auto rock = _world->AddRock({0.0f, 0.0f, 0.0f});
	_world->underCursor = rock;
	Press();
	for (int i = 0; i < 20 && !_system->GetHeld().has_value(); ++i)
	{
		Frame(10);
	}
	Release();
	ASSERT_EQ(_system->GetHeld(), std::optional(rock));
	_world->registry.Destroy(rock);
	Turn();
	EXPECT_FALSE(_system->GetHeld().has_value());
}

// The hand sends the take as a command, with its pick-up sound. The thing stays on the land until the turn that does
// the command, and the hand keeps its grip on it until then.
TEST_F(HandGrabSystemWithWorld, ATakeWaitsForTheTurnThatDoesItsCommand)
{
	_deferCommands = true;
	const auto rock = _world->AddRock({0.0f, 0.0f, 0.0f});
	_world->underCursor = rock;
	Press();
	for (int i = 0; i < 20 && _world->sent.empty(); ++i)
	{
		Frame(10);
	}
	ASSERT_EQ(_world->sent.size(), 1u);
	ASSERT_TRUE(std::holds_alternative<player_commands::HandTake>(_world->sent.front()));
	EXPECT_EQ(std::get<player_commands::HandTake>(_world->sent.front()).object, rock);
	EXPECT_FALSE(_world->samples.empty());
	EXPECT_FALSE(_world->registry.AllOf<InHand>(rock));
	EXPECT_FALSE(_system->GetHeld().has_value());
	EXPECT_TRUE(_system->IsBusy());
	// More frames send no second take
	Frame(10);
	EXPECT_EQ(_world->sent.size(), 1u);
	RunWaiting();
	EXPECT_EQ(_system->GetHeld(), std::optional(rock));
	EXPECT_TRUE(_world->registry.AllOf<InHand>(rock));
}

// The hand lets go of the thing as a command, with the velocity that it had when the player let go of the button. It
// holds the thing until the turn that does the command.
TEST_F(HandGrabSystemWithWorld, ALetGoWaitsForItsTurnWithTheVelocityOfTheRelease)
{
	const auto rock = _world->AddRock({0.0f, 0.0f, 0.0f});
	_world->underCursor = rock;
	Press();
	for (int i = 0; i < 20 && !_system->GetHeld().has_value(); ++i)
	{
		Frame(10);
	}
	Release();
	ASSERT_EQ(_system->GetHeld(), std::optional(rock));
	_deferCommands = true;
	_world->sent.clear();
	Press();
	Frame(10, {5.0f, 0.0f, 0.0f});
	Frame(10, {10.0f, 0.0f, 0.0f});
	Release();
	ASSERT_EQ(_world->sent.size(), 1u);
	ASSERT_TRUE(std::holds_alternative<player_commands::HandLetGo>(_world->sent.front()));
	const auto letGo = std::get<player_commands::HandLetGo>(_world->sent.front());
	EXPECT_EQ(letGo.object, rock);
	EXPECT_GT(letGo.velocity.x, 0.0f);
	EXPECT_TRUE(_world->released.empty());
	EXPECT_EQ(_system->GetHeld(), std::optional(rock));
	EXPECT_FALSE(_system->IsAboutToThrow());
	// A press before the turn does nothing more
	EXPECT_TRUE(Press());
	Release();
	EXPECT_EQ(_world->sent.size(), 1u);
	RunWaiting();
	ASSERT_EQ(_world->released.size(), 1u);
	EXPECT_EQ(_world->released.front().first, rock);
	EXPECT_EQ(_world->released.front().second, letGo.velocity);
	EXPECT_FALSE(_system->GetHeld().has_value());
}

// The release asks the world where the thing starts, as this computer drew it, and the command takes the start. The
// turn that does the command puts the thing at the start and lets it go with the spin of the start. The hand does not
// ask again at the turn.
TEST_F(HandGrabSystemWithWorld, ALetGoTakesTheStartOfTheReleaseToItsTurn)
{
	const auto rock = _world->AddRock({0.0f, 0.0f, 0.0f});
	_world->underCursor = rock;
	Press();
	for (int i = 0; i < 20 && !_system->GetHeld().has_value(); ++i)
	{
		Frame(10);
	}
	Release();
	ASSERT_EQ(_system->GetHeld(), std::optional(rock));
	_world->throwStart = {
	    .angularMomentum = {0.5f, -0.0f, 1.0e-40f}, .origin = {1.0f, 2.0f, 3.0f}, .angles = {0.25f, -1.5f, 3.0f}};
	_deferCommands = true;
	_world->sent.clear();
	Press();
	Frame(10, {5.0f, 0.0f, 0.0f});
	Frame(10, {10.0f, 0.0f, 0.0f});
	Release();
	ASSERT_EQ(_world->sent.size(), 1u);
	const auto letGo = std::get<player_commands::HandLetGo>(_world->sent.front());
	ASSERT_EQ(_world->startsAsked.size(), 1u);
	EXPECT_EQ(_world->startsAsked.front().first, rock);
	// A fast throw does not lie along the slope
	EXPECT_FALSE(_world->startsAsked.front().second);
	EXPECT_EQ(letGo.start.origin, _world->throwStart.origin);
	EXPECT_EQ(letGo.start.angles, _world->throwStart.angles);
	EXPECT_TRUE(_world->startedFrom.empty());
	RunWaiting();
	EXPECT_EQ(_world->startsAsked.size(), 1u);
	ASSERT_EQ(_world->startedFrom.size(), 1u);
	EXPECT_EQ(_world->startedFrom.front().first, rock);
	EXPECT_EQ(_world->startedFrom.front().second.origin, _world->throwStart.origin);
	EXPECT_EQ(_world->startedFrom.front().second.angles, _world->throwStart.angles);
	ASSERT_EQ(_world->releasedSpins.size(), 1u);
	EXPECT_EQ(std::bit_cast<uint32_t>(_world->releasedSpins.front().y), 0x80000000U);
	EXPECT_EQ(_world->releasedSpins.front().x, 0.5f);
}

// The release asks for as many turns of lead as the reports come late. The command takes the velocity after the lead.
// Until the turn of the command, the frames draw the thing along the flight from the turn of the send.
TEST_F(HandGrabSystemWithWorld, AThrowLeadsByTheLatencyOfTheReports)
{
	const auto rock = _world->AddRock({0.0f, 0.0f, 0.0f});
	_world->underCursor = rock;
	Press();
	for (int i = 0; i < 20 && !_system->GetHeld().has_value(); ++i)
	{
		Frame(10);
	}
	Release();
	ASSERT_EQ(_system->GetHeld(), std::optional(rock));
	_world->reportLatencyMs = 250;
	_world->ledVelocity = glm::vec3(7.0f, -3.0f, 2.0f);
	_world->flight = hand_grab::throw_lead::Flight {
	    .places = {{.axes = glm::mat3(1.0f), .centre = {0.0f, 5.0f, 0.0f}},
	               {.axes = glm::mat3(1.0f), .centre = {2.0f, 6.0f, 0.0f}},
	               {.axes = glm::mat3(1.0f), .centre = {4.0f, 6.0f, 0.0f}}},
	};
	_world->frameTurn = 30;
	_deferCommands = true;
	_world->sent.clear();
	Press();
	Frame(10, {5.0f, 0.0f, 0.0f});
	Frame(10, {10.0f, 0.0f, 0.0f});
	Release();
	ASSERT_EQ(_world->leadsAsked.size(), 1U);
	EXPECT_EQ(_world->leadsAsked.front(), 2);
	ASSERT_EQ(_world->sent.size(), 1U);
	const auto letGo = std::get<player_commands::HandLetGo>(_world->sent.front());
	EXPECT_EQ(letGo.velocity, *_world->ledVelocity);
	// The frames draw the thing on the flight: half of the way through the turn after the send
	_world->frameTurn = 31;
	_world->frameTurnFraction = 0.5f;
	_system->DrawThrownForFrame();
	ASSERT_EQ(_world->drawnThrown.size(), 1U);
	EXPECT_EQ(_world->drawnThrown.front().first, rock);
	EXPECT_NEAR(_world->drawnThrown.front().second.x, 3.0f, 1.0e-4f);
	EXPECT_NEAR(_world->drawnThrown.front().second.y, 6.0f, 1.0e-4f);
	// The turn of the command lets go, and the frames draw the thing no more
	RunWaiting();
	_world->drawnThrown.clear();
	_system->DrawThrownForFrame();
	EXPECT_TRUE(_world->drawnThrown.empty());
}
