/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <cstdint>

#include <span>

#include <glm/vec3.hpp>

#include "ECS/Components/FloatingNumber.h"

namespace openblack::ecs::systems
{

/// The numbers that float up from things, as the share a totem was left at. They are drawn only: they are no things of
/// the world, and the frames move them on.
class FloatingNumberSystemInterface
{
public:
	virtual ~FloatingNumberSystemInterface() = default;
	/// A number floats up from a point, written three wide with nothing after the point, in a colour (0xAARRGGBB)
	virtual void Add(const glm::vec3& position, float value, uint32_t colour) = 0;
	/// Once a frame: the numbers rise, and those whose time is up go
	virtual void Step(float seconds) = 0;
	/// The numbers that float now
	[[nodiscard]] virtual std::span<const components::FloatingNumber> Numbers() const = 0;
	/// A new land: no numbers
	virtual void Reset() = 0;
};

} // namespace openblack::ecs::systems
