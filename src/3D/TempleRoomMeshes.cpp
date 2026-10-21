/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "TempleRoomMeshes.h"

namespace openblack::temple_room_meshes
{

std::optional<std::string> LoadedName(std::string_view stem, bool lightmaps, bool hasLowMesh)
{
	if (stem.ends_with(k_LowEnd))
	{
		if (lightmaps)
		{
			return std::nullopt;
		}
		// "mainlo_l3d" stands for "main_l3d"
		return std::string(stem.substr(0, stem.size() - k_LowEnd.size())) + std::string(k_End);
	}
	if (!lightmaps && hasLowMesh)
	{
		return std::nullopt;
	}
	return std::string(stem);
}

std::string LowName(std::string_view stem)
{
	if (!stem.ends_with(k_End))
	{
		return std::string(stem) + std::string(k_LowEnd);
	}
	return std::string(stem.substr(0, stem.size() - k_End.size())) + std::string(k_LowEnd);
}

} // namespace openblack::temple_room_meshes
