/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <string>
#include <string_view>

#include "Camera/CameraZones.h"

namespace openblack::ecs::systems
{

/// The camera zones of the land being played: the fence the world camera is kept inside, its height limits and the
/// places it is kept out of, as the land's scripts last set them. A new land starts with none.
class CameraZoneSystemInterface
{
public:
	virtual ~CameraZoneSystemInterface() = default;

	/// A script sets the zones from one of the files under Data/Zones: those it set before go, and the file's are kept
	/// to, its fence and exclusions switched on. Whether the file could be read; when it can't, the land has none.
	virtual bool SetZones(std::string_view fileName) = 0;
	[[nodiscard]] virtual const camera_zones::Zones& GetZones() const = 0;
	/// The file the zones came from, empty for none
	[[nodiscard]] virtual const std::string& GetFileName() const = 0;
};

} // namespace openblack::ecs::systems
