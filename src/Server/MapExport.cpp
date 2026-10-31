/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "MapExport.h"

#include <cstdint>

#include <fstream>
#include <iterator>
#include <vector>

#include "FileSystem/FileSystemInterface.h"
#include "Online/GodsRingMap.h"
#include "Online/GodsRingMapFile.h"

namespace openblack::server
{

std::optional<std::string> WriteGodsRingMap(const std::filesystem::path& gamePath, const std::filesystem::path& out)
{
	const auto templatePath =
	    filesystem::FileSystemInterface::FindIgnoringCase(gamePath, std::filesystem::path(gods_ring_map::k_TemplateMap));
	if (!templatePath.has_value())
	{
		return "the game folder has no " + std::string(gods_ring_map::k_TemplateMap);
	}
	std::ifstream in(*templatePath, std::ios::binary);
	const std::vector<uint8_t> templateMap((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
	const auto file = gods_ring_map_file::Make(templateMap);
	if (!file.has_value())
	{
		return "the land " + templatePath->generic_string() + " gives no village for the map";
	}
	std::ofstream stream(out, std::ios::binary | std::ios::trunc);
	stream.write(reinterpret_cast<const char*>(file->data()), static_cast<std::streamsize>(file->size()));
	if (!stream.good())
	{
		return "the file " + out.generic_string() + " could not be written";
	}
	return std::nullopt;
}

} // namespace openblack::server
