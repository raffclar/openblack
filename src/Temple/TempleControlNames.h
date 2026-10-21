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

#include <span>
#include <string>
#include <vector>

#include "3D/TempleInteriorInterface.h"

/// The controls of the temple's rooms: the submeshes that the game draws with a colour of their own
namespace openblack::temple::controls
{

/// The names of a room's controls, as the game finds them among the room's submeshes. A room with none gives none.
[[nodiscard]] std::vector<std::string> Names(TempleRoom room);

/// The submeshes of a room that are its controls, by the names of the room's submeshes in order, each once and in
/// order. A control is the first submesh of its name, compared with no difference between capital and small letters.
/// A control whose name the room does not have is the first submesh, as the game takes it.
[[nodiscard]] std::vector<uint32_t> SubMeshes(TempleRoom room, std::span<const std::string> subMeshNames);

} // namespace openblack::temple::controls
