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

#include <glm/vec3.hpp>

#include "PSysManager.h"

namespace openblack::psys::town_belief
{
/// The belief symbols over every town centre (TOWN_BELIEF, SF_TownBelief, UR_TownCentreBelief).
/// Every rendered frame, paused too: one step of each centre's effect with dt = the ms of a turn x 0.001 (0.1 s, not
/// the frame's time), and the glows' frame
void Step();
/// The symbols' atoms from the last Step; draws nothing and may be called several times a frame
void Collect(const glm::vec3& camera, std::vector<manager::Drawable>& out);
void Clear();
/// The centre's effect deleted, as when the town centre is deleted
void RemoveCentre(entt::entity townCentre);
} // namespace openblack::psys::town_belief
