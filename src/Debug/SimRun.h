/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <cstdint>

#include <chrono>
#include <filesystem>
#include <optional>
#include <span>
#include <vector>

/// The fast simulation mode for long tool runs, such as a creature's whole life. Each frame takes a fixed game time, so a
/// frame plays the same turns at any speed. In fast mode the game plays its frames one after the other as fast as the
/// processor can, and it draws none of them or only a few. Without the mode the game runs as usual.
namespace openblack::sim_run
{

/// How the game runs its frames
struct Settings
{
	/// Play the frames one after the other and draw only some of them
	bool fast {false};
	/// In fast mode, draw one frame in this many (0 for no frame)
	uint32_t drawEvery {0};
	/// Without fast mode, and with a fixed frame time: wait at the end of each frame until the frame takes its game
	/// time in real time too, so that the run goes at the normal speed
	bool paceToWallClock {false};
	/// Stop the game once it gets to this turn
	std::optional<uint32_t> untilTurn;
};

/// What the command line asks of a simulation run
struct Request
{
	Settings settings {};
	/// The game time of each frame in milliseconds, none for the wall clock
	std::optional<uint32_t> frameMs;
	/// The file to write the run record to, as JSON lines
	std::optional<std::filesystem::path> recordPath;
	/// Write a record each time this many turns go by
	uint32_t recordEvery {k_DefaultRecordEvery};
	/// The table of the longest time that a plan or an activity can stay the same, as JSON
	std::optional<std::filesystem::path> limitsPath;
	/// The records also keep the hash of the transforms of the entities of each component
	bool hashParts {false};
	/// Each land starts paused, before any of its frames, for a tool that steps it through the inspector
	bool startPaused {false};

	static constexpr uint32_t k_DefaultRecordEvery = 100;
};

/// The game time of each frame in fast mode when the command line gives none: five frames a turn, near the 60 frames a
/// second of the screen's refresh
constexpr uint32_t k_DefaultFastFrameMs = 20;

/// The longest fixed frame time, which is the longest time the inspector also accepts
constexpr uint32_t k_LongestFrameMs = 1000;

/// Whether the game draws this frame
[[nodiscard]] bool DrawsFrame(const Settings& settings, uint64_t frame);

/// Whether the run is at its end at this turn
[[nodiscard]] bool Finished(const Settings& settings, uint32_t turn);

/// How long the frame waits at its end so that it takes its fixed game time in real time; no wait in fast mode, without
/// a fixed frame time, or once the frame took that long or longer
[[nodiscard]] std::chrono::microseconds PaceWait(const Settings& settings, std::optional<uint32_t> frameMs,
                                                 std::chrono::microseconds frameTook);

/// The times of the turns and the frames since the last record
struct Timings
{
	uint32_t turns {0};
	float turnTotalMs {0.0f};
	float turnLongestMs {0.0f};
	uint32_t frames {0};
	float frameTotalMs {0.0f};
	float frameLongestMs {0.0f};
	/// The time of each stage of the profiler over the frames, by the stage's number
	std::vector<float> stageTotalMs;

	void AddTurn(float ms);
	void AddFrame(float ms);
	/// Adds the time that each stage took in a frame, by the stage's number
	void AddStages(std::span<const float> ms);
	[[nodiscard]] float TurnAverageMs() const;
	[[nodiscard]] float FrameAverageMs() const;
};

} // namespace openblack::sim_run
