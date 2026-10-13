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

#include <functional>
#include <vector>

#include <glm/vec2.hpp>

#include "3D/MapCoords.h"
#include "Enums.h"

// How a fully open vortex that leads away from the land pulls things in. Each turn it looks through the land's cells
// in a spiral out from its own, as far as it reaches that turn: its whole radius on every third turn and half of it on
// the others, so things near its middle are taken three times as often. A creature is only taken when a script lets it
// through and it is on the leash (or a script is moving it), and then only once it comes close to the middle, where it
// sparkles away. A thing flying in the physics that strikes it while it is fully open is taken as it flies. Pure rules, tested
// without the game.

namespace openblack::vortex
{

/// How far the vortex reaches on a game turn
[[nodiscard]] float PullReach(uint32_t turn, float radius);

/// At most this many cells are looked at in a turn's spiral
inline constexpr int32_t k_MostCellsSearched = 9999;

/// The cells a turn's search looks through, in order: from the vortex's own cell outwards in the game's spiral, cells
/// off the map passed over, ending before the first cell on the map farther from the vortex than the reach. A cell's
/// distance is measured from the vortex's place moved by whole cells, as the game measures it.
[[nodiscard]] std::vector<glm::ivec2> CellsSearched(const map_coords::MapCoords& centre, float reach);

/// Whether a creature may be taken by a vortex that leads away: let through by a script while on the leash, or
/// controlled by a script
[[nodiscard]] bool CreatureMayBeTaken(bool letThrough, bool leashed, bool movedByScript);

/// A creature that may be taken sparkles away once within this share of the vortex model's radius of its middle
inline constexpr float k_CreatureTakenShare = 0.7f;
/// It sparkles away over this many seconds and is gone for good
inline constexpr float k_CreatureSparkleSeconds = 3.0f;

/// How near the middle a creature must come: the share of the radius of the vortex's model across the land, the larger
/// of how far its model reaches along x and along z, scaled
[[nodiscard]] float CreatureTakenDistance(float modelReachX, float modelReachZ, float scale);

/// Whether a vortex takes a thing that struck it as it flew: only a vortex that leads away, only while fully open, and
/// only a thing still flying in the physics that the vortex would draw in from the land
[[nodiscard]] bool TakesWhatStrikesIt(VortexType type, VortexStateType state, bool hitterFlying, bool hitterMayBeTaken);

} // namespace openblack::vortex
