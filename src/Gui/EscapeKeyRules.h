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

#include <chrono>
#include <optional>

/// What the Escape key does in play, once a full-screen video and the game's open dialogs have had their turn. Free of
/// game state so that the game and the tests share it.
namespace openblack::escape_key
{

/// Escape doesn't open the menu again until this long after it last closed
inline constexpr std::chrono::milliseconds k_MenuReopenGuard {300};

/// The land number of the Gods' Playground
inline constexpr int32_t k_GodsPlaygroundLand = 6;

/// What the game is doing as Escape is pressed
struct Situation
{
	/// Shift or Ctrl is held
	bool shiftOrCtrl {false};
	/// A script has the cinema bars in for its cut scene
	bool scriptHoldsCinemaBars {false};
	/// A help script holds the dialogue
	bool helpScriptHoldsDialogue {false};
	/// A single player game on the Gods' Playground
	bool inGodsPlayground {false};
	/// No screen fade is under way and the screen isn't faded
	bool screenClear {true};
	/// One of the Gods' Playground's lessons is running, as it is when the player has just gone there
	bool playgroundLessonRunning {false};
	/// How long ago the menu last closed, nothing if it hasn't since the game started
	std::optional<std::chrono::milliseconds> sinceMenuClosed;
};

enum class Outcome : uint8_t
{
	Nothing,
	OpenMenu,
	/// "Are you sure you want to leave the Gods' Playground?", the game paused for the answer
	AskLeavePlayground,
	/// The Playground's lesson ends: its scripts start again with the land's own running control
	EndPlaygroundLesson,
};

struct Response
{
	Outcome outcome {Outcome::Nothing};
	/// The help scripts stop, and both advisors are cut short in what they say
	bool stopHelpScripts {false};
	/// The screen's fade is put back to clear at once
	bool clearScreenFade {false};
};

/// Whether the menu may open, given how long ago it last closed
[[nodiscard]] bool MenuMayOpen(std::optional<std::chrono::milliseconds> sinceMenuClosed) noexcept;

/// What Escape does. While a script has the cinema bars in, nothing: there is no skipping a script's cut scene
[[nodiscard]] Response Respond(const Situation& situation) noexcept;

} // namespace openblack::escape_key
