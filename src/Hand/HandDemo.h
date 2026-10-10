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

#include <span>
#include <vector>

#include <HNDFile.h>
#include <glm/vec2.hpp>

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

private:
	void Play(size_t recordsRead, bool& triggerReached, std::vector<Played>& played);

	std::span<const hnd::HNDRecord> _records;
	size_t _next {0};
	/// The first record's time and the game's time it started at
	int64_t _firstRecordTime {0};
	int64_t _startTime {0};
};

} // namespace openblack::hand_demo
