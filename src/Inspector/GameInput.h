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

#include "InputControl.h"

namespace openblack::inspector
{

/// The inspector's input made at the game's action layer, as the testbed's scenarios make theirs: the pointer stands in
/// for the mouse, and its moves, buttons, keys and wheel go through the window's event queue to the debug windows, the
/// game's menu and the game's actions, exactly as the player's do
class GameInput final: public InputTargetInterface
{
public:
	std::string Apply(const InputEvent& event) override;
	[[nodiscard]] glm::ivec2 ScreenSize() const override;
	[[nodiscard]] std::optional<glm::ivec2> WorldToScreen(glm::vec3 point) const override;
	[[nodiscard]] float GroundHeight(glm::vec2 point) const override;
	[[nodiscard]] bool HasKey(std::string_view name) const override;
	[[nodiscard]] bool HasAction(std::string_view name) const override;
	[[nodiscard]] bool HasGesture(std::string_view name) const override;
	[[nodiscard]] std::vector<std::string> ActionNames() const override;
	[[nodiscard]] std::vector<std::string> GestureNames() const override;
	[[nodiscard]] Json State() const override;
	void SetLockMode(std::string_view mode) override;
	[[nodiscard]] Json LockState() const override;

private:
	/// Gives the mouse back once the inspector holds nothing
	void Release();

	/// The keys the inspector holds down, by scancode, for their modifiers and for letting go of them all
	std::vector<int> _heldKeys;
};

/// The lock on the player's mouse and keyboard: its mode, and whether it keeps them out now
[[nodiscard]] Json InputLockState();

} // namespace openblack::inspector
