/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <cstdint>

#include <entt/entity/entity.hpp>

#include "Common/Zoomer.h"

namespace openblack::ecs::components
{

/// The floating "needs" sign of a building (the workshop's, GShowNeedsInfo row 3) on its own entity, with its Mesh and
/// Transform. It rises to the building's need and is drawn only while the need is above the info's ShowNeedGreater
struct ShowNeedsVisuals
{
	entt::entity owner {entt::null}; ///< the thing it shows (at its show needs position)
	uint8_t infoIndex {0};           ///< the GShowNeedsInfo row
	float desire {0.0f};             ///< set by the owner's process
	Zoomer height {};                ///< the shown need, 0..1, eased in 1 s
};

} // namespace openblack::ecs::components
