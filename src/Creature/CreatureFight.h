/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <cstddef>
#include <cstdint>

#include <array>
#include <functional>
#include <optional>
#include <span>
#include <string_view>
#include <utility>
#include <vector>

#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

#include "FightMarks.h"
#include "Physics/Body.h"

/// Creature fights: two creatures duel inside a circular arena. Each plays a small set of fight states (starting,
/// standing in its stance, acting, blocking, casting, reeling, fainting) fed by a queue of up to twelve moves. The
/// player fills the queue by clicking the opponent's body high, in the middle or low, their own creature to block, or
/// the ground to step, holding the click to charge a blow. Left alone, the creature fights by itself, more or less
/// aggressively by what it has learnt of fighting. Each blow takes from a fight health of its own, which starts at
/// (life + 1) / 2; at nothing the creature faints, and a quarter of what it lost comes off its real life afterwards.
/// Creatures never die of it: the loser lies out cold a while, is taken home, rests and gets up again.
namespace openblack::creature_fight
{
/// The places of the fight animations in the creature spec file
namespace animations
{
constexpr size_t k_Faint = 106;
constexpr size_t k_GetUp = 107;
/// Played while a charged blow waits to be let go
constexpr size_t k_PowerUp = 108;
constexpr size_t k_StartCast = 110;
constexpr size_t k_Cast = 111;
constexpr size_t k_EndCast = 112;
constexpr size_t k_Start = 122;
constexpr size_t k_Stance = 123;
constexpr size_t k_Finish = 124;
/// The blows: high, eye, belly, pelvis, knee and special, then a second of each
constexpr size_t k_FirstAttack = 125;
constexpr size_t k_AttackCount = 12;
constexpr size_t k_AttackHigh = 125;
constexpr size_t k_AttackEye = 126;
constexpr size_t k_AttackBelly = 127;
constexpr size_t k_AttackPelvis = 128;
constexpr size_t k_AttackKnee = 129;
constexpr size_t k_AttackSpecial = 130;
constexpr size_t k_AttackSpecial2 = 136;
constexpr size_t k_StepForward = 137;
constexpr size_t k_StepBack = 138;
constexpr size_t k_StepRight = 139;
constexpr size_t k_StepLeft = 140;
constexpr size_t k_StartBlock = 142;
constexpr size_t k_Block = 143;
constexpr size_t k_EndBlock = 144;
/// Reeling from a blow high, in the middle and low, each followed by its right, left, top and bottom versions
constexpr size_t k_RecoilHigh = 179;
constexpr size_t k_RecoilMid = 184;
constexpr size_t k_RecoilLow = 189;
/// The wobbles a blow sends through the body, high and low, side to side and front to back, played on top of it
constexpr size_t k_WobbleHighSide = 194;
constexpr size_t k_WobbleHighFrontBack = 195;
constexpr size_t k_WobbleLowSide = 196;
constexpr size_t k_WobbleLowFrontBack = 197;
/// Reeling back while blocking
constexpr size_t k_RecoilBlock = 198;
/// The actions played around a fight: the taunt or the rude gesture before it, and being happy, sad or showing off after
/// it
constexpr size_t k_Taunt = 70;
constexpr size_t k_Rude = 222;
constexpr size_t k_Happy = 55;
constexpr size_t k_Sad = 56;
constexpr size_t k_Impress = 65;
} // namespace animations

[[nodiscard]] bool IsAttack(size_t animation);
[[nodiscard]] bool IsSpecialAttack(size_t animation);
[[nodiscard]] bool IsStep(size_t animation);

/// random(n) is a whole number from 0 to n - 1
using Random = std::function<uint32_t(uint32_t)>;

// The arena

/// The arena's radius is 2.6 times the bigger creature's height, 15 units for each of its size, at most 60
constexpr float k_ArenaHeightPerSize = 15.0f;
constexpr float k_ArenaRadiusPerHeight = 2.6f;
constexpr float k_MaxArenaRadius = 60.0f;
/// The two creatures take their places this many units of their size either side of its middle
constexpr float k_SpotPerSize = 15.0f;
/// Within this many units of their size of their place counts as arrived
constexpr float k_ArrivalPerSize = 1.5f * 15.0f;
/// Moves are only made, and creatures only step back or sideways, within this share of the radius
constexpr float k_RangeShare = 0.8f;

struct Arena
{
	glm::vec2 centre {0.0f};
	float radius {0.0f};
};
[[nodiscard]] float ArenaRadius(float sizeA, float sizeB);
/// Where a creature takes its place: the one that made the arena to the east of the middle, the other to the west
[[nodiscard]] glm::vec2 ArenaSpot(const Arena& arena, float size, bool madeIt);
[[nodiscard]] float ArrivalDistance(float size);
/// Whether a point is within the range moves are made in
[[nodiscard]] bool WithinRange(const Arena& arena, glm::vec2 point);
/// Fighters keep at least half their own radius and all of their opponent's apart
[[nodiscard]] float ClosestApart(float radius, float opponentRadius);
/// Where a fighter moving from one point to another ends up without coming closer to its opponent than a distance:
/// moving in that far, it stops on the circle round the opponent
[[nodiscard]] glm::vec2 KeepApart(glm::vec2 from, glm::vec2 to, glm::vec2 opponent, float closest);
/// Fighters only make moves facing their opponents to within this many radians, turning to them first
constexpr float k_FacingTolerance = 0.35f;
/// Where a creature that reels back in its block goes in one frame: to the new point only when that point is inside the
/// range, else it stays. Blows, steps and the reel from a blow move it with no limit.
[[nodiscard]] glm::vec2 BlockRecoilTo(const Arena& arena, glm::vec2 from, glm::vec2 to);
/// Whether a point is not inside the range. When either fighter is not inside it, the fighters stop their moves and
/// step back to the middle.
[[nodiscard]] bool OutsideRange(const Arena& arena, glm::vec2 point);
/// How far the body of a fighter reaches in front of its feet, from the points of its posed bones relative to its feet
/// and the direction that it faces. It is never less than zero.
[[nodiscard]] float ForwardExtent(std::span<const glm::vec2> bones, glm::vec2 forward);
/// Where two fighters go when their bodies overlap. Each one moves by half the overlap, along the direction that the
/// first one faces: the first one moves back, the other one moves away. A fighter only moves to a point inside the range.
struct PullApart
{
	std::optional<glm::vec2> first;
	std::optional<glm::vec2> second;
};
/// The two fighters stand at two points. The extents are the sum of how far their two bodies reach in front of them.
[[nodiscard]] PullApart PullApartOf(const Arena& arena, glm::vec2 first, glm::vec2 second, glm::vec2 forward, float extents);

// Bands of the body

/// High, middle and low on the body, by shares of its height
enum class Band : uint8_t
{
	High,
	Mid,
	Low,
};
constexpr float k_LowBelow = 0.33f;
constexpr float k_HighAbove = 0.66f;
/// The band a height up the body falls in, as a share of the body's height
[[nodiscard]] Band BandOf(float shareOfHeight);

// Moves

enum class Step : uint8_t
{
	Forward,
	Back,
	Right,
	Left,
};
[[nodiscard]] size_t StepAnimation(Step step);
/// The step towards a point, given in the creature's frame (x to its right, y ahead): whichever axis is longer picks
/// stepping forward or back, or right or left
[[nodiscard]] Step StepTowards(glm::vec2 local);
/// The step that a fighter in its stance takes back to the middle of the arena, when it is farther from the middle than
/// its body radius. The middle is given in the frame of the creature's model (x to the side of its model, y ahead), and
/// the distance is in three dimensions. A step forward or back needs the middle 1.5 times farther ahead than to the side.
[[nodiscard]] std::optional<Step> StepToMiddle(glm::vec2 local, float distance, float bodyRadius);
constexpr float k_StepToMiddleAheadShare = 1.5f;
/// Whether a creature at a point may take a step: forward always, back or sideways only within range
[[nodiscard]] bool CanStep(const Arena& arena, glm::vec2 position, Step step);

// The player's controls

/// The two buttons of the hand. The Move button makes its move at once, throwing away the moves queued; the Action
/// button adds its move to the end of the queue. Both charge a blow for as long as they are held.
enum class Button : uint8_t
{
	Move,
	Action,
};
/// Whether a press makes its move in place of the queue
[[nodiscard]] bool ReplacesQueue(Button button);
/// A press on the ground only directs the fight within the arena
[[nodiscard]] bool GroundPressCounts(const Arena& arena, glm::vec2 point);
/// The hand directs the fight of the player's creature only when all of these are true:
/// - the creature duels,
/// - the camera shows a fight,
/// - the hand holds no thing and no miracle.
[[nodiscard]] bool DirectsFight(bool playersCreatureDuels, bool cameraShowsFight, bool handEmpty);
/// Whether a point on the land is in the arena. A double click on such a point makes the camera show the fight.
[[nodiscard]] bool InArena(const Arena& arena, glm::vec2 point);
/// What the hand offers over a fight: blocking over the player's own creature, attacking over its opponent, and
/// manoeuvring anywhere else
enum class Tip : uint8_t
{
	Block,
	Attack,
	Manoeuvre,
};
[[nodiscard]] Tip TipOver(bool overOwnCreature, bool overOpponent);
/// The tooltip shown for each, by its place among the game's tooltips
[[nodiscard]] uint32_t ToolTipIndex(Tip tip);

struct Move
{
	enum class Kind : uint8_t
	{
		/// Blows at a band of the opponent's body
		High,
		Mid,
		Low,
		Block,
		Special,
		/// Playing an animation, as stepping is
		Animation,
		Spell,
	};
	Kind kind {Kind::Mid};
	/// The animation played, or the spell cast
	uint32_t value {0};

	bool operator==(const Move&) const = default;
};
[[nodiscard]] Move AttackMove(Band band);
[[nodiscard]] Move StepMove(Step step);
[[nodiscard]] Move BlockMove();
[[nodiscard]] bool IsBlow(Move::Kind kind);

/// A move as scripts number them: 0, 1 and 2 blows high, in the middle and low, 3 a block, 4 the special move; with the
/// 0x8000 bit an animation played (a step), with the 0x4000 bit a miracle cast, each by the number in the low 14 bits.
/// None for any other number, which the game would try as a blow of that number.
[[nodiscard]] std::optional<Move> ScriptMove(uint32_t value);
constexpr uint32_t k_ScriptAnimationBit = 0x8000;
constexpr uint32_t k_ScriptSpellBit = 0x4000;

/// What a fighter is doing, as scripts ask it, by the animation playing
enum class FightAction : uint8_t
{
	/// Starting, in its stance, or finishing
	Stance = 0,
	Block = 1,
	Step = 2,
	Blow = 3,
	Casting = 4,
	Special = 5,
	ReelingHigh = 6,
	ReelingMiddle = 7,
	ReelingLow = 8,
	Fainted = 9,
	Other = 10,
};
[[nodiscard]] FightAction FightActionOf(size_t animation);

/// A blow is charged for as long as the click is held, up to 1.2 seconds
constexpr float k_MaxChargeMs = 1200.0f;
/// What a move starts charged at: a blow waits to be let go (none), blocks and animations need no charge, and special
/// moves and spells are fully charged
[[nodiscard]] std::optional<float> InitialCharge(Move::Kind kind);
[[nodiscard]] float ReleasedCharge(float heldMs);
/// How fast a blow plays by its charge, 0.5 to 1.5
[[nodiscard]] float BlowSpeed(float chargeMs);
/// How long a press counts as held for its charge: the time it was held, but no more than a tenth of a second for each
/// game turn since it was pressed, and one more
constexpr float k_HeldMsPerTurn = 100.0f;
[[nodiscard]] float HeldMs(float realMs, uint32_t turnsSincePress);

/// The fight's computer opponent hits at this speed
constexpr float k_AiBlowSpeed = 0.5f;

struct QueuedMove
{
	Move move {};
	std::optional<float> chargeMs;
};

/// The moves a creature has been told to make, in turn, at most twelve
class MoveQueue
{
public:
	static constexpr size_t k_Capacity = 12;

	/// A move added at the back, charged as its kind starts, or in place of every other at the front. False when full.
	bool Push(const Move& move, bool replace);
	[[nodiscard]] std::optional<QueuedMove> Front() const;
	void Pop();
	void Clear();
	/// The move added last gets its charge, if it is a blow still waiting for it. False otherwise. A blow left waiting
	/// behind a later move never gets one, and waits until a hit takes it away.
	bool Release(float heldMs);
	/// Getting hit takes away a charged blow still waiting
	void CancelWaiting();
	[[nodiscard]] bool HasWaiting() const;
	[[nodiscard]] size_t Size() const { return _count; }
	[[nodiscard]] bool Empty() const { return _count == 0; }
	[[nodiscard]] std::span<const QueuedMove> Moves() const { return {_moves.data(), _count}; }
	/// The queue made these moves (as many as it holds) with their charges as given, a blow without a charge waiting
	/// for the button to be let go if it is the last
	void Assign(std::span<const QueuedMove> moves);

private:
	std::array<QueuedMove, k_Capacity> _moves {};
	size_t _count {0};
	/// The move added last is a blow waiting for the button to be let go
	bool _awaitingRelease {false};
};

/// How many of the queued moves are blows at a band (not blocks, steps, miracles or the special move)
[[nodiscard]] uint32_t QueuedBlows(std::span<const QueuedMove> moves);

// Learning to fight

/// How the creature leans in fights, from -1 (defensive) to 1 (aggressive). A move nudges it as the move is made,
/// whoever asked for it: a block or a step towards defence, a blow or the special move towards attack; a miracle leaves
/// it be. See LearnsFromMove for the creatures that learn.
[[nodiscard]] float LearnTendency(float tendency, Move::Kind kind);
/// Whether a move from the queue teaches a creature to lean: a step teaches every creature, the other moves teach only
/// the human player's creature. The moves that the computer chooses do not go into the queue, thus they teach nothing.
[[nodiscard]] bool LearnsFromMove(Move::Kind kind, bool humanPlayers);
/// A creature's first fight starts it leaning against its alignment: evil creatures attack, good ones defend
[[nodiscard]] float FirstTendency(float alignment);
/// The lean a creature has learnt, as it starts a fight that it asked for. Until it completes a fight (a knock-out or a
/// fight that a script controls does not count), each fight that it starts sets the lean against its alignment again.
[[nodiscard]] float TendencyAtStart(uint32_t fightsCompleted, float learnt, float alignment);

// Health, stamina and fainting

[[nodiscard]] float FightHealthAtStart(float life);
[[nodiscard]] float StaminaAtStart(float energy, float exhaustion);
/// Stamina comes back a little every game turn, up to 1. Nothing is known to use it up.
constexpr float k_StaminaPerTurn = 1.0f / 600.0f;
[[nodiscard]] float RegainStamina(float stamina);
/// A quarter of the fight health lost comes off the real life at the end
constexpr float k_LifeLostShare = 0.25f;
[[nodiscard]] float LifeAfterFight(float life, float fightHealth);
/// A creature this healthy or less won't start a fight
constexpr float k_MinLifeToFight = 0.1f;
[[nodiscard]] bool HealthyEnoughToFight(float life);

/// What a creature is to the fight that it is in, for its mind
struct MindInFight
{
	/// It lies out cold, or it is in a fight
	bool knockedOut {false};
	bool fighting {false};
	/// It started the fight, by a plan of its own
	bool startedIt {false};
	bool planned {false};
	/// Its agenda waits for the fight now
	bool waitsForFight {false};
	/// It won by a knock-out, and is made to do something to the loser
	bool won {false};
};
/// Whether the creature can choose a new plan. A creature that lies out cold cannot. In a fight, only the creature that
/// started the fight by a plan of its own can, while its agenda waits for the fight. A plan that it chooses ends the
/// fight. The other creature obeys, and a creature that the leash, a script or a knock-out makes do something cannot
/// choose either.
[[nodiscard]] bool LetsChoose(const MindInFight& mind);
/// Whether the creature obeys the creature that started its fight: its desires stand still until the fight is over, or
/// until it wins by a knock-out
[[nodiscard]] bool ObeysInFight(const MindInFight& mind);
/// A creature asked to fight agrees only to one between half and twice its own height, not while it lies dead, and only
/// when both or neither of them are under a script's control
constexpr float k_SmallestOpponentShare = 0.5f;
constexpr float k_BiggestOpponentShare = 2.0f;
[[nodiscard]] bool AgreesToFight(float height, float askerHeight, bool dead, bool scripted, bool askerScripted);
/// Knocked out, a creature lies 8 seconds and 4 more for each of its size
[[nodiscard]] float FaintSeconds(float size);
/// Taken home (fizzing out and back in, see creature_fizz), it waits this long before resting
constexpr float k_WaitAfterHomeSeconds = 3.0f;
/// After a faint, it rests to get better when its life is below this or it is more tired than this
constexpr float k_GetUpLife = 0.4f;
constexpr float k_RestedExhaustion = 0.3f;
[[nodiscard]] bool NeedsRest(float life, float exhaustion);

// Blows

constexpr float k_HighDamage = 0.05f;
constexpr float k_LowDamage = 0.03f;
/// A blocked blow does a tenth of the damage
constexpr float k_BlockedShare = 0.1f;
constexpr float k_MinDamageFactor = 0.04f;
constexpr float k_MaxDamageFactor = 2.0f;

/// One blow landing
struct Blow
{
	float attackerSize {1.0f};
	float defenderSize {1.0f};
	/// From 0 (weak) to 1 (strong)
	float attackerStrength {0.5f};
	float defenderStrength {0.5f};
	/// How fast the blow played, by its charge
	float speed {1.0f};
	bool special {false};
	/// Where it landed, as a share of the defender's height
	float heightShare {0.5f};
	bool blocked {false};
};
/// How hard a blow lands: the attacker's size and strength against the defender's, times its speed, kept from 0.04
/// to 2, and twice that for a special move
[[nodiscard]] float DamageFactor(const Blow& blow);

enum class RecoilDirection : uint8_t
{
	Plain,
	Right,
	Left,
	Top,
	Bottom,
};
struct Hit
{
	Band band;
	float damage;
	/// The reeling animation played by a body that isn't blocking, and its plain version for a species without the
	/// directed one
	size_t recoil;
	size_t plainRecoil;
};
[[nodiscard]] Hit ResolveBlow(const Blow& blow, RecoilDirection direction);
/// Which way a blow throws the body, by the swing of the attacker's striking bone as the blow lands, in the attacker's
/// mesh space (ahead is -z). A swing that goes more ahead than across and up or down throws the body straight back. A
/// swing that goes more across than up or down reels it: to -x gives the right reel, to +x the left reel. Else a swing
/// down gives the top reel and a swing up the bottom reel.
[[nodiscard]] RecoilDirection RecoilDirectionOf(const glm::vec3& swing);
/// The wobble played on top of the body after a blow: high or low, side to side or front to back
[[nodiscard]] size_t WobbleAnimation(Band band, bool sideways);
/// Which step an animation is, if it is one
[[nodiscard]] std::optional<Step> StepOf(size_t animation);
/// The kind of wound a blow leaves: half the time the first kind, else by the blow's own wound type. Type 0 gives kind
/// 1, type 1 one of kinds 3, 4 and 5, and type 2 kind 2. Any other type gives the first kind.
[[nodiscard]] uint8_t WoundKind(int32_t blowWoundType, const Random& random);
/// The wound type of each of the twelve blows, in the order of the blow animations, as the species' file gives them
using BlowWoundTypes = std::array<int32_t, animations::k_AttackCount>;
/// The wound type a blow leaves, from its species' wound types; -1 for an animation that isn't a blow
[[nodiscard]] int32_t BlowWoundType(size_t animation, const BlowWoundTypes& types);
/// Wounds of these kinds also bleed
[[nodiscard]] bool Bleeds(uint8_t woundKind);

// Playing the fight

/// The fight animations play at this speed, steps at 0.9 of it and everything else at 0.6, and an action's own speed
/// on top
constexpr float k_FightSpeed = 1.1f;
constexpr float k_StepSpeedShare = 0.9f;
constexpr float k_OtherSpeedShare = 0.6f;
[[nodiscard]] float PlaybackSpeed(size_t animation, float actionSpeed);

enum class State : uint8_t
{
	/// Not fighting
	Idle,
	Start,
	Stance,
	/// A blow, a step or reeling from a blow
	Action,
	Finish,
	BlockStart,
	Block,
	BlockRecoil,
	BlockEnd,
	Faint,
	GetUp,
	CastStart,
	Cast,
	CastEnd,
	/// Lying where it fainted
	Lying,
};
[[nodiscard]] std::string_view Name(State state);
/// The animation a state plays, other than an action's own
[[nodiscard]] size_t AnimationOf(State state);
/// Whether a state's animation plays round and round until something else happens, or holds its last frame
[[nodiscard]] bool Loops(State state);
/// What comes after a state's animation has played through
[[nodiscard]] State AfterAnimation(State state, bool hasOpponent);
/// The next state of a fighter whose fight was called off, if it moves on now: in its stance it finishes, blocking it
/// ends the block, casting it ends the cast; starting, starting a block, reeling in one or starting a cast (which is then
/// never cast) end the same way once their animation is done, and a blow or step finishes once done. None: it carries on.
[[nodiscard]] std::optional<State> StopTransition(State state, bool animationDone);
/// The casting pose is held only until a moment of game time has passed in it (one update), then the cast ends
constexpr float k_CastHoldMs = 2.0f;
/// Whether a state is done: its animation has played through (by the animation's own time), or, for the casting pose,
/// more than a moment of game time has passed in it
[[nodiscard]] bool StateDone(State state, float animationMs, float durationMs, float stateMs);
/// Whether the creature is blocking: while blocking, once half way into starting to block, and until half way out of
/// it again
[[nodiscard]] bool IsBlocking(State state, float timeMs, float startBlockDurationMs);
/// Whether a creature in a state can be told to make a move
[[nodiscard]] bool TakesMoves(State state);
/// Whether a creature in a state turns to face its opponent
[[nodiscard]] bool FacesOpponent(State state);

/// Who chooses the creature's moves
enum class Control : uint8_t
{
	/// Nobody: it only makes the moves a script queues for it
	None,
	Player,
	Computer,
};
/// The computer leaves a player's creature alone for 3 seconds as a fight starts, and for 15 after each of the
/// player's moves
constexpr float k_ComputerWaitsAtStartMs = 3000.0f;
constexpr float k_ComputerWaitsMs = 15000.0f;

/// A creature's side of a fight
struct Fighter
{
	State state {State::Idle};
	/// The animation playing, how far through it is and the speed of the action
	size_t animation {0};
	float timeMs {0.0f};
	/// The game time since the state began, whatever the animation's speed
	float stateMs {0.0f};
	float speed {1.0f};
	bool mirrored {false};
	/// The last action that it started: a blow, a step, a reel, or the reel in a block. It stays after the action ends.
	size_t lastAction {0};
	/// A blow that is the special move
	bool special {false};
	/// The spell being cast
	std::optional<uint32_t> spell;
	/// Its fight was called off: it finishes the move under way, then finishes and stands
	bool stopping {false};
	MoveQueue queue;
	/// The sparkles marking the moves the player queued, in step with the queue
	marks::MarkList marks;
	float health {1.0f};
	float stamina {1.0f};
	Control control {Control::Computer};
	float computerWaitMs {k_ComputerWaitsAtStartMs};
	/// The lean that the computer chooses its moves by. It does not change during the fight.
	float tendency {0.0f};
};
/// Plays a state, with an action's animation and speed
void Enter(Fighter& fighter, State state, std::optional<size_t> animation = std::nullopt, float speed = 1.0f);
/// The player (or a script) adds a move: a creature the computer was fighting with is given back to the player, and
/// the computer waits 15 seconds
bool PlayerMove(Fighter& fighter, const Move& move, bool replace);

/// What to do with the move at the front of the queue
struct Order
{
	enum class Kind : uint8_t
	{
		EndBlock,
		Animation,
		Cast,
		Block,
		/// A blow at a band, or the special move
		Blow,
		Special,
	};
	Kind kind {Kind::Blow};
	Band band {Band::Mid};
	uint32_t value {0};
	float speed {1.0f};
};
/// The move at the front of the queue taken off it, if one can be made now: only in range of the opponent, in the
/// stance or blocking (when anything ends the block), and a blow only once its charge is known
[[nodiscard]] std::optional<Order> TakeOrder(Fighter& fighter, bool inRange);

// The computer's choices

/// What the opponent is doing, as the computer sees it
enum class Opponent : uint8_t
{
	Stance,
	EndingCast,
	BlockRecoil,
	Attacking,
	Casting,
	SteppingForward,
	OtherAction,
	Blocking,
	Other,
};
[[nodiscard]] Opponent OpponentOf(State state, size_t animation);
/// How aggressive the computer is, 0 to 2, by the tendency. The game meant 1 for the middle, but only ever gives 0 below
/// 0.33 and 2 from it, which is kept.
[[nodiscard]] uint32_t TierOf(float tendency);
struct Choice
{
	enum class Kind : uint8_t
	{
		None,
		Blow,
		Block,
		Step,
	};
	Kind kind {Kind::None};
	Band band {Band::Mid};
	Step step {Step::Back};
};
/// The computer's move this game turn, given its tier and what the opponent is doing
[[nodiscard]] Choice ChooseMove(uint32_t tier, Opponent opponent, const Random& random);
/// While blocking, the computer stops one turn in eight
[[nodiscard]] bool ComputerEndsBlock(const Random& random);

// Choosing the blow

/// One of the creature's blows as it lands: where it lands in the creature's mesh space, how high it lands in the
/// world, and the swing of its striking bone in the mesh space. A blow that the creature has but does not measure lands
/// at the origin of the mesh, with no swing.
struct Reach
{
	size_t animation;
	glm::vec3 point {0.0f};
	float height;
	glm::vec3 swing {0.0f};
};

// How blows are measured and how they land

/// A blow is measured only when it lands at least this late in its animation. Its swing is how far the striking bone
/// moves over that time before it lands, times 20: its speed in units a second.
constexpr float k_LeastLandMs = 50.0f;
constexpr float k_SwingPerSecond = 20.0f;
/// Where a blow lands and how it swings, in the attacker's mesh space with ahead being -z
struct BlowShape
{
	/// The striking bone as the blow lands, carried ahead as far as the root travels until then and taken to the
	/// middle, neither to the left nor to the right
	glm::vec3 point;
	glm::vec3 swing;
};
/// The shape of a blow from its striking bone as it lands and the land time before that. rootTravel is how far the
/// whole animation carries the root along z.
[[nodiscard]] BlowShape ShapeOfBlow(const glm::vec3& boneAtLand, const glm::vec3& boneBefore, float landMs, float durationMs,
                                    float rootTravel);
/// How high a blow lands as a share of the victim's height: how high the blow reaches over the attacker's feet over
/// how high the victim's head is over the land under it
[[nodiscard]] float HeightShareOf(float blowHeight, float headHeight);
/// Each game turn a blow is tested at thirty times through a span of the animation from where it was at the turn
/// before. The span is the turn's time scaled down for a bigger creature, (1.6 - 0.425 size) of it, in whole
/// milliseconds.
constexpr int32_t k_BlowTests = 30;
[[nodiscard]] int32_t TestSpanMs(float turnMs, float size);
/// Only the tests within 0.6 of that span of the blow's land time, either way, count, in whole milliseconds
[[nodiscard]] int32_t TestWindowMs(int32_t spanMs);
/// The animation times a blow is tested at in a turn, from the time at the turn before, that fall within the window
[[nodiscard]] std::vector<int32_t> BlowTestTimes(int32_t fromMs, int32_t spanMs, int32_t landMs);
/// At each test, five lines go from the attacker's groin. The first goes to the striking bone moved along its swing
/// since the last test by the blow's reach, the others to points that far to either side of the bone and above and
/// below it, across the swing. A swing of no length moves none of them.
[[nodiscard]] std::array<glm::vec3, 5> BlowTestPoints(const glm::vec3& bone, const glm::vec3& boneBefore, float reach);
/// The blow's reach about the bone: a fifth of the attacker's height, twice that for the special move
[[nodiscard]] float BlowTestReach(float attackerSize, bool special);
/// At each test a faint white puff marks the striking bone, still, twice the attacker's size across, four times for the
/// special move. It lives as dust does.
constexpr uint32_t k_SwingPuffColour = 0x30FFFFFFu;
[[nodiscard]] float SwingPuffSize(float attackerSize, bool special);
/// Where the blow meets the victim's body, its vector is the striking bone's movement since the last test, times 150
constexpr float k_BlowVectorScale = 150.0f;
/// Where a blow lands, 40 drops of blood fly unless the victim blocks, then 8 puffs of dust whether it blocks or not.
/// Each one flies along the blow's vector, plus a spread along each axis of -25 to 25 of a step. The blood's step is a
/// hundredth of the vector's length; the dust flies at a quarter of the vector, with 0.6 of the blood's step.
constexpr int32_t k_BloodDrops = 40;
constexpr int32_t k_BlowDustPuffs = 8;
constexpr float k_BloodSpreadShare = 0.01f;
constexpr float k_DustVectorShare = 0.25f;
constexpr float k_DustSpreadShare = 0.6f;
/// The blood is one of three colours at random, a tenth of the victim's size across; the dust is pale sand, seven
/// times the attacker's size across
constexpr std::array<uint32_t, 3> k_BloodColours {0xFF960014u, 0xFF781414u, 0xFF502800u};
constexpr float k_BloodSizePerSize = 0.1f;
constexpr uint32_t k_BlowDustColour = 0x30F0C070u;
constexpr float k_BlowDustSizePerSize = 7.0f;
/// Each game turn a drip of blood moves this far down, then back onto the skin
constexpr float k_DripFallPerTurn = 0.012f;
/// The spread step of the blood from the blow's vector
[[nodiscard]] float BloodSpreadStep(const glm::vec3& blowVector);
/// How one drop or puff flies: the vector plus, along each axis, a draw of 0 to 50 less 25, times the step
[[nodiscard]] glm::vec3 SprayVelocity(const glm::vec3& vector, float step, const glm::ivec3& draws);
/// The blow to strike, or the step to take first
struct AttackChoice
{
	std::optional<size_t> animation;
	std::optional<Step> step;
};
/// Steps are tried this many each way
constexpr int k_MaxSteps = 10;
/// A blow lands well when it reaches into the opponent by less than 7.5 units of the attacker's size
constexpr float k_DepthPerSize = 7.5f;
/// A blow as the creature thinks of it before it chooses: how high it lands in the world, and how far the opponent's
/// body is ahead of where it lands. None when no line from the blow meets the body.
struct BlowDepth
{
	size_t animation;
	float height;
	std::optional<float> depth;
};
/// Before a creature chooses a blow, it finds how far the blow reaches into the opponent. Lines go from the point
/// where the blow lands, towards the opponent, in a row across. The row has eleven lines, from 2.25 opponent sizes to
/// one side (15 x 0.15) to as far to the other side, each 0.1 of that width from the last.
constexpr int k_DepthLines = 11;
constexpr float k_DepthWidthPerSize = 15.0f;
constexpr double k_DepthWidthShare = 0.15;
constexpr float k_DepthSpacingShare = 0.1f;
/// The depth before a line meets the body: the most a blow can be from the body
constexpr float k_NoDepth = 1000.0f;
/// How far ahead of the point the body starts, along the direction to the opponent (a unit vector), as the nearest of
/// the lines that meet the bones of the body. A negative value is a point inside the body. None when no line meets it.
[[nodiscard]] std::optional<float> DepthToBody(std::span<const physics::Ellipsoid> bones, const glm::vec3& point,
                                               const glm::vec3& towards, float opponentSize);
/// Whether a point where a blow line meets a bone lies ahead of the start of the line and nearer than its end
[[nodiscard]] bool WithinBlowLine(const glm::vec3& point, const glm::vec3& origin, const glm::vec3& direction);
/// How far one step forward and one step back carry the creature
struct StepLengths
{
	float forward;
	float back;
};
/// The lowest and the highest that a blow can land to be in a band, as heights over the land
[[nodiscard]] std::pair<float, float> BandLimits(Band band, float headHeight);
/// The blow to strike, or the step to take first. The special blow and blows higher than the opponent's head are not
/// used. A blow lands after a number of steps when the opponent's body starts behind where it lands, but by less than
/// 7.5 units of the attacker's size. The creature tries the steps from ten back to ten forward. For each, it tries the
/// blows in turn:
/// - a blow in the band asked for scores its number of steps, any other blow 999,
/// - a lower score wins and takes its number of steps,
/// - on the same score, the blow that goes in less wins but keeps the number of steps that it found first.
/// If the best needs steps, the creature steps once instead. When no blow lands, the creature does nothing.
[[nodiscard]] AttackChoice ChooseAttack(std::span<const BlowDepth> blows, Band band, float headHeight, StepLengths steps,
                                        float attackerSize);

// The fight's end

/// The winner of a knock out shows off, unless evil, when it has a poo on the loser
[[nodiscard]] bool WinnerPoos(float alignment);
/// Whether the living around the winner see a fight won: only when a blow knocked the loser out and the winner has a
/// player. A loser that a miracle or a loss of life knocked out gives no such sight.
[[nodiscard]] bool FightWonIsSeen(bool byBlow, bool winnerHasPlayer);
/// After a fight that ended otherwise, the creature with more life is happy and the other sad. When the two have the
/// same life, both are sad.
[[nodiscard]] size_t Response(float life, float otherLife);
/// After a fight that ended otherwise, the creature that has no more life than the other is made to fear the other
[[nodiscard]] bool FearsAfterFight(float life, float otherLife);
/// Before a fight, each creature plays the taunt or the rude gesture, by a random choice
[[nodiscard]] size_t FaceOffAnimation(bool rude);
/// The creature that starts the fight points at the arena for this many game turns
constexpr uint32_t k_PointAtArenaTurns = 20;

/// The names of the values, as the fight transcript writes them. They do not change between versions of the transcript.
[[nodiscard]] std::string_view Name(Band band);
[[nodiscard]] std::string_view Name(Step step);
[[nodiscard]] std::string_view Name(Move::Kind kind);
[[nodiscard]] std::string_view Name(FightAction action);
[[nodiscard]] std::string_view Name(Control control);
[[nodiscard]] std::string_view Name(Order::Kind kind);
[[nodiscard]] std::string_view Name(Opponent opponent);
[[nodiscard]] std::string_view Name(Choice::Kind kind);
[[nodiscard]] std::string_view Name(RecoilDirection direction);

/// The event of the help record that the move at the front of the queue gives, each time the player's creature looks at
/// it from its stance with its opponent in range: a block, an attack (a blow, also while it waits to be let go, or the
/// special move), a miracle, or a step or another animation
[[nodiscard]] uint32_t HelpEventOf(Move::Kind kind);
/// The player's creature counts its moves in the help record only from its stance, with a move in its queue and its
/// opponent in range. A creature that blocks ends its block with the next move, and that counts nothing.
[[nodiscard]] bool CountsHelpEvent(State state, bool inRange, bool queued);

/// The fight music of the player's creature is heard from this distance to the middle of its arena, in metres
constexpr float k_FightMusicDistance = 100.0f;
/// Whether the fight music of the player's creature plays. It plays while the hand directs the fight. It also plays
/// while the camera is less than the music distance from the middle of an arena where a fight of the creature is on.
[[nodiscard]] bool FightMusicHeard(bool directsFight, std::span<const glm::vec3> arenasOfItsFights, const glm::vec3& camera);
/// The hit that a blow that lands gives its victim from the attacker, which changes how nice the victim finds it
constexpr float k_BlowHitEffect = 0.05f;
/// The fight health of a creature in a duel after it loses damage to its life: the same amount less, never below none
[[nodiscard]] float HealthAfterLifeLost(float health, float damage);
} // namespace openblack::creature_fight
