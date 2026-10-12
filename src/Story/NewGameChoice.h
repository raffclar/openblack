/*******************************************************************************
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

#include <algorithm>
#include <optional>
#include <string_view>

namespace openblack::new_game_choice
{

/// The ways a player can start a new game, in the order the start-of-game question lists them. The last is not in the
/// original game: a debug quickstart that skips the creature choice and gives the player the Cow
enum class Choice : uint8_t
{
	StartNormally,
	SkipToCreatureSelect,
	SkipCreatureTutorial,
	KeepOldCreature,
	DebugStartWithCow,
};

inline constexpr size_t k_ChoiceCount = 5;

/// What the story's scripts are told about the player's choice. The first land's set-up reads them through
/// CAN_SKIP_TUTORIAL, CAN_SKIP_CREATURE_TRAINING and IS_KEEPING_OLD_CREATURE.
struct TutorialSkip
{
	bool skipTutorial {false};
	bool skipCreatureTraining {false};
	bool keepOldCreature {false};
	/// Not in the original game: the creature choice is skipped and the Cow is the player's creature
	bool startWithCow {false};

	[[nodiscard]] constexpr bool operator==(const TutorialSkip&) const = default;
};

/// Each choice skips everything the one before it skips, and one thing more
[[nodiscard]] constexpr TutorialSkip SkipFor(Choice choice) noexcept
{
	if (choice == Choice::DebugStartWithCow)
	{
		// Goes the way keeping an old creature does (the glade's choice is skipped), with the Cow as the creature kept
		return {.skipTutorial = true, .skipCreatureTraining = true, .keepOldCreature = true, .startWithCow = true};
	}
	const auto level = static_cast<uint8_t>(choice);
	return {.skipTutorial = level >= 1, .skipCreatureTraining = level >= 2, .keepOldCreature = level >= 3};
}

/// Whether a choice can be taken: keeping the old creature needs a creature the current profile has kept. The question
/// itself is put at every new game (the original only asked a player with two or more profiles or a kept creature).
[[nodiscard]] constexpr bool IsAvailable(Choice choice, bool currentProfileHasCreature) noexcept
{
	return choice != Choice::KeepOldCreature || currentProfileHasCreature;
}

/// A choice brought to one that can be taken. Keeping the old creature with none kept becomes skipping all of the
/// creature tutorial, which is what the first land's scripts do with that answer when there is no creature to keep.
[[nodiscard]] constexpr Choice AvailableChoice(Choice choice, bool currentProfileHasCreature) noexcept
{
	return IsAvailable(choice, currentProfileHasCreature) ? choice : Choice::SkipCreatureTutorial;
}

/// A selection brought into the range of the choices
[[nodiscard]] constexpr Choice ClampChoice(int selection) noexcept
{
	return static_cast<Choice>(std::clamp(selection, 0, static_cast<int>(k_ChoiceCount) - 1));
}

/// How a developer or an agent starts a new game from the command line or the inspector (not in the original game):
/// with the start-of-game question answered at once, the story then going exactly as it does after the player answers
/// it, or asked it as every new game is
struct NewGameStart
{
	bool ask {false};
	std::optional<Choice> answer;

	[[nodiscard]] constexpr bool operator==(const NewGameStart&) const = default;
};

/// How a new game's start-of-game question is answered: at once with the developers' start's answer (one the box would
/// offer), or, with nothing, put to the player, as every new game puts it
[[nodiscard]] constexpr std::optional<Choice> AnswerAtOnce(const std::optional<NewGameStart>& start,
                                                           bool currentProfileHasCreature) noexcept
{
	if (!start.has_value() || !start->answer.has_value())
	{
		return std::nullopt;
	}
	return AvailableChoice(*start->answer, currentProfileHasCreature);
}

/// "ask", "normal", "creature" (straight to choosing a creature), "story" (skip all of the creature's tutorial too)
/// or "old" (keep the old creature) or "cow" (debug: start with the Cow); nothing for anything else
[[nodiscard]] constexpr std::optional<NewGameStart> ParseNewGameStart(std::string_view text) noexcept
{
	if (text == "ask")
	{
		return NewGameStart {.ask = true};
	}
	if (text == "normal")
	{
		return NewGameStart {.answer = Choice::StartNormally};
	}
	if (text == "creature")
	{
		return NewGameStart {.answer = Choice::SkipToCreatureSelect};
	}
	if (text == "story")
	{
		return NewGameStart {.answer = Choice::SkipCreatureTutorial};
	}
	if (text == "old")
	{
		return NewGameStart {.answer = Choice::KeepOldCreature};
	}
	if (text == "cow")
	{
		return NewGameStart {.answer = Choice::DebugStartWithCow};
	}
	return std::nullopt;
}

/// The name ParseNewGameStart reads a choice's answer by: "normal", "creature", "story" or "old"
[[nodiscard]] constexpr std::string_view StartName(Choice choice) noexcept
{
	switch (choice)
	{
	case Choice::SkipToCreatureSelect:
		return "creature";
	case Choice::SkipCreatureTutorial:
		return "story";
	case Choice::KeepOldCreature:
		return "old";
	case Choice::DebugStartWithCow:
		return "cow";
	case Choice::StartNormally:
	default:
		return "normal";
	}
}

} // namespace openblack::new_game_choice
