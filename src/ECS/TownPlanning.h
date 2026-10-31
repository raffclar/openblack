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
#include <glm/vec3.hpp>

#include "ECS/ScaffoldOfferRules.h"
#include "Enums.h"

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

/// How much one of a town's plans wants repairing: a plan of a building that had been finished before it went wants it
/// as much as its kind wants mending, any other plan not at all
[[nodiscard]] float PlanDesireToBeRepaired(entt::entity plan);

/// A town's turn: it puts up at most one thing for mending, the building with no site it hasn't put up already or the
/// plan of a finished building that went, whichever wants repair most (see planned_building::ChooseToMend). A building
/// gets a site for its builders; a plan goes up again with nothing of it built.
void MendBest(entt::entity town);

/// A town's building going from the land is planned again by its town, where it stood and as it was turned, after the
/// town's other plans; the plan remembers whether the building had been finished. A building of no town is not.
void PlanAgain(entt::entity building);

/// The buildings of a tribe that a town can build from a held scaffold of a value at a place, in the order of their
/// abode numbers, each with how much the town wants it. With an abode number, only the building of that number. A
/// number that the tribe has no building of is left out. The wonder is of the wonder tribe, and its size comes from the
/// power of a wonder of the town at the place.
[[nodiscard]] std::vector<scaffold_offer::Candidate> ScaffoldCandidates(entt::entity town, Tribe tribe, uint32_t value,
                                                                        std::optional<AbodeNumber> only, glm::vec3 place,
                                                                        Tribe wonderTribe);

/// Whether a building that a scaffold offers fits on the land at a place, turned by an angle and at a scale. The land at
/// the place must be dry and not on the coast. No building may stand in the cells of its outline nearer than their radii
/// together. An outline that covers no cell fits. The check ignores the building `ignored`, when it is given.
[[nodiscard]] bool FitsForScaffold(glm::vec3 place, MeshId mesh, float yAngle, float scale, entt::entity ignored = entt::null);

/// A town starts a building that a held scaffold offered, at a place on the land, turned by an angle and at a scale. The
/// building stands with nothing of it built, and its site wants builders. The building, none when it did not go up.
entt::entity StartForScaffold(entt::entity town, AbodeInfo info, glm::vec3 place, float yAngle, float scale);

/// The plans of every town under a new building go: each plan whose model reaches the building's circle, of a radius
/// about a place
void DropPlansUnder(glm::vec3 place, float buildingRadius);

} // namespace openblack::ecs::town_planning
