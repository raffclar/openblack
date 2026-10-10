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

namespace openblack::new_game_choice
{

/// The four ways a returning player can start a new game, in the order the start-of-game question lists them
enum class Choice : uint8_t
{
	StartNormally,
	SkipToCreatureSelect,
	SkipCreatureTutorial,
	KeepOldCreature,
};

inline constexpr size_t k_ChoiceCount = 4;

/// What the story's scripts are told about the player's choice. The first land's set-up reads them through
/// CAN_SKIP_TUTORIAL, CAN_SKIP_CREATURE_TRAINING and IS_KEEPING_OLD_CREATURE.
struct TutorialSkip
{
	bool skipTutorial {false};
	bool skipCreatureTraining {false};
	bool keepOldCreature {false};

	[[nodiscard]] constexpr bool operator==(const TutorialSkip&) const = default;
};

/// Each choice skips everything the one before it skips, and one thing more
[[nodiscard]] constexpr TutorialSkip SkipFor(Choice choice) noexcept
{
	const auto level = static_cast<uint8_t>(choice);
	return {.skipTutorial = level >= 1, .skipCreatureTraining = level >= 2, .keepOldCreature = level >= 3};
}

/// The question is only put to a player who has played before: when there are at least two player profiles, or the
/// current profile already has a creature
[[nodiscard]] constexpr bool AsksAtNewGame(size_t profileCount, bool currentProfileHasCreature) noexcept
{
	return profileCount >= 2 || currentProfileHasCreature;
}

/// A selection brought into the range of the choices
[[nodiscard]] constexpr Choice ClampChoice(int selection) noexcept
{
	return static_cast<Choice>(std::clamp(selection, 0, static_cast<int>(k_ChoiceCount) - 1));
}

} // namespace openblack::new_game_choice
