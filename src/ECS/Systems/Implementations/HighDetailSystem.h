/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include "ECS/Systems/HighDetailSystemInterface.h"

#if !defined(LOCATOR_IMPLEMENTATIONS)
#error "ECS System implementations should only be included in Locator.cpp"
#endif

namespace openblack::ecs::systems
{

class HighDetailSystem final: public HighDetailSystemInterface
{
public:
	void Make(entt::entity thing) override;
	void Release(entt::entity thing) override;
	bool Order(entt::entity thing, high_detail_rules::ThingSpecial special, bool on) override;
	void Update() override;
};

} // namespace openblack::ecs::systems
