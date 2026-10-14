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
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "FightAutopilotRules.h"

namespace openblack::fight_autopilot
{

struct Settings
{
	Policy policy {Policy::Lesson};
	/// The seed of the random policy, and of the bands that the other policies choose at random
	uint32_t seed {1};
	/// The stage of the lesson to start at; the first stage when there is none
	std::optional<LessonStage> stage;
	/// The moves of the sequence policy
	std::vector<SequenceMove> sequence;
	/// Whether the autopilot shows the fight with a double click into the arena, as a player does, when the camera does
	/// not show it
	bool enterView {true};
};

/// One input that the autopilot gave, or one result that it saw
struct LogEntry
{
	uint64_t frame {0};
	uint32_t turn {0};
	/// The game time in milliseconds since the autopilot started
	double elapsedMs {0.0};
	/// What it was: "start", "stop", "view", "aim", "press", "release", "result", "stage", "lesson_check" and others
	std::string event;
	std::string detail;
	/// The pixel of the pointer, for an input
	std::optional<std::pair<int32_t, int32_t>> pointer;
};

struct Status
{
	bool active {false};
	Policy policy {Policy::Lesson};
	LessonStage stage {LessonStage::Hit};
	/// What the hand does now: "waiting", "showing_fight", "aiming", "holding" or "cooling"
	std::string phase;
	uint32_t presses {0};
	uint32_t accepted {0};
	uint32_t landed {0};
	uint32_t blocked {0};
	uint32_t missed {0};
	/// The fight in the transcript that the autopilot plays, and the check of the lesson in it
	std::optional<uint32_t> fight;
	std::optional<LessonCheck> lesson;
};

/// The fight autopilot: a debug tool that fights for the player with the real mouse, in builds with the inspector. It
/// moves the pointer and presses the buttons as the inspector's input does, so the hand's real path decides what each
/// press does. It never changes the fight itself.
class AutopilotInterface
{
public:
	virtual ~AutopilotInterface() = default;
	/// Starts with the settings; empty when it started, else why not
	virtual std::string Start(Settings settings) = 0;
	virtual void Stop(std::string_view why) = 0;
	[[nodiscard]] virtual Status GetStatus() const = 0;
	/// The log, the oldest entry first
	[[nodiscard]] virtual std::span<const LogEntry> GetLog() const = 0;
	virtual void ClearLog() = 0;
};

} // namespace openblack::fight_autopilot
