/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cmath>
#include <cstdint>

#include <array>
#include <deque>
#include <vector>

#include <glm/ext/matrix_transform.hpp>
#include <gtest/gtest.h>

#include "Creature/CreatureFight.h"
#include "Creature/CreatureFightHud.h"

using namespace openblack;
using namespace openblack::creature_fight;

namespace
{
/// Random numbers given in turn, then 0
Random Sequence(std::deque<uint32_t> values)
{
	return [values = std::move(values)](uint32_t range) mutable {
		if (values.empty())
		{
			return 0u;
		}
		const auto value = values.front();
		values.pop_front();
		return range == 0 ? 0u : value % range;
	};
}

Random Always(uint32_t value)
{
	return [value](uint32_t range) { return range == 0 ? 0u : std::min(value, range - 1); };
}
} // namespace

TEST(CreatureFight, ArenaIsSizedByTheBiggerCreatureAndCapped)
{
	EXPECT_FLOAT_EQ(ArenaRadius(1.0f, 0.5f), 39.0f);
	EXPECT_FLOAT_EQ(ArenaRadius(0.5f, 1.2f), 39.0f * 1.2f);
	EXPECT_FLOAT_EQ(ArenaRadius(3.0f, 1.0f), 60.0f);
	const Arena arena {.centre = {20.0f, 10.0f}, .radius = ArenaRadius(1.0f, 1.0f)};
	EXPECT_EQ(ArenaSpot(arena, 1.0f, true), glm::vec2(35.0f, 10.0f));
	EXPECT_EQ(ArenaSpot(arena, 2.0f, false), glm::vec2(-10.0f, 10.0f));
	EXPECT_FLOAT_EQ(ArrivalDistance(1.0f), 22.5f);
}

TEST(CreatureFight, ABlowPushesBothFightersApartByHalfTheOverlap)
{
	// Reaches of 6 and 4 count as 6 + 2 = 8; 4 apart, each moves 2 along the striker's facing
	const auto pushed = BlowPushOf({0.0f, 0.0f}, {0.0f, 4.0f}, {0.0f, 1.0f}, 6.0f, 4.0f);
	ASSERT_TRUE(pushed.first.has_value());
	ASSERT_TRUE(pushed.second.has_value());
	EXPECT_FLOAT_EQ(pushed.first->y, -2.0f);
	EXPECT_FLOAT_EQ(pushed.second->y, 6.0f);
	// Only half of the opponent's reach counts: 8 apart is not an overlap, although the full reaches are 10
	const auto apart = BlowPushOf({0.0f, 0.0f}, {0.0f, 8.0f}, {0.0f, 1.0f}, 6.0f, 4.0f);
	EXPECT_FALSE(apart.first.has_value());
	EXPECT_FALSE(apart.second.has_value());
	// The push follows the striker's facing, not the line between them, and the range of the arena does not stop it
	const auto sideways = BlowPushOf({100.0f, 0.0f}, {101.0f, 0.0f}, {0.0f, -1.0f}, 3.0f, 2.0f);
	ASSERT_TRUE(sideways.first.has_value());
	EXPECT_FLOAT_EQ(sideways.first->x, 100.0f);
	EXPECT_FLOAT_EQ(sideways.first->y, 1.5f);
	EXPECT_FLOAT_EQ(sideways.second->y, -1.5f);
}

TEST(CreatureFight, StepsBackOrSidewaysOnlyWithinRange)
{
	const Arena arena {.centre = {0.0f, 0.0f}, .radius = 50.0f};
	EXPECT_TRUE(WithinRange(arena, {39.0f, 0.0f}));
	EXPECT_FALSE(WithinRange(arena, {41.0f, 0.0f}));
	EXPECT_TRUE(CanStep(arena, {45.0f, 0.0f}, Step::Forward));
	EXPECT_FALSE(CanStep(arena, {45.0f, 0.0f}, Step::Back));
	EXPECT_FALSE(CanStep(arena, {45.0f, 0.0f}, Step::Left));
	EXPECT_TRUE(CanStep(arena, {10.0f, 0.0f}, Step::Right));
}

TEST(CreatureFight, AReelInABlockStopsAtTheRange)
{
	const Arena arena {.centre = {0.0f, 0.0f}, .radius = 50.0f};
	// Inside the range it moves; a move to the edge or past it does not occur
	EXPECT_EQ(BlockRecoilTo(arena, {0.0f, 30.0f}, {0.0f, 39.0f}), glm::vec2(0.0f, 39.0f));
	EXPECT_EQ(BlockRecoilTo(arena, {0.0f, 39.0f}, {0.0f, 40.0f}), glm::vec2(0.0f, 39.0f));
	EXPECT_EQ(BlockRecoilTo(arena, {0.0f, 39.0f}, {0.0f, 41.0f}), glm::vec2(0.0f, 39.0f));
}

TEST(CreatureFight, ClickHeightPicksTheBand)
{
	EXPECT_EQ(BandOf(0.1f), Band::Low);
	EXPECT_EQ(BandOf(0.32f), Band::Low);
	EXPECT_EQ(BandOf(0.33f), Band::Mid);
	EXPECT_EQ(BandOf(0.5f), Band::Mid);
	EXPECT_EQ(BandOf(0.66f), Band::Mid);
	EXPECT_EQ(BandOf(0.67f), Band::High);
	EXPECT_EQ(AttackMove(Band::High).kind, Move::Kind::High);
	EXPECT_EQ(AttackMove(Band::Low).kind, Move::Kind::Low);
}

TEST(CreatureFight, GroundClickStepsAlongTheLongerAxis)
{
	EXPECT_EQ(StepTowards({1.0f, 5.0f}), Step::Forward);
	EXPECT_EQ(StepTowards({1.0f, -5.0f}), Step::Back);
	EXPECT_EQ(StepTowards({5.0f, 1.0f}), Step::Right);
	EXPECT_EQ(StepTowards({-5.0f, -1.0f}), Step::Left);
	EXPECT_EQ(StepMove(Step::Left).value, animations::k_StepLeft);
	EXPECT_EQ(StepOf(animations::k_StepBack), Step::Back);
	EXPECT_FALSE(StepOf(animations::k_AttackHigh).has_value());
}

TEST(CreatureFight, ChargeRunsToOnePointTwoSeconds)
{
	EXPECT_FALSE(InitialCharge(Move::Kind::Mid).has_value());
	EXPECT_EQ(InitialCharge(Move::Kind::Block), 0.0f);
	EXPECT_EQ(InitialCharge(Move::Kind::Animation), 0.0f);
	EXPECT_EQ(InitialCharge(Move::Kind::Special), k_MaxChargeMs);
	EXPECT_EQ(InitialCharge(Move::Kind::Spell), k_MaxChargeMs);
	EXPECT_FLOAT_EQ(ReleasedCharge(5000.0f), 1200.0f);
	EXPECT_FLOAT_EQ(ReleasedCharge(-3.0f), 0.0f);
	EXPECT_FLOAT_EQ(BlowSpeed(0.0f), 0.5f);
	EXPECT_FLOAT_EQ(BlowSpeed(600.0f), 1.0f);
	EXPECT_FLOAT_EQ(BlowSpeed(3000.0f), 1.5f);
}

TEST(CreatureFight, QueueHoldsTwelveAndAClickReplacesIt)
{
	MoveQueue queue;
	for (size_t i = 0; i < MoveQueue::k_Capacity; ++i)
	{
		EXPECT_TRUE(queue.Push(BlockMove(), false));
	}
	EXPECT_FALSE(queue.Push(BlockMove(), false));
	EXPECT_EQ(queue.Size(), 12u);
	EXPECT_TRUE(queue.Push(AttackMove(Band::High), true));
	EXPECT_EQ(queue.Size(), 1u);
	EXPECT_TRUE(queue.HasWaiting());
	EXPECT_TRUE(queue.Release(700.0f));
	EXPECT_EQ(queue.Front()->chargeMs, 700.0f);
	EXPECT_FALSE(queue.Release(100.0f));
	queue.Push(StepMove(Step::Back), false);
	queue.Pop();
	EXPECT_EQ(queue.Front()->move, StepMove(Step::Back));
	queue.Pop();
	EXPECT_TRUE(queue.Empty());
	queue.Pop();
	EXPECT_TRUE(queue.Empty());
}

TEST(CreatureFight, LettingGoChargesOnlyTheMoveAddedLast)
{
	MoveQueue queue;
	queue.Push(AttackMove(Band::High), false);
	// A step added after the blow: letting go charges nothing, and the blow keeps waiting
	queue.Push(StepMove(Step::Left), false);
	EXPECT_FALSE(queue.Release(500.0f));
	EXPECT_FALSE(queue.Moves()[0].chargeMs.has_value());
	queue.Push(AttackMove(Band::Low), false);
	EXPECT_TRUE(queue.Release(5000.0f));
	EXPECT_FALSE(queue.Moves()[0].chargeMs.has_value());
	EXPECT_FLOAT_EQ(*queue.Moves()[2].chargeMs, k_MaxChargeMs);
	// A full queue refuses a blow, and letting go then charges nothing
	MoveQueue full;
	for (size_t i = 0; i < MoveQueue::k_Capacity; ++i)
	{
		full.Push(AttackMove(Band::Mid), false);
	}
	EXPECT_FALSE(full.Push(AttackMove(Band::Mid), false));
	EXPECT_FALSE(full.Release(300.0f));
}

TEST(CreatureFight, HeldTimeIsKeptToTheGameTurns)
{
	EXPECT_FLOAT_EQ(HeldMs(250.0f, 5), 250.0f);
	EXPECT_FLOAT_EQ(HeldMs(900.0f, 2), 300.0f);
	EXPECT_FLOAT_EQ(HeldMs(40.0f, 0), 40.0f);
	EXPECT_FLOAT_EQ(HeldMs(400.0f, 0), 100.0f);
}

TEST(CreatureFight, ButtonsAndTips)
{
	EXPECT_TRUE(ReplacesQueue(Button::Move));
	EXPECT_FALSE(ReplacesQueue(Button::Action));
	const Arena arena {.centre = {10.0f, 0.0f}, .radius = 20.0f};
	EXPECT_TRUE(GroundPressCounts(arena, {29.0f, 0.0f}));
	EXPECT_FALSE(GroundPressCounts(arena, {30.5f, 0.0f}));
	EXPECT_EQ(TipOver(true, false), Tip::Block);
	EXPECT_EQ(TipOver(true, true), Tip::Block);
	EXPECT_EQ(TipOver(false, true), Tip::Attack);
	EXPECT_EQ(TipOver(false, false), Tip::Manoeuvre);
	EXPECT_EQ(ToolTipIndex(Tip::Block), 22u);
	EXPECT_EQ(ToolTipIndex(Tip::Attack), 23u);
	EXPECT_EQ(ToolTipIndex(Tip::Manoeuvre), 46u);
}

TEST(CreatureFight, TheHandDirectsTheFightOnlyInTheFightViewWithAnEmptyHand)
{
	EXPECT_TRUE(DirectsFight(true, true, true));
	// The creature does not duel
	EXPECT_FALSE(DirectsFight(false, true, true));
	// The world camera shows the duel, but not the fight view
	EXPECT_FALSE(DirectsFight(true, false, true));
	// The hand holds a thing or a miracle
	EXPECT_FALSE(DirectsFight(true, true, false));
}

TEST(CreatureFight, APlaceIsInTheArenaOnlyWithinItsRadius)
{
	const Arena arena {.centre = {10.0f, 0.0f}, .radius = 20.0f};
	EXPECT_TRUE(InArena(arena, {10.0f, 0.0f}));
	EXPECT_TRUE(InArena(arena, {10.0f, 19.9f}));
	// The edge is out
	EXPECT_FALSE(InArena(arena, {30.0f, 0.0f}));
	EXPECT_FALSE(InArena(arena, {-10.5f, 0.0f}));
}

TEST(CreatureFight, GettingHitTakesAWaitingBlowAway)
{
	MoveQueue queue;
	queue.Push(BlockMove(), false);
	queue.Push(AttackMove(Band::Mid), false);
	queue.Push(StepMove(Step::Forward), false);
	queue.CancelWaiting();
	ASSERT_EQ(queue.Size(), 2u);
	EXPECT_EQ(queue.Moves()[0].move, BlockMove());
	EXPECT_EQ(queue.Moves()[1].move, StepMove(Step::Forward));
}

TEST(CreatureFight, OrdersAreTakenInRangeAndOnceCharged)
{
	Fighter fighter;
	Enter(fighter, State::Stance);
	fighter.queue.Push(AttackMove(Band::Low), false);
	EXPECT_FALSE(TakeOrder(fighter, true).has_value());
	fighter.queue.Release(1200.0f);
	EXPECT_FALSE(TakeOrder(fighter, false).has_value());
	const auto order = TakeOrder(fighter, true);
	ASSERT_TRUE(order.has_value());
	EXPECT_EQ(order->kind, Order::Kind::Blow);
	EXPECT_EQ(order->band, Band::Low);
	EXPECT_FLOAT_EQ(order->speed, 1.5f);
	EXPECT_TRUE(fighter.queue.Empty());

	// Blocking, anything ends the block and is made after it
	Enter(fighter, State::Block);
	fighter.queue.Push(StepMove(Step::Back), false);
	EXPECT_EQ(TakeOrder(fighter, true)->kind, Order::Kind::EndBlock);
	EXPECT_EQ(fighter.queue.Size(), 1u);
	Enter(fighter, State::Action, animations::k_AttackHigh);
	EXPECT_FALSE(TakeOrder(fighter, true).has_value());
}

TEST(CreatureFight, PlayerMovesTakeBackControlAndTeach)
{
	Fighter fighter;
	fighter.control = Control::Computer;
	fighter.computerWaitMs = 0.0f;
	EXPECT_TRUE(PlayerMove(fighter, AttackMove(Band::High), true));
	EXPECT_EQ(fighter.control, Control::Player);
	EXPECT_FLOAT_EQ(fighter.computerWaitMs, k_ComputerWaitsMs);
	// Queueing teaches nothing: the move teaches as it is made
	EXPECT_FLOAT_EQ(fighter.tendency, 0.0f);
	EXPECT_FLOAT_EQ(LearnTendency(0.0f, Move::Kind::High), 0.02f);
	// A difference of nearly equal numbers: a fused multiply-add, as arm64 compilers make, moves its last bits
	EXPECT_NEAR(LearnTendency(0.02f, Move::Kind::Block), (0.98f * 0.02f) - 0.02f, 1e-7f);
	EXPECT_NEAR(LearnTendency(0.02f, Move::Kind::Animation), (0.98f * 0.02f) - 0.02f, 1e-7f);
	EXPECT_FLOAT_EQ(LearnTendency(0.5f, Move::Kind::Spell), 0.5f);
	EXPECT_FLOAT_EQ(LearnTendency(0.0f, Move::Kind::Special), 0.02f);
	EXPECT_FLOAT_EQ(LearnTendency(1.0f, Move::Kind::Mid), 1.0f);
	EXPECT_FLOAT_EQ(LearnTendency(-1.0f, Move::Kind::Block), -1.0f);
	EXPECT_FLOAT_EQ(FirstTendency(-0.7f), 0.7f);
}

TEST(CreatureFight, StepsTeachEveryCreatureAndOtherMovesOnlyTheHumanPlayers)
{
	EXPECT_TRUE(LearnsFromMove(Move::Kind::Animation, false));
	EXPECT_TRUE(LearnsFromMove(Move::Kind::Animation, true));
	EXPECT_FALSE(LearnsFromMove(Move::Kind::Block, false));
	EXPECT_FALSE(LearnsFromMove(Move::Kind::High, false));
	EXPECT_TRUE(LearnsFromMove(Move::Kind::Block, true));
	EXPECT_TRUE(LearnsFromMove(Move::Kind::Special, true));
}

TEST(CreatureFight, TheLeanIsSetAgainstItsAlignmentUntilAFightIsCompleted)
{
	// No completed fight: the alignment sets the learnt lean again
	EXPECT_FLOAT_EQ(TendencyAtStart(0, 0.4f, 0.6f), -0.6f);
	EXPECT_FLOAT_EQ(TendencyAtStart(0, -0.9f, -0.3f), 0.3f);
	// After a completed fight, the creature keeps the lean that it learnt
	EXPECT_FLOAT_EQ(TendencyAtStart(1, 0.4f, 0.6f), 0.4f);
	EXPECT_FLOAT_EQ(TendencyAtStart(7, -0.25f, -1.0f), -0.25f);
}

TEST(CreatureFight, ABlockQueuedWhileBlockingEndsTheBlock)
{
	Fighter fighter;
	Enter(fighter, State::Block);
	fighter.queue.Push(BlockMove(), false);
	fighter.queue.Push(StepMove(Step::Back), false);
	const auto order = TakeOrder(fighter, true);
	ASSERT_TRUE(order.has_value());
	EXPECT_EQ(order->kind, Order::Kind::EndBlock);
	ASSERT_EQ(fighter.queue.Size(), 1u);
	EXPECT_EQ(fighter.queue.Front()->move, StepMove(Step::Back));
	// Anything else ends it and is kept for the stance after
	EXPECT_EQ(TakeOrder(fighter, true)->kind, Order::Kind::EndBlock);
	EXPECT_EQ(fighter.queue.Size(), 1u);
}

TEST(CreatureFight, HealthStaminaAndLife)
{
	EXPECT_FLOAT_EQ(FightHealthAtStart(1.0f), 1.0f);
	EXPECT_FLOAT_EQ(FightHealthAtStart(0.2f), 0.6f);
	EXPECT_FLOAT_EQ(StaminaAtStart(0.8f, 0.3f), 0.5f);
	EXPECT_FLOAT_EQ(StaminaAtStart(0.2f, 0.9f), 0.0f);
	EXPECT_FLOAT_EQ(RegainStamina(1.0f), 1.0f);
	EXPECT_NEAR(RegainStamina(0.5f), 0.5f + (1.0f / 600.0f), 1e-6f);
	// A knock out costs a quarter of the bar
	EXPECT_FLOAT_EQ(LifeAfterFight(1.0f, 0.0f), 0.75f);
	EXPECT_FLOAT_EQ(LifeAfterFight(0.8f, 0.6f), 0.7f);
	EXPECT_FLOAT_EQ(LifeAfterFight(0.1f, 0.0f), 0.0f);
	EXPECT_TRUE(HealthyEnoughToFight(0.11f));
	EXPECT_FALSE(HealthyEnoughToFight(0.1f));
	EXPECT_FLOAT_EQ(FaintSeconds(1.0f), 12.0f);
	EXPECT_FLOAT_EQ(FaintSeconds(2.0f), 16.0f);
	EXPECT_TRUE(NeedsRest(0.3f, 0.0f));
	EXPECT_TRUE(NeedsRest(0.9f, 0.5f));
	EXPECT_FALSE(NeedsRest(0.4f, 0.3f));
}

TEST(CreatureFight, DamageFollowsTheFormula)
{
	// Even creatures at normal speed: high 0.05, middle and low 0.03
	Blow even {.heightShare = 0.8f};
	EXPECT_FLOAT_EQ(DamageFactor(even), 1.0f);
	EXPECT_FLOAT_EQ(ResolveBlow(even, RecoilDirection::Plain).damage, 0.05f);
	even.heightShare = 0.5f;
	EXPECT_FLOAT_EQ(ResolveBlow(even, RecoilDirection::Plain).damage, 0.03f);
	even.heightShare = 0.1f;
	EXPECT_FLOAT_EQ(ResolveBlow(even, RecoilDirection::Plain).damage, 0.03f);

	// Size, strength and speed scale it
	const Blow strong {
	    .attackerSize = 1.0f, .defenderSize = 1.0f, .attackerStrength = 1.0f, .defenderStrength = 0.5f, .speed = 1.0f};
	EXPECT_FLOAT_EQ(DamageFactor(strong), 2.5f / 1.5f);
	EXPECT_FLOAT_EQ(DamageFactor({.attackerSize = 2.0f, .defenderSize = 1.0f, .speed = 0.5f}), 1.0f);
	// Kept from 0.04 to 2, doubled for the special move
	EXPECT_FLOAT_EQ(DamageFactor({.attackerSize = 4.0f, .defenderSize = 1.0f, .speed = 1.5f}), 2.0f);
	EXPECT_FLOAT_EQ(DamageFactor({.attackerSize = 4.0f, .defenderSize = 1.0f, .speed = 1.5f, .special = true}), 4.0f);
	EXPECT_FLOAT_EQ(DamageFactor({.attackerSize = 0.1f, .defenderSize = 2.0f, .speed = 0.5f}), 0.04f);
	// The most a blow does: 0.1, or 0.2 for the special move
	EXPECT_FLOAT_EQ(ResolveBlow({.attackerSize = 4.0f, .speed = 1.5f, .heightShare = 0.9f}, RecoilDirection::Plain).damage,
	                0.1f);
	EXPECT_FLOAT_EQ(
	    ResolveBlow({.attackerSize = 4.0f, .speed = 1.5f, .special = true, .heightShare = 0.9f}, RecoilDirection::Plain).damage,
	    0.2f);
}

TEST(CreatureFight, ABlockedBlowDoesATenth)
{
	const auto open = ResolveBlow({.heightShare = 0.8f}, RecoilDirection::Plain);
	const auto blocked = ResolveBlow({.heightShare = 0.8f, .blocked = true}, RecoilDirection::Plain);
	EXPECT_FLOAT_EQ(blocked.damage, open.damage * 0.1f);
}

TEST(CreatureFight, RecoilsByHeightAndDirection)
{
	EXPECT_EQ(ResolveBlow({.heightShare = 0.9f}, RecoilDirection::Plain).recoil, animations::k_RecoilHigh);
	EXPECT_EQ(ResolveBlow({.heightShare = 0.5f}, RecoilDirection::Right).recoil, animations::k_RecoilMid + 1);
	const auto low = ResolveBlow({.heightShare = 0.1f}, RecoilDirection::Bottom);
	EXPECT_EQ(low.recoil, animations::k_RecoilLow + 4);
	EXPECT_EQ(low.plainRecoil, animations::k_RecoilLow);
	// By the swing of the striking bone, ahead being -z
	EXPECT_EQ(RecoilDirectionOf({0.1f, 0.1f, -1.0f}), RecoilDirection::Plain);
	EXPECT_EQ(RecoilDirectionOf({-0.8f, 0.2f, -0.1f}), RecoilDirection::Right);
	EXPECT_EQ(RecoilDirectionOf({0.8f, 0.2f, -0.1f}), RecoilDirection::Left);
	EXPECT_EQ(RecoilDirectionOf({0.1f, -0.9f, 0.0f}), RecoilDirection::Top);
	EXPECT_EQ(RecoilDirectionOf({0.1f, 0.9f, 0.0f}), RecoilDirection::Bottom);
	// A swing back towards the attacker reels the body too; an even swing ahead and across throws it straight back
	EXPECT_EQ(RecoilDirectionOf({0.0f, 0.5f, 1.0f}), RecoilDirection::Bottom);
	EXPECT_EQ(RecoilDirectionOf({1.0f, 0.0f, -1.0f}), RecoilDirection::Plain);
	// No swing at all throws it straight back, and a swing equally across and up reels it to the left or right
	EXPECT_EQ(RecoilDirectionOf({0.0f, 0.0f, 0.0f}), RecoilDirection::Plain);
	EXPECT_EQ(RecoilDirectionOf({1.0f, 1.0f, 0.0f}), RecoilDirection::Left);
	EXPECT_EQ(RecoilDirectionOf({-1.0f, 1.0f, 0.0f}), RecoilDirection::Right);
	EXPECT_EQ(WobbleAnimation(Band::High, true), animations::k_WobbleHighSide);
	EXPECT_EQ(WobbleAnimation(Band::Low, false), animations::k_WobbleLowFrontBack);
}

TEST(CreatureFight, BlowsTakeTheirShapeFromTheStrikingBone)
{
	// A blow lands 400 ms into a 1000 ms animation that carries the root 10 units along -z
	const auto shape = ShapeOfBlow({3.0f, 8.0f, -6.0f}, {3.0f, 7.0f, -4.0f}, 400.0f, 1000.0f, -10.0f);
	EXPECT_FLOAT_EQ(shape.point.x, 0.0f);
	EXPECT_FLOAT_EQ(shape.point.y, 8.0f);
	EXPECT_FLOAT_EQ(shape.point.z, -10.0f);
	EXPECT_FLOAT_EQ(shape.swing.y, 20.0f);
	EXPECT_FLOAT_EQ(shape.swing.z, -40.0f);
	EXPECT_FLOAT_EQ(HeightShareOf(9.0f, 12.0f), 0.75f);
}

TEST(CreatureFight, BlowsAreTestedThirtyTimesATurnNearTheirLanding)
{
	// A creature of size 1: a turn of 100 ms gives a span of 117 ms, tests 3 ms apart, within 70 ms of the land time
	EXPECT_EQ(TestSpanMs(100.0f, 1.0f), 117);
	EXPECT_EQ(TestWindowMs(117), 70);
	const auto times = BlowTestTimes(200, 117, 300);
	ASSERT_FALSE(times.empty());
	EXPECT_EQ(times.front(), 230);
	EXPECT_EQ(times.back(), 290);
	EXPECT_EQ(times.size(), 21u);
	EXPECT_TRUE(BlowTestTimes(0, 117, 600).empty());
	// A span shorter than thirty milliseconds tests thirty times at the same moment
	const auto still = BlowTestTimes(100, 29, 110);
	EXPECT_EQ(still.size(), 30u);
	EXPECT_EQ(still.front(), 100);
}

TEST(CreatureFight, BlowsAreTestedAlongFiveLines)
{
	const auto points = BlowTestPoints({0.0f, 10.0f, 0.0f}, {0.0f, 10.0f, 2.0f}, 3.0f);
	// Moved on along the swing, then to either side and above and below
	EXPECT_EQ(points[0], glm::vec3(0.0f, 10.0f, -3.0f));
	EXPECT_EQ(points[1], glm::vec3(-3.0f, 10.0f, 0.0f));
	EXPECT_EQ(points[2], glm::vec3(0.0f, 7.0f, 0.0f));
	EXPECT_EQ(points[3], glm::vec3(3.0f, 10.0f, 0.0f));
	EXPECT_EQ(points[4], glm::vec3(0.0f, 13.0f, 0.0f));
	// With no swing, every line goes to the bone
	for (const auto& point : BlowTestPoints({1.0f, 2.0f, 3.0f}, {1.0f, 2.0f, 3.0f}, 3.0f))
	{
		EXPECT_EQ(point, glm::vec3(1.0f, 2.0f, 3.0f));
	}
	EXPECT_FLOAT_EQ(BlowTestReach(2.0f, false), 6.0f);
	EXPECT_FLOAT_EQ(BlowTestReach(2.0f, true), 12.0f);
	EXPECT_FLOAT_EQ(SwingPuffSize(1.5f, false), 3.0f);
	EXPECT_FLOAT_EQ(SwingPuffSize(1.5f, true), 6.0f);
}

TEST(CreatureFight, BloodAndDustFlyAlongTheBlow)
{
	const glm::vec3 vector(30.0f, 0.0f, 40.0f);
	EXPECT_FLOAT_EQ(BloodSpreadStep(vector), 0.5f);
	EXPECT_EQ(SprayVelocity(vector, 0.5f, {25, 25, 25}), vector);
	EXPECT_EQ(SprayVelocity(vector, 0.5f, {0, 50, 35}), glm::vec3(17.5f, 12.5f, 45.0f));
}

TEST(CreatureFight, WoundsByTheBlow)
{
	EXPECT_EQ(WoundKind(1, Sequence({0})), 0);
	EXPECT_EQ(WoundKind(0, Sequence({1})), 1);
	EXPECT_EQ(WoundKind(2, Sequence({1})), 2);
	EXPECT_EQ(WoundKind(1, Sequence({1, 2})), 5);
	// A type other than 0, 1 or 2, as from a file that lacks the types, gives the first kind
	EXPECT_EQ(WoundKind(-1, Sequence({1})), 0);
	EXPECT_EQ(WoundKind(3, Sequence({1})), 0);
	// The species gives each blow its type, in the order of the blow animations
	const BlowWoundTypes types {0, 1, 2, 0, 1, 2, 2, 1, 0, 2, 1, 0};
	EXPECT_EQ(BlowWoundType(animations::k_AttackEye, types), 1);
	EXPECT_EQ(BlowWoundType(animations::k_FirstAttack + 11, types), 0);
	EXPECT_EQ(BlowWoundType(animations::k_FirstAttack + 9, types), 2);
	EXPECT_EQ(BlowWoundType(animations::k_FirstAttack - 1, types), -1);
	EXPECT_EQ(BlowWoundType(animations::k_FirstAttack + 12, types), -1);
	EXPECT_TRUE(Bleeds(3));
	EXPECT_FALSE(Bleeds(4));
}

TEST(CreatureFight, StatesFollowOneAnother)
{
	EXPECT_EQ(AfterAnimation(State::Start, true), State::Stance);
	EXPECT_EQ(AfterAnimation(State::Start, false), State::Finish);
	EXPECT_EQ(AfterAnimation(State::Stance, false), State::Finish);
	EXPECT_EQ(AfterAnimation(State::Action, true), State::Stance);
	EXPECT_EQ(AfterAnimation(State::Finish, true), State::Idle);
	EXPECT_EQ(AfterAnimation(State::BlockStart, true), State::Block);
	EXPECT_EQ(AfterAnimation(State::BlockRecoil, true), State::Block);
	EXPECT_EQ(AfterAnimation(State::BlockEnd, true), State::Stance);
	EXPECT_EQ(AfterAnimation(State::Faint, true), State::Lying);
	EXPECT_EQ(AfterAnimation(State::CastStart, true), State::Cast);
	EXPECT_EQ(AfterAnimation(State::Cast, true), State::CastEnd);
	EXPECT_EQ(AfterAnimation(State::CastEnd, true), State::Stance);
	EXPECT_EQ(AnimationOf(State::BlockRecoil), animations::k_RecoilBlock);
	EXPECT_TRUE(IsBlocking(State::Block, 0.0f, 1000.0f));
	EXPECT_FALSE(IsBlocking(State::BlockStart, 400.0f, 1000.0f));
	EXPECT_TRUE(IsBlocking(State::BlockStart, 600.0f, 1000.0f));
	EXPECT_TRUE(IsBlocking(State::BlockEnd, 400.0f, 1000.0f));
	EXPECT_FALSE(IsBlocking(State::BlockEnd, 600.0f, 1000.0f));
	EXPECT_FLOAT_EQ(PlaybackSpeed(animations::k_StepBack, 1.0f), 1.1f * 0.9f);
	EXPECT_FLOAT_EQ(PlaybackSpeed(animations::k_AttackHigh, 1.5f), 1.1f * 0.6f * 1.5f);

	Fighter fighter;
	Enter(fighter, State::Action, animations::k_AttackSpecial, 1.5f);
	EXPECT_TRUE(fighter.special);
	EXPECT_EQ(fighter.animation, animations::k_AttackSpecial);
	Enter(fighter, State::Stance);
	EXPECT_FALSE(fighter.special);
	EXPECT_EQ(fighter.animation, animations::k_Stance);
}

TEST(CreatureFight, ComputerKeepsTheGameTiers)
{
	EXPECT_EQ(TierOf(0.33f), 2u);
	EXPECT_EQ(TierOf(0.1f), 0u);
	EXPECT_EQ(TierOf(-0.9f), 0u);
}

TEST(CreatureFight, ComputerChoosesByWhatTheOpponentDoes)
{
	// A waiting opponent: the top tier always attacks, the bottom one turn in sixteen
	EXPECT_EQ(ChooseMove(2, Opponent::Stance, Always(5)).kind, Choice::Kind::Blow);
	EXPECT_EQ(ChooseMove(0, Opponent::Stance, Always(5)).kind, Choice::Kind::None);
	EXPECT_EQ(ChooseMove(0, Opponent::Stance, Sequence({0, 1})).kind, Choice::Kind::Blow);
	EXPECT_EQ(ChooseMove(0, Opponent::Stance, Sequence({0, 1})).band, Band::Mid);
	// Reeling in a block, or doing anything else, the opponent is attacked
	EXPECT_EQ(ChooseMove(0, Opponent::BlockRecoil, Always(0)).kind, Choice::Kind::Blow);
	EXPECT_EQ(ChooseMove(0, Opponent::OtherAction, Always(0)).kind, Choice::Kind::Blow);
	// Attacked, the bottom tier blocks a quarter of the time, else steps away
	EXPECT_EQ(ChooseMove(0, Opponent::Attacking, Sequence({0})).kind, Choice::Kind::Block);
	const auto away = ChooseMove(0, Opponent::Attacking, Sequence({1, 2}));
	EXPECT_EQ(away.kind, Choice::Kind::Step);
	EXPECT_EQ(away.step, Step::Back);
	// The top tier counter-attacks four times in five
	EXPECT_EQ(ChooseMove(2, Opponent::Attacking, Sequence({1, 0})).kind, Choice::Kind::Blow);
	EXPECT_EQ(ChooseMove(2, Opponent::Attacking, Sequence({0, 0})).kind, Choice::Kind::Block);
	// Stepping in: block, step, step or attack
	EXPECT_EQ(ChooseMove(0, Opponent::SteppingForward, Sequence({0})).kind, Choice::Kind::Block);
	EXPECT_EQ(ChooseMove(0, Opponent::SteppingForward, Sequence({1, 0})).kind, Choice::Kind::Step);
	EXPECT_EQ(ChooseMove(0, Opponent::SteppingForward, Sequence({3, 0})).kind, Choice::Kind::Blow);
	// Blocking: mostly waits, one turn in ten a step or a blow
	EXPECT_EQ(ChooseMove(0, Opponent::Blocking, Sequence({0})).kind, Choice::Kind::None);
	EXPECT_EQ(ChooseMove(0, Opponent::Blocking, Sequence({1, 0})).kind, Choice::Kind::Step);
	EXPECT_EQ(ChooseMove(0, Opponent::Blocking, Sequence({1, 1})).kind, Choice::Kind::Blow);
	EXPECT_EQ(ChooseMove(2, Opponent::Other, Always(0)).kind, Choice::Kind::None);
	EXPECT_TRUE(ComputerEndsBlock(Always(1)));
	EXPECT_FALSE(ComputerEndsBlock(Always(0)));
}

TEST(CreatureFight, OpponentStatesAsTheComputerSeesThem)
{
	EXPECT_EQ(OpponentOf(State::Action, animations::k_AttackKnee), Opponent::Attacking);
	EXPECT_EQ(OpponentOf(State::Action, animations::k_StepForward), Opponent::SteppingForward);
	EXPECT_EQ(OpponentOf(State::Action, animations::k_RecoilMid), Opponent::OtherAction);
	EXPECT_EQ(OpponentOf(State::BlockEnd, animations::k_EndBlock), Opponent::Blocking);
	EXPECT_EQ(OpponentOf(State::Cast, animations::k_Cast), Opponent::Casting);
	EXPECT_EQ(OpponentOf(State::Start, animations::k_Start), Opponent::Other);
}

TEST(CreatureFight, ABlowIsChosenThatLandsAtTheBand)
{
	constexpr float k_Head = 15.0f;
	constexpr StepLengths k_Steps {.forward = 4.0f, .back = 3.0f};
	const auto blows = [](float high, float belly, float knee) {
		return std::array<BlowDepth, 4> {{
		    {.animation = animations::k_AttackHigh, .height = 12.0f, .depth = high},
		    {.animation = animations::k_AttackBelly, .height = 7.0f, .depth = belly},
		    {.animation = animations::k_AttackKnee, .height = 3.0f, .depth = knee},
		    {.animation = animations::k_AttackSpecial, .height = 7.0f, .depth = -1.0f},
		}};
	};
	// The body starts a little behind where each blow lands: the band asked for wins
	const auto near = blows(-2.0f, -3.0f, -1.0f);
	EXPECT_EQ(ChooseAttack(near, Band::Mid, k_Head, k_Steps, 1.0f).animation, animations::k_AttackBelly);
	EXPECT_EQ(ChooseAttack(near, Band::Low, k_Head, k_Steps, 1.0f).animation, animations::k_AttackKnee);
	EXPECT_EQ(ChooseAttack(near, Band::High, k_Head, k_Steps, 1.0f).animation, animations::k_AttackHigh);
	// The body is far ahead: the creature steps forward first
	const auto far = ChooseAttack(blows(20.0f, 20.0f, 20.0f), Band::Low, k_Head, k_Steps, 1.0f);
	EXPECT_FALSE(far.animation.has_value());
	EXPECT_EQ(far.step, Step::Forward);
	// The knee goes in too deep, so the creature steps back
	EXPECT_EQ(ChooseAttack(blows(-2.0f, -3.0f, -9.0f), Band::Low, k_Head, k_Steps, 1.0f).step, Step::Back);
	// A blow higher than the head is not used
	EXPECT_EQ(ChooseAttack(near, Band::High, 10.0f, k_Steps, 1.0f).animation, animations::k_AttackBelly);
	// When no blow lands from ten steps either way, the creature does nothing
	const auto none = ChooseAttack(blows(80.0f, 80.0f, 80.0f), Band::Low, k_Head, k_Steps, 1.0f);
	EXPECT_FALSE(none.animation.has_value());
	EXPECT_FALSE(none.step.has_value());
	EXPECT_FALSE(ChooseAttack({}, Band::High, k_Head, k_Steps, 1.0f).step.has_value());
}

TEST(CreatureFight, OnTheSameScoreTheBlowChangesButTheStepsStay)
{
	// No blow is low. The high blow lands first after three steps back, then the belly blow lands with no step and goes
	// in less. The belly blow wins, but the creature keeps the three steps back.
	const std::array<BlowDepth, 2> blows {{
	    {.animation = animations::k_AttackHigh, .height = 12.0f, .depth = -10.0f},
	    {.animation = animations::k_AttackBelly, .height = 7.0f, .depth = -0.5f},
	}};
	const auto choice = ChooseAttack(blows, Band::Low, 15.0f, {.forward = 4.0f, .back = 3.0f}, 1.0f);
	EXPECT_FALSE(choice.animation.has_value());
	EXPECT_EQ(choice.step, Step::Back);
	// The bands share their limits
	const auto [highLowest, highHighest] = BandLimits(Band::High, 10.0f);
	EXPECT_FLOAT_EQ(highLowest, 6.6f);
	EXPECT_FLOAT_EQ(highHighest, 10.0f);
	const auto [lowLowest, lowHighest] = BandLimits(Band::Low, 10.0f);
	EXPECT_FLOAT_EQ(lowLowest, 0.0f);
	EXPECT_FLOAT_EQ(lowHighest, 3.3f);
}

namespace
{
/// A bone whose ellipsoid has these half widths round a middle in the world
openblack::physics::Ellipsoid Bone(const glm::vec3& middle, const glm::vec3& half)
{
	const auto toWorld = glm::translate(glm::mat4(1.0f), middle);
	return {.boxMin = -half, .boxMax = half, .localToWorld = toWorld, .worldToLocal = glm::inverse(toWorld)};
}
} // namespace

TEST(CreatureFight, ABlowLineTakesOnlyAPointAheadOfItsStart)
{
	// The bone behind the start of the line is the nearest, so the blow line refuses the point
	const std::array bones {Bone({10.0f, 0.0f, 0.0f}, glm::vec3(1.0f)), Bone({-10.0f, 0.0f, 0.0f}, glm::vec3(1.0f))};
	const glm::vec3 origin(0.0f);
	const glm::vec3 direction(20.0f, 0.0f, 0.0f);
	const auto hit = openblack::physics::EllipsoidHit(bones, origin, direction);
	ASSERT_TRUE(hit.has_value());
	EXPECT_NEAR(hit->first.point.x, -11.0f, 1e-4f);
	EXPECT_FALSE(WithinBlowLine(hit->first.point, origin, direction));
	EXPECT_TRUE(WithinBlowLine({9.0f, 0.0f, 0.0f}, origin, direction));
	EXPECT_FALSE(WithinBlowLine({21.0f, 0.0f, 0.0f}, origin, direction));
	EXPECT_FALSE(WithinBlowLine({1.0f, 0.0f, 0.0f}, origin, glm::vec3(0.0f)));
}

TEST(CreatureFight, TheDepthToTheBodyIsTheNearestOfARowOfLines)
{
	// A ball 10 ahead, and a ball 8 ahead and 2 to the side: the row 4.5 wide meets both
	const std::array bones {Bone({10.0f, 0.0f, 0.0f}, glm::vec3(1.0f)), Bone({8.0f, 0.0f, 2.0f}, glm::vec3(1.0f))};
	const auto depth = DepthToBody(bones, glm::vec3(0.0f), {1.0f, 0.0f, 0.0f}, 1.0f);
	ASSERT_TRUE(depth.has_value());
	// The line 1.8 to the side meets the side ball first, 0.2 from its middle
	EXPECT_NEAR(*depth, 8.0f - std::sqrt(0.96f), 1e-3f);
	// A ball outside the row is not found
	const std::array wide {Bone({10.0f, 0.0f, 5.0f}, glm::vec3(1.0f))};
	EXPECT_FALSE(DepthToBody(wide, glm::vec3(0.0f), {1.0f, 0.0f, 0.0f}, 1.0f).has_value());
	// A point inside the body has a negative depth
	const auto inside = DepthToBody(bones, {10.0f, 0.0f, 0.0f}, {1.0f, 0.0f, 0.0f}, 1.0f);
	ASSERT_TRUE(inside.has_value());
	EXPECT_LT(*inside, 0.0f);
}

TEST(CreatureFight, OutcomesAndStarting)
{
	EXPECT_TRUE(WinnerPoos(-0.6f));
	EXPECT_FALSE(WinnerPoos(-0.4f));
	// The limit itself is not evil enough
	EXPECT_FALSE(WinnerPoos(-0.5f));
	// Only a blow that knocks the loser out, won by a creature with a player, is seen
	EXPECT_TRUE(FightWonIsSeen(true, true));
	EXPECT_FALSE(FightWonIsSeen(false, true));
	EXPECT_FALSE(FightWonIsSeen(true, false));
	EXPECT_EQ(Response(0.8f, 0.5f), animations::k_Happy);
	EXPECT_EQ(Response(0.4f, 0.5f), animations::k_Sad);
}

TEST(CreatureFightHud, AtTheDefaultScreenThePanelIsFlushInTheCorner)
{
	const auto layout = creature_fight_hud::Compute({800, 600});
	EXPECT_EQ(layout.unit, 8);
	EXPECT_EQ(layout.box.min, glm::vec2(0.0f, 0.0f));
	EXPECT_EQ(layout.box.max, glm::vec2(264.0f, 72.0f));
	EXPECT_FLOAT_EQ(layout.textSize, 24.0f);
	EXPECT_FLOAT_EQ(layout.nameWidth, 240.0f);
	// Health over the lower stamina bar, the name to their right
	EXPECT_EQ(layout.rows[0].health.min, glm::vec2(8.0f, 8.0f));
	EXPECT_EQ(layout.rows[0].health.max, glm::vec2(208.0f, 24.0f));
	EXPECT_EQ(layout.rows[0].stamina.min, glm::vec2(8.0f, 20.0f));
	EXPECT_EQ(layout.rows[0].stamina.max, glm::vec2(208.0f, 32.0f));
	EXPECT_EQ(layout.rows[0].name, glm::vec2(216.0f, 8.0f));
	EXPECT_EQ(layout.rows[1].health.min, glm::vec2(8.0f, 36.0f));
	EXPECT_EQ(layout.rows[1].stamina.max, glm::vec2(208.0f, 60.0f));
	EXPECT_EQ(layout.rows[1].name, glm::vec2(216.0f, 36.0f));
}

TEST(CreatureFightHud, ThePanelGrowsWithTheScreensHeightOnly)
{
	// A seventieth of the height in whole pixels, the same on a 4:3 screen and a wide one as tall
	const auto layout = creature_fight_hud::Compute({1440, 1080});
	const auto wide = creature_fight_hud::Compute({3440, 1080});
	EXPECT_EQ(layout.unit, 15);
	EXPECT_EQ(layout.box.max, glm::vec2(495.0f, 135.0f));
	EXPECT_EQ(wide.box.max, layout.box.max);
	// Three and a half units between the rows, cut to whole pixels; one and a half down to the stamina bar
	EXPECT_FLOAT_EQ(layout.rows[1].health.min.y, 67.0f);
	EXPECT_FLOAT_EQ(layout.rows[0].stamina.min.y, 15.0f + 22.0f);
}

TEST(CreatureFightHud, BarColours)
{
	EXPECT_EQ(creature_fight_hud::HealthColour(0.0f), glm::u8vec3(255, 0, 0));
	EXPECT_EQ(creature_fight_hud::HealthColour(0.25f), glm::u8vec3(255, 127, 0));
	EXPECT_EQ(creature_fight_hud::HealthColour(0.5f), glm::u8vec3(255, 255, 0));
	EXPECT_EQ(creature_fight_hud::HealthColour(0.75f), glm::u8vec3(127, 255, 0));
	EXPECT_EQ(creature_fight_hud::HealthColour(1.0f), glm::u8vec3(0, 255, 0));
	EXPECT_EQ(creature_fight_hud::HealthColour(2.0f), glm::u8vec3(0, 255, 0));
	EXPECT_EQ(creature_fight_hud::StaminaColour(0.0f), glm::u8vec3(32, 128, 255));
	EXPECT_EQ(creature_fight_hud::StaminaColour(1.0f), glm::u8vec3(127, 128, 255));
	EXPECT_EQ(creature_fight_hud::StaminaColour(0.5f), glm::u8vec3(79, 128, 255));
}

TEST(CreatureFightHud, ALongNameIsCutToFit)
{
	const auto tenEach = [](std::u16string_view text) { return 10.0f * static_cast<float>(text.size()); };
	EXPECT_EQ(creature_fight_hud::FitName(u"Bartholomew", 50.0f, tenEach), u"Barth");
	EXPECT_EQ(creature_fight_hud::FitName(u"Rex", 50.0f, tenEach), u"Rex");
	EXPECT_EQ(creature_fight_hud::FitName(u"", 50.0f, tenEach), u"");
}

TEST(CreatureFight, AMoveForACreatureNobodyFightsLeavesItToTheQueue)
{
	// A script turned the computer off: moves added for it don't hand it to the player
	Fighter fighter;
	fighter.control = Control::None;
	EXPECT_TRUE(PlayerMove(fighter, AttackMove(Band::High), false));
	EXPECT_EQ(fighter.control, Control::None);
	// The player's own creature stays the player's
	fighter.control = Control::Player;
	EXPECT_TRUE(PlayerMove(fighter, BlockMove(), false));
	EXPECT_EQ(fighter.control, Control::Player);
}

TEST(CreatureFight, ScriptsNumberMovesAsTheGameQueuesThem)
{
	EXPECT_EQ(ScriptMove(0), Move {.kind = Move::Kind::High});
	EXPECT_EQ(ScriptMove(1), Move {.kind = Move::Kind::Mid});
	EXPECT_EQ(ScriptMove(2), Move {.kind = Move::Kind::Low});
	EXPECT_EQ(ScriptMove(3), Move {.kind = Move::Kind::Block});
	EXPECT_EQ(ScriptMove(4), Move {.kind = Move::Kind::Special});
	EXPECT_FALSE(ScriptMove(5).has_value());
	// A step is its animation with the high bit, a miracle its number with the next
	EXPECT_EQ(ScriptMove(137 | k_ScriptAnimationBit), (Move {.kind = Move::Kind::Animation, .value = 137}));
	EXPECT_EQ(ScriptMove(7 | k_ScriptSpellBit), (Move {.kind = Move::Kind::Spell, .value = 7}));
	// The animation bit wins over the miracle's
	EXPECT_EQ(ScriptMove(9 | k_ScriptAnimationBit | k_ScriptSpellBit), (Move {.kind = Move::Kind::Animation, .value = 9}));
}

TEST(CreatureFight, WhatAFighterIsDoingGoesByItsAnimation)
{
	EXPECT_EQ(FightActionOf(animations::k_Start), FightAction::Stance);
	EXPECT_EQ(FightActionOf(animations::k_Stance), FightAction::Stance);
	EXPECT_EQ(FightActionOf(animations::k_Finish), FightAction::Stance);
	EXPECT_EQ(FightActionOf(animations::k_Block), FightAction::Block);
	EXPECT_EQ(FightActionOf(142), FightAction::Block);
	EXPECT_EQ(FightActionOf(144), FightAction::Block);
	EXPECT_EQ(FightActionOf(141), FightAction::Other);
	EXPECT_EQ(FightActionOf(animations::k_StepForward), FightAction::Step);
	EXPECT_EQ(FightActionOf(animations::k_StepLeft), FightAction::Step);
	EXPECT_EQ(FightActionOf(animations::k_AttackHigh), FightAction::Blow);
	EXPECT_EQ(FightActionOf(129), FightAction::Blow);
	EXPECT_EQ(FightActionOf(130), FightAction::Special);
	EXPECT_EQ(FightActionOf(131), FightAction::Blow);
	EXPECT_EQ(FightActionOf(135), FightAction::Blow);
	EXPECT_EQ(FightActionOf(136), FightAction::Special);
	EXPECT_EQ(FightActionOf(110), FightAction::Casting);
	EXPECT_EQ(FightActionOf(112), FightAction::Casting);
	EXPECT_EQ(FightActionOf(179), FightAction::ReelingHigh);
	EXPECT_EQ(FightActionOf(184), FightAction::ReelingMiddle);
	EXPECT_EQ(FightActionOf(193), FightAction::ReelingLow);
	EXPECT_EQ(FightActionOf(194), FightAction::Other);
	EXPECT_EQ(FightActionOf(animations::k_Faint), FightAction::Fainted);
	EXPECT_EQ(FightActionOf(animations::k_Taunt), FightAction::Other);
}

TEST(CreatureFight, OnlyBlowsAtABandCountAsQueuedHits)
{
	MoveQueue queue;
	ASSERT_TRUE(queue.Push(AttackMove(Band::High), false));
	ASSERT_TRUE(queue.Push(BlockMove(), false));
	ASSERT_TRUE(queue.Push(AttackMove(Band::Low), false));
	ASSERT_TRUE(queue.Push({.kind = Move::Kind::Special}, false));
	ASSERT_TRUE(queue.Push(StepMove(Step::Back), false));
	ASSERT_TRUE(queue.Push({.kind = Move::Kind::Spell, .value = 3}, false));
	EXPECT_EQ(QueuedBlows(queue.Moves()), 2u);
}

TEST(CreatureFight, ACreatureAgreesToFightOnlyOneOfASizeAndControlledAsItIs)
{
	EXPECT_TRUE(AgreesToFight(10.0f, 10.0f, false, false, false));
	EXPECT_TRUE(AgreesToFight(5.0f, 10.0f, false, true, true));
	EXPECT_TRUE(AgreesToFight(20.0f, 10.0f, false, false, false));
	EXPECT_FALSE(AgreesToFight(4.9f, 10.0f, false, false, false));
	EXPECT_FALSE(AgreesToFight(20.1f, 10.0f, false, false, false));
	EXPECT_FALSE(AgreesToFight(10.0f, 10.0f, true, false, false));
	EXPECT_FALSE(AgreesToFight(10.0f, 10.0f, false, true, false));
	EXPECT_FALSE(AgreesToFight(10.0f, 10.0f, false, false, true));
}

TEST(CreatureFight, TheCastingPoseIsHeldOnlyAMoment)
{
	// The pose is held until more than two milliseconds of game time have passed, whatever its animation's length
	EXPECT_FALSE(StateDone(State::Cast, 0.0f, 1000.0f, 0.0f));
	EXPECT_FALSE(StateDone(State::Cast, 0.0f, 1000.0f, 2.0f));
	EXPECT_TRUE(StateDone(State::Cast, 1.0f, 1000.0f, 16.0f));
	// Other states play their animation through
	EXPECT_FALSE(StateDone(State::CastStart, 500.0f, 1000.0f, 5000.0f));
	EXPECT_TRUE(StateDone(State::CastStart, 1000.0f, 1000.0f, 0.0f));
	EXPECT_TRUE(StateDone(State::Action, 0.0f, 0.0f, 0.0f));
}

TEST(CreatureFight, AFightCalledOffWindsDownAndFinishes)
{
	// At once from the stance, a block and the casting pose
	EXPECT_EQ(StopTransition(State::Stance, false), State::Finish);
	EXPECT_EQ(StopTransition(State::Block, false), State::BlockEnd);
	EXPECT_EQ(StopTransition(State::Cast, false), State::CastEnd);
	// The move under way plays out first
	EXPECT_FALSE(StopTransition(State::Action, false).has_value());
	EXPECT_EQ(StopTransition(State::Action, true), State::Finish);
	EXPECT_EQ(StopTransition(State::Start, true), State::Finish);
	EXPECT_EQ(StopTransition(State::BlockStart, true), State::BlockEnd);
	EXPECT_EQ(StopTransition(State::BlockRecoil, true), State::BlockEnd);
	// A cast started is never cast: it goes straight to ending the cast
	EXPECT_FALSE(StopTransition(State::CastStart, false).has_value());
	EXPECT_EQ(StopTransition(State::CastStart, true), State::CastEnd);
	// Ending a block or a cast carries on to the stance, then the finish
	EXPECT_FALSE(StopTransition(State::BlockEnd, true).has_value());
	EXPECT_FALSE(StopTransition(State::CastEnd, true).has_value());
	EXPECT_FALSE(StopTransition(State::Finish, true).has_value());
}

// The fighters' panel shows only under the player's own camera: a script taking the camera for a cut scene hides it, as
// the temple does; the cinema bars by themselves don't
TEST(CreatureFightHud, PanelOnlyUnderThePlayersCamera)
{
	EXPECT_TRUE(creature_fight_hud::PanelAllowed(false, false));
	EXPECT_FALSE(creature_fight_hud::PanelAllowed(false, true));
	EXPECT_FALSE(creature_fight_hud::PanelAllowed(true, false));
	EXPECT_FALSE(creature_fight_hud::PanelAllowed(true, true));
}

TEST(CreatureFight, OnlyTheCreatureThatPlannedAFightStillChoosesInIt)
{
	using creature_fight::LetsChoose;
	using creature_fight::ObeysInFight;
	// Out of a fight it chooses, unless it lies out cold
	EXPECT_TRUE(LetsChoose({}));
	EXPECT_FALSE(LetsChoose({.knockedOut = true}));
	// It started the fight by its own plan, which waits for the fight
	EXPECT_TRUE(LetsChoose({.fighting = true, .startedIt = true, .planned = true, .waitsForFight = true}));
	// Made to fight by the leash, a script or the debug tools
	EXPECT_FALSE(LetsChoose({.fighting = true, .startedIt = true, .planned = false, .waitsForFight = true}));
	// Made to do something to the loser after a knock-out
	EXPECT_FALSE(LetsChoose({.fighting = true, .startedIt = true, .planned = true, .waitsForFight = false, .won = true}));
	// The other creature obeys
	EXPECT_FALSE(LetsChoose({.fighting = true, .waitsForFight = true}));
	EXPECT_TRUE(ObeysInFight({.fighting = true}));
	EXPECT_FALSE(ObeysInFight({.fighting = true, .won = true}));
	EXPECT_FALSE(ObeysInFight({.fighting = true, .startedIt = true}));
	EXPECT_FALSE(ObeysInFight({}));
}

TEST(CreatureFight, TheFaceOffIsTheTauntOrTheRudeGesture)
{
	EXPECT_EQ(FaceOffAnimation(false), animations::k_Taunt);
	EXPECT_EQ(FaceOffAnimation(true), animations::k_Rude);
	EXPECT_EQ(animations::k_Rude, 222u);
	EXPECT_EQ(k_PointAtArenaTurns, 20u);
}

TEST(CreatureFight, ATieAfterAFightIsSadAndTheWeakerFearsTheOther)
{
	EXPECT_EQ(Response(0.5f, 0.5f), animations::k_Sad);
	EXPECT_FALSE(FearsAfterFight(0.8f, 0.5f));
	EXPECT_TRUE(FearsAfterFight(0.5f, 0.5f));
	EXPECT_TRUE(FearsAfterFight(0.4f, 0.5f));
}

TEST(CreatureFight, AFighterOnTheEdgeOfTheRangeIsOutsideIt)
{
	const Arena arena {.centre = {0.0f, 0.0f}, .radius = 10.0f};
	EXPECT_FALSE(OutsideRange(arena, {7.9f, 0.0f}));
	EXPECT_TRUE(OutsideRange(arena, {8.0f, 0.0f}));
	EXPECT_TRUE(OutsideRange(arena, {0.0f, -9.0f}));
}

TEST(CreatureFight, AFighterStepsBackToTheMiddle)
{
	// Within its body radius of the middle it takes no step
	EXPECT_FALSE(StepToMiddle({0.0f, 2.0f}, 2.0f, 2.0f).has_value());
	// Ahead or behind when the middle is 1.5 times farther that way than to the side
	EXPECT_EQ(StepToMiddle({2.0f, 3.0f}, 4.0f, 1.0f), Step::Forward);
	EXPECT_EQ(StepToMiddle({-2.0f, -3.0f}, 4.0f, 1.0f), Step::Back);
	EXPECT_EQ(StepToMiddle({0.0f, 0.0f}, 4.0f, 1.0f), Step::Back);
	// Else to the side
	EXPECT_EQ(StepToMiddle({2.1f, 3.0f}, 4.0f, 1.0f), Step::Left);
	EXPECT_EQ(StepToMiddle({-2.1f, 3.0f}, 4.0f, 1.0f), Step::Right);
}

TEST(CreatureFight, TheForwardExtentIsTheFarthestBoneAhead)
{
	const std::array<glm::vec2, 3> bones {glm::vec2 {0.0f, 1.0f}, glm::vec2 {0.0f, 3.0f}, glm::vec2 {5.0f, -2.0f}};
	EXPECT_FLOAT_EQ(ForwardExtent(bones, {0.0f, 1.0f}), 3.0f);
	EXPECT_FLOAT_EQ(ForwardExtent(bones, {0.0f, -1.0f}), 2.0f);
	EXPECT_FLOAT_EQ(ForwardExtent({}, {0.0f, 1.0f}), 0.0f);
}

TEST(CreatureFight, OverlappingFightersMoveApartInsideTheRange)
{
	const Arena arena {.centre = {0.0f, 0.0f}, .radius = 10.0f};
	// Two units apart and reaching four: each moves one unit along the first one's facing
	const auto pulled = PullApartOf(arena, {0.0f, 0.0f}, {0.0f, 2.0f}, {0.0f, 1.0f}, 4.0f);
	ASSERT_TRUE(pulled.first.has_value());
	ASSERT_TRUE(pulled.second.has_value());
	EXPECT_FLOAT_EQ(pulled.first->y, -1.0f);
	EXPECT_FLOAT_EQ(pulled.second->y, 3.0f);
	// Far enough apart, nothing moves
	const auto apart = PullApartOf(arena, {0.0f, 0.0f}, {0.0f, 4.0f}, {0.0f, 1.0f}, 4.0f);
	EXPECT_FALSE(apart.first.has_value());
	EXPECT_FALSE(apart.second.has_value());
	// A fighter does not move to a point outside the range
	const auto edge = PullApartOf(arena, {0.0f, -7.5f}, {0.0f, -6.0f}, {0.0f, 1.0f}, 3.0f);
	EXPECT_FALSE(edge.first.has_value());
	ASSERT_TRUE(edge.second.has_value());
	EXPECT_FLOAT_EQ(edge.second->y, -5.25f);
}

TEST(CreatureFight, AFighterKeepsItsLastActionAfterItEnds)
{
	Fighter fighter;
	Enter(fighter, State::Action, animations::k_AttackHigh);
	EXPECT_EQ(fighter.lastAction, animations::k_AttackHigh);
	Enter(fighter, State::Stance);
	EXPECT_EQ(fighter.lastAction, animations::k_AttackHigh);
	Enter(fighter, State::BlockRecoil);
	EXPECT_EQ(fighter.lastAction, animations::k_RecoilBlock);
}

TEST(CreatureFight, ABlockOrACastIsTheLastActionAfterABlow)
{
	Fighter fighter;
	Enter(fighter, State::Action, animations::k_AttackHigh);
	Enter(fighter, State::Stance);
	Enter(fighter, State::BlockStart);
	EXPECT_EQ(fighter.lastAction, animations::k_Block);
	EXPECT_FALSE(IsAttack(fighter.lastAction));
	Enter(fighter, State::Block);
	Enter(fighter, State::BlockEnd);
	EXPECT_EQ(fighter.lastAction, animations::k_Block);
	Enter(fighter, State::Action, animations::k_AttackKnee);
	Enter(fighter, State::CastStart);
	EXPECT_EQ(fighter.lastAction, animations::k_Cast);
	EXPECT_FALSE(IsAttack(fighter.lastAction));
}

TEST(CreatureFight, FightMusicIsHeardWhileTheHandDirectsTheFightOrNearItsArena)
{
	const glm::vec3 camera {0.0f, 50.0f, 0.0f};
	EXPECT_TRUE(FightMusicHeard(true, {}, camera));
	EXPECT_FALSE(FightMusicHeard(false, {}, camera));
	// The distance is measured in all three directions, from the middle of the arena on the land
	const std::array near {glm::vec3 {80.0f, 0.0f, 0.0f}};
	EXPECT_TRUE(FightMusicHeard(false, near, camera));
	const std::array edge {glm::vec3 {0.0f, -50.0f, 0.0f}};
	EXPECT_FALSE(FightMusicHeard(false, edge, camera));
	const std::array several {glm::vec3 {500.0f, 0.0f, 0.0f}, glm::vec3 {0.0f, 0.0f, 99.0f}};
	EXPECT_FALSE(FightMusicHeard(false, std::span(several).first(1), camera));
	EXPECT_TRUE(FightMusicHeard(false, several, glm::vec3 {0.0f}));
}

TEST(CreatureFight, LifeLostInADuelComesOffTheFightHealth)
{
	EXPECT_FLOAT_EQ(HealthAfterLifeLost(0.6f, 0.25f), 0.35f);
	EXPECT_FLOAT_EQ(HealthAfterLifeLost(0.2f, 0.5f), 0.0f);
	EXPECT_FLOAT_EQ(HealthAfterLifeLost(0.2f, 0.0f), 0.2f);
}
