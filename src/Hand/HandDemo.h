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
#include <span>
#include <vector>

#include <HNDFile.h>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

#include "3D/MapCoords.h"
#include "ECS/ScriptFind.h"

/// The tutorial's hand demonstrations: a recording of the player's interface played back through the player's own hand
/// and camera, record by record as the game's time reaches each, holding at the marks the scripts wait for
namespace openblack::hand_demo
{

/// The game's time as the recordings count it: a hundredth of a turn, the part into the turn held under a whole turn
[[nodiscard]] uint32_t GameHundredths(uint32_t turn, float turnFraction);

/// Where a recorded cursor is on a screen: across the whole width, and down the 16:9 picture between the cinema bars
/// as far in as they are
[[nodiscard]] glm::ivec2 CursorPixel(std::span<const float, 2> cursor, glm::ivec2 screen, float wideScreenFraction);

/// A record played, with the camera hints it shows (none near the start and the end of the recording)
struct Played
{
	const hnd::HNDRecord* record {nullptr};
	uint32_t hints {0};
};

/// The mouse buttons a recording holds down: the one that moves the hand and the action button
struct HeldButtons
{
	bool move {false};
	bool action {false};
};
/// The buttons held once a record has played: each from its press to its release
[[nodiscard]] HeldButtons AfterRecord(HeldButtons held, hnd::HNDMessage message);

/// The velocity the hand was ready to throw at when a record was made, which a button's record gives back to the hand
[[nodiscard]] glm::vec3 RecordedThrowVelocity(const hnd::HNDRecord& record);

/// What a replayed click on a thing lands on
enum class ClickTarget : uint8_t
{
	/// Whatever is under the hand now
	UnderHand,
	/// No thing at all, whatever is under the hand: the recording had none under it
	NoThing,
	/// The nearest thing of the recorded kind near the recorded place, if there is one; else whatever is under the hand
	NearestOfKind,
};

/// A thing under the hand: its kind as the scripts see it, and where it is
struct ThingUnderHand
{
	ecs::script_find::Kind kind;
	map_coords::MapCoords at;
};

/// How far from the recorded place a click looks for a thing of the recorded kind, in metres
constexpr float k_RetargetReach = 3.0f;

/// Whether a record's click may land on a thing other than the one under the hand: the action button going down or up,
/// and the one message the recordings never use
[[nodiscard]] bool ClickCanRetarget(hnd::HNDMessage message);

/// Where a recorded click lands. With a thing recorded, the click stays on the thing under the hand only when that is of
/// exactly the recorded kind (a thing with no subtype matching only a recorded "none") and less than the reach from the
/// recorded place
[[nodiscard]] ClickTarget TargetOfClick(const hnd::HNDRecord& record, const std::optional<ThingUnderHand>& underHand);

/// The recorded place of the thing a click was on
[[nodiscard]] map_coords::MapCoords RecordedPlace(const hnd::HNDRecord& record);

/// Whether a thing may take a recorded click instead of the one under the hand: of the recorded type, of its subtype or
/// any when the recording says any, and no farther than the reach from the recorded place
[[nodiscard]] bool TakesRecordedClick(const hnd::HNDRecord& record, const ecs::script_find::Kind& kind,
                                      const map_coords::MapCoords& at);

/// What a thing that a recorded click found near its place is, for whether the click can land on it
struct FoundThing
{
	/// It is a building, and how much of it is built
	bool building {false};
	float built {0.0f};
	/// It is a villager that dies now
	bool dyingVillager {false};
};

/// Whether the hand can click a thing that a recorded click found. A building with nothing built and a villager that dies
/// now cannot take the click, so the click lands on no thing.
[[nodiscard]] constexpr bool FoundThingTakesClick(const FoundThing& thing)
{
	return !(thing.building && thing.built == 0.0f) && !thing.dyingVillager;
}

/// The camera hints are hidden for this many records at each end of the recording
constexpr size_t k_HintlessRecords = 20;

class Playback
{
public:
	/// Plays a recording from its first record, at once, at a game time; `played` gets it
	Playback(std::span<const hnd::HNDRecord> records, uint32_t gameTime, std::vector<Played>& played);

	/// Plays every record the game's time has reached since the start. A record with a mark sets `triggerReached`;
	/// with `pauseOnTrigger` nothing more plays while it is set, and the clock waits with it so that the rest keeps its
	/// pace. False once the recording has run out.
	bool Advance(uint32_t gameTime, bool pauseOnTrigger, bool& triggerReached, std::vector<Played>& played);

	[[nodiscard]] bool Ended() const { return _next >= _records.size(); }
	/// The next record to play
	[[nodiscard]] size_t Next() const { return _next; }
	/// The clock starts where the clock of another play of the same recording is: its records come due at the same times
	void KeepClockOf(const Playback& other) { _startTime = other._startTime; }

private:
	void Play(size_t recordsRead, bool& triggerReached, std::vector<Played>& played);

	std::span<const hnd::HNDRecord> _records;
	size_t _next {0};
	/// The first record's time and the game's time it started at
	int64_t _firstRecordTime {0};
	int64_t _startTime {0};
};

/// A demonstration as the turns and the frames play it. The turns play its records by the time of each turn alone, and
/// the scripts read only this side: whether the demonstration plays and whether it reached a mark. Thus the scripts get
/// the same answers at all frame rates. The frames play the same records by the time into the turn, for the cursor, the
/// buttons and the camera. The frames use the clock of the turns, and with a pause at the marks the frames hold at a
/// mark until the scripts have read it.
class SplitPlayback
{
public:
	/// Starts at the time of a turn. The first record plays at once on the two sides, and `played` gets it for the frames.
	SplitPlayback(std::span<const hnd::HNDRecord> records, uint32_t turnTime, bool pauseOnTrigger, std::vector<Played>& played);

	/// The side of the turns plays every record that the time of the turn has reached. A record with a mark sets
	/// `triggerReached`, which the scripts read; with the pause, nothing more plays while it is set. False once the side
	/// of the turns has run out.
	bool AdvanceTurn(uint32_t turnTime, bool& triggerReached);
	/// The side of the frames plays every record that the time into the turn has reached; `played` gets them. False once
	/// the side of the frames has run out.
	bool AdvanceFrame(uint32_t frameTime, std::vector<Played>& played);
	/// The scripts read a mark that the side of the turns reached. With the pause, this lets the two sides go on.
	void MarkRead() { ++_marksRead; }

	[[nodiscard]] bool TurnEnded() const { return _turn.Ended(); }
	[[nodiscard]] bool FrameEnded() const { return _frame.Ended(); }
	/// The next record that each side plays
	[[nodiscard]] size_t TurnNext() const { return _turn.Next(); }
	[[nodiscard]] size_t FrameNext() const { return _frame.Next(); }
	/// The side of the frames reached a mark that the scripts have not read yet, and holds there
	[[nodiscard]] bool FrameHeld() const { return _pauseOnTrigger && _frameMarks > _marksRead; }

private:
	Playback _turn;
	Playback _frame;
	bool _pauseOnTrigger {false};
	/// How many marks the scripts read, and how many marks the side of the frames reached
	uint32_t _marksRead {0};
	uint32_t _frameMarks {0};
};

/// Whether a demonstration that starts takes the leash off the player's creature. It does so unless the demonstration
/// keeps the hand as it is, and only when the leash is on and not tied to a thing.
[[nodiscard]] constexpr bool TakesOffLeash(bool withoutHandModify, bool leashOn, bool tiedToAThing)
{
	return !withoutHandModify && leashOn && !tiedToAThing;
}

} // namespace openblack::hand_demo
