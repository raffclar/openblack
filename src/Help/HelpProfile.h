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

/// The player's help statistics: the help profile and the counters of the camera help.
///
/// - One accumulator per HELP_EVENT_TYPE (49): the total count, a smoothed rate (float), the ring head, how many ring
///   slots are used (<= 64), "triggered this turn", and 64 trigger times in the profile's accumulated time (ms).
/// - A trigger counts at most once a turn: the "triggered this turn" flag blocks it until Process clears it (once a
///   game turn, after the scripts and the help system), which also advances the accumulated time by 100 ms. So
///   GET_TOTAL_EVENTS(n) is "the turns in which the event happened".
/// - Nothing is counted while the game is paused or a script holds the wide screen.
/// - The counts belong to the player profile, not to the game: the original saves and loads the 49 accumulators to
///   "<user path>\helpstats.dat" with the profile. (not ported) openblack has no profiles: the counts start at 0 every
///   run.
namespace openblack::help_profile
{

/// HELP_EVENT_TYPE, by the original's names
enum class Event : int32_t
{
	Dummy = 0,
	HandMove = 1,
	PickUp = 2,
	Catch = 3,
	Throw = 4,
	Give = 5,
	Supply = 6,
	Sacrifice = 7,
	Tap = 8,
	CastSpell = 9,
	CastCreatureSpell = 10,
	CastAll = 11,
	GetSpell = 12,
	StopSpell = 13,
	RepeatGesture = 14,
	SelectGesture = 15,
	StageGesture = 16,
	GetSpellGesture = 17,
	PowerUpGesture = 18,
	GestureCreatureSpecial = 19,
	GestureCancelPowerUp = 20,
	GestureCancelSelect = 21,
	GestureCancelHeld = 22,
	GestureOnCast = 23,
	GestureTotal = 24,
	Rotate = 25,
	RotateCW = 26,
	RotateCCW = 27,
	Pitch = 28,
	Zoom = 29,
	DoubleClickPos = 30,
	DoubleClickObject = 31,
	ZoomToCitadel = 32,
	Drag = 33,
	Reminder = 34,
	ScriptActivate = 35,
	FightBlock = 36,
	FightAttack = 37,
	FightSpell = 38,
	FightStep = 39,
	AttachLeash = 40,
	DetachLeash = 41,
	HelpQuery = 42,
	AllInterface = 43,
	LookAtLand = 44,
	LookAtLandTooClose = 45,
	LookAtSky = 46,
	FocusLeash = 47,
	AllEvents = 48,
};
inline constexpr int32_t k_EventCount = 0x31; ///< 49 accumulators
inline constexpr uint32_t k_RingSize = 0x40;  ///< 64 trigger times
/// The accumulated time goes up 100 ms a turn
inline constexpr uint32_t k_MsPerProcess = 100;
/// The smoothing of the rate
inline constexpr float k_RateSmoothing = 0.005f;

/// The counter of one help event
struct Accumulator
{
	int32_t totalTriggerCount {0}; ///< signed: GET_TOTAL_EVENTS reads it as a signed integer
	float rate {0.0f};             ///< rate += (triggered - rate) x 0.005 each Process
	uint8_t head {0};
	int8_t used {0}; ///< signed in TriggersPerSecond
	bool triggeredThisTurn {false};
	std::array<uint32_t, k_RingSize> times {};

	/// Count, rate, head, used and the turn flag to 0 (the ring is left as it is)
	void Reset();
	/// Once a turn, ++count, times[head] = now, head = (head + 1) & 63, used = min(used + 1, 64)
	void Trigger(uint32_t now);
	/// The step of Process: rate += ((int)flag - rate) x 0.005; flag = 0
	void EndTurn();
	/// 0 if never; (now - times[(head - 1) & 63]) x 0.001; a time in the future (a profile loaded over a newer clock)
	/// brings every slot down to now (ClampTimes) and gives 0
	[[nodiscard]] float TimeSinceLastUsed(uint32_t now);
	/// 0 with fewer than 2 times; d = now - times[(head - used) & 63]; d < 0 clamps the ring (ClampTimes) and gives 0,
	/// d == 0 gives 0, else (used x 1000 - 1000) / d
	[[nodiscard]] float TriggersPerSecond(uint32_t now);
	/// Every time later than now becomes now
	void ClampTimes(uint32_t now);
};

/// Why the camera help is called: the high byte picks the table, the low byte the entry
enum class CameraReason : int32_t
{
	InsideExclusionZone = 0x100, ///< inside an exclusion zone
	InsideInclusionZone = 0x101, ///< inside an inclusion zone
	Exclusion2 = 0x102,
	Move0 = 0x200,
	Move1 = 0x201,
	Move2 = 0x202,
	Move3 = 0x203,
	Rotate = 0x300,            ///< -> Event::Rotate (25)
	RotateCW = 0x301,          ///< -> 26
	RotateCCW = 0x302,         ///< -> 27
	Pitch = 0x303,             ///< -> 28
	Zoom = 0x304,              ///< -> 29
	DoubleClickPos = 0x305,    ///< -> 30
	DoubleClickObject = 0x306, ///< -> 31
	ZoomToCitadel = 0x307,     ///< -> 32
	Drag = 0x308,              ///< -> 33
};
/// The player camera's input mask, as the camera update builds it
enum class CameraInput : uint32_t
{
	Keyboard = 0x01,         ///< the camera is moving, without WheelDown
	MouseButton = 0x02,      ///< the low bit of the camera's drag mode
	BothMouseButtons = 0x04, ///< a rotate drag
	WheelSpin = 0x08,        ///< the wheel turned
	WheelDown = 0x10,
};

/// What ProcessSpecialTriggers reads of the player's camera mode (nothing for another mode): whether the centre of the
/// screen is on the land (inferred), the heading distance (inferred) and the pitch
struct CameraView
{
	bool screenCentreOnLand {false};
	float headingDistance {0.0f};
	float pitch {0.0f};
};
struct Queries
{
	/// The game is paused. Unset: game_clock::IsPaused()
	std::function<bool()> paused;
	/// A script holds the wide screen. Unset: help::Get()->IsScriptWideScreen() (false without a help system)
	std::function<bool()> scriptWideScreen;
	/// A game flag (meaning pending: the game clears it before going inside the citadel; the hand is not drawn while it
	/// is set). Unset: false
	std::function<bool()> processBlocked;
	/// The player's camera mode, nullopt while another mode (the script's) is current. (pending, camera) openblack's
	/// DefaultWorldCameraModel has no land-centre flag or heading distance yet. Unset: nullopt (events 44..46 never
	/// come)
	std::function<std::optional<CameraView>()> playerCamera;
};
void SetQueries(Queries queries);

/// Nothing paused or in a script's wide screen; the type's accumulator, then 14..23 also GestureTotal (24), 1..42
/// AllInterface (43), 9..11 CastAll (11) and every type AllEvents (48)
void Trigger(Event type);
/// Once a game turn: nothing paused, in a script's wide screen or with processBlocked; else ProcessSpecialTriggers,
/// the 49 EndTurn, accumulated time += 100 and the same EndTurn for the camera help's 12 accumulators
void Process();
/// With the player's camera mode: centre on the land -> LookAtLand (44), then heading distance < 15 and pitch > 0.55
/// -> LookAtLandTooClose (45); not on the land and pitch < -0.3 -> LookAtSky (46)
void ProcessSpecialTriggers();

/// The camera help callback (reason, inputs): 0x3nn -> Trigger(25 + nn) and, for each bit of `inputs`, the camera
/// help's input accumulator (5); 0x2nn -> the move accumulator nn (4); 0x1nn -> the exclusion accumulator nn (3).
/// The original's point argument is not read
void CameraHelpCallback(CameraReason reason, uint32_t inputs);
/// The calls the camera update makes for the player's rotate, pitch and zoom, from openblack's
/// DefaultWorldCameraModel: `yaw` is its _rotateAroundDelta.y (turned into radians as yaw x pi / width), `pitch` its
/// _rotateAroundDelta.x (x 0.002) and `zoom` its zoomDelta (x 0.0015 x the distance factor). Zoom when zoom != 0;
/// Rotate when |yaw| > 0.01 and then RotateCW (yaw > 0) or RotateCCW (yaw < 0); Pitch when pitch != 0.
/// (approximate) the original also needs the camera's drag mode to be 2 or 3 and, for the pitch, no auto-pitch; the
/// camera features (rotate, pitch, zoom) zero the deltas before, once openblack's camera reads them (pending, camera)
void OnPlayerCameraMove(float yaw, float pitch, float zoom, uint32_t inputs);

/// GET_TOTAL_EVENTS 237 and the other two readers: nullopt for a type outside 1..48 (the script error "Invalid event",
/// the script gets 0.0)
[[nodiscard]] std::optional<float> TotalEvents(int32_t type);
/// GET_EVENTS_PER_SECOND 235: TriggersPerSecond
[[nodiscard]] std::optional<float> EventsPerSecond(int32_t type);
/// GET_TIME_SINCE 236: TimeSinceLastUsed
[[nodiscard]] std::optional<float> TimeSince(int32_t type);

[[nodiscard]] const Accumulator& Get(Event type);
/// The profile's accumulated time, ms
[[nodiscard]] uint32_t AccumulatedTime();
/// The 49 Reset, then the camera help's 12. The accumulated time is a static and keeps going
void SetToZero();
/// The tests: SetToZero, AccumulatedTime 0 and no queries
void Reset();

} // namespace openblack::help_profile
