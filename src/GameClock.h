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

#include <optional>

/// The game clock, the one clock of the original. The original has no "get the time" routine: its game loop works the
/// clock out once a loop and publishes it in fields that many readers read. The state lives in the TimeSystemInterface
/// service (Locator::time); the free functions below read and drive it, and Game drives the loop.
/// Details: docs/bw1-notes/day-night-weather.md and engine-math.md.
///
/// - The game timer is the wall clock scaled by the game speed, stopped while paused.
/// - A turn is due when that timer reaches turn * 100 ms: an absolute comparison, so no turn loses its leftover. More
///   than 2 s behind, the lag is dropped. At most one turn a frame in a single player game.
/// - The turn number goes up when the turn STARTS, and only unpaused.
/// - After the turns, the frame clock: the remainder of the turn in whole ms (0..99), the visual clock turn * 100 +
///   remainder (it can go back; the frame's dt is then 0), the frame's game ms (whole ms, 0 while paused, at most 199)
///   and the fraction of the turn, remainder * 0.01 (kept, not zeroed, while paused).
/// - The engine's wall clock is another timer, started when the renderer opens and read at the start of each frame:
///   whole ms, at least 1, it does not stop in pause.
///
/// Values that look alike and are NOT the same:
/// - k_MsPerTurn / MsPerTurn() (what the game logic reads) vs k_SchedulerMsPerTurn (the 100 the scheduler and the
///   frame clock use as a literal) vs k_TurnSeconds (the 0.1 the turn hands to the systems as a literal). They are all
///   100 ms, but the original does not tie them: SET_GAME_TICK_TIME only changes the first (no map uses it).
/// - FrameGameMs() (game time, stops in pause, follows the speed) vs FrameRealMs() (wall clock, never 0).
/// - Turn() vs the game's other counters (turns started, loops, frames since the turn): only the first is the clock.
///
/// The game logic runs with the FPU at single precision: the timer arithmetic is float.
namespace openblack::game_clock
{

/// The ms of a turn that the game logic reads
inline constexpr uint32_t k_MsPerTurn = 100;
/// The 100 the scheduler and the frame clock use as a literal instead of reading MsPerTurn()
inline constexpr uint32_t k_SchedulerMsPerTurn = 100;
/// The seconds of a turn the turn hands to the weather, the land alignment and the music as a literal
inline constexpr float k_TurnSeconds = 0.1f;
/// More than this behind the turn, the local timer is reset to it
inline constexpr int32_t k_MaxLagMs = 2000;
/// Turns a frame: 1 in a single player game, 10 in a network game
inline constexpr uint32_t k_MaxTurnsPerFrame = 1;
inline constexpr uint32_t k_MaxTurnsPerFrameNetwork = 10;
/// The remainder of the turn is clamped to 0..99
inline constexpr int32_t k_MaxRemainderMs = 99;
/// The fraction is the remainder times 0.01
inline constexpr float k_FractionPerMs = 0.01f;
/// The readers turn the frame ms into seconds with 0.001
inline constexpr float k_SecondsPerMs = 0.001f;
/// The clamped frame ms are at most 500
inline constexpr uint32_t k_ClampedFrameMaxMs = 500;
/// The speed a timer is given when it starts, so that SetSpeed takes its running path before the saved speed
/// goes back
inline constexpr float k_StartSpeed = 0.00001f;

/// The game's timer, the same routines for every timer of the game
struct Timer
{
	uint32_t tickCount {0};   ///< The wall clock ticks of the last rebase
	int32_t elapsedTime {0};  ///< The scaled ms gathered up to it
	float speed {1.0f};       ///< The speed (0 = stopped)
	float savedFactor {1.0f}; ///< The speed kept while stopped

	/// truncate((now - base) * speed + elapsed)
	[[nodiscard]] int32_t ElapsedMs(uint32_t now) const;
	/// When running: keep the speed, gather the time and stop
	void Stop(uint32_t now);
	/// Stopped: only keep the new speed. Running: rebase and take it
	void SetSpeed(float factor, uint32_t now);
	/// Speed 1e-5, then SetSpeed(saved), which rebases: the time the timer was stopped is dropped
	void Start(uint32_t now);
};

/// The milliseconds of the wall clock, wrapping at 2^32
[[nodiscard]] uint32_t WallTicks();
using TickSource = uint32_t (*)();

} // namespace openblack::game_clock

namespace openblack
{
/// The state behind game_clock: the game timer, the engine timer, the turn and the frame clock
class TimeSystemInterface
{
public:
	virtual ~TimeSystemInterface() = default;

	/// The ticks the clock reads; the music thread reads them too, so implementations make this thread-safe
	[[nodiscard]] virtual uint32_t TickCount() const noexcept = 0;
	/// Tests and the fixed step: nullptr = the wall clock
	virtual void SetTickSource(game_clock::TickSource source) noexcept = 0;
	/// The fixed step (OPENBLACK_FIXED_FRAME_MS): every frame the ticks go up by `frameMs`; 0 = the wall clock
	virtual void SetFixedFrameMs(uint32_t frameMs) noexcept = 0;
	[[nodiscard]] virtual uint32_t FixedFrameMs() const noexcept = 0;
	/// The fixed step's ticks go up by one frame
	virtual void AdvanceFixedFrame() noexcept = 0;

	virtual void Reset() noexcept = 0;
	virtual void StartEngineTimer() noexcept = 0;
	[[nodiscard]] virtual uint32_t MsPerTurn() const noexcept = 0;
	virtual void SetMsPerTurn(uint32_t ms) noexcept = 0;
	[[nodiscard]] virtual uint32_t Turn() const noexcept = 0;
	virtual void SetTurn(uint32_t turn) noexcept = 0;
	[[nodiscard]] virtual bool IsTurnScheduled() noexcept = 0;
	[[nodiscard]] virtual uint32_t TurnsThisFrame() const noexcept = 0;
	virtual void StartTurn() noexcept = 0;
	virtual void ResetLocalTimer() noexcept = 0;
	virtual void Start(bool paused) noexcept = 0;
	virtual void OnLoad() noexcept = 0;
	virtual void Pause(bool paused) noexcept = 0;
	[[nodiscard]] virtual bool IsPaused() const noexcept = 0;
	[[nodiscard]] virtual int32_t SequenceMode() const noexcept = 0;
	virtual void SetSequenceMode(int32_t mode) noexcept = 0;
	virtual void SetSpeed(float speed) noexcept = 0;
	[[nodiscard]] virtual float Speed() const noexcept = 0;
	virtual void UpdateFrameClock() noexcept = 0;
	virtual void UpdateRealClock() noexcept = 0;
	[[nodiscard]] virtual int32_t EngineMs() const noexcept = 0;
	[[nodiscard]] virtual uint32_t FrameGameMs() const noexcept = 0;
	[[nodiscard]] virtual float TurnFraction() const noexcept = 0;
	[[nodiscard]] virtual uint32_t VisualMs() const noexcept = 0;
	[[nodiscard]] virtual uint32_t FrameRealMs() const noexcept = 0;
	[[nodiscard]] virtual uint32_t EngineFrameSampleMs() const noexcept = 0;
};
} // namespace openblack

namespace openblack::game_clock
{
/// The ticks of the clock's source (the wall clock unless a test or the fixed step set another)
[[nodiscard]] uint32_t TickCount();
/// Tests: the clock reads the ticks from here (nullptr = the wall clock)
void SetTickSource(TickSource source);

/// The state at program start: the game timer at speed 1, the engine timer stopped
void Reset();
/// When the renderer opens: the engine timer starts from about 0 at speed 1. Until then EngineMs() stays at 0 and
/// FrameRealMs() at 1
void StartEngineTimer();

/// The ms of a turn for the game logic
[[nodiscard]] uint32_t MsPerTurn();
/// SET_GAME_TICK_TIME: only MsPerTurn(); the scheduler keeps its 100
void SetMsPerTurn(uint32_t ms);
/// The turns of `seconds`: truncate(1000 / MsPerTurn() (integer division) * seconds)
[[nodiscard]] int32_t TicksForSeconds(float seconds);

/// The turns played (not the paused ones)
[[nodiscard]] uint32_t Turn();
/// Loading a game sets it; a new game starts at 0
void SetTurn(uint32_t turn);

/// The timer has reached turn * 100. Paused, never. More than 2 s behind, the timer is reset to the turn and the
/// answer is still the one of the sample taken before.
[[nodiscard]] bool IsTurnScheduled();
/// IsTurnScheduled() (asked first, so it is asked once more after the last turn of the frame) and fewer than
/// k_MaxTurnsPerFrame turns this frame
[[nodiscard]] bool TurnDue();
/// The turn number goes up at the start of the turn, unpaused only
void StartTurn();
/// Stop, the timer = turn * 100 from now, and start it again
void ResetLocalTimer();

/// The start of the game loop: the timer from 0 and started, the frame ms and the fraction at 0, then
/// ResetLocalTimer. `paused` is the pause flag the loop starts with
void Start(bool paused);
/// Loading a game: the frame ms and the fraction at 0, the visual clock at turn * 100. The loop's own samples are not
/// touched
void OnLoad();

/// The pause flag, and the game timer stopped (pausing) or started again from now (unpausing: the paused time does not
/// count)
void Pause(bool paused);
[[nodiscard]] bool IsPaused();

/// The game's sequence mode, read by many (the temple, the sound, the camera, the help texts, the engine). The last
/// writer wins: a new game (0), the falling spell video (2, then 0), leaving and entering the citadel (0, 1)
inline constexpr int32_t k_SequenceModeNone = 0;
inline constexpr int32_t k_SequenceModeCitadel = 1;
inline constexpr int32_t k_SequenceModeFallingSpell = 2;
[[nodiscard]] int32_t SequenceMode();
/// The writers above (openblack: Game::LoadMap, FallingSpellVideo::Start / End, TempleInterior::Deactivate /
/// Activate)
void SetSequenceMode(int32_t mode);
/// Sequence mode 1: inside the citadel
[[nodiscard]] bool IsInsideCitadel();
/// The speed-up factor of the timer (1 = normal, 2 = twice as fast). Running, the timer is rebased, so the time
/// already gone keeps the old speed. Stopped (paused), the new speed is kept for when it starts again
void SetSpeed(float speed);
/// The speed asked for
[[nodiscard]] float Speed();

/// Once a loop after the turns: the remainder, the visual clock, the frame's game ms and the fraction (unpaused); the
/// frame ms at 0 (paused). Then the turns of this frame back to 0
void UpdateFrameClock();
/// The start of a frame: the engine timer's ms since the last frame, 1 if <= 0
void UpdateRealClock();
/// The engine timer in ms, the wall clock since StartEngineTimer
[[nodiscard]] int32_t EngineMs();

/// The game ms of this frame (whole, 0 paused, <= 199)
[[nodiscard]] uint32_t FrameGameMs();
/// FrameGameMs() * 0.001, the readers' seconds
[[nodiscard]] float FrameGameSeconds();
/// The remainder * 0.01, 0..0.99; one turn behind (0 just after a turn) and kept in pause
[[nodiscard]] float TurnFraction();
/// turn * 100 + remainder, the visual clock in ms (it can go back; the dt never is < 0)
[[nodiscard]] uint32_t VisualMs();
/// The wall clock ms of this frame (>= 1, does not stop in pause)
[[nodiscard]] uint32_t FrameRealMs();
/// The engine timer sample of this frame, in ms, taken by UpdateRealClock. The hand measures a press's hold with it
[[nodiscard]] uint32_t EngineFrameSampleMs();
/// The game ms while the interface plays a recording back, the wall clock ms otherwise. (inferred) openblack has no
/// playback: `playingBack` stands for it
[[nodiscard]] uint32_t CameraFrameMs(bool playingBack = false);
/// The wall clock ms in the temple, the game ms otherwise; <= 0 gives 0 and it is at most 500. `inTemple` is the
/// citadel's sequence mode (IsInsideCitadel), which the caller reads
[[nodiscard]] uint32_t ClampedFrameMs(bool inTemple = false);

/// The ms between two paused turns, and how far behind the count may fall before it starts again from now
inline constexpr uint32_t k_PausedTurnMs = 100;
inline constexpr uint32_t k_PausedTurnMaxLagMs = 200;

/// The paused turn's schedule: while the game is paused inside the citadel, the temple has a turn of its own every
/// 100 ms of the clock's ticks. The count is never reset: it is kept from one visit to the next
struct PausedTurnTimer
{
	std::optional<uint32_t> last; ///< The ticks of the last paused turn, unset until the first ask

	/// The first ask starts the count from now. A turn is due once more than 100 ms have gone since the last one: the
	/// count moves on by 100, or to now when it is still more than 200 behind. Unsigned, so the ticks may wrap
	[[nodiscard]] bool Due(uint32_t now);
};

} // namespace openblack::game_clock
