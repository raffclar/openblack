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

#include "ECS/Systems/CameraZoneSystemInterface.h"

#if !defined(LOCATOR_IMPLEMENTATIONS)
#error "Locator interface implementations should only be included in Locator.cpp, use interface instead."
#endif

namespace openblack::exc
{
struct EXCZones;
} // namespace openblack::exc

namespace openblack::ecs::systems
{

class CameraZoneSystem final: public CameraZoneSystemInterface
{
public:
	bool SetZones(std::string_view fileName) override;
	[[nodiscard]] const camera_zones::Zones& GetZones() const override { return _zones; }
	[[nodiscard]] const std::string& GetFileName() const override { return _fileName; }

	/// The zones a file holds, switched on as a script's are
	[[nodiscard]] static camera_zones::Zones FromFile(const exc::EXCZones& file);

private:
	camera_zones::Zones _zones;
	std::string _fileName;
};

} // namespace openblack::ecs::systems
