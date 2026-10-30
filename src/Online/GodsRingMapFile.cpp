/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "GodsRingMapFile.h"

#include <exception>
#include <filesystem>

#include <LNDFile.h>
#include <OnlineMapFile.h>
#include <spdlog/spdlog.h>

#include "3D/GodsRingLand.h"
#include "FileSystem/FileSystemInterface.h"
#include "GodsRingMap.h"
#include "OnlineMapLand.h"

using namespace openblack;

std::string gods_ring_map_file::WireName()
{
	return std::string(online_map_land::k_Folder) + "/" + std::string(gods_ring_map::k_File) +
	       std::string(online_map_land::k_Extension);
}

std::optional<std::vector<uint8_t>> gods_ring_map_file::Make(std::span<const uint8_t> templateMap)
{
	const auto file = onlinemap::Parse(std::as_bytes(templateMap));
	if (!file.has_value())
	{
		return std::nullopt;
	}
	const auto village = gods_ring_map::ReadTemplate(file->script, gods_ring_map::k_TemplateGod, gods_ring_land::k_MapMiddle);
	if (!village.has_value())
	{
		return std::nullopt;
	}
	const auto slots = gods_ring_map::LayOutSlots(gods_ring_land::k_MapMiddle);
	return Write(gods_ring_map::MapScript(*village, slots), gods_ring_land::Build());
}

std::optional<std::vector<uint8_t>> gods_ring_map_file::MakeFromGameFolder(filesystem::FileSystemInterface& files)
{
	std::optional<std::vector<uint8_t>> file;
	try
	{
		const std::filesystem::path templatePath(gods_ring_map::k_TemplateMap);
		if (files.Exists(templatePath))
		{
			file = Make(files.ReadAll(templatePath));
		}
	}
	catch (const std::exception& e)
	{
		SPDLOG_LOGGER_ERROR(spdlog::get("game"), "The land {} does not load: {}", gods_ring_map::k_TemplateMap, e.what());
	}
	if (!file.has_value())
	{
		SPDLOG_LOGGER_ERROR(spdlog::get("game"), "The map for 32 gods needs the village of the land {}",
		                    gods_ring_map::k_TemplateMap);
	}
	return file;
}

std::optional<std::vector<uint8_t>> gods_ring_map_file::Write(std::string_view script, const LandData& land)
{
	return onlinemap::Write(script, land.LndBytes());
}

std::optional<gods_ring_map_file::Parts> gods_ring_map_file::Read(std::span<const uint8_t> bytes)
{
	const auto file = onlinemap::Parse(std::as_bytes(bytes));
	if (!file.has_value())
	{
		return std::nullopt;
	}
	const auto* landscape = reinterpret_cast<const uint8_t*>(file->landscape.data());
	const std::vector<uint8_t> landscapeBytes(landscape, landscape + file->landscape.size());
	lnd::LNDFile lnd;
	if (lnd.Open(landscapeBytes) != lnd::LNDResult::Success)
	{
		return std::nullopt;
	}
	return Parts {.script = std::string(file->script), .land = LandData::FromLnd(lnd)};
}
