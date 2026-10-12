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
#include <glm/vec3.hpp>

/// A town starting the buildings it planned: a villager with nothing to build asks its town, at most once a game turn,
/// for a new house or civic building, and the town starts the one of its plans it wants most, standing with nothing of
/// it built and a site for its builders
namespace openblack::ecs::town_planning
{

/// Whether a town may still be asked this game turn to start a planned building; asking it uses the turn's ask
[[nodiscard]] bool AskThisTurn(entt::entity town);

/// The town starts the planned house it wants most, if the land where it was planned takes a new house. Whether it
/// started one.
bool StartBestHouse(entt::entity town);

/// The town starts the planned civic building (or temple) it wants most, wherever it was planned. Whether it started one.
bool StartBestCivicBuilding(entt::entity town);

/// A script names a place with a desire: every town, the players' towns first and then the neutral ones, starts the one
/// of its own plans nearest the place (within a metre beyond its model's reach), whatever kind of building it is and
/// without checking the land, its site wanting builders by five times the script's desire. The buildings started.
std::vector<entt::entity> StartPlannedAt(glm::vec3 place, float scriptDesire);

/// A town's building going from the land is planned again by its town, where it stood and as it was turned, after the
/// town's other plans; the plan remembers whether the building had been finished. A building of no town is not.
void PlanAgain(entt::entity building);

} // namespace openblack::ecs::town_planning
