/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "CreatureFight.h"

#include <cmath>
#include <cstdlib>

#include <algorithm>
#include <array>

#include <glm/geometric.hpp>

#include "Common/ModelInverseSquareRoot.h"
#include "Common/X87.h"
#include "Common/X87Vector.h"
#include "Help/HelpProfile.h"

using namespace openblack;
using namespace openblack::creature_fight;

namespace
{
/// The computer attacks a waiting opponent one turn in this many, by its tier
constexpr std::array<uint32_t, 3> k_AttackChances {16, 8, 1};
/// A computer of the top tier counter-attacks all but one time in this many
constexpr uint32_t k_CounterLots = 5;
/// The computer stops blocking one turn in this many
constexpr uint32_t k_EndBlockLots = 8;
/// A blow at a band asked for scores its steps; any other this
constexpr int k_OutOfBandScore = 999;
/// The tendency clamps here, and each move nudges it by this share
constexpr float k_TendencyKeep = 0.98f;
constexpr float k_TendencyNudge = 0.02f;
/// A winner more evil than this has a poo on the loser
constexpr float k_AlignmentPooBelow = -0.5f;
/// Knocked out, it lies this long and this much longer for each of its size
constexpr float k_FaintBaseSeconds = 8.0f;
constexpr float k_FaintSecondsPerSize = 4.0f;
/// The special move plays at this speed
constexpr float k_SpecialSpeed = 1.5f;

constexpr size_t RecoilOffset(RecoilDirection direction)
{
	return static_cast<size_t>(direction);
}
} // namespace

bool creature_fight::IsAttack(size_t animation)
{
	return animation >= animations::k_FirstAttack && animation < animations::k_FirstAttack + animations::k_AttackCount;
}

bool creature_fight::IsSpecialAttack(size_t animation)
{
	return animation == animations::k_AttackSpecial || animation == animations::k_AttackSpecial2;
}

bool creature_fight::IsStep(size_t animation)
{
	return animation >= animations::k_StepForward && animation <= animations::k_StepLeft;
}

float creature_fight::ArenaRadius(float sizeA, float sizeB)
{
	// The bigger creature's height, then the share of it
	const float height = std::max(sizeA * k_ArenaHeightPerSize, sizeB * k_ArenaHeightPerSize);
	return std::clamp(height * k_ArenaRadiusPerHeight, 0.0f, k_MaxArenaRadius);
}

glm::vec2 creature_fight::ArenaSpot(const Arena& arena, float size, bool madeIt)
{
	const auto offset = k_SpotPerSize * size;
	return arena.centre + glm::vec2(madeIt ? offset : -offset, 0.0f);
}

float creature_fight::ArrivalDistance(float size)
{
	return k_ArrivalPerSize * size;
}

bool creature_fight::WithinRange(const Arena& arena, glm::vec2 point)
{
	return glm::distance(point, arena.centre) <= k_RangeShare * arena.radius;
}

glm::vec2 creature_fight::BlockRecoilTo(const Arena& arena, glm::vec2 from, glm::vec2 to)
{
	return OutsideRange(arena, to) ? from : to;
}

bool creature_fight::OutsideRange(const Arena& arena, glm::vec2 point)
{
	return !(glm::distance(point, arena.centre) < k_RangeShare * arena.radius);
}

bool creature_fight::KeepsInArena(const Arena& arena, glm::vec2 self, glm::vec2 opponent)
{
	return OutsideRange(arena, opponent) || OutsideRange(arena, self);
}

float creature_fight::ForwardExtent(std::span<const glm::vec2> bones, glm::vec2 forward)
{
	float extent = 0.0f;
	for (const auto& bone : bones)
	{
		extent = std::max(extent, glm::dot(bone, forward));
	}
	return extent;
}

float creature_fight::ForwardExtentFromLastTurn(std::span<const glm::vec2> bones, glm::vec2 at, float heading)
{
	// The direction is kept as floats, and each difference, product and sum is rounded to a float in this order
	const auto ahead = x87::RoundToFloat(x87::Sin(heading));
	const auto across = x87::RoundToFloat(-x87::Cos(heading));
	float extent = 0.0f;
	for (const auto& bone : bones)
	{
		const auto x = x87::Sub(bone.x, at.x);
		const auto z = x87::Sub(bone.y, at.y);
		const auto reach = x87::Add(x87::Mul(x, ahead), x87::Mul(across, z));
		if (extent <= reach)
		{
			extent = reach;
		}
	}
	return extent;
}

creature_fight::PullApart creature_fight::PullApartOf(const Arena& arena, glm::vec2 first, glm::vec2 second, glm::vec2 forward,
                                                      float extents)
{
	const auto apart = glm::distance(first, second);
	if (!(apart < extents))
	{
		return {};
	}
	const auto push = forward * ((extents - apart) * 0.5f);
	const auto inside = [&arena](glm::vec2 point) {
		return glm::distance(point, arena.centre) < k_RangeShare * arena.radius ? std::optional(point) : std::nullopt;
	};
	return {.first = inside(first - push), .second = inside(second + push)};
}

creature_fight::PullApart creature_fight::BlowPushOf(glm::vec2 striker, glm::vec2 opponent, glm::vec2 forward,
                                                     float strikerExtent, float opponentExtent)
{
	const auto apart = glm::distance(striker, opponent);
	const auto extents = strikerExtent + (opponentExtent * 0.5f);
	if (!(apart < extents))
	{
		return {};
	}
	const auto push = forward * ((extents - apart) * 0.5f);
	return {.first = striker - push, .second = opponent + push};
}

Band creature_fight::BandOf(float shareOfHeight)
{
	if (shareOfHeight > k_HighAbove)
	{
		return Band::High;
	}
	return shareOfHeight < k_LowBelow ? Band::Low : Band::Mid;
}

size_t creature_fight::StepAnimation(Step step)
{
	switch (step)
	{
	case Step::Forward:
		return animations::k_StepForward;
	case Step::Back:
		return animations::k_StepBack;
	case Step::Right:
		return animations::k_StepRight;
	case Step::Left:
		return animations::k_StepLeft;
	}
	return animations::k_StepForward;
}

Step creature_fight::StepTowards(glm::vec2 local)
{
	if (std::abs(local.y) >= std::abs(local.x))
	{
		return local.y >= 0.0f ? Step::Forward : Step::Back;
	}
	return local.x >= 0.0f ? Step::Right : Step::Left;
}

std::optional<Step> creature_fight::StepToMiddle(glm::vec2 local, float distance, float bodyRadius)
{
	if (!(bodyRadius < distance))
	{
		return std::nullopt;
	}
	if (std::abs(local.x) * k_StepToMiddleAheadShare <= std::abs(local.y))
	{
		return local.y > 0.0f ? Step::Forward : Step::Back;
	}
	// A middle on the negative side of the x axis of the model takes the step to the right
	return local.x < 0.0f ? Step::Right : Step::Left;
}

bool creature_fight::CanStep(const Arena& arena, glm::vec2 position, Step step)
{
	return step == Step::Forward || WithinRange(arena, position);
}

bool creature_fight::ReplacesQueue(Button button)
{
	return button == Button::Move;
}

bool creature_fight::GroundPressCounts(const Arena& arena, glm::vec2 point)
{
	return InArena(arena, point);
}

bool creature_fight::DirectsFight(bool playersCreatureDuels, bool cameraShowsFight, bool handEmpty)
{
	return playersCreatureDuels && cameraShowsFight && handEmpty;
}

bool creature_fight::InArena(const Arena& arena, glm::vec2 point)
{
	return glm::distance(point, arena.centre) < arena.radius;
}

Tip creature_fight::TipOver(bool overOwnCreature, bool overOpponent)
{
	if (overOwnCreature)
	{
		return Tip::Block;
	}
	return overOpponent ? Tip::Attack : Tip::Manoeuvre;
}

uint32_t creature_fight::ToolTipIndex(Tip tip)
{
	// "Block", "Attack" and "Manoeuvre"
	constexpr std::array<uint32_t, 3> k_Indices {22, 23, 46};
	return k_Indices.at(static_cast<size_t>(tip));
}

Move creature_fight::AttackMove(Band band)
{
	switch (band)
	{
	case Band::High:
		return {.kind = Move::Kind::High};
	case Band::Mid:
		return {.kind = Move::Kind::Mid};
	case Band::Low:
		return {.kind = Move::Kind::Low};
	}
	return {.kind = Move::Kind::Mid};
}

std::optional<Move> creature_fight::ScriptMove(uint32_t value)
{
	constexpr uint32_t k_NumberMask = 0x3FFF;
	if ((value & k_ScriptAnimationBit) != 0)
	{
		return Move {.kind = Move::Kind::Animation, .value = value & k_NumberMask};
	}
	if ((value & k_ScriptSpellBit) != 0)
	{
		return Move {.kind = Move::Kind::Spell, .value = value & k_NumberMask};
	}
	switch (value)
	{
	case 0:
		return Move {.kind = Move::Kind::High};
	case 1:
		return Move {.kind = Move::Kind::Mid};
	case 2:
		return Move {.kind = Move::Kind::Low};
	case 3:
		return Move {.kind = Move::Kind::Block};
	case 4:
		return Move {.kind = Move::Kind::Special};
	default:
		return std::nullopt;
	}
}

FightAction creature_fight::FightActionOf(size_t animation)
{
	const auto within = [animation](size_t first, size_t last) { return animation >= first && animation <= last; };
	if (animation == animations::k_Faint)
	{
		return FightAction::Fainted;
	}
	if (within(110, 112))
	{
		return FightAction::Casting;
	}
	if (within(122, 124))
	{
		return FightAction::Stance;
	}
	if (animation == 130 || animation == 136)
	{
		return FightAction::Special;
	}
	if (within(125, 135))
	{
		return FightAction::Blow;
	}
	if (within(137, 140))
	{
		return FightAction::Step;
	}
	if (within(142, 144))
	{
		return FightAction::Block;
	}
	if (within(179, 183))
	{
		return FightAction::ReelingHigh;
	}
	if (within(184, 188))
	{
		return FightAction::ReelingMiddle;
	}
	if (within(189, 193))
	{
		return FightAction::ReelingLow;
	}
	return FightAction::Other;
}

uint32_t creature_fight::QueuedBlows(std::span<const QueuedMove> moves)
{
	return static_cast<uint32_t>(std::ranges::count_if(moves, [](const QueuedMove& queued) {
		return queued.move.kind == Move::Kind::High || queued.move.kind == Move::Kind::Mid ||
		       queued.move.kind == Move::Kind::Low;
	}));
}

Move creature_fight::StepMove(Step step)
{
	return {.kind = Move::Kind::Animation, .value = static_cast<uint32_t>(StepAnimation(step))};
}

Move creature_fight::BlockMove()
{
	return {.kind = Move::Kind::Block};
}

bool creature_fight::IsBlow(Move::Kind kind)
{
	return kind == Move::Kind::High || kind == Move::Kind::Mid || kind == Move::Kind::Low;
}

std::optional<float> creature_fight::InitialCharge(Move::Kind kind)
{
	switch (kind)
	{
	case Move::Kind::High:
	case Move::Kind::Mid:
	case Move::Kind::Low:
		return std::nullopt;
	case Move::Kind::Block:
	case Move::Kind::Animation:
		return 0.0f;
	case Move::Kind::Special:
	case Move::Kind::Spell:
		return k_MaxChargeMs;
	}
	return 0.0f;
}

float creature_fight::ReleasedCharge(float heldMs)
{
	return std::clamp(heldMs, 0.0f, k_MaxChargeMs);
}

float creature_fight::HeldMs(float realMs, uint32_t turnsSincePress)
{
	return std::min(realMs, static_cast<float>(turnsSincePress + 1) * k_HeldMsPerTurn);
}

float creature_fight::BlowSpeed(float chargeMs)
{
	return 0.5f + (ReleasedCharge(chargeMs) / k_MaxChargeMs);
}

bool MoveQueue::Push(const Move& move, bool replace)
{
	if (replace)
	{
		Clear();
	}
	if (_count >= k_Capacity)
	{
		_awaitingRelease = false;
		return false;
	}
	_moves.at(_count) = {.move = move, .chargeMs = InitialCharge(move.kind)};
	++_count;
	_awaitingRelease = !_moves.at(_count - 1).chargeMs.has_value();
	return true;
}

std::optional<QueuedMove> MoveQueue::Front() const
{
	return _count > 0 ? std::optional(_moves.front()) : std::nullopt;
}

void MoveQueue::Pop()
{
	if (_count == 0)
	{
		return;
	}
	std::shift_left(_moves.begin(), _moves.begin() + static_cast<std::ptrdiff_t>(_count), 1);
	--_count;
	_awaitingRelease = _awaitingRelease && _count > 0;
}

void MoveQueue::Assign(std::span<const QueuedMove> moves)
{
	_count = std::min(moves.size(), k_Capacity);
	std::ranges::copy(moves.first(_count), _moves.begin());
	_awaitingRelease = _count > 0 && !_moves.at(_count - 1).chargeMs.has_value();
}

void MoveQueue::Clear()
{
	_count = 0;
	_awaitingRelease = false;
}

bool MoveQueue::Release(float heldMs)
{
	if (!_awaitingRelease || _count == 0)
	{
		return false;
	}
	_awaitingRelease = false;
	_moves.at(_count - 1).chargeMs = ReleasedCharge(heldMs);
	return true;
}

void MoveQueue::CancelWaiting()
{
	const auto moves = std::span(_moves.data(), _count);
	const auto kept = std::ranges::remove_if(moves, [](const QueuedMove& move) { return !move.chargeMs.has_value(); });
	_count -= kept.size();
	_awaitingRelease = false;
}

bool MoveQueue::HasWaiting() const
{
	return std::ranges::any_of(Moves(), [](const QueuedMove& move) { return !move.chargeMs.has_value(); });
}

float creature_fight::LearnTendency(float tendency, Move::Kind kind)
{
	if (kind == Move::Kind::Spell)
	{
		return tendency;
	}
	// Blocking and stepping lean it towards defence, blows towards attack
	const auto nudge = kind == Move::Kind::Block || kind == Move::Kind::Animation ? -k_TendencyNudge : k_TendencyNudge;
	return std::clamp((k_TendencyKeep * tendency) + nudge, -1.0f, 1.0f);
}

float creature_fight::FirstTendency(float alignment)
{
	return std::clamp(-alignment, -1.0f, 1.0f);
}

bool creature_fight::LearnsFromMove(Move::Kind kind, bool humanPlayers)
{
	return kind == Move::Kind::Animation || humanPlayers;
}

bool creature_fight::LearnsFromPresses(bool networkGame, bool firstPlayer, bool humanGod)
{
	return networkGame ? humanGod : firstPlayer;
}

float creature_fight::TendencyAtStart(uint32_t fightsCompleted, float learnt, float alignment)
{
	return fightsCompleted == 0 ? FirstTendency(alignment) : learnt;
}

float creature_fight::FightHealthAtStart(float life)
{
	return (life + 1.0f) * 0.5f;
}

float creature_fight::StaminaAtStart(float energy, float exhaustion)
{
	return std::clamp(energy - exhaustion, 0.0f, 1.0f);
}

float creature_fight::RegainStamina(float stamina)
{
	return std::min(stamina + k_StaminaPerTurn, 1.0f);
}

float creature_fight::LifeAfterFight(float life, float fightHealth)
{
	return std::clamp(life - ((1.0f - std::clamp(fightHealth, 0.0f, 1.0f)) * k_LifeLostShare), 0.0f, 1.0f);
}

bool creature_fight::LetsChoose(const MindInFight& mind)
{
	if (mind.knockedOut)
	{
		return false;
	}
	return !mind.fighting || (mind.startedIt && mind.planned && mind.waitsForFight && !mind.won);
}

bool creature_fight::ObeysInFight(const MindInFight& mind)
{
	return mind.fighting && !mind.startedIt && !mind.won;
}

bool creature_fight::HealthyEnoughToFight(float life)
{
	return life > k_MinLifeToFight;
}

bool creature_fight::AgreesToFight(float height, float askerHeight, bool dead, bool scripted, bool askerScripted)
{
	const float share = height / askerHeight;
	return share >= k_SmallestOpponentShare && share <= k_BiggestOpponentShare && !dead && scripted == askerScripted;
}

float creature_fight::FaintSeconds(float size)
{
	return (k_FaintSecondsPerSize * size) + k_FaintBaseSeconds;
}

bool creature_fight::NeedsRest(float life, float exhaustion)
{
	return !(exhaustion <= k_RestedExhaustion && life >= k_GetUpLife);
}

creature_fight::FaintWait creature_fight::LyingTurn(FaintWait wait, const FaintBody& body, float turnSeconds,
                                                    float notStartedSeconds)
{
	// The fall of the faint ends only when the body lies still in its loop
	if (!wait.started && !(body.faintStarted && body.lies))
	{
		return {.seconds = notStartedSeconds, .started = false};
	}
	return {.seconds = WaitTurnCounts(body) ? wait.seconds + turnSeconds : wait.seconds, .started = true};
}

float creature_fight::DamageFactor(const Blow& blow)
{
	// The game multiplies in this order: the strengths, then the sizes, then the speed
	const auto strengths = (blow.attackerStrength + k_StrengthShift) / (blow.defenderStrength + k_StrengthShift);
	const auto factor =
	    std::clamp((strengths * (blow.attackerSize / blow.defenderSize)) * blow.speed, k_MinDamageFactor, k_MaxDamageFactor);
	return blow.special ? factor + factor : factor;
}

Hit creature_fight::ResolveBlow(const Blow& blow, RecoilDirection direction)
{
	const auto band = BandOf(blow.heightShare);
	const auto base = band == Band::High ? k_HighDamage : k_LowDamage;
	const auto plain = band == Band::High  ? animations::k_RecoilHigh
	                   : band == Band::Mid ? animations::k_RecoilMid
	                                       : animations::k_RecoilLow;
	auto damage = base * DamageFactor(blow);
	if (blow.blocked)
	{
		damage *= k_BlockedShare;
	}
	return {.band = band, .damage = damage, .recoil = plain + RecoilOffset(direction), .plainRecoil = plain};
}

RecoilDirection creature_fight::RecoilDirectionOf(const glm::vec3& swing)
{
	if (-swing.z >= std::sqrt((swing.x * swing.x) + (swing.y * swing.y)))
	{
		return RecoilDirection::Plain;
	}
	const auto upright = std::abs(swing.y);
	if (swing.x >= upright)
	{
		return RecoilDirection::Left;
	}
	if (-swing.x >= upright)
	{
		return RecoilDirection::Right;
	}
	return std::abs(swing.x) > swing.y ? RecoilDirection::Top : RecoilDirection::Bottom;
}

BlowShape creature_fight::ShapeOfBlow(const glm::vec3& boneAtLand, const glm::vec3& boneBefore, float landMs, float durationMs,
                                      float rootTravel)
{
	const auto travel = durationMs > 0.0f ? landMs * rootTravel / durationMs : 0.0f;
	return {
	    .point = glm::vec3(0.0f, boneAtLand.y, boneAtLand.z + travel),
	    .swing = (boneAtLand - boneBefore) * k_SwingPerSecond,
	};
}

float creature_fight::HeightShareOf(float blowHeight, float headHeight)
{
	return headHeight > 0.0f ? blowHeight / headHeight : 1.0f;
}

int32_t creature_fight::TestSpanMs(float turnMs, float size, bool realTime)
{
	if (realTime)
	{
		return static_cast<int32_t>(turnMs);
	}
	return static_cast<int32_t>(turnMs * (1.6f - (size * 0.5f * 0.85f)));
}

int32_t creature_fight::TestWindowMs(int32_t spanMs)
{
	return static_cast<int32_t>(static_cast<float>(spanMs) * 0.6f);
}

std::vector<int32_t> creature_fight::BlowTestTimes(int32_t fromMs, int32_t spanMs, int32_t landMs)
{
	const auto step = spanMs / k_BlowTests;
	const auto window = TestWindowMs(spanMs);
	std::vector<int32_t> times;
	auto time = fromMs;
	for (int32_t i = 0; i < k_BlowTests; ++i)
	{
		time += step;
		if (time >= landMs - window && time <= landMs + window)
		{
			times.push_back(time);
		}
	}
	return times;
}

std::array<glm::vec3, 5> creature_fight::BlowTestPoints(const glm::vec3& bone, const glm::vec3& boneBefore, float reach)
{
	const auto swing = bone - boneBefore;
	// Across the swing on the level, then across both
	const glm::vec3 across(swing.z, 0.0f, -swing.x);
	const glm::vec3 over(swing.x * swing.y, -((swing.x * swing.x) + (swing.z * swing.z)), swing.y * swing.z);
	std::array<glm::vec3, 5> points {swing, across, over, -across, -over};
	for (auto& point : points)
	{
		if (point != glm::vec3(0.0f))
		{
			point = x87::Scaled<x87::SumOrder::Zyx>(point, reach);
		}
		point += bone;
	}
	return points;
}

float creature_fight::BlowTestReach(float attackerSize, bool special)
{
	return (special ? 2.0f : 1.0f) * attackerSize * 0.2f * 15.0f;
}

float creature_fight::SwingPuffSize(float attackerSize, bool special)
{
	const auto size = (special ? 2.0f : 1.0f) * attackerSize;
	return size + size;
}

float creature_fight::BloodSpreadStep(const glm::vec3& blowVector)
{
	return glm::length(blowVector) * k_BloodSpreadShare;
}

glm::vec3 creature_fight::SprayVelocity(const glm::vec3& vector, float step, const glm::ivec3& draws)
{
	return (glm::vec3(draws - 25) * step) + vector;
}

std::optional<Step> creature_fight::StepOf(size_t animation)
{
	switch (animation)
	{
	case animations::k_StepForward:
		return Step::Forward;
	case animations::k_StepBack:
		return Step::Back;
	case animations::k_StepRight:
		return Step::Right;
	case animations::k_StepLeft:
		return Step::Left;
	default:
		return std::nullopt;
	}
}

size_t creature_fight::WobbleAnimation(Band band, bool sideways)
{
	if (band == Band::High)
	{
		return sideways ? animations::k_WobbleHighSide : animations::k_WobbleHighFrontBack;
	}
	return sideways ? animations::k_WobbleLowSide : animations::k_WobbleLowFrontBack;
}

uint8_t creature_fight::WoundKind(int32_t blowWoundType, const Random& random)
{
	if (random(2) == 0)
	{
		return 0;
	}
	switch (blowWoundType)
	{
	case 0:
		return 1;
	case 1:
		return static_cast<uint8_t>(3 + random(3));
	case 2:
		return 2;
	default:
		return 0;
	}
}

int32_t creature_fight::BlowWoundType(size_t animation, const BlowWoundTypes& types)
{
	if (!IsAttack(animation))
	{
		return -1;
	}
	return types.at(animation - animations::k_FirstAttack);
}

bool creature_fight::Bleeds(uint8_t woundKind)
{
	return woundKind == 2 || woundKind == 3 || woundKind == 5;
}

float creature_fight::PlaybackSpeed(size_t animation, float actionSpeed)
{
	return k_FightSpeed * (IsStep(animation) ? k_StepSpeedShare : k_OtherSpeedShare) * actionSpeed;
}

int32_t creature_fight::TurnAdvance(float speed, int32_t turnMs)
{
	return static_cast<int32_t>(x87::Mul(speed, static_cast<double>(turnMs)));
}

glm::vec2 creature_fight::TurnRootMove(glm::vec2 travel, float speed, int32_t turnMs, float scale, int32_t durationMs,
                                       float heading)
{
	if (durationMs <= 0)
	{
		return glm::vec2(0.0f);
	}
	const auto share = x87::Div(x87::Mul(x87::Mul(speed, static_cast<double>(turnMs)), scale), static_cast<double>(durationMs));
	const auto across = x87::Mul(travel.x, share);
	const auto along = x87::Mul(travel.y, share);
	// The direction is kept as floats
	const auto sine = x87::RoundToFloat(x87::Sin(heading));
	const auto cosine = x87::RoundToFloat(x87::Cos(heading));
	return {x87::Add(x87::Mul(-sine, along), x87::Mul(across, cosine)),
	        x87::Add(x87::Mul(along, cosine), x87::Mul(across, sine))};
}

std::string_view creature_fight::Name(State state)
{
	switch (state)
	{
	case State::Idle:
		return "idle";
	case State::Start:
		return "starting";
	case State::Stance:
		return "stance";
	case State::Action:
		return "acting";
	case State::Finish:
		return "finishing";
	case State::BlockStart:
		return "starting to block";
	case State::Block:
		return "blocking";
	case State::BlockRecoil:
		return "reeling in a block";
	case State::BlockEnd:
		return "ending a block";
	case State::Faint:
		return "fainting";
	case State::GetUp:
		return "getting up";
	case State::CastStart:
		return "starting to cast";
	case State::Cast:
		return "casting";
	case State::CastEnd:
		return "ending a cast";
	case State::Lying:
		return "lying";
	}
	return {};
}

size_t creature_fight::AnimationOf(State state)
{
	switch (state)
	{
	case State::Start:
		return animations::k_Start;
	case State::Finish:
		return animations::k_Finish;
	case State::BlockStart:
		return animations::k_StartBlock;
	case State::Block:
		return animations::k_Block;
	case State::BlockRecoil:
		return animations::k_RecoilBlock;
	case State::BlockEnd:
		return animations::k_EndBlock;
	case State::Faint:
	case State::Lying:
		return animations::k_Faint;
	case State::GetUp:
		return animations::k_GetUp;
	case State::CastStart:
		return animations::k_StartCast;
	case State::Cast:
		return animations::k_Cast;
	case State::CastEnd:
		return animations::k_EndCast;
	case State::Idle:
	case State::Stance:
	case State::Action:
		break;
	}
	return animations::k_Stance;
}

bool creature_fight::Loops(State state)
{
	return state == State::Stance || state == State::Block || state == State::Lying;
}

State creature_fight::AfterAnimation(State state, bool hasOpponent)
{
	switch (state)
	{
	case State::Idle:
		return State::Idle;
	case State::Start:
	case State::Stance:
		return hasOpponent ? State::Stance : State::Finish;
	case State::Action:
	case State::BlockEnd:
	case State::CastEnd:
		return State::Stance;
	case State::Finish:
	case State::GetUp:
		return State::Idle;
	case State::BlockStart:
	case State::BlockRecoil:
		return State::Block;
	case State::Block:
		return hasOpponent ? State::Block : State::BlockEnd;
	case State::Faint:
	case State::Lying:
		return State::Lying;
	case State::CastStart:
		return State::Cast;
	case State::Cast:
		return State::CastEnd;
	}
	return State::Idle;
}

std::optional<State> creature_fight::StopTransition(State state, bool animationDone)
{
	switch (state)
	{
	case State::Stance:
		return State::Finish;
	case State::Block:
		return State::BlockEnd;
	case State::Cast:
		return State::CastEnd;
	case State::Start:
	case State::Action:
		return animationDone ? std::optional(State::Finish) : std::nullopt;
	case State::BlockStart:
	case State::BlockRecoil:
		return animationDone ? std::optional(State::BlockEnd) : std::nullopt;
	case State::CastStart:
		return animationDone ? std::optional(State::CastEnd) : std::nullopt;
	default:
		return std::nullopt;
	}
}

bool creature_fight::StateDone(State state, float animationMs, float durationMs, float stateMs)
{
	if (state == State::Cast)
	{
		return stateMs > k_CastHoldMs;
	}
	return durationMs <= 0.0f || animationMs >= durationMs;
}

bool creature_fight::ActionEnds(float animationMs, int32_t advanceMs, float durationMs)
{
	return durationMs <= 0.0f || static_cast<int32_t>(animationMs) + advanceMs >= static_cast<int32_t>(durationMs);
}

bool creature_fight::Hurries(State state)
{
	switch (state)
	{
	case State::Start:
	case State::Stance:
	case State::Action:
	case State::Finish:
		return true;
	default:
		return false;
	}
}

bool creature_fight::EndsByOwnTest(State state)
{
	switch (state)
	{
	case State::Start:
	case State::Finish:
	case State::BlockStart:
	case State::BlockEnd:
	case State::CastStart:
	case State::Cast:
	case State::CastEnd:
		return true;
	default:
		return false;
	}
}

int32_t creature_fight::StateAdvanceMs(State state, float speed, int32_t turnMs, int32_t spanMs)
{
	return state == State::Start ? spanMs : TurnAdvance(speed, turnMs);
}

int32_t creature_fight::EndTestMs(State state, float speed, int32_t turnMs, int32_t spanMs)
{
	return state == State::Start || state == State::Finish ? spanMs : TurnAdvance(speed, turnMs);
}

bool creature_fight::OwnTestEnds(State state, float animationMs, int32_t testMs, float durationMs, float stateMs,
                                 int32_t turnMs)
{
	if (state == State::Cast)
	{
		return static_cast<int32_t>(stateMs) + turnMs > k_CastHoldMs;
	}
	return durationMs <= 0.0f || static_cast<int32_t>(animationMs) + testMs >= static_cast<int32_t>(durationMs);
}

bool creature_fight::IsBlocking(State state, float timeMs, float startBlockDurationMs)
{
	const auto half = startBlockDurationMs * 0.5f;
	switch (state)
	{
	case State::Block:
		return true;
	case State::BlockStart:
		return timeMs > half;
	case State::BlockEnd:
		return timeMs < half;
	default:
		return false;
	}
}

bool creature_fight::TakesMoves(State state)
{
	return state == State::Stance || state == State::Block;
}

bool creature_fight::FacesOpponentEachTurn(State state, size_t animation, bool turning)
{
	switch (state)
	{
	case State::Start:
		return !turning;
	case State::Stance:
	case State::Block:
		return true;
	case State::Action:
		return !IsAttack(animation);
	default:
		return false;
	}
}

void creature_fight::Enter(Fighter& fighter, State state, std::optional<size_t> animation, float speed)
{
	fighter.state = state;
	fighter.animation = animation.value_or(AnimationOf(state));
	fighter.timeMs = 0.0f;
	fighter.stateMs = 0.0f;
	fighter.speed = speed;
	fighter.turning = false;
	fighter.special = state == State::Action && IsSpecialAttack(fighter.animation);
	// The last action counts every move that the fighter starts: a blow, a step, a reel, a block or a cast. A block
	// counts as its hold and a cast as its loop.
	if (state == State::Action || state == State::BlockRecoil)
	{
		fighter.lastAction = fighter.animation;
	}
	else if (state == State::BlockStart)
	{
		fighter.lastAction = animations::k_Block;
	}
	else if (state == State::CastStart)
	{
		fighter.lastAction = animations::k_Cast;
	}
}

bool creature_fight::PlayerMove(Fighter& fighter, const Move& move, bool replace)
{
	if (fighter.control == Control::Computer)
	{
		fighter.control = Control::Player;
	}
	fighter.computerWaitMs = k_ComputerWaitsMs;
	return fighter.queue.Push(move, replace);
}

bool creature_fight::CountComputerWait(Fighter& fighter, int32_t turnMs)
{
	if (fighter.control != Control::Player)
	{
		return false;
	}
	const auto state = fighter.state;
	if (state != State::Stance && state != State::Action && state != State::Block && state != State::BlockRecoil)
	{
		return false;
	}
	fighter.computerWaitMs -= static_cast<float>(turnMs);
	if ((state == State::Stance || state == State::Block) && fighter.computerWaitMs < 0.0f)
	{
		fighter.control = Control::Computer;
		return true;
	}
	return false;
}

bool creature_fight::PlayerMove(Fighter& fighter, marks::MarkList& marks, const Move& move, bool replace)
{
	// A move made in place of the queue keeps only its own sparkle
	if (replace)
	{
		marks.KeepNewest();
	}
	return PlayerMove(fighter, move, replace);
}

std::optional<Order> creature_fight::TakeOrder(Fighter& fighter, marks::MarkList& marks, bool inRange)
{
	const auto queued = fighter.queue.Size();
	auto order = TakeOrder(fighter, inRange);
	if (fighter.queue.Size() < queued)
	{
		marks.PopFront();
	}
	return order;
}

std::optional<Order> creature_fight::TakeOrder(Fighter& fighter, bool inRange)
{
	const auto front = fighter.queue.Front();
	if (!inRange || !front.has_value())
	{
		return std::nullopt;
	}
	// Anything at all ends a block, and is made from the stance after it; another block is used up ending it
	if (fighter.state == State::Block)
	{
		fighter.computerWaitMs = k_ComputerWaitsMs;
		if (front->move.kind == Move::Kind::Block)
		{
			fighter.queue.Pop();
		}
		return Order {.kind = Order::Kind::EndBlock};
	}
	if (fighter.state != State::Stance)
	{
		return std::nullopt;
	}
	fighter.computerWaitMs = k_ComputerWaitsMs;
	const auto& move = front->move;
	std::optional<Order> order;
	switch (move.kind)
	{
	case Move::Kind::Animation:
		order = Order {.kind = Order::Kind::Animation, .value = move.value};
		break;
	case Move::Kind::Spell:
		order = Order {.kind = Order::Kind::Cast, .value = move.value};
		break;
	case Move::Kind::Block:
		order = Order {.kind = Order::Kind::Block};
		break;
	case Move::Kind::Special:
		order = Order {.kind = Order::Kind::Special, .speed = k_SpecialSpeed};
		break;
	case Move::Kind::High:
	case Move::Kind::Mid:
	case Move::Kind::Low:
		// A blow waits until it is let go
		if (!front->chargeMs.has_value())
		{
			return std::nullopt;
		}
		order = Order {.kind = Order::Kind::Blow,
		               .band = move.kind == Move::Kind::High  ? Band::High
		                       : move.kind == Move::Kind::Mid ? Band::Mid
		                                                      : Band::Low,
		               .speed = BlowSpeed(*front->chargeMs)};
		break;
	}
	fighter.queue.Pop();
	return order;
}

Opponent creature_fight::OpponentOf(State state, size_t animation)
{
	switch (state)
	{
	case State::Stance:
		return Opponent::Stance;
	case State::CastEnd:
		return Opponent::EndingCast;
	case State::BlockRecoil:
		return Opponent::BlockRecoil;
	// The computer strikes at once at an opponent that starts a cast or casts
	case State::CastStart:
	case State::Cast:
		return Opponent::OtherAction;
	case State::Action:
		if (IsAttack(animation))
		{
			return Opponent::Attacking;
		}
		return animation == animations::k_StepForward ? Opponent::SteppingForward : Opponent::OtherAction;
	case State::BlockStart:
	case State::Block:
	case State::BlockEnd:
		return Opponent::Blocking;
	default:
		return Opponent::Other;
	}
}

uint32_t creature_fight::TierOf(float tendency)
{
	// The middle tier was meant for tendencies from -0.33, but the game never gives it
	return tendency >= 0.33f ? 2 : 0;
}

Choice creature_fight::ChooseMove(uint32_t tier, Opponent opponent, const Random& random)
{
	const auto blow = [&random] { return Choice {.kind = Choice::Kind::Blow, .band = static_cast<Band>(random(3))}; };
	const auto step = [&random] {
		constexpr std::array k_Steps {Step::Left, Step::Right, Step::Back};
		return Choice {.kind = Choice::Kind::Step, .step = k_Steps.at(random(3))};
	};
	const auto block = Choice {.kind = Choice::Kind::Block};
	// Only the most aggressive tier draws this lot, and only against the moves that it can strike back at
	const auto counters = [tier, &random] { return tier >= 2 && random(k_CounterLots) != 0; };
	switch (opponent)
	{
	case Opponent::Stance:
	case Opponent::EndingCast:
		return random(k_AttackChances.at(std::min<size_t>(tier, 2))) == 0 ? blow() : Choice {};
	case Opponent::BlockRecoil:
	case Opponent::OtherAction:
		return blow();
	case Opponent::Attacking:
		if (counters())
		{
			return blow();
		}
		return random(4) == 0 ? block : step();
	case Opponent::SteppingForward:
	{
		if (counters())
		{
			return blow();
		}
		const auto pick = random(4);
		if (pick == 0)
		{
			return block;
		}
		return pick == 3 ? blow() : step();
	}
	case Opponent::Blocking:
		if (counters())
		{
			return blow();
		}
		if (random(10) == 1)
		{
			return random(2) == 0 ? step() : blow();
		}
		return {};
	case Opponent::Other:
		break;
	}
	return {};
}

bool creature_fight::ComputerEndsBlock(const Random& random)
{
	return random(k_EndBlockLots) == 1;
}

std::optional<float> creature_fight::DepthToBody(std::span<const physics::Ellipsoid> bones, const glm::vec3& point,
                                                 const glm::vec3& towards, float opponentSize)
{
	const float width = x87::Mul(opponentSize * k_DepthWidthPerSize, k_DepthWidthShare);
	const auto spacing = (width + width) * k_DepthSpacingShare;
	// The row goes across the direction, flat on the land. The 3D engine's inverse square root makes it a unit vector
	const auto across = glm::vec3(towards.z, 0.0f, -towards.x);
	const float acrossSquared = glm::dot(across, across);
	const auto unitAcross = acrossSquared > 0.0f ? across * gutils::ModelInverseSquareRoot(acrossSquared) : glm::vec3(0.0f);
	auto origin = point - (unitAcross * width);
	const auto next = unitAcross * spacing;
	std::optional<float> depth;
	for (int i = 0; i < k_DepthLines; ++i)
	{
		if (const auto hit = physics::EllipsoidHit(bones, origin, towards); hit.has_value())
		{
			depth = std::min(depth.value_or(k_NoDepth), x87::DotZyx(hit->first.point - point, towards));
		}
		origin += next;
	}
	return depth;
}

bool creature_fight::WithinBlowLine(const glm::vec3& point, const glm::vec3& origin, const glm::vec3& direction)
{
	const auto length = x87::LengthZyx(direction);
	if (length <= 0.0f)
	{
		return false;
	}
	// The game multiplies the direction by one over its length
	const auto ahead = x87::DotZyx(point - origin, direction * (1.0f / length));
	return ahead > 0.0f && ahead < length;
}

std::pair<float, float> creature_fight::BandLimits(Band band, float headHeight)
{
	switch (band)
	{
	case Band::High:
		return {headHeight * k_HighAbove, headHeight};
	case Band::Mid:
		return {headHeight * k_LowBelow, headHeight * k_HighAbove};
	case Band::Low:
		break;
	}
	return {0.0f, headHeight * k_LowBelow};
}

AttackChoice creature_fight::ChooseAttack(std::span<const BlowDepth> blows, Band band, float headHeight, StepLengths steps,
                                          float attackerSize)
{
	const auto [lowest, highest] = BandLimits(band, headHeight);
	const auto deepest = -k_DepthPerSize * attackerSize;
	std::optional<size_t> bestBlow;
	int bestSteps = 0;
	int bestScore = 0;
	float bestDepth = -k_NoDepth;
	for (int count = -k_MaxSteps; count <= k_MaxSteps; ++count)
	{
		float moved = 0.0f;
		if (count < 0)
		{
			moved = static_cast<float>(count) * steps.back;
		}
		else if (count > 0)
		{
			moved = static_cast<float>(count) * steps.forward;
		}
		for (const auto& blow : blows)
		{
			if (blow.animation == animations::k_AttackSpecial || !blow.depth.has_value() || !(blow.height <= headHeight))
			{
				continue;
			}
			const auto depth = *blow.depth - moved;
			if (!(depth < 0.0f) || !(depth > deepest))
			{
				continue;
			}
			const bool inBand = !(highest < blow.height || blow.height < lowest);
			const auto score = inBand ? std::abs(count) : k_OutOfBandScore;
			if (bestBlow.has_value() && score == bestScore)
			{
				// The same score: the blow that goes in less wins, with the steps found first
				if (depth > bestDepth)
				{
					bestBlow = blow.animation;
					bestDepth = depth;
				}
			}
			else if (!bestBlow.has_value() || score < bestScore)
			{
				bestBlow = blow.animation;
				bestSteps = count;
				bestScore = score;
				bestDepth = depth;
			}
		}
	}
	if (!bestBlow.has_value())
	{
		return {};
	}
	if (bestSteps != 0)
	{
		return {.step = bestSteps > 0 ? Step::Forward : Step::Back};
	}
	return {.animation = bestBlow};
}

bool creature_fight::WinnerPoos(float alignment)
{
	return alignment < k_AlignmentPooBelow;
}

bool creature_fight::FightWonIsSeen(bool byBlow, bool winnerHasPlayer)
{
	return byBlow && winnerHasPlayer;
}

bool creature_fight::WinsByOpponentsLife(float opponentLife, bool scriptControlled)
{
	return !(0.0f < opponentLife) && !scriptControlled;
}

bool creature_fight::WinPlanForced(bool byBlow)
{
	return !byBlow;
}

size_t creature_fight::Response(float life, float otherLife)
{
	return otherLife < life ? animations::k_Happy : animations::k_Sad;
}

bool creature_fight::FearsAfterFight(float life, float otherLife)
{
	return !(otherLife < life);
}

size_t creature_fight::FaceOffAnimation(bool rude)
{
	return rude ? animations::k_Rude : animations::k_Taunt;
}

namespace
{
/// The name at the place of an enum value in a list, or "unknown" past its end
template <typename E, size_t N>
std::string_view NameIn(const std::array<std::string_view, N>& names, E value)
{
	const auto index = static_cast<size_t>(value);
	return index < names.size() ? names.at(index) : std::string_view("unknown");
}
} // namespace

std::string_view creature_fight::Name(Band band)
{
	static constexpr std::array<std::string_view, 3> k_Names {"high", "middle", "low"};
	return NameIn(k_Names, band);
}

std::string_view creature_fight::Name(Step step)
{
	static constexpr std::array<std::string_view, 4> k_Names {"forward", "back", "right", "left"};
	return NameIn(k_Names, step);
}

std::string_view creature_fight::Name(Move::Kind kind)
{
	static constexpr std::array<std::string_view, 7> k_Names {"high",    "middle",    "low",  "block",
	                                                          "special", "animation", "spell"};
	return NameIn(k_Names, kind);
}

std::string_view creature_fight::Name(FightAction action)
{
	static constexpr std::array<std::string_view, 11> k_Names {"stance",      "block",   "step",         "blow",
	                                                           "casting",     "special", "reeling_high", "reeling_middle",
	                                                           "reeling_low", "fainted", "other"};
	return NameIn(k_Names, action);
}

std::string_view creature_fight::Name(Control control)
{
	static constexpr std::array<std::string_view, 3> k_Names {"none", "player", "computer"};
	return NameIn(k_Names, control);
}

std::string_view creature_fight::Name(Order::Kind kind)
{
	static constexpr std::array<std::string_view, 6> k_Names {"end_block", "animation", "cast", "block", "blow", "special"};
	return NameIn(k_Names, kind);
}

std::string_view creature_fight::Name(Opponent opponent)
{
	static constexpr std::array<std::string_view, 9> k_Names {"stance",       "ending_cast", "block_recoil",
	                                                          "attacking",    "casting",     "stepping_forward",
	                                                          "other_action", "blocking",    "other"};
	return NameIn(k_Names, opponent);
}

std::string_view creature_fight::Name(Choice::Kind kind)
{
	static constexpr std::array<std::string_view, 4> k_Names {"none", "blow", "block", "step"};
	return NameIn(k_Names, kind);
}

std::string_view creature_fight::Name(RecoilDirection direction)
{
	static constexpr std::array<std::string_view, 5> k_Names {"plain", "right", "left", "top", "bottom"};
	return NameIn(k_Names, direction);
}

uint32_t creature_fight::HelpEventOf(Move::Kind kind)
{
	switch (kind)
	{
	case Move::Kind::Block:
		return help::profile::k_FightBlock;
	case Move::Kind::Spell:
		return help::profile::k_FightCast;
	case Move::Kind::Animation:
		return help::profile::k_FightStep;
	case Move::Kind::High:
	case Move::Kind::Mid:
	case Move::Kind::Low:
	case Move::Kind::Special:
		break;
	}
	return help::profile::k_FightAttack;
}

bool creature_fight::CountsHelpEvent(State state, bool inRange, bool queued)
{
	return state == State::Stance && inRange && queued;
}

bool creature_fight::FightMusicHeard(bool directsFight, std::span<const glm::vec3> arenasOfItsFights, const glm::vec3& camera)
{
	return directsFight || std::ranges::any_of(arenasOfItsFights, [&camera](const glm::vec3& middle) {
		       return x87::LengthZyx(middle - camera) < k_FightMusicDistance;
	       });
}

float creature_fight::HealthAfterLifeLost(float health, float damage)
{
	return std::max(health - damage, 0.0f);
}
