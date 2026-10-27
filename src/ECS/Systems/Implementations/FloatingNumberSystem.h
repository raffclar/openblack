/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <vector>

#include "Common/FixedFormat.h"
#include "ECS/FloatingNumber.h"
#include "ECS/Systems/FloatingNumberSystemInterface.h"

#if !defined(LOCATOR_IMPLEMENTATIONS)
#error "ECS System implementations should only be included in Locator.cpp"
#endif

namespace openblack::ecs::systems
{

class FloatingNumberSystem final: public FloatingNumberSystemInterface
{
public:
	void Add(const glm::vec3& position, float value, uint32_t colour) override
	{
		_numbers.push_back({.text = fixed_format::Fixed(value, 3, 0),
		                    .position = position,
		                    .colour = colour,
		                    .life = floating_number::k_LifeSeconds});
	}
	void Step(float seconds) override { floating_number::StepAll(_numbers, seconds); }
	[[nodiscard]] std::span<const components::FloatingNumber> Numbers() const override { return _numbers; }
	void Reset() override { _numbers.clear(); }

private:
	std::vector<components::FloatingNumber> _numbers;
};

} // namespace openblack::ecs::systems
