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

#include "ECS/WorshipSiteWorld.h"

namespace openblack::ecs::systems
{

/// The worship sites' world in the game
class GameWorshipSiteWorld final: public worship_site::WorldInterface
{
public:
	[[nodiscard]] Registry& Entities() override;
	[[nodiscard]] int32_t LandNumber() const override;
	[[nodiscard]] std::optional<glm::vec3> SitePoint(uint32_t index) const override;
	[[nodiscard]] entt::id_type SiteMesh(entt::entity temple) override;
	[[nodiscard]] entt::id_type AltarMesh(Tribe tribe) const override;
	[[nodiscard]] float LandHeightAt(glm::vec2 point) const override;
	[[nodiscard]] uint32_t PopulationOf(entt::entity town) const override;
};

} // namespace openblack::ecs::systems
