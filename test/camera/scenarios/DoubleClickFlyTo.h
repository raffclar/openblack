/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include "Mock.h"

class DoubleClickFlyToMockAction final: public MockAction
{
public:
	~DoubleClickFlyToMockAction() final = default;

	[[nodiscard]] bool GetUnbindable(openblack::input::UnbindableActionMap action) const final
	{
		using openblack::input::UnbindableActionMap;
		if (frameNumber == k_StabilizeFrames + 1)
		{
			return (static_cast<uint32_t>(action) & static_cast<uint32_t>(UnbindableActionMap::DOUBLE_CLICK)) != 0;
		}

		return false;
	}

	[[nodiscard]] std::array<std::optional<glm::vec3>, 2> GetHandPositions() const final
	{
		return {{{}, {{1080.70996f, 0.0f, 966.307434f}}}};
	}

	[[nodiscard]] glm::uvec2 GetMousePosition() const final { return {100, 600 - 290}; }
};
