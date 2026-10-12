/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <optional>
#include <vector>

#include <entt/entity/entity.hpp>

#include "ECS/Components/Construction.h"

/// A building's sites, one for each town building it, as the towns open and close them: a town's builders work only at
/// their own town's site, while wood brought by a hand or a creature goes onto the newest site
namespace openblack::ecs::building_site_list
{

/// A town's site at the building, none if the town has none there (the newest, should it have more)
[[nodiscard]] components::BuildingSite* Find(components::BuildingSites& sites, entt::entity town);
[[nodiscard]] const components::BuildingSite* Find(const components::BuildingSites& sites, entt::entity town);

/// The site wood brought by a hand or a creature goes onto, none if none takes it
[[nodiscard]] components::BuildingSite* TakingBroughtWood(components::BuildingSites& sites);
[[nodiscard]] const components::BuildingSite* TakingBroughtWood(const components::BuildingSites& sites);

/// A town opens a new site at the building, even if it has one there already: it goes first, and wood brought to the
/// building goes onto it from now on
components::BuildingSite& Open(components::BuildingSites& sites, entt::entity town);

/// A town's site closes and is handed back, none if the town had none there. From then on none of the building's sites
/// takes brought wood, whichever of them it was, until another site opens.
std::optional<components::BuildingSite> Close(components::BuildingSites& sites, entt::entity town);

/// The towns whose sites close, in turn, when the building is finished or goes: the site taking brought wood first, then
/// the others, the newest first
[[nodiscard]] std::vector<entt::entity> ClosingOrder(const components::BuildingSites& sites);

} // namespace openblack::ecs::building_site_list
