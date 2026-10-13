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

#include "ECS/CreatureAttitudeRules.h"

namespace openblack::ecs::components
{

/// How a town feels about each creature it watches, from the first time one of its people took up the creature's
/// reaction (see creature_attitude). A creature that goes is forgotten.
struct TownCreatureAttitude
{
	struct Watched
	{
		entt::entity creature {entt::null};
		creature_attitude::Feeling feeling;
	};
	std::vector<Watched> creatures;
};

} // namespace openblack::ecs::components
