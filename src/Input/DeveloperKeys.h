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

#include <SDL_keycode.h>
#include <SDL_scancode.h>

#include "Common/Platform.h"
#include "Input/KeyBindings.h"
#include "Input/PlatformKeys.h"

/// The keys of the developer tools: the editor, the console and the renderer statistics. The game's keys go first. A
/// developer key never uses a key that the game uses, and a key that the player binds to an action of the game stops
/// the developer key.
namespace openblack::input::developer_keys
{

/// True when the game uses the key itself, with no binding: the menu, the trip question, the miracle keys, the pause,
/// the bookmarks, the town and bookmark moves, the dialogue skip, and the key that the scripts ask about. The game uses
/// most of these keys with any modifier, thus every modifier counts.
[[nodiscard]] constexpr bool IsFixedGameKey(Platform platform, SDL_Scancode key, platform_keys::Modifiers held) noexcept
{
	switch (key)
	{
	case SDL_SCANCODE_ESCAPE:
	case SDL_SCANCODE_F2:
	case SDL_SCANCODE_R:
	case SDL_SCANCODE_P:
	case SDL_SCANCODE_M:
	case SDL_SCANCODE_1:
	case SDL_SCANCODE_2:
	case SDL_SCANCODE_3:
	case SDL_SCANCODE_4:
	case SDL_SCANCODE_5:
	case SDL_SCANCODE_6:
	case SDL_SCANCODE_7:
	case SDL_SCANCODE_TAB:
	case SDL_SCANCODE_BACKSPACE:
	case SDL_SCANCODE_KP_ENTER:
	case SDL_SCANCODE_SPACE:
		return true;
	case SDL_SCANCODE_T:
		// Ctrl, Alt and T keep a picture of the creatures
		return held.ctrl && held.alt;
	case SDL_SCANCODE_MINUS:
	case SDL_SCANCODE_EQUALS:
		// On the web, Alt and these keys change the speed of the game, with no Shift or Ctrl
		return platform == Platform::Web && held.alt && !held.shift && !held.ctrl;
	default:
		return false;
	}
}

/// The modifiers in the modifier state of SDL
[[nodiscard]] constexpr platform_keys::Modifiers HeldModifiers(uint16_t sdlModifiers) noexcept
{
	return {
	    .shift = (sdlModifiers & KMOD_SHIFT) != 0,
	    .ctrl = (sdlModifiers & KMOD_CTRL) != 0,
	    .alt = (sdlModifiers & KMOD_ALT) != 0,
	};
}

/// True when the game uses the key with these modifiers: an action of the game has the key in the bindings, or the
/// game uses the key itself
[[nodiscard]] inline bool GameUsesKey(std::span<const KeyBinding> bindings, Platform platform, SDL_Scancode key,
                                      uint16_t sdlModifiers) noexcept
{
	return IsFixedGameKey(platform, key, HeldModifiers(sdlModifiers)) ||
	       ActionsForKeyDown(bindings, key, sdlModifiers) != BindableActionMap::NONE;
}

/// The key that opens and closes the console
inline constexpr SDL_Scancode k_ConsoleKey = SDL_SCANCODE_GRAVE;

/// The key that shows or hides the statistics of the renderer, as a scancode
[[nodiscard]] constexpr SDL_Scancode RendererStatisticsScancode(Platform platform) noexcept
{
	return platform == Platform::Web ? SDL_SCANCODE_INSERT : SDL_SCANCODE_F11;
}

/// What a key does while the editor is open
enum class EditorKeyAction : uint8_t
{
	/// Remove the picked things from the land
	Remove,
	/// Make a copy beside each picked thing
	Duplicate,
	Undo,
	Redo,
	/// Bring the picked things into view
	Frame,
	/// Turn the camera around the picked thing, or stop it
	Orbit,
	/// Follow the picked thing with the camera, or stop it
	Follow,
	SelectTool,
	MoveTool,
	RotateTool,
	/// The game's own hand
	HandTool,
	/// Play one game turn
	StepTurn,
	/// Turn the thing to place to the left
	TurnPlacementLeft,
	/// Turn the thing to place to the right
	TurnPlacementRight,
	CameraFaster,
	CameraSlower,
	/// Put the speed of the camera back to the speed of the game
	CameraSpeedReset,
	/// Let go of the thing to place, the drag, the camera mode or the picks, in this order
	LetGo,
};

/// What a key with these modifiers does while the editor is open. None of these keys is a key of the game.
[[nodiscard]] constexpr std::optional<EditorKeyAction> EditorKeyActionFor(SDL_Scancode key,
                                                                          platform_keys::Modifiers held) noexcept
{
	switch (key)
	{
	case SDL_SCANCODE_DELETE:
		return EditorKeyAction::Remove;
	case SDL_SCANCODE_D:
		return held.ctrl ? EditorKeyAction::Duplicate : EditorKeyAction::HandTool;
	case SDL_SCANCODE_LEFTBRACKET:
		if (held.ctrl)
		{
			return held.shift ? EditorKeyAction::Redo : EditorKeyAction::Undo;
		}
		return EditorKeyAction::TurnPlacementLeft;
	case SDL_SCANCODE_RIGHTBRACKET:
		return held.ctrl ? EditorKeyAction::Redo : EditorKeyAction::TurnPlacementRight;
	case SDL_SCANCODE_Y:
		return held.ctrl ? EditorKeyAction::Redo : EditorKeyAction::RotateTool;
	case SDL_SCANCODE_HOME:
		return EditorKeyAction::Frame;
	case SDL_SCANCODE_O:
		return held.shift ? EditorKeyAction::Follow : EditorKeyAction::Orbit;
	case SDL_SCANCODE_W:
		return EditorKeyAction::SelectTool;
	case SDL_SCANCODE_E:
		return EditorKeyAction::MoveTool;
	case SDL_SCANCODE_PERIOD:
		return EditorKeyAction::StepTurn;
	case SDL_SCANCODE_EQUALS:
	case SDL_SCANCODE_KP_PLUS:
		return held.ctrl ? std::optional(EditorKeyAction::CameraFaster) : std::nullopt;
	case SDL_SCANCODE_MINUS:
	case SDL_SCANCODE_KP_MINUS:
		return held.ctrl ? std::optional(EditorKeyAction::CameraSlower) : std::nullopt;
	case SDL_SCANCODE_0:
	case SDL_SCANCODE_KP_0:
		return held.ctrl ? std::optional(EditorKeyAction::CameraSpeedReset) : std::nullopt;
	case SDL_SCANCODE_X:
		return EditorKeyAction::LetGo;
	default:
		return std::nullopt;
	}
}

} // namespace openblack::input::developer_keys
