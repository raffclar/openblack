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

class TiltUpDownMockAction final: public MockAction
{
	static constexpr uint32_t k_Start1 = k_StabilizeFrames + 1;
	static constexpr uint32_t k_End1 = k_StabilizeFrames + k_InteractionFrames - 1;
	static constexpr uint32_t k_Start2 = k_StabilizeFrames + k_InteractionFrames + k_StabilizeFrames + 1;
	static constexpr uint32_t k_End2 = k_StabilizeFrames + k_InteractionFrames + k_StabilizeFrames + k_InteractionFrames - 1;

public:
	~TiltUpDownMockAction() final = default;

	[[nodiscard]] bool GetBindable(openblack::input::BindableActionMap action) const final
	{
		using openblack::input::BindableActionMap;
		if (frameNumber >= k_Start1 && frameNumber <= k_End1)
		{
			return (static_cast<uint32_t>(action) & (static_cast<uint32_t>(BindableActionMap::ROTATE_ON) |
			                                         static_cast<uint32_t>(BindableActionMap::MOVE_BACKWARDS))) != 0;
		}

		if (frameNumber >= k_Start2 && frameNumber <= k_End2)
		{
			return (static_cast<uint32_t>(action) & (static_cast<uint32_t>(BindableActionMap::ROTATE_ON) |
			                                         static_cast<uint32_t>(BindableActionMap::MOVE_FORWARDS))) != 0;
		}

		return false;
	}
};
