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
#include <random>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "Creature/CreatureFight.h"
#include "Creature/FightTranscript.h"

/// The rules of the fight autopilot, a debug tool that fights for the player with the real mouse. The rules choose
/// what the hand does next. They do not move the pointer or press a button.
namespace openblack::fight_autopilot
{

enum class Policy : uint8_t
{
	/// Does what the guide's fight lesson asks at each stage: a blow, three quick blows, a block, a step, then hits
	Lesson,
	/// Hits only
	Aggressive,
	/// A random mix of hits, blocks and steps from a seed
	Random,
	/// A list of moves at given times
	Sequence,
};
[[nodiscard]] std::string_view Name(Policy policy);
[[nodiscard]] std::optional<Policy> PolicyFromName(std::string_view name);

/// What the hand does with one press
struct Intent
{
	enum class Kind : uint8_t
	{
		None,
		/// A press on the opponent's body, at a band
		Hit,
		/// A press on the player's own creature
		Block,
		/// A press on the arena's ground, in a direction from the player's creature
		Step,
	};
	Kind kind {Kind::None};
	creature_fight::Band band {creature_fight::Band::Mid};
	creature_fight::Step step {creature_fight::Step::Forward};
	/// How long the button stays down, in milliseconds of game time. A blow charges while the button is down.
	float holdMs {200.0f};

	bool operator==(const Intent& other) const = default;
};
[[nodiscard]] std::string Describe(const Intent& intent);

/// The stages of the guide's fight lesson, in the order that the lesson asks for them
enum class LessonStage : uint8_t
{
	/// A blow that makes the guide reel
	Hit,
	/// More than two blows in the queue at one time
	Hits,
	/// A block
	Block,
	/// A step
	Step,
	/// The bout, in which both creatures fight by themselves and the hand hits
	Bout,
};
[[nodiscard]] std::string_view Name(LessonStage stage);
[[nodiscard]] std::optional<LessonStage> LessonStageFromName(std::string_view name);

/// What the autopilot sees of the fight at a frame
struct Observation
{
	creature_fight::FightAction own {creature_fight::FightAction::Other};
	creature_fight::FightAction opponent {creature_fight::FightAction::Other};
	/// The blows in the queue of the player's creature, and all of its moves in the queue
	uint32_t queuedBlows {0};
	uint32_t queued {0};
};

/// The stage of the lesson after what the autopilot sees now. Each stage ends when its goal shows.
[[nodiscard]] LessonStage Advance(LessonStage stage, const Observation& seen);
/// The stage that the lesson's script waits for, from its reads of the fight in the transcript, or none when the script
/// has read nothing yet. The script reads what the opponent does in the first stage and the blows in the queue in the
/// second. It reads what the player's creature does in the third and the fourth: a block seen moves it to the fourth.
/// The script gives both creatures to the computer for the bout.
[[nodiscard]] std::optional<LessonStage> ScriptStage(const fight_transcript::Fight& fight, uint32_t playersCreature);

/// A move of a sequence and the game time in milliseconds after the start of the autopilot when it is due
struct SequenceMove
{
	float atMs {0.0f};
	Intent intent;

	bool operator==(const SequenceMove& other) const = default;
};
/// A sequence from text: moves divided by commas, each a time in seconds and a move with a colon between them
/// ("1.5:high,3:block,4:step_back"). The moves are high, middle, low, block, step_forward, step_back, step_left and
/// step_right. None when the text has an error.
[[nodiscard]] std::optional<std::vector<SequenceMove>> ParseSequence(std::string_view text);

/// The next press of a policy, or none. The autopilot asks only when the hand is free for a new press.
struct Choice
{
	Intent intent;
	/// The game time in milliseconds to wait after this press before the next one
	float waitMs {0.0f};
};
[[nodiscard]] Choice Choose(Policy policy, LessonStage stage, const Observation& seen, std::mt19937& random);
/// The next move of a sequence that is due at a game time, from the index of the next move
[[nodiscard]] std::optional<size_t> DueMove(std::span<const SequenceMove> sequence, size_t next, float elapsedMs);

/// The check of a fight lesson played by the autopilot, from the transcript of its fight: the index of the first
/// record of each taught stage, in the order of the lesson, and whether they occur in that order
struct LessonCheck
{
	std::optional<uint64_t> hit;
	std::optional<uint64_t> hits;
	std::optional<uint64_t> block;
	std::optional<uint64_t> step;
	[[nodiscard]] bool InOrder() const;
};
/// A hit is a blow of the player's creature that lands on the opponent. Hits are a queue of more than two blows. A block
/// and a step are actions of the player's creature.
[[nodiscard]] LessonCheck CheckLesson(const fight_transcript::Fight& fight, uint32_t playersCreature);

} // namespace openblack::fight_autopilot
