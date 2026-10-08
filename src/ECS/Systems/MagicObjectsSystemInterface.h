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

#include <entt/entity/entity.hpp>

namespace openblack::ecs::systems
{
/// The magic objects' lists: the map shields and the fireballs, each newest first (magic's MapShield and
/// MagicFireBall code goes through it; each clears its own on a land load)
class MagicObjectsSystemInterface
{
public:
	virtual ~MagicObjectsSystemInterface() = default;

	[[nodiscard]] virtual std::vector<entt::entity>& Shields() = 0;
	[[nodiscard]] virtual std::vector<entt::entity>& FireBalls() = 0;
};
} // namespace openblack::ecs::systems
