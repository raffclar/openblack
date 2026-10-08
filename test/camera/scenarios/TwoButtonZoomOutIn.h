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

class TwoButtonZoomOutInMockAction final: public MockAction
{
	static constexpr uint32_t k_Start1 = k_StabilizeFrames;
	static constexpr uint32_t k_End1 = k_StabilizeFrames + k_InteractionFrames;
	static constexpr uint32_t k_Start2 = k_StabilizeFrames + k_InteractionFrames + k_StabilizeFrames;

public:
	~TwoButtonZoomOutInMockAction() final = default;

	[[nodiscard]] bool GetUnbindable(openblack::input::UnbindableActionMap action) const final
	{
		using openblack::input::UnbindableActionMap;
		if ((frameNumber > k_Start1 && frameNumber < k_End1) || frameNumber > k_Start2)
		{
			return (static_cast<uint8_t>(action) & (static_cast<uint8_t>(UnbindableActionMap::TWO_BUTTON_CLICK))) != 0;
		}
		return false;
	}

	[[nodiscard]] glm::uvec2 GetMousePosition() const override { return {400, 450}; }

	[[nodiscard]] glm::ivec2 GetMouseDelta() const final
	{
		if (frameNumber > k_Start1 && frameNumber < k_End1)
		{
			return {0, 3};
		}
		if (frameNumber > k_Start2)
		{
			return {0, -3};
		}
		return {0, 0};
	}
};
