/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include "ECS/Systems/MagicObjectsSystemInterface.h"

#if !defined(LOCATOR_IMPLEMENTATIONS)
#error "ECS System implementations should only be included in Locator.cpp"
#endif

namespace openblack::ecs::systems
{
class MagicObjectsSystem final: public MagicObjectsSystemInterface
{
public:
	[[nodiscard]] std::vector<entt::entity>& Shields() override { return _shields; }
	[[nodiscard]] std::vector<entt::entity>& FireBalls() override { return _fireBalls; }

private:
	std::vector<entt::entity> _shields;
	std::vector<entt::entity> _fireBalls;
};
} // namespace openblack::ecs::systems
