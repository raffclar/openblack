/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#define LOCATOR_IMPLEMENTATIONS

#include "CameraZoneSystem.h"

#include <stdexcept>

#include <EXCFile.h>
#include <entt/core/hashed_string.hpp>
#include <spdlog/spdlog.h>

#include "FileSystem/FileSystemInterface.h"
#include "Locator.h"
#include "Resources/ResourcesInterface.h"

using namespace openblack;
using namespace openblack::ecs::systems;

camera_zones::Zones CameraZoneSystem::FromFile(const exc::EXCZones& file)
{
	camera_zones::Zones zones {
	    // A script's zones are always kept to, whatever the file says
	    .exclusionsOn = true,
	    .fenceOn = true,
	    .useMaxAltitude = file.useMaxAltitude,
	    .useHeightAboveLand = file.useHeightAboveLand,
	    .maxAltitude = file.maxAltitude,
	    .heightAboveLand = file.heightAboveLand,
	};
	for (const auto& corner : file.fence)
	{
		zones.fence.emplace_back(corner[0], corner[1], corner[2]);
	}
	for (const auto& exclusion : file.exclusions)
	{
		zones.exclusions.push_back({
		    .position = {exclusion.position[0], exclusion.position[1], exclusion.position[2]},
		    .radius = exclusion.radius,
		    .height = exclusion.height,
		    .kind = exclusion.kind == exc::EXCExclusion::Kind::Cylinder ? camera_zones::Exclusion::Kind::Cylinder
		                                                                : camera_zones::Exclusion::Kind::Dome,
		});
	}
	return zones;
}

bool CameraZoneSystem::SetZones(std::string_view fileName)
{
	// The zones set before go first, so a file that can't be read leaves none
	_zones = {};
	_fileName.clear();
	if (!Locator::resources::has_value() || !Locator::filesystem::has_value())
	{
		return false;
	}
	const auto name = std::string(fileName);
	const auto id = entt::hashed_string::value(("camera/zones/" + name).c_str());
	auto& cache = Locator::resources::value().GetCameraZones();
	if (!cache.Contains(id))
	{
		try
		{
			const auto path = Locator::filesystem::value().GetPath<filesystem::Path::Data>() / "Zones" / name;
			cache.Load(id, resources::CameraZoneLoader::FromDiskTag {}, path);
		}
		catch (const std::runtime_error& error)
		{
			SPDLOG_LOGGER_ERROR(spdlog::get("scripting"), "Camera zone file {} can't be opened: {}", name, error.what());
			return false;
		}
	}
	_zones = FromFile(cache.Handle(id)->GetZones());
	_fileName = name;
	return true;
}
