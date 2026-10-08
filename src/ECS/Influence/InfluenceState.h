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

#include <array>
#include <vector>

#include <entt/entity/entity.hpp>
#include <glm/vec3.hpp>

#include "ECS/Systems/MapScriptSystemInterface.h"
#include "Game.h"
#include "Influence.h"

// Private to ECS/Influence: the game's fields the influence reads, kept on one registry entity so that a new land (the
// registry's Reset) puts them back to the game's init defaults before the map script runs.

namespace openblack::influence::detail
{
struct InfluenceGlobals
{
	/// the rings, newest first (a new one is pushed at the head of the list)
	std::vector<entt::entity> rings;
	/// each player's influence power (PlayerNames), CalculateInfluencePower. (approximate) kept with the land: the
	/// original keeps it in the player, but recomputes it every turn before anyone reads it
	std::array<float, 8> power {};
	/// the influence circles, newest first. Clearing them with the map is the registry's reset here
	std::vector<Circle> circles;
	/// Update3DInfluence's dirty flag. (inferred) 0 after the game's variables are cleared (the value written was not
	/// traced): the first circles of a land wait for the first radius that moves by more than 0.01 from the 0 drawn
	/// radius of a citadel or a town, which every new citadel and town with influence gives
	bool circlesDirty {false};
	/// the players' border latch (BoundaryShown), cleared on every land load
	std::array<bool, 8> boundaryShown {};
	/// the hand's ripples, newest first. (inferred) cleared with the land: nothing that empties the list on
	/// ClearMap was traced, and a ripple lives 2 s
	std::vector<Ripple> ripples;
};

/// The land's globals (made on first use)
InfluenceGlobals& Globals();
/// Read only: the defaults when there is no registry entity yet
[[nodiscard]] const InfluenceGlobals& GlobalsOrDefault();
/// The land's globals when they were made, else nullptr (for the draw, which must not create the entity)
[[nodiscard]] InfluenceGlobals* TryGlobals();

/// The game's fields the map script sets (land number, town / player influence multipliers):
/// Locator::mapScriptSystem (the one copy, set by FeatureScriptCommands and reset by Game::LoadMap as the game's init
/// does); the unit tests get an empty one for each test (Tests/support/TestServices)
[[nodiscard]] MapScriptGlobals& MapGlobals();

/// The distance in metres: x,z only
[[nodiscard]] float DistanceXZ(const glm::vec3& a, const glm::vec3& b);
} // namespace openblack::influence::detail
