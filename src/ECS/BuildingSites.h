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

#include <optional>
#include <vector>

#include <entt/entity/entity.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

namespace openblack::ecs::components
{
struct BuildingSite;
} // namespace openblack::ecs::components

/// A town's building sites as its villagers see them: which sites a town has, whether one is still worth working at, how
/// many builders it wants and how much, the wood at it and its worth, and its builders coming and going. A building has
/// a site for each town building it, so a site is named by its town and its building.
namespace openblack::ecs::construction
{

/// The buildings at which a town has a site
[[nodiscard]] std::vector<entt::entity> SitesOfTown(entt::entity town);

/// The town a building going up was planned by, none if none
[[nodiscard]] entt::entity TownOfSite(entt::entity building);

/// A town's site at a building, none if it has none there
[[nodiscard]] components::BuildingSite* SiteOf(entt::entity town, entt::entity building);
/// The site of a building that wood brought by a hand or a creature goes onto, none if none takes it
[[nodiscard]] components::BuildingSite* BroughtWoodSite(entt::entity building);

/// Whether the town has a site at the building and the building still wants building or mending
[[nodiscard]] bool IsSiteValid(entt::entity town, entt::entity building);

/// The most builders the building takes
[[nodiscard]] int32_t MaxBuilders(entt::entity building);
/// The builders the town's site still wants, below none when more work at it than the building takes
[[nodiscard]] int32_t BuildersNeeded(entt::entity town, entt::entity building);
/// How much the town's site wants villagers, 0 to 1
[[nodiscard]] float DesireForVillagers(entt::entity town, entt::entity building);

/// How much a built building wants mending, 0 to 1: an abode of a town only once it is worn down to its town's mark and,
/// where people live, while someone lives there; nothing else is mended by builders
[[nodiscard]] float DesireToBeRepaired(entt::entity building);
/// Whether the town's site was opened to mend a building already built
[[nodiscard]] bool IsRepairSite(entt::entity town, entt::entity building);
/// How much the town's mending site wants its builders: its want of villagers by its building's want of mending
[[nodiscard]] float SiteDesireToBeRepaired(entt::entity town, entt::entity building);

/// The wood lying at a site
[[nodiscard]] uint32_t WoodAt(const components::BuildingSite& site);
/// The wood lying at the town's site
[[nodiscard]] uint32_t WoodAtSite(entt::entity town, entt::entity building);
/// What a site counts the building worth in wood
[[nodiscard]] float SiteWoodValue(entt::entity building);
/// The wood the town's site still wants to build it
[[nodiscard]] float WoodNeededToBuild(entt::entity town, entt::entity building);

/// The building's reach across the ground, and its point nearest a place
[[nodiscard]] float BuildingRadius(entt::entity building);
[[nodiscard]] glm::vec2 NearestEdgeToPos(entt::entity building, glm::vec2 place);

/// A villager sets to work at its town's site, counted again each time; and is let go of it, every time it was counted
/// gone
void AddWorker(entt::entity town, entt::entity building, entt::entity villager);
void RemoveWorker(entt::entity town, entt::entity building, entt::entity villager);

/// The town's storage pit, if it has one
[[nodiscard]] std::optional<entt::entity> StoragePitOf(entt::entity town);

} // namespace openblack::ecs::construction
