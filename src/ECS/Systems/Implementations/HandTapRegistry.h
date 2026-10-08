/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <vector>

#include "ECS/Systems/HandTap.h"
#include "ECS/Systems/HandTapRegistryInterface.h"

#if !defined(LOCATOR_IMPLEMENTATIONS)
#error "Locator interface implementations should only be included in Locator.cpp, use interface instead."
#endif

namespace openblack::ecs::systems
{
/// Holds the hand-tap handlers; empty until the first class registers
class HandTapRegistry final: public HandTapRegistryInterface
{
public:
	[[nodiscard]] std::vector<hand_tap::Handler>& Handlers() noexcept override { return _handlers; }

private:
	std::vector<hand_tap::Handler> _handlers;
};
} // namespace openblack::ecs::systems
