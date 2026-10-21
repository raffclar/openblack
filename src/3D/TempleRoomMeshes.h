/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <optional>
#include <string>
#include <string_view>

/// The temple's rooms have two meshes each: one with light maps, and a low one without them. At the lowest detail level,
/// which draws the temple with no light maps, the game builds each room from its low mesh: the room, its floor and its
/// pools. The domes have no low mesh. The game uses the low mesh for all of the room, so it takes the room mesh's name.
namespace openblack::temple_room_meshes
{

/// The end of a low mesh's file name, before the file's extension
inline constexpr std::string_view k_LowEnd = "lo_l3d";
inline constexpr std::string_view k_End = "_l3d";

/// The name of a mesh file without its extension, as the game loads it: the low meshes take the names of the meshes they
/// stand for. None for a mesh that is not loaded: a low one while the light maps are on, and one with a low mesh while
/// they are off.
[[nodiscard]] std::optional<std::string> LoadedName(std::string_view stem, bool lightmaps, bool hasLowMesh);

/// The file name, without its extension, of a mesh's low mesh
[[nodiscard]] std::string LowName(std::string_view stem);

} // namespace openblack::temple_room_meshes
