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

#include "Enums.h"

namespace openblack::ecs::components
{

/// A temple a town plans to build for its player, not yet started: nothing of it stands, it isn't drawn and it is no
/// temple yet. A script starts it, and the temple then goes up where its Transform places it.
struct PlannedTemple
{
	int32_t townId;
	PlayerNames owner;
	/// How far round it will be turned, in radians, as the game measures a temple's turn
	float yAngle {0.0f};
};

/// A building a town plans to build later, made by the land's script or left by a building of the town that went from
/// the land: nothing of it stands, it isn't drawn and it holds nothing yet. The Transform places it; a town centre is
/// planned the same way, by its row.
struct PlannedAbode
{
	uint32_t townId;
	AbodeInfo info;
	/// How far round it will be turned, in radians
	float yAngle {0.0f};
	/// It was left by a building that had once been finished
	bool wasBuilt {false};
};

/// One town's site at a building going up or being mended: how much the town wants it built, which a script may force,
/// and its builders, their places round it and the wood brought for it
struct BuildingSite
{
	/// The town whose builders work at it
	entt::entity town {entt::null};
	float desire {0.0f};
	/// Opened for a building already built, to mend it
	bool repair {false};
	/// Wood brought to the building by a hand or a creature goes onto this site: the newest one. Once any of the
	/// building's sites closes, none of them takes such wood until another site opens.
	bool takesBroughtWood {false};
	/// The villagers working at it, a villager once each time it set to work, and the count of them the site keeps
	std::vector<entt::entity> workers;
	int32_t workerCount {0};
	/// The places round the building its builders stand at, worked out from its model when the site is opened
	std::array<glm::vec3, 128> places {};
	/// A temple's site keeps its wood in six piles round it, each gone when it has gone; any other site in the first, made
	/// for the first wood brought
	std::array<entt::entity, 6> piles {entt::null, entt::null, entt::null, entt::null, entt::null, entt::null};
};

/// A building going up or being mended with its builders' sites, one for each town building it: a worship site going up
/// has one for each of its towns asked to build it, each with its own builders and wood; any other building has one,
/// for its own town
struct BuildingSites
{
	/// The newest first
	std::vector<BuildingSite> sites;
};

/// A pile made to hold a building site's wood. It stays one after its building is finished: the villagers always come
/// for what is left in it, where a pile the hand made draws them only when their town wants wood.
struct SitePile
{
};

} // namespace openblack::ecs::components
