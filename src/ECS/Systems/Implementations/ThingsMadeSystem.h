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

#include <functional>

#include "ECS/Systems/ThingsMadeSystemInterface.h"

#if !defined(LOCATOR_IMPLEMENTATIONS)
#error "ECS System implementations should only be included in Locator.cpp"
#endif

namespace openblack::ecs::systems
{

class ThingsMadeSystem final: public ThingsMadeSystemInterface
{
public:
	/// The count is kept where `count` gives it: in the game, with the world, so that a new land starts it again and a
	/// saved game keeps it
	explicit ThingsMadeSystem(std::function<uint32_t&()> count);

	[[nodiscard]] uint32_t TakeNext() override;
	[[nodiscard]] uint32_t Made() const override;

private:
	std::function<uint32_t&()> _count;
};

} // namespace openblack::ecs::systems
