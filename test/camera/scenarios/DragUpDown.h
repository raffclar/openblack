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

#include <glm/ext/vector_int2.hpp>

#include "Mock.h"

class DragUpDownMockAction final: public MockAction
{
	static constexpr uint32_t k_Start1 = k_StabilizeFrames + 2;
	static constexpr uint32_t k_End1 = k_Start1 - 1 + k_InteractionFrames;
	static constexpr uint32_t k_Start2 = k_End1 + 1;
	static constexpr uint32_t k_End2 = k_Start2 - 1 + k_StabilizeFrames;
	static constexpr uint32_t k_Start3 = k_End2 + 1;
	static constexpr uint32_t k_End3 = k_Start3 - 1 + k_InteractionFrames;
	static constexpr uint32_t k_LiftMouse =
	    k_StabilizeFrames + k_InteractionFrames + k_StabilizeFrames + k_InteractionFrames + k_StabilizeFrames;

public:
	~DragUpDownMockAction() final = default;

	[[nodiscard]] bool GetBindable(openblack::input::BindableActionMap action) const final
	{
		using openblack::input::BindableActionMap;
		if (frameNumber >= k_Start1 - 1 && frameNumber < k_End3)
		{
			return (static_cast<uint32_t>(action) & static_cast<uint32_t>(BindableActionMap::MOVE)) != 0;
		}

		return false;
	}

	[[nodiscard]] glm::uvec2 GetMousePosition() const final
	{
		if (frameNumber < k_Start1)
		{
			return {k_Width / 2, 450};
		}
		if (frameNumber < k_Start2)
		{
			auto t = std::max((frameNumber - 1.0f - k_StabilizeFrames) / k_InteractionFrames, 0.0f);
			return {k_Width / 2, (k_Height - 150.0f - t * 100.0f)};
		}
		if (frameNumber < k_Start3)
		{
			return {k_Width / 2, k_Height - 250};
		}
		if (frameNumber <= k_End3)
		{
			auto t = std::max((frameNumber - 1.0f - (k_StabilizeFrames + k_InteractionFrames + k_StabilizeFrames)) /
			                      k_InteractionFrames,
			                  0.0f);
			return {k_Width / 2, (k_Height - 150.0f - (1.0f - t) * 100.0f)};
		}

		return {k_Width / 2, 450};
	}

	[[nodiscard]] std::array<std::optional<glm::vec3>, 2> GetHandPositions() const final
	{
		return {{{}, {{1000.0f, 0.0f, 1000.0f}}}};
	}
};
