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

#include <array>
#include <optional>
#include <span>
#include <variant>

#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

#include "BindableActions.h"
#include "InputLock.h"
#include "KeyBindings.h"

union SDL_Event;

namespace openblack::input
{

class GameActionInterface
{
	template <typename T, typename... Args>
	static T GetAnyAccumulateActions(Args... actions)
	{
		T accumulator = T::NONE;
		((accumulator = static_cast<T>(static_cast<std::underlying_type_t<T>>(accumulator) |
		                               static_cast<std::underlying_type_t<T>>(actions))),
		 ...);
		return accumulator;
	}

public:
	// clang-format is having trouble with requires
	// clang-format off
	template <typename... Args>
	    requires(... && (std::is_same_v<Args, BindableActionMap> || std::is_same_v<Args, UnbindableActionMap>))
	[[nodiscard]] bool GetAny(Args... actions) const
	{
		const auto bindableAccumulator = GetAnyAccumulateActions<BindableActionMap>(actions...);
		const auto unbindableAccumulator = GetAnyAccumulateActions<UnbindableActionMap>(actions...);
		return GetBindable(bindableAccumulator) || GetUnbindable(unbindableAccumulator);
	}

	template <typename... Args>
	    requires(... && (std::is_same_v<Args, BindableActionMap> || std::is_same_v<Args, UnbindableActionMap>))
	[[nodiscard]] bool GetChangedAny(Args... actions) const
	{
		const auto bindableAccumulator = GetAnyAccumulateActions<BindableActionMap>(actions...);
		const auto unbindableAccumulator = GetAnyAccumulateActions<UnbindableActionMap>(actions...);
		return GetBindableChanged(bindableAccumulator) || GetUnbindableChanged(unbindableAccumulator);
	}

	template <typename... Args>
	    requires(... && (std::is_same_v<Args, BindableActionMap> || std::is_same_v<Args, UnbindableActionMap>))
	[[nodiscard]] bool GetRepeatAny(Args... actions) const
	{
		const auto bindableAccumulator = GetAnyAccumulateActions<BindableActionMap>(actions...);
		const auto unbindableAccumulator = GetAnyAccumulateActions<UnbindableActionMap>(actions...);
		return GetBindableRepeat(bindableAccumulator) || GetUnbindableRepeat(unbindableAccumulator);
	}

	template <typename... Args>
	    requires(... && (std::is_same_v<Args, BindableActionMap> || std::is_same_v<Args, UnbindableActionMap>))
	[[nodiscard]] bool GetAll(Args... actions) const
	{
		return (Get(actions) && ...);
	}

	template <typename... Args>
	    requires(... && (std::is_same_v<Args, BindableActionMap> || std::is_same_v<Args, UnbindableActionMap>))
	[[nodiscard]] bool GetChangedAll(Args... actions) const
	{
		return (GetChanged(actions) && ...);
	}

	template <typename... Args>
	    requires(... && (std::is_same_v<Args, BindableActionMap> || std::is_same_v<Args, UnbindableActionMap>))
	[[nodiscard]] bool GetRepeatAll(Args... actions) const
	{
		return (GetRepeat(actions) && ...);
	}
	// clang-format on

	[[nodiscard]] bool Get(ActionMap action) const
	{
		return std::visit(
		    [&](auto&& arg) {
			    using T = std::decay_t<decltype(arg)>;
			    if constexpr (std::is_same_v<T, BindableActionMap>)
			    {
				    return GetBindable(arg);
			    }
			    else if constexpr (std::is_same_v<T, UnbindableActionMap>)
			    {
				    return GetUnbindable(arg);
			    }
		    },
		    action);
	}

	[[nodiscard]] bool GetChanged(ActionMap action) const
	{
		return std::visit(
		    [&](auto&& arg) {
			    using T = std::decay_t<decltype(arg)>;
			    if constexpr (std::is_same_v<T, BindableActionMap>)
			    {
				    return GetBindableChanged(arg);
			    }
			    else if constexpr (std::is_same_v<T, UnbindableActionMap>)
			    {
				    return GetUnbindableChanged(arg);
			    }
		    },
		    action);
	}

	[[nodiscard]] bool GetRepeat(ActionMap action) const
	{
		return std::visit(
		    [&](auto&& arg) {
			    using T = std::decay_t<decltype(arg)>;
			    if constexpr (std::is_same_v<T, BindableActionMap>)
			    {
				    return GetBindableRepeat(arg);
			    }
			    else if constexpr (std::is_same_v<T, UnbindableActionMap>)
			    {
				    return GetUnbindableRepeat(arg);
			    }
		    },
		    action);
	}

	[[nodiscard]] virtual bool GetBindable(BindableActionMap action) const = 0;
	[[nodiscard]] virtual bool GetUnbindable(UnbindableActionMap action) const = 0;
	[[nodiscard]] virtual bool GetBindableChanged(BindableActionMap action) const = 0;
	[[nodiscard]] virtual bool GetUnbindableChanged(UnbindableActionMap action) const = 0;
	[[nodiscard]] virtual bool GetBindableRepeat(BindableActionMap action) const = 0;
	[[nodiscard]] virtual bool GetUnbindableRepeat(UnbindableActionMap action) const = 0;
	[[nodiscard]] virtual glm::uvec2 GetMousePosition() const = 0;
	[[nodiscard]] virtual glm::ivec2 GetMouseDelta() const = 0;
	/// Mouse wheel notches turned this frame, positive away from the user
	[[nodiscard]] virtual float GetMouseWheelDelta() const = 0;
	[[nodiscard]] virtual std::array<std::optional<glm::vec3>, 2> GetHandPositions() const = 0;

	/// The options screen's actions and what each is bound to now
	[[nodiscard]] virtual std::span<const KeyBinding> GetKeyBindings() const { return k_DefaultKeyBindings; }
	/// Binds an action to a key chord, or unbinds its key
	virtual void SetKeyBinding([[maybe_unused]] BindableActionMap action, [[maybe_unused]] std::optional<KeyChord> key) {}
	/// Puts every binding back to the game's defaults
	virtual void ResetKeyBindings() {}
	/// Presses an action's key for one frame, as if the player had, so its handling can be tried out
	virtual void QueuePress([[maybe_unused]] BindableActionMap action) {}
	/// Whether presses are waiting to be made
	[[nodiscard]] virtual bool HasQueuedPresses() const { return false; }

	/// A pointer the testbed's scenarios move and press in place of the mouse: where it is in the window and the
	/// buttons held, as SDL's button masks
	struct ScriptedPointer
	{
		glm::ivec2 position {0, 0};
		uint32_t buttons {0};
	};
	/// Reads the scripted pointer in place of the mouse, or the mouse again
	virtual void SetScriptedPointer([[maybe_unused]] std::optional<ScriptedPointer> pointer) {}
	/// While set (a hand demonstration plays), the player's own mouse moves, buttons and wheel don't reach the game; the
	/// game's own injected ones still do
	virtual void SetPlayerMouseBlocked([[maybe_unused]] bool blocked) {}
	[[nodiscard]] virtual bool IsPlayerMouseBlocked() const { return false; }
	[[nodiscard]] virtual std::optional<ScriptedPointer> GetScriptedPointer() const { return std::nullopt; }
	/// While an agent drives the game through the debug inspector, the player's mouse and keyboard are kept out (see
	/// InputLock): their events are dropped before the game, the debug windows and the camera see them, and the pointer
	/// stays where the game's own input last put it
	virtual void SetInputLockMode([[maybe_unused]] LockMode mode) {}
	[[nodiscard]] virtual LockMode GetInputLockMode() const { return LockMode::Unlocked; }
	/// Once a frame with the inspector: whether a client is connected, and the seconds since the last frame
	virtual void UpdateInputLock([[maybe_unused]] bool clientConnected, [[maybe_unused]] float seconds) {}
	/// Whether the player's input is kept out now
	[[nodiscard]] virtual bool IsPlayerInputBlocked() const { return false; }
	/// Whether an event from the window's queue reaches the game, under the lock
	[[nodiscard]] virtual bool AdmitEvent([[maybe_unused]] const SDL_Event& event) { return true; }
	/// Where the pointer the game follows is in the window, and its buttons held as SDL's masks: the mouse's, the scripted
	/// pointer's, or none held while the player's devices are kept out
	[[nodiscard]] virtual glm::ivec2 GetPointerPosition() const { return glm::ivec2(GetMousePosition()); }
	[[nodiscard]] virtual uint32_t GetPointerButtons() const { return 0; }
	/// A key held by a script or the debug inspector rather than on the keyboard, by its SDL scancode: it stays held until
	/// its own key-up comes, though the keyboard says it is up
	virtual void HoldScriptedKey([[maybe_unused]] int scancode, [[maybe_unused]] bool held) {}
	/// Puts the cursor, and the pointer, somewhere in the window, as the camera does dragging round the screen's edge
	virtual void WarpCursor([[maybe_unused]] glm::ivec2 position) {}
	/// Where the cursor was put this frame, if it was
	[[nodiscard]] virtual std::optional<glm::ivec2> GetCursorWarp() const { return std::nullopt; }
	/// Actions the land's scripts don't allow now, which read as not held
	virtual void SetBlockedActions([[maybe_unused]] BindableActionMap actions) {}
	/// Whether the camera the player has can be turned with the mouse, which holds the cursor still while it is
	virtual void AllowCursorFreeze([[maybe_unused]] bool allowed) {}
	/// The cursor is held still while the mouse turns the camera, and the pointer's position isn't followed
	[[nodiscard]] virtual bool IsCursorFrozen() const { return false; }
	/// The cursor's image is pinned where it is as the hand scoops, and is free again once let go. Only the drawn image
	/// stays: the pointer the hand, the gestures and the camera follow keeps moving with the mouse
	virtual void PinCursor([[maybe_unused]] bool pinned) {}
	/// Where the cursor's image is drawn: where it was pinned, else at the pointer. openblack draws the hand as the cursor
	/// and no separate image
	[[nodiscard]] virtual glm::ivec2 GetCursorImagePosition() const { return glm::ivec2(GetMousePosition()); }

	virtual void Frame() = 0;
	virtual void ProcessEvent(const SDL_Event& event) = 0;
};
} // namespace openblack::input
