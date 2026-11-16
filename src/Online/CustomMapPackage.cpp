/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "CustomMapPackage.h"

#include <string>

#include <OnlineMapFile.h>

#include "GodsRingMap.h"
#include "OnlineMapLand.h"
#include "OnlineMapStoreInterface.h"
#include "Sha256.h"

using namespace openblack;

namespace
{
constexpr size_t k_MostNameCharacters = 64;

std::span<const uint8_t> BytesOf(std::string_view text)
{
	return {reinterpret_cast<const uint8_t*>(text.data()), text.size()};
}

/// Whether the script loads the landscape of its own ".map" file. The game finds the landscape of a network map only
/// by that name.
bool LoadsOwnLandscape(std::string_view script, std::string_view map)
{
	constexpr std::string_view k_Load = "LOAD_LANDSCAPE(\"";
	for (auto at = script.find(k_Load); at != std::string_view::npos; at = script.find(k_Load, at + 1))
	{
		const auto start = at + k_Load.size();
		const auto end = script.find('"', start);
		if (end != std::string_view::npos && online_map_land::IsOwnLandscape(map, script.substr(start, end - start)))
		{
			return true;
		}
	}
	return false;
}
} // namespace

std::optional<std::vector<uint8_t>> custom_map_package::Make(std::string_view file, std::string_view name, int32_t players,
                                                             std::span<const uint8_t> map)
{
	if (!online::IsStoreFileName(std::string(file) + ".map") || name.empty() || name.size() > k_MostNameCharacters ||
	    players < 1 || players > static_cast<int32_t>(gods_ring_map::k_Slots))
	{
		return std::nullopt;
	}
	const auto parts = onlinemap::Parse(std::as_bytes(map));
	if (!parts.has_value() || !LoadsOwnLandscape(parts->script, std::string(file) + ".map"))
	{
		return std::nullopt;
	}
	const auto* landscape = reinterpret_cast<const uint8_t*>(parts->landscape.data());
	onlinemap::package::MapPackage package {
	    .manifest = {},
	    .frame = std::string(parts->script),
	    .landscape = std::vector<uint8_t>(landscape, landscape + parts->landscape.size()),
	};
	package.manifest = {
	    .file = std::string(file),
	    .name = std::string(name),
	    .players = players,
	    .needs = {},
	    .frameSha256 = online::sha256::HexOf(BytesOf(package.frame)),
	    .landscapeSha256 = online::sha256::HexOf(package.landscape),
	};
	return onlinemap::package::Write(package);
}

std::variant<custom_map_package::Assembled, gods_ring_map_package::Failure>
custom_map_package::Assemble(std::span<const uint8_t> packageBytes)
{
	using Failure = gods_ring_map_package::Failure;
	auto package = onlinemap::package::Read(std::as_bytes(packageBytes));
	if (!package.has_value())
	{
		return Failure::NotAPackage;
	}
	// A package that needs the player's game data gives its land only with the code of that land
	if (!package->manifest.needs.empty())
	{
		return Failure::OtherMap;
	}
	// The hashes of the manifest find a frame or a landscape that changed after the package was made
	if (package->manifest.frameSha256 != online::sha256::HexOf(BytesOf(package->frame)) ||
	    package->manifest.landscapeSha256 != online::sha256::HexOf(package->landscape) ||
	    !online::IsStoreFileName(package->manifest.file + ".map"))
	{
		return Failure::NotAPackage;
	}
	auto map = onlinemap::Write(package->frame, package->landscape);
	if (!map.has_value())
	{
		return Failure::NotAPackage;
	}
	return Assembled {.manifest = std::move(package->manifest), .map = std::move(*map)};
}
