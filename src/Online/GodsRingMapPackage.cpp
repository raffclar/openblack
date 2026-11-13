/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "GodsRingMapPackage.h"

#include <MapPackageFile.h>
#include <OnlineMapFile.h>

#include "3D/GodsRingLand.h"
#include "GodsRingMap.h"
#include "PlayerSlots.h"
#include "Sha256.h"

using namespace openblack;

namespace
{
std::span<const uint8_t> BytesOf(std::string_view text)
{
	return {reinterpret_cast<const uint8_t*>(text.data()), text.size()};
}

onlinemap::package::Manifest ManifestOf(std::string_view frame, std::span<const uint8_t> landscape)
{
	return {
	    .file = std::string(gods_ring_map::k_File),
	    .name = std::string(gods_ring_map::k_Name),
	    .players = static_cast<int32_t>(gods_ring_map::k_Slots),
	    .needs = {{.file = std::string(gods_ring_map::k_TemplateMap),
	               .god = player_slots::ScriptNameOf(gods_ring_map::k_TemplateGod)}},
	    .frameSha256 = online::sha256::HexOf(BytesOf(frame)),
	    .landscapeSha256 = online::sha256::HexOf(landscape),
	};
}
} // namespace

std::vector<uint8_t> gods_ring_map_package::Make()
{
	onlinemap::package::MapPackage package {
	    .manifest = {},
	    .frame = gods_ring_map::ScriptFrame(gods_ring_map::LayOutSlots(gods_ring_land::k_MapMiddle)),
	    .landscape = gods_ring_land::Build().LndBytes(),
	};
	package.manifest = ManifestOf(package.frame, package.landscape);
	// The frame and the landscape are far below the limit of a 32-bit length
	return onlinemap::package::Write(package).value_or(std::vector<uint8_t> {});
}

std::string gods_ring_map_package::FileName(std::span<const uint8_t> package)
{
	return online::sha256::HexOf(package) + std::string(onlinemap::package::k_Extension);
}

std::variant<std::vector<uint8_t>, gods_ring_map_package::Failure>
gods_ring_map_package::Assemble(std::span<const uint8_t> packageBytes, std::span<const uint8_t> templateMap)
{
	const auto package = onlinemap::package::Read(std::as_bytes(packageBytes));
	if (!package.has_value())
	{
		return Failure::NotAPackage;
	}
	// The manifest of this map with the hashes of the parts finds a frame or a landscape that changed after the package
	// was made, and a package of another map
	const auto expected = ManifestOf(package->frame, package->landscape);
	if (package->manifest.file != expected.file || package->manifest.needs != expected.needs)
	{
		return Failure::OtherMap;
	}
	if (package->manifest != expected)
	{
		return Failure::NotAPackage;
	}
	const auto file = onlinemap::Parse(std::as_bytes(templateMap));
	if (!file.has_value())
	{
		return Failure::GameDataDoesNotMatch;
	}
	const auto village = gods_ring_map::ReadTemplate(file->script, gods_ring_map::k_TemplateGod, gods_ring_land::k_MapMiddle);
	if (!village.has_value())
	{
		return Failure::GameDataDoesNotMatch;
	}
	const auto slots = gods_ring_map::LayOutSlots(gods_ring_land::k_MapMiddle);
	const auto script = gods_ring_map::FillFrame(package->frame, *village, slots);
	if (!script.has_value())
	{
		return Failure::NotAPackage;
	}
	auto map = onlinemap::Write(*script, package->landscape);
	if (!map.has_value())
	{
		return Failure::NotAPackage;
	}
	return std::move(*map);
}

std::variant<std::vector<uint8_t>, gods_ring_map_package::Failure>
gods_ring_map_package::AssembleDownloaded(const lobby_api::OnlineMap& map, std::span<const uint8_t> package,
                                          std::optional<std::span<const uint8_t>> templateMap)
{
	if (online::sha256::HexOf(package) != map.packageSha256)
	{
		return Failure::WrongPackage;
	}
	if (map.file != gods_ring_map::k_File)
	{
		return Failure::OtherMap;
	}
	if (!templateMap.has_value())
	{
		return Failure::NoGameData;
	}
	return Assemble(package, *templateMap);
}

std::string_view gods_ring_map_package::TextOf(Failure failure)
{
	switch (failure)
	{
	case Failure::NotAPackage:
		return "the file is not a map package, or it is damaged";
	case Failure::WrongPackage:
		return "the file is not the package that the lobby names";
	case Failure::OtherMap:
		return "the package is of another map";
	case Failure::NoGameData:
		return "the game data has no land that the package needs";
	case Failure::GameDataDoesNotMatch:
		return "the game data does not match";
	}
	return "unknown";
}
