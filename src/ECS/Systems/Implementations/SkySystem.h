/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include "ECS/Systems/SkySystemInterface.h"

#if !defined(LOCATOR_IMPLEMENTATIONS)
#error "ECS System implementations should only be included in Locator.cpp"
#endif

namespace openblack::ecs::systems
{

class SkySystem final: public SkySystemInterface
{
public:
	void UpdateFrame(bool skyDrawn) override;
	void JumpToClock() override;
	[[nodiscard]] entt::entity GetDome() const override;
	[[nodiscard]] entt::entity GetSun() const override;
	[[nodiscard]] entt::entity GetMoon() const override;
};

} // namespace openblack::ecs::systems
