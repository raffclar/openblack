/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <algorithm>

#include <glm/vec2.hpp>

#include "Input/GameActionMapInterface.h"
#include "Mock.h"

class MiddleDragRightUpMockAction final: public MockAction
{
	static constexpr uint32_t k_Start1 = k_StabilizeFrames;
	static constexpr uint32_t k_End1 = k_StabilizeFrames + k_InteractionFrames;
	static constexpr uint32_t k_Start2 = k_StabilizeFrames + k_InteractionFrames + k_StabilizeFrames;
	static constexpr uint32_t k_End2 = k_StabilizeFrames + k_InteractionFrames + k_StabilizeFrames + k_InteractionFrames;

public:
	~MiddleDragRightUpMockAction() final = default;

	[[nodiscard]] bool GetBindable(openblack::input::BindableActionMap action) const final
	{
		using openblack::input::BindableActionMap;
		if (frameNumber > k_Start1 - 1 && frameNumber < k_End2 + 1)
		{
			return (static_cast<uint32_t>(action) & static_cast<uint32_t>(BindableActionMap::ROTATE_AROUND_MOUSE_ON)) != 0;
		}

		return false;
	}

	[[nodiscard]] glm::uvec2 GetMousePosition() const final { return {k_Width / 2, 450}; }

	[[nodiscard]] glm::ivec2 GetMouseDelta() const final
	{
		if (frameNumber > k_Start1 && frameNumber < k_End1)
		{
			return {1, 0};
		}
		if (frameNumber > k_Start2 && frameNumber < k_End2)
		{
			return {0, 1};
		}
		return {0, 0};
	}
};
