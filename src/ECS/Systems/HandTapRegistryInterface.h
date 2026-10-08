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

namespace openblack::ecs::hand_tap
{
struct Handler;
} // namespace openblack::ecs::hand_tap

namespace openblack::ecs::systems
{
/// The hand-tap classes' handlers, in registration order: the first class an object belongs to answers
/// (hand_tap::Register and hand_tap::Find; Locator::handTapRegistry)
class HandTapRegistryInterface
{
public:
	virtual ~HandTapRegistryInterface() = default;

	[[nodiscard]] virtual std::vector<hand_tap::Handler>& Handlers() noexcept = 0;
};
} // namespace openblack::ecs::systems
