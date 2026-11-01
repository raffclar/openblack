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
#include <array>
#include <optional>
#include <string_view>

#include "Enums.h"

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

/// Whether the start-of-game question shows a choice. The question shows the debug answer (start with the Cow) only in
/// a build with the developer tools, so that the question of a player has only the answers of the original game.
[[nodiscard]] constexpr bool IsShown(Choice choice, bool currentProfileHasCreature, bool developerTools) noexcept
{
	return IsAvailable(choice, currentProfileHasCreature) && (developerTools || choice != Choice::DebugStartWithCow);
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
	/// Debug only, not in the original game: the species of the creature that a story started directly on a later land
	/// makes for the player, because no creature came through the vortex. With nothing, the land plays as the game
	/// plays it.
	std::optional<CreatureType> directCreature {std::nullopt};

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

/// Whether the game's first land starts the story: its scripts and the start-of-game question. A playground is started
/// as the playground menu starts one, with the scripts made ready but none of the story begun and nothing asked. A
/// network game also starts no story and asks nothing.
[[nodiscard]] constexpr bool FirstLandStartsStory(bool playground, bool networkGame) noexcept
{
	return !playground && !networkGame;
}

/// The story's top script starts only when the new game opens on the first land of the story. A new game on another
/// land starts none of the story, and the start-of-game question comes all the same.
[[nodiscard]] constexpr bool LandStartsStoryScript(int32_t landNumber) noexcept
{
	return landNumber == 1;
}

/// Whether the debug quickstart makes the Cow when a land loads the kept creature. Only the first land makes it. On a
/// later land the Cow came through the vortex as the kept creature, and the land loads it at the given place.
[[nodiscard]] constexpr bool MakesDebugCow(const TutorialSkip& skip, int32_t landNumber) noexcept
{
	return skip.startWithCow && landNumber == 1;
}

/// The name a developer gives a species, in lower case without spaces
struct DirectCreatureName
{
	std::string_view name;
	CreatureType species;
};

inline constexpr std::array<DirectCreatureName, 17> k_DirectCreatureNames {{
    {.name = "cow", .species = CreatureType::Cow},
    {.name = "tiger", .species = CreatureType::Tiger},
    {.name = "leopard", .species = CreatureType::Leopard},
    {.name = "wolf", .species = CreatureType::Wolf},
    {.name = "lion", .species = CreatureType::Lion},
    {.name = "horse", .species = CreatureType::Horse},
    {.name = "tortoise", .species = CreatureType::Tortoise},
    {.name = "zebra", .species = CreatureType::Zebra},
    {.name = "brownbear", .species = CreatureType::BrownBear},
    {.name = "polarbear", .species = CreatureType::PolarBear},
    {.name = "sheep", .species = CreatureType::Sheep},
    {.name = "chimp", .species = CreatureType::Chimp},
    {.name = "ogre", .species = CreatureType::Ogre},
    {.name = "mandrill", .species = CreatureType::Mandrill},
    {.name = "rhino", .species = CreatureType::Rhino},
    {.name = "gorilla", .species = CreatureType::Gorilla},
    {.name = "giantape", .species = CreatureType::GiantApe},
}};

/// The species that a developer's name stands for; nothing for a name that is none of them
[[nodiscard]] constexpr std::optional<CreatureType> ParseDirectCreature(std::string_view text) noexcept
{
	for (const auto& entry : k_DirectCreatureNames)
	{
		if (entry.name == text)
		{
			return entry.species;
		}
	}
	return std::nullopt;
}

/// The land whose control script a developer's direct start runs: a land after the first, when the developer asked for
/// a creature. The story's top script starts at the step of this land. The first land has the story's top script from
/// its start, and a land with no such request plays as the game plays it.
[[nodiscard]] constexpr std::optional<int32_t> DirectStartLand(const std::optional<NewGameStart>& start,
                                                               int32_t landNumber) noexcept
{
	if (!start.has_value() || !start->directCreature.has_value() || LandStartsStoryScript(landNumber))
	{
		return std::nullopt;
	}
	return landNumber;
}

/// The species of the creature that a direct start of a later land makes for the player. Nothing when the developer
/// asked for none, on the first land, when a creature came through the vortex, or when the player has one already: the
/// land then plays as the game plays it.
[[nodiscard]] constexpr std::optional<CreatureType> DirectStartCreature(const std::optional<NewGameStart>& start,
                                                                        int32_t landNumber, bool creatureLoaded,
                                                                        bool playerHasCreature) noexcept
{
	if (!DirectStartLand(start, landNumber).has_value() || creatureLoaded || playerHasCreature)
	{
		return std::nullopt;
	}
	return start->directCreature;
}

/// Whether a land's load of the player's creature can read the creature that the profile's file keeps from an earlier
/// game. A direct start of a later land cannot: only a creature that came through the vortex in this game is used.
/// Thus each run of a direct start with the same seed makes the same creature, whatever an earlier run wrote.
[[nodiscard]] constexpr bool LoadsCreatureOfEarlierGame(const std::optional<NewGameStart>& start, int32_t landNumber) noexcept
{
	return !DirectStartLand(start, landNumber).has_value();
}

/// "ask", "normal", "creature" (straight to choosing a creature), "story" (skip all of the creature's tutorial too)
/// or "old" (keep the old creature) or "cow" (debug: start with the Cow); nothing for anything else
[[nodiscard]] constexpr std::optional<NewGameStart> ParseNewGameStart(std::string_view text) noexcept
{
	if (text == "ask")
	{
		return NewGameStart {.ask = true, .answer = std::nullopt};
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
