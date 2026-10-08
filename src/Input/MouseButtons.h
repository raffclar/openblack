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

#include <glm/vec2.hpp>

// The mouse buttons and motion as the hand reads them, kept apart from SDL so they can be tested with fake events

namespace openblack::input
{
enum class MouseButton : uint8_t
{
	Left,
	Middle,
	Right,
	Other,
};

/// One button going down or up, at a window position
struct MouseButtonEvent
{
	MouseButton button {MouseButton::Other};
	bool down {false};
	glm::ivec2 position {0, 0};
};

struct MouseButtonsState
{
	bool left {false};
	bool middle {false};
	bool right {false};
	/// Where the middle button last went down (the cursor goes back there when it is let go)
	glm::ivec2 middlePressPosition {0, 0};
};

/// The left and the middle buttons flip on every down and every up; the right one is held while down. A middle down
/// keeps its position. Any other button changes nothing
void ApplyMouseButton(MouseButtonsState& state, const MouseButtonEvent& event);

/// The hand grips while the left or the middle button is held
[[nodiscard]] constexpr bool HandGripping(const MouseButtonsState& state)
{
	return state.middle || state.left;
}

/// The hand's action while the right button is held
[[nodiscard]] constexpr bool HandAction(const MouseButtonsState& state)
{
	return state.right;
}

struct MouseMotionState
{
	/// Unset until the first frame, which takes its own position as the previous one
	std::optional<glm::ivec2> previous;
};

/// The cursor's motion since the previous call (0 on the first call)
[[nodiscard]] glm::ivec2 MouseMotion(MouseMotionState& state, const glm::ivec2& position);
} // namespace openblack::input
