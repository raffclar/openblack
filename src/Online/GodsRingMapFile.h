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
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "3D/LandData.h"

namespace openblack::filesystem
{
class FileSystemInterface;
}

/// The file of the network map for 32 gods (deviation 171). The file has the layout of the game's ".map" files: the
/// land's script and its landscape. The game server makes it from the land that gives the village, and the players get
/// it from the lobby. Make, Write and Read are pure, so that the tests can give their own small lands.
namespace openblack::gods_ring_map_file
{

/// The name of the file on the game socket and in the lobby, under the folder of the lands of network games
[[nodiscard]] std::string WireName();

/// The bytes of the map's file, from the bytes of the ".map" file that gives the village. Nothing when that file is
/// not a land of a network game, or its script has no village for the god of the template.
[[nodiscard]] std::optional<std::vector<uint8_t>> Make(std::span<const uint8_t> templateMap);

/// The bytes of the map's file, from the land that gives the village in the game folder. Nothing, with a line in the
/// log, when that land is not there or does not read.
[[nodiscard]] std::optional<std::vector<uint8_t>> MakeFromGameFolder(filesystem::FileSystemInterface& files);

/// The bytes of the map's file, from the script of the map and its land
[[nodiscard]] std::optional<std::vector<uint8_t>> Write(std::string_view script, const LandData& land);

/// The two parts of the map's file
struct Parts
{
	std::string script;
	LandData land;
};

/// The script and the land of the bytes of a map's file. Nothing when the bytes are not a ".map" file, or its
/// landscape does not read.
[[nodiscard]] std::optional<Parts> Read(std::span<const uint8_t> file);

} // namespace openblack::gods_ring_map_file
