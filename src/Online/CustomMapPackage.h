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
#include <string_view>
#include <variant>
#include <vector>

#include <MapPackageFile.h>

#include "GodsRingMapPackage.h"

/// The map package of a custom land of a network game (deviation 249). The package needs no game data: its frame is
/// the full script of the land, and the ".map" file is the frame and the landscape. The operator of a site puts such a
/// package in the folder of map packages, so that a game server of the pool can play the land and the players can get
/// it. Everything here is pure, so that the tests can give their own small lands.
namespace openblack::custom_map_package
{

/// The land that a package gives
struct Assembled
{
	onlinemap::package::Manifest manifest;
	/// The bytes of the ".map" file
	std::vector<uint8_t> map;
};

/// The bytes of the package of a custom land, from its ".map" file. file is the name of the file with no folder and no
/// extension. Nothing when the bytes are not a ".map" file, when the script does not load the landscape of the file
/// ("Online Maps\<file>.lnd"), or when the name or the gods are not valid.
[[nodiscard]] std::optional<std::vector<uint8_t>> Make(std::string_view file, std::string_view name, int32_t players,
                                                       std::span<const uint8_t> map);

/// The land of a package that needs no game data. OtherMap when the package needs game data (the map for 32 gods),
/// NotAPackage when the bytes are not a package or a part does not have the hash of the manifest.
[[nodiscard]] std::variant<Assembled, gods_ring_map_package::Failure> Assemble(std::span<const uint8_t> package);

} // namespace openblack::custom_map_package
