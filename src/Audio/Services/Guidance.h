/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <cstdint>

#include <array>
#include <functional>
#include <optional>
#include <vector>

#include <glm/vec3.hpp>

#include "Audio/Audio.h"

// Guidance: the villagers' reactions on Guidance.sad (their desires, a resource dropped on their town, an attack, a
// disciple, belief, the deaths) and the advisors' remarks (types 9..32 through the help script
// "MultiHelpJustTalkWithText"), each gated by the type's interval and the help level. Notes: docs/bw1-notes/audio.md.
//
// The original keeps one guidance state per interface, but every caller uses the local player's, so openblack keeps
// one. What it reads from the game comes from GameQueries (the guidance section): towns, worship sites, the citadel
// heart... Unset, they are the neutral values of a game without those systems, and nothing plays. The callers that
// openblack does not have yet (the town's aggressor, the totems, the creature, belief, disciples, the villagers' death)
// find the functions here with the original's arguments.
//
// Random numbers: LocalRand (0 for 0, else a number in [0, n)) and LocalFloatRand (0 for 0, else x times a draw in
// [0, 65535] times 1 / 65535), on game_random's local stream (the one the rest of the game draws from); SetRandom gives
// the tests their own generator.

namespace openblack::audio::guidance
{

/// The 33 guidance sound types
enum class Type : uint8_t
{
	TownDesire = 0,        ///< UpdateTownDesireRemarks
	ResourceDrop = 1,      ///< PlayResourceDropRemark
	TownAttack = 2,        ///< PlayTownAttackRemark
	RaiseTotem = 3,        ///< EndTotemRaiseSound
	Disciple = 4,          ///< PlayDiscipleRemark
	Belief = 5,            ///< PlayBeliefSample (from PlayBeliefRemark)
	HeartBeat = 6,         ///< HeartBeat
	DeathInVillage = 7,    ///< PlayVillageDeathRemark (a villager's death)
	HelpSprites = 8,       ///< the gate of CanPlaySpiritRemark
	TownBeingAttacked = 9, ///< 9..30: HELP_SPRITES_GUIDANCE (info.dat, 22 lists) + 9
	CreatureBeingAttacked = 10,
	CreatureAttackingThem = 11,
	LosingVillagers = 12,
	AttackingTown = 13,
	LowOnFood = 14,
	LowOnWood = 15,
	InjuredPeople = 16,
	LowOnPeople = 17,
	VillagersUnhappy = 18,
	LosingBelief = 19,
	OtherVillages = 20,
	CreatureFight = 21,
	GeneralBad = 22,
	GeneralGood = 23,
	KillingPeople = 24,
	GoodBeingEvil = 25,
	EvilBeingGood = 26,
	WorshippersDying = 27,
	VeryGood = 28,
	VeryEvil = 29,
	DestroyBuilding = 30,
	JustTalkNoText = 31, ///< SpiritSay's "MultiHelpJustTalkWithNoText" (no caller found)
	OneOff = 32,         ///< OneOff's remarks

	_Count
};
inline constexpr size_t k_TypeCount = static_cast<size_t>(Type::_Count);

/// A row of the type table
struct TypeInfo
{
	uint32_t base;     ///< The interval's base in turns, and Init's spread
	int32_t helpLevel; ///< The help level it needs (PlayNow)
	bool always;       ///< Not muted on land 1 of a single-player campaign (PlayNow)
};
/// The type table
inline constexpr std::array<TypeInfo, k_TypeCount> k_Types {{
    {50, 1, false},   {25, 1, false},   {40, 1, true},    {50, 1, false},   {0, 0, true},      {30, 1, true},
    {0, 1, true},     {0, 1, true},     {100, 1, true},   {1000, 2, false}, {1500, 2, false},  {2000, 3, false},
    {1000, 2, false}, {1000, 3, true},  {2500, 3, false}, {2500, 3, false}, {1000, 4, false},  {2500, 2, false},
    {3000, 3, false}, {1000, 2, false}, {1000, 2, false}, {200, 1, true},   {1000, 2, false},  {1000, 2, false},
    {500, 2, false},  {5000, 2, true},  {5000, 2, true},  {600, 1, false},  {10000, 2, false}, {10000, 2, false},
    {1000, 3, true},  {0, 0, true},     {0, 0, true},
}};

/// The 22 lists of HELP_SPRITES_GUIDANCE (info.dat): HELP_TEXT ids, the list ends at the first 0 (at most 34)
using SpriteList = std::array<uint32_t, 34>;
inline constexpr size_t k_SpriteLists = 22;

/// OneOff's table: {HELP_TEXT, probability} by its argument
struct OneOffInfo
{
	uint32_t text;
	float probability;
};
inline constexpr std::array<OneOffInfo, 7> k_OneOffs {{
    {3308, 0.02f},
    {3317, 0.0002f},
    {3318, 0.01f},
    {3321, 0.01f},
    {3325, 0.05f},
    {3326, 0.1f},
    {3328, 0.025f},
}};

/// DesireSample's table by TOWN_DESIRE_INFO (the samples for x >= 0.85, >= 0.65 and >= 0.45)
inline constexpr std::array<std::array<uint32_t, 3>, 19> k_DesireTexts {{
    {4967, 4968, 4969}, // 0 VILLAGER_VOICE_DESIRE_FOOD_01..03
    {4964, 4965, 4966}, // 1 DESIRE_WOOD
    {0, 0, 0},
    {4983, 4984, 4985}, // 3 DESIRE_PROTECTION
    {4977, 4978, 4979}, // 4 DESIRE_MERCY
    {4973, 4973, 4973}, // 5 DESIRE_BUILD_03 (three times)
    {4980, 4981, 4982}, // 6 DESIRE_EXPAND
    {0, 0, 0},
    {4974, 4975, 4976}, // 8 DESIRE_OFFSPRING
    {0, 0, 0},
    {0, 0, 0},
    {0, 0, 0},
    {0, 0, 0},
    {0, 0, 0},
    {0, 0, 0},
    {0, 0, 0},
    {0, 0, 0},
    {4992, 4993, 4994}, // 17 GUIDANCE_SAMPLE_NEEDWORSHIPPERS_FOOD_01..03
                        // (the worship site's food)
    {4996, 4997, 4998}, // 18 GUIDANCE_SAMPLE_NEEDWORSHIPPERS_01..03 (the citadel's need)
}};

/// A number in [0, n), 0 for n == 0
using RandomFn = std::function<uint32_t(uint32_t n)>;
/// The generator of LocalRand / LocalFloatRand (tests); an empty one is game_random's local stream
void SetRandom(RandomFn random);
/// The game's generator: seed = ror13(seed * 9377 + 9439); seed % n
[[nodiscard]] uint32_t SeededRandom(uint32_t n, uint32_t& seed);
/// A number in [0, n) from the local stream, 0 for n == 0
[[nodiscard]] uint32_t LocalRand(uint32_t n);
/// x * LocalRand(65535) * (1 / 65535), 0 for x == 0
[[nodiscard]] float LocalFloatRand(float x);

/// The HELP_SPRITES_GUIDANCE lists of info.dat (Game, after loading it; the tests)
void SetSpriteLists(const std::array<SpriteList, k_SpriteLists>& lists);

// ---- the core ------------------------------------------------------------------------------------------------------

/// Resets the guidance when an interface starts or loads: the persistent options (bank Guidance, 2D, no tracking,
/// mode 2, the defaults otherwise); for each type lastPlayed = turn - LocalRand(base) if that is > 0, else 0 (a
/// positive first draw is drawn again for the value kept); the remembered things emptied; the last samples, the heart
/// beat, totem, alignment and belief values cleared, the heart beat's pitch 30.0, the one-off flags cleared; and
/// guidance enabled. openblack calls it when a land starts (its turn counter back to 0).
void Init();
/// The options freed, the remembered things emptied
void Close();

/// turn - lastPlayed[t] (unsigned)
[[nodiscard]] uint32_t TimeSinceLastPlayed(Type type);
/// base + LocalRand(trunc(5 base (1 - r^3))), r = LocalFloatRand(1) (turns; new numbers each call)
[[nodiscard]] uint32_t Interval(Type type);
/// A type not `always` is muted on land 1 of a single-player game that is not the playground; then the help level must
/// reach the type's, and TimeSinceLastPlayed > Interval (unsigned)
[[nodiscard]] bool PlayNow(Type type);
/// PlayNow(HelpSprites) && PlayNow(type)
[[nodiscard]] bool CanPlaySpiritRemark(Type type);

/// Plays a guidance sample: the persistent options get the volume, the pitch, field2C, the sample (looked up in the
/// voice table when isText) and the owner (the player's number); 3D: only with a point (none: nothing at all), at the
/// point, with maxDistance and a min distance of maxDistance * 0.333, the caller mask 0x180 (the .sad keeps neither)
/// and no tracking; 2D: the rest stays as the last 3D one left it. Then the sound effect plays and, for a type < 33,
/// lastPlayed = turn.
/// `point` is the map position as a world point (x, altitude + its height, z).
Channel PlaySample(bool is3D, uint32_t textOrSample, uint32_t owner, int type, int volume, int pitch, int field2C,
                   std::optional<glm::vec3> point, float maxDistance, bool isText);

/// Runs the help message (text, text, type == 31 ? "MultiHelpJustTalkWithNoText" : "MultiHelpJustTalkWithText")
/// (GameQueries::helpRunMessage), triggers help category 8 (GameQueries::helpTriggerCategory), then lastPlayed[type] =
/// turn for a type < 33 and lastSpiritSay = turn (whether the script started or not)
void SpiritSay(uint32_t text, Type type);
/// Once per land per index: if LocalFloatRand(1) <= k_OneOffs[k].probability, SpiritSay(text, OneOff) and the flag
/// set (spooky voices 1, the moon phase 5, the gathering box 3, another caller 4)
void OneOff(int index);

/// list[LocalRand(count)] (count: the entries before the first 0; a list with all 34 set counts 33)
[[nodiscard]] uint32_t GetRandomSample(size_t list);
/// value clamped to 0..1, list[LocalRand(trunc(count * value))]
[[nodiscard]] uint32_t RandomSampleForValue(size_t list, float value);

/// The things the guidance remembers ({thing, turn}): the turns since the thing was first seen, starting again past
/// 600; a new thing is added with the turn and gives 0
[[nodiscard]] uint32_t TimeSinceThingSeen(uint32_t thing);
/// x = value - LocalFloatRand(value / 3); x >= 0.85 / 0.65 / 0.45 -> k_DesireTexts[desire][0 / 1 / 2], else 0
[[nodiscard]] uint32_t DesireSample(uint32_t desire, float value);
/// 2 t0 s v^3 (1 - d^2) (1 - t1^3) with t0 = min(TimeSince(TownDesire) / 50, 1), t1 = min(TimeSinceThingSeen(thing) /
/// 300, 1), d = min(distance / 200, 1), v = min(value, 1), s = t0^3 when the sample is the last desire's, else 1
[[nodiscard]] float DesireScore(uint32_t thing, float distance, float value, uint32_t sample);

// ---- the turn ------------------------------------------------------------------------------------------------------

/// Every 10 turns, PlayNow(TownDesire): the town and worship site desires (GameQueries::desireTowns / worshipSites)
/// choose a sample and its value; above 0.3, x = value - LocalFloatRand(value / 2) and the sample plays 3D at the thing
/// with maxDistance 200 x, volume 127, pitch 100, field2C 90; the sample is remembered as the last desire's
void UpdateTownDesireRemarks();
/// Every turn, for the local interface: every 10 turns the heart beat's value from GameQueries::heartBeat, clamped to
/// 0..1; then, every turn, HeartBeat(value) (so the pitch, the phase and the pulse move each turn)
void UpdateHeartBeat();
/// Every turn: a countdown; at its end, at visual night, the moon's phase - pi: real night and |phase - pi| < 0.15 ->
/// OneOff(5) and 600000 turns; else trunc((phase - pi)^2 * 12000)
void RemarkOnMoonPhase();
/// The audio part of the game turn, in its order: spooky voices, RemarkOnMoonPhase, UpdateTownDesireRemarks, the
/// confirmation, and the heart beat of the interface's turn
void ProcessGameTurn();

/// The moon's phase from the real clock, 2 pi (1 - frac(days * 0.03386318)) (1 / 29.5306 of a cycle a day; days =
/// time / 86400 - 10962 as an int)
[[nodiscard]] float MoonPhase(int64_t unixTime);

// ---- the events (callers in the game) ------------------------------------------------------------------------------

/// The guidance resource type (RESOURCE_RAIN_TYPE) and the type of a resource added to a pile
enum class RainType : uint8_t
{
	None = 0,
	Food = 1, ///< Pot food, Animal; RESOURCE_TYPE 0
	Wood = 2, ///< Pot wood, Tree, DeadTree; RESOURCE_TYPE 1
	Rain = 3,
};
/// A resource dropped near a town (a new pile from the local interface, or a resource given to a store by the local
/// interface, with the receiver's point and its resource type: a storage pit's is 0, silent): PlayNow(ResourceDrop),
/// the nearest town within 100 (GameQueries::nearestTownAt), ResourceDropSample (its values:
/// GameQueries::townResourceNeeds), then 3D at the point, maxDistance 200
void PlayResourceDropRemark(glm::vec3 point, RainType type);
/// The sum of the town's three values for the type >= 0.5 -> PLEASED_<type>_01 + LocalRand(3); < 0.25 ->
/// DISPLEASED_FOOD for food, PLEASED_<type> for wood and rain (the original's: their DISPLEASED texts are never used);
/// else 0
[[nodiscard]] uint32_t ResourceDropSample(float need, RainType type);

/// The attack sample of each kind: a thing 0; a lightning spell a LIGHTNING text, other spells 0; a rock ROCKS; a
/// creature MONSTER (each 10 + LocalRand(10))
enum class Attacker : uint8_t
{
	Thing,
	LightningSpell,
	OtherSpell,
	Rock,
	Creature,
};
[[nodiscard]] uint32_t AttackerSample(Attacker attacker);
/// What PlayTownAttackRemark and WarnTownUnderAttack read of a town and its effect values
struct TownAttack
{
	uint32_t population {0};         ///< The town's people
	glm::vec3 position {0.0f};       ///< The town's position
	float severity {0.0f};           ///< Scales maxDistance by min(0.2 v, 1) + 1
	std::array<float, 7> effects {}; ///< The effect values (StrongestEffect: 0 x 0.001 clamped, 1, 2, 4)
	/// The thing that caused it (nullopt: none): its attack sample is asked inside PlayTownAttackRemark, after the lists are
	/// built, as the original draws its LocalRand(10) there
	std::optional<Attacker> attacker;
	bool townIsMine {false};              ///< The town belongs to the local interface's player
	bool townOfLocalPlayer {false};       ///< The town's player is the game's local one
	std::optional<uint32_t> causedPlayer; ///< The player who caused it (nullopt: none), its number
	bool causedIsLocalPlayer {false};     ///< That player is the local one
	std::array<float, 8> aggression {};   ///< The town's aggression towards each player (by number)
};
/// The strongest of the effects 0, 1, 2 and 4 (0 scaled by 0.001 and capped at 1) above 0; 7 when none
[[nodiscard]] int StrongestEffect(const std::array<float, 7>& effects);
/// A town attacked (from the town's aggressor update; not in openblack yet): PlayNow(TownAttack), a town with people
/// and an effect (StrongestEffect != 7): a list of the ten ATTACK texts, + the ten FIRE ones for effect 0, + the
/// attacker's sample ten times, one of them at random (LocalRand(n)) 3D at the town, maxDistance 200 x (min(severity x
/// 0.2, 1) + 1); then, always, WarnTownUnderAttack when the town is the local interface's
void PlayTownAttackRemark(const TownAttack& attack);
/// The local player's town, caused by another player, with people, PlayNow(TownBeingAttacked) (not
/// CanPlaySpiritRemark) and that player's aggression > 1 -> SpiritSay(list 0)
void WarnTownUnderAttack(const TownAttack& attack);

/// A totem starts being raised: remembers its height
void StartTotemRaiseSound(float height);
/// A totem stops being raised: height > the remembered one and PlayNow(RaiseTotem) -> AlignmentClass, and nothing else
/// (the code tests it and returns: no sample plays)
void EndTotemRaiseSound(float height);
/// The local player's alignment > 0.55 -> 1, < -0.55 -> 2, else 0
[[nodiscard]] int AlignmentClass(float alignment);
/// VILLAGER_DISCIPLE 10 -> class 0 / 1: LocalFloatRand(1) > 0.5 ? GOOD_LIVE_HERE_01 (4859) : _02, class 2:
/// EVIL_LIVE_HERE_01 / _02 (4861 / 4862), other 0; disciple 0..9 -> a table
[[nodiscard]] uint32_t DiscipleText(uint32_t disciple, int alignmentClass);
/// A villager made a disciple from the hand: PlayNow(Disciple), then DiscipleText(disciple, AlignmentClass) 2D, volume
/// 85, pitch 100, field2C 90
void PlayDiscipleRemark(uint32_t disciple, float localAlignment);

/// The alignment of PlayBeliefRemark: 1 good, 2 evil, any other a coin (LocalRand(2) ? 2 : 1)
/// Belief added (not in openblack yet): the believed-in player's belief below the strongest of the 8,
/// PlayBeliefSample(point, (b + 0.0001) / (max + 0.0001), alignment)
void PlayBeliefRemark(const std::array<float, 8>& beliefs, uint32_t player, glm::vec3 point, float distanceToCamera,
                      int alignment);
/// PlayNow(Belief); x - LocalFloatRand(x / 3); good: >= 0.7 GOOD_AWE_01, >= 0.4 _02, > 0.05 _03; evil EVIL_AWE;
/// BeliefVisibility(the distance from the interface to the point, text) > 0.3 -> 3D at the point, maxDistance 200
void PlayBeliefSample(glm::vec3 point, float distance, float value, int alignment);
/// t0 (1 - d^2) with t0 = min(TimeSince(TownDesire) / 50, 1), d = min(distance / 200, 1); t0^4 (1 - d^2) when text is
/// the last belief sample (which nothing writes: always the first)
[[nodiscard]] float BeliefVisibility(float distance, uint32_t text);

/// A villager of the local player killed by another (with the death table's flag): PlayNow(DeathInVillage) ->
/// DEATH_IN_VILLAGE_06 + LocalRand(5) (5720) 2D, volume 127
void PlayVillageDeathRemark();

/// The heart beat's input (HeartBeat is fed by UpdateHeartBeat; the citadel heart sets the override)
/// Sets the override (0: the beat follows the heart beat's value)
void SetHeartBeatOverride(float value);
/// pitch = override ? override x 100 x 0.4 : (30 + 70 v - pitch) x 0.1 + pitch; the phase += pitch x 0.025 x the
/// milliseconds per turn x 0.001 brought to <= 1; the previous pulse = the pulse, the pulse = (1 - cos(2 pi phase)) /
/// 2; for the local interface with a living citadel heart: InGame 45 looping (-1), mode 2, 3D at the citadel,
/// maxDistance 500, pitch trunc(pitch), then the options back (Guidance, loops 0) and the playing sample's pitch set
void HeartBeat(float value);
/// (No caller found): stops InGame 45 of the owner and clears heartBeatPlaying
void StopHeartBeat();
/// The pulse interpolated from the previous one by the turn's fraction
[[nodiscard]] float HeartBeatPulse(float turnFraction);

// The advisors' remarks: each needs CanPlaySpiritRemark(type) (TownBeingAttacked: PlayNow) and then says one text of
// its list (type - 9) through SpiritSay.

/// (No caller found): enabled && CanPlaySpiritRemark(CreatureBeingAttacked) -> list 1
void WarnCreatureUnderAttack();
/// (From the town's aggressor update): enabled && CanPlaySpiritRemark(CreatureAttackingThem) -> list 2
void RemarkCreatureAttacking();
/// (A villager's death): the villager within 300 of the hand -> list 3
void WarnLosingVillagers(glm::vec3 villager);
/// (From the town's aggressor update): a town not of the local player, with people -> list 4
void RemarkAttackingTown(bool townOfLocalPlayer, uint32_t population);
/// What the HelpSprites remarks about a town read of it
struct HelpTown
{
	uint32_t population {0};           ///< The town's people
	bool storagePitFunctional {false}; ///< The town has a working storage pit
	glm::vec3 position {0.0f};         ///< The town's position
};
/// (The town's desire for food): people, a working storage pit, within 300 of the hand -> list 5 by value
/// (RandomSampleForValue)
void WarnLowOnFood(const HelpTown& town, float value);
/// (The town's desire for wood): the same with list 6 (LowOnWood)
void WarnLowOnWood(const HelpTown& town, float value);
/// (No caller found): people, within 300 of the hand -> list 7
void WarnInjuredPeople(const HelpTown& town);
/// (A villager's death): people, within 300 -> list 8
void WarnLowOnPeople(const HelpTown& town);
/// (The town's desires): people, a working storage pit, within 300 -> list 9 (the value is not used)
void WarnVillagersUnhappy(const HelpTown& town);
/// (From the belief code): people -> list 10
void WarnLosingBelief(uint32_t population);
/// (No caller found): people -> list 11
void RemarkOtherVillages(uint32_t population);
/// (A creature fight starts) -> list 12
void RemarkCreatureFight();
/// (From the belief code, a creature's dance): the point on screen (GameQueries::pointOnScreen) -> list 13 / 14
void RemarkGeneralBad(glm::vec3 point);
void RemarkGeneralGood(glm::vec3 point);
/// (A villager's death): the villager's object on screen (its bounding box, GameQueries::thingOnScreen) -> list 15
void RemarkKillingPeople(bool onScreen);
/// Every turn for the local player, with the alignment change of the turn: alignmentChange = 0.95 alignmentChange +
/// change; past 2 x the player's maximum change a turn, with the alignment a: same sign as alignmentChange and |a| >
/// 0.75 -> a > 0 ? VeryEvil (list 20) : VeryGood (list 19) (sic); opposite and |a| > 0.4 -> a > 0 ? GoodBeingEvil (25,
/// list 16) : EvilBeingGood (26, list 17)
void UpdateAlignmentRemarks(float change, float alignment, float maxChangePerTurn);
/// (A villager of the local player dies of death reason 4) -> list 18
void WarnWorshippersDying();
/// (An abode destroyed by the local player, one of its values below 0.4): the abode on screen -> list 21
void RemarkBuildingDestroyed(bool onScreen);

/// The guidance state for the tests and the debug view
struct State
{
	std::array<uint32_t, k_TypeCount> lastPlayed {};
	uint32_t lastSpiritSay {0};
	uint32_t lastDesireSample {0};
	uint32_t lastBeliefSample {0};
	bool heartBeatPlaying {false}; ///< Only StopHeartBeat writes it
	float heartBeatValue {0.0f};
	float heartBeatPitch {30.0f}; ///< Init: 30.0
	float heartBeatPulse {0.0f};
	float heartBeatPulsePrevious {0.0f};
	float heartBeatPhase {0.0f};
	float heartBeatOverride {0.0f};
	float totemHeight {0.0f};
	float alignmentChange {0.0f};
	float believers {0.0f};   ///< Smoothed share of the world population who believe in me
	float beliefShare {0.0f}; ///< Smoothed belief share
	std::array<bool, 7> oneOffs {};
	bool enabled {false};         ///< Init sets it; two of the remarks read it
	uint32_t moonCountdown {1};   ///< 1 at start
	sample_play::Options options; ///< The persistent sample play options
	Sample sample;                ///< Its bank and sample
	/// Recorded only (default 90; sample_play::Options does not model it)
	int field2C {90};
};
[[nodiscard]] const State& GetState();
/// For the tests: the state as Init leaves it (Init itself needs the turn query)
void ResetForTests();

} // namespace openblack::audio::guidance
