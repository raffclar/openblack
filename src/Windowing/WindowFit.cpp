/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "WindowFit.h"

#include <algorithm>

#include <glm/common.hpp>

namespace openblack::windowing
{

std::optional<DesktopRect> FitWindow(DesktopRect picture, WindowBorders borders, DesktopRect usable)
{
	const auto frame = glm::ivec2(borders.left + borders.right, borders.top + borders.bottom);
	const auto topLeft = picture.position - glm::ivec2(borders.left, borders.top);
	const auto bottomRight = picture.position + picture.size + glm::ivec2(borders.right, borders.bottom);
	const auto usableEnd = usable.position + usable.size;
	if (topLeft.x >= usable.position.x && topLeft.y >= usable.position.y && bottomRight.x <= usableEnd.x &&
	    bottomRight.y <= usableEnd.y)
	{
		return std::nullopt;
	}

	auto size = picture.size;
	const auto room = glm::max(usable.size - frame, glm::ivec2(1));
	if (size.x > room.x || size.y > room.y)
	{
		const auto scale = std::min(static_cast<float>(room.x) / static_cast<float>(size.x),
		                            static_cast<float>(room.y) / static_cast<float>(size.y));
		size = glm::clamp(glm::ivec2(glm::vec2(size) * scale), glm::ivec2(1), room);
	}
	const auto position = usable.position + (usable.size - (size + frame)) / 2 + glm::ivec2(borders.left, borders.top);
	return DesktopRect {.position = position, .size = size};
}

} // namespace openblack::windowing
