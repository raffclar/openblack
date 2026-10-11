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

#include <algorithm>
#include <limits>
#include <optional>

#include <glm/vec2.hpp>

/// The window is the one source of the size the game draws at. Each frame the game asks the window its size and
/// follows it: the back buffer, the views drawn over the whole screen and the camera's shape are made again only on the
/// frame the size changes, however the change came (resizing, maximising, restoring, going full screen) and whatever
/// screen was up at the time (the start-up logos, the loading screen, the game). Everything else asks the window its
/// size as it draws.
namespace openblack::windowing
{

class SizeFollower
{
public:
	SizeFollower() = default;
	/// Starts from what is drawn now
	explicit SizeFollower(glm::u16vec2 drawn)
	    : _drawn(drawn)
	{
	}

	/// The size to draw at from this frame on, when the window's size differs from what is drawn. A window without an
	/// area (minimised, as some systems report it) keeps what was drawn, so that nothing is made again until it comes
	/// back, and then only if it came back at another size.
	[[nodiscard]] std::optional<glm::u16vec2> Follow(glm::ivec2 window)
	{
		if (window.x <= 0 || window.y <= 0)
		{
			return std::nullopt;
		}
		constexpr int k_Largest = std::numeric_limits<uint16_t>::max();
		const auto size = glm::u16vec2(std::min(window.x, k_Largest), std::min(window.y, k_Largest));
		if (size == _drawn)
		{
			return std::nullopt;
		}
		_drawn = size;
		return size;
	}

	/// The size drawn at
	[[nodiscard]] glm::u16vec2 Drawn() const { return _drawn; }

	/// The width over the height of what is drawn, for the camera
	[[nodiscard]] float Aspect() const
	{
		return _drawn.y == 0 ? 1.0f : static_cast<float>(_drawn.x) / static_cast<float>(_drawn.y);
	}

private:
	glm::u16vec2 _drawn {0, 0};
};

} // namespace openblack::windowing
