/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#if !defined(LOCATOR_IMPLEMENTATIONS)
#error "Locator interface implementations should only be included in Locator.cpp, use interface instead."
#endif

#include "ECS/VillageTotemWorld.h"

namespace openblack::ecs::systems
{

/// The village totems' world in the game
class GameVillageTotemWorld final: public village_totem::WorldInterface
{
public:
	[[nodiscard]] Registry& Entities() override;
	[[nodiscard]] entt::id_type IconMeshFor(PlayerNames player) const override;
	[[nodiscard]] Size SizeOf(entt::entity object) const override;
	[[nodiscard]] float LandHeightAt(glm::vec2 point) const override;
	[[nodiscard]] bool TempleBuilt(PlayerNames player) const override;
	[[nodiscard]] bool Built(entt::entity building) const override;
	void SetMovingSound(entt::entity totem, bool on) override;
	void RingBell(glm::vec3 position) override;
};

} // namespace openblack::ecs::systems
