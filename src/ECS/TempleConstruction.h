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

#include <entt/entity/entity.hpp>
#include <glm/vec3.hpp>

namespace openblack::ecs::components
{
struct BuildingSite;
} // namespace openblack::ecs::components

/// Buildings going up in the world, the temple first: a town's planned temple started by a script, how much of a
/// building is built as scripts and builders change it, and what a temple does once it is finished
namespace openblack::ecs::construction
{

/// A script asks the towns to build what they planned at a place with a desire: the planned temple nearest it, if any is
/// near enough, starts going up with nothing of it built and a site for its builders. The temple it started, if any.
std::optional<entt::entity> StartPlannedAt(glm::vec3 place, float scriptDesire);

/// A planned temple starts going up where it was planned, with nothing of it built and a site for its builders wanting
/// them by a desire; it is no longer among its town's plans. The temple.
entt::entity StartPlannedTemple(entt::entity plan, float siteDesire);

/// How much of an object is built, 0 to 1: all of anything not under construction
[[nodiscard]] float BuiltOf(entt::entity object);

/// Whether a building is built: a temple whose heart is built has its features (the creature shrinks to go in, its
/// entrance can be clicked, its heart beats)
[[nodiscard]] bool IsBuilt(entt::entity object);

/// A script sets how much of a building is built: it is finished at all of it, and a town's building left unfinished
/// gets a site for its builders if it has none
void SetBuilt(entt::entity building, float built);

/// Builders add to how much of a building under construction is built, finishing it at all of it. On a built building
/// short of its full life they add to its life instead, and once it is whole it is repaired: a repaired town building
/// starts working again as when it was built (a graveyard counts a burial and takes its town back).
void BuildBy(entt::entity building, float amount);

/// A building going up gets a site of its own town's for its builders (a new one, even if the town has one there), with
/// a desire, its builders' places worked out round its model and, for a temple, its six piles of wood
void OpenSite(entt::entity building, float desire);

/// A town gets a site at a building, if it has none there, wanting builders no more than for its other building; its
/// builders' places are worked out round its model. Whether the town has a site there.
bool OpenSiteForTown(entt::entity building, entt::entity town);

/// A town asked to build a worship site going up gets a site of its own there, if it has none, which wants builders by a
/// desire
void OpenWorshipSite(entt::entity site, entt::entity town, float desire);

/// A temple's site makes again those of its six piles of wood that have gone; any other site makes its one pile if it has
/// none
void MakeSitePiles(entt::entity building, components::BuildingSite& site);

/// Every site at a building goes, the one taking brought wood first: its builders are let go of it, and of its piles of
/// wood, those with wood in them stay on as piles of their own for anyone to take from, and the empty ones go
void CloseSite(entt::entity building);
/// A town's site at a building goes, the same way. From then on none of the building's other sites takes wood brought by
/// a hand or a creature, until another site opens.
void CloseTownsSite(entt::entity building, entt::entity town);

} // namespace openblack::ecs::construction
