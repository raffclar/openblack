/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <glm/vec2.hpp>

#include "ECS/Systems/UnderwaterSystemInterface.h"
#include "Locator.h"

/// EXPERIMENT (not for merge): while the experiment's view is on, the map has no boundary. The camera and the hand go out
/// past the map's edge over the open sea and its sea bed. With the experiment off, the land's own limits hold.
namespace openblack::underwater_boundary
{

/// True while the experiment's view is on, so the map's edge limits do not apply
[[nodiscard]] inline bool Off()
{
	return Locator::underwaterSystem::has_value() && Locator::underwaterSystem::value().IsEnabled();
}

/// The height of the floor at a point while the experiment's view is on: the land where it is above the sea, else the
/// sea bed under the water, past the land's blocks and past the map's edge too. With the experiment off, the land's own
/// height `landHeight`.
[[nodiscard]] inline float FloorAt(glm::vec2 point, float landHeight)
{
	return Locator::underwaterSystem::has_value() ? Locator::underwaterSystem::value().FloorAt(point, landHeight) : landHeight;
}

} // namespace openblack::underwater_boundary
