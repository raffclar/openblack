/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include "ECS/Systems/RoutePlanStateSystemInterface.h"

#if !defined(LOCATOR_IMPLEMENTATIONS)
#error "ECS System implementations should only be included in Locator.cpp"
#endif

namespace openblack::ecs::systems
{
class RoutePlanStateSystem final: public RoutePlanStateSystemInterface
{
public:
	[[nodiscard]] Callbacks& GetCallbacks() override { return _callbacks; }
	[[nodiscard]] HolderPool& GetHolderPool() override { return _holders; }

private:
	Callbacks _callbacks;
	HolderPool _holders;
};
} // namespace openblack::ecs::systems
