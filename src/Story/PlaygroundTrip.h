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

#include <string_view>

#include "Input/BoxKeyboard.h"

/// The trip to the Gods' Playground and back. In single player, F2 asks whether the player wants to go there. Yes saves
/// the game into the slot of the save on exit, stops every script, loads the Playground's land with no loading screen and
/// starts its lessons. Escape on the Playground first ends the lesson that runs. The next Escape asks whether the player
/// wants to leave, and Yes loads the saved game again. These rules keep no game state, so the game and the tests share
/// them.
namespace openblack::playground_trip
{

/// The land number of the Gods' Playground
inline constexpr int32_t k_PlaygroundLand = 6;
/// The script of the Playground's land
inline constexpr std::string_view k_LandScript = "Scripts/LandT.txt";
/// The script that runs the lessons one after the other
inline constexpr std::string_view k_LessonsScript = "LandControlT";
/// The script that the end of a lesson starts: the scrolls that run each lesson again
inline constexpr std::string_view k_PracticeScript = "KeepRunningControl";
/// "Are you sure you want to go to the Gods' Playground?"
inline constexpr std::string_view k_GoQuestion = "HELP_TEXT_TUTORIAL_LAND_116";
/// "Are you sure you want to leave the Gods' Playground?"
inline constexpr std::string_view k_LeaveQuestion = "HELP_TEXT_TUTORIAL_LAND_117";
/// The number of the developer function that each lesson calls as it starts
inline constexpr int32_t k_LessonStartsFunction = 5;

/// The two questions of the trip
enum class Question
{
	/// F2: whether the player wants to go to the Playground
	Go,
	/// Escape on the Playground: whether the player wants to leave it
	Leave,
};

/// What the game does as the player presses F2
struct GoSituation
{
	/// A skirmish on a playground land of the land menu
	bool skirmish {false};
	bool networkGame {false};
	/// The player is in the temple
	bool inTemple {false};
	/// The land number of the open land
	int32_t land {0};
	/// A box is on the screen and takes the input
	bool boxUp {false};
	/// A script has the cinema bars in for its cut scene
	bool scriptHoldsCinemaBars {false};
};

/// Whether F2 asks the question. F2 does nothing in a skirmish, in a network game, in the temple, on the Playground,
/// while a box is up, or while a script has the cinema bars in.
[[nodiscard]] bool MayAskToGo(const GoSituation& situation) noexcept;

/// The boxes of the game's menu. Each page of the menu is a box of its own, over the main menu.
enum class MenuBox
{
	Main,
	Options,
	Advanced,
	Players,
	Controls,
	Statistics,
	/// The winning conditions of a network game
	Conditions,
};

/// The game's menu as the player presses F2
struct MenuSituation
{
	MenuBox box {MenuBox::Main};
	/// The box shows a question or a message, or waits for a key or a mouse button
	bool questionUp {false};
	/// The player changed the detail, and the game did not tell the player yet that the change starts at the next start
	bool detailUntold {false};
	/// When the player leaves the Players page, the game starts again
	bool restartPending {false};
};

/// Whether F2 can ask the question over the open menu. F2 asks over the main menu and over each page that Escape can
/// close. While a box shows a question, F2 does not ask.
[[nodiscard]] bool MenuLetsAsk(const MenuSituation& menu) noexcept;

/// Whether a box other than the menu stops F2 from asking the question. Each box that takes the keyboard stops F2, but the
/// land information box: Escape can close it, and it never holds a message of its own. The open menu has its own rule.
[[nodiscard]] bool OtherBoxRefuses(input::box_keyboard::Boxes boxes) noexcept;

/// Whether the box pauses the game when it comes back after the question of the trip. Only the main menu pauses the
/// game each time that it comes back.
[[nodiscard]] bool PausesWhenBack(MenuBox box) noexcept;

/// The lessons of the Playground. Each lesson tells the game that it runs as it starts. Escape asks for the end of the
/// lesson that runs. The end comes at the end of the next game turn.
struct Lessons
{
	/// A lesson runs. The trip to the Playground sets it too.
	bool running {false};
	/// Escape asked for the end of the lesson
	bool endAsked {false};
};

/// A lesson starts, or the trip arrives on the Playground
void StartLesson(Lessons& lessons) noexcept;
/// Escape asks for the end of the lesson that runs. Returns false, and asks for nothing, when no lesson runs.
bool AskEnd(Lessons& lessons) noexcept;
/// At the end of a game turn: whether the lesson ends now. When it does, no lesson runs after this and the ask goes.
[[nodiscard]] bool TakeEnd(Lessons& lessons) noexcept;
/// The player leaves the Playground: no lesson runs
void Leave(Lessons& lessons) noexcept;

} // namespace openblack::playground_trip
