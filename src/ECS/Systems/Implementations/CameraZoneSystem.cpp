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

#include <cstddef>

#include <stdexcept>
#include <vector>

#include <EXCFile.h>
#include <entt/core/hashed_string.hpp>
#include <spdlog/spdlog.h>

#include "Audio/GameSoundEffects.h"
#include "Audio/Sound.h"
#include "ECS/Systems/ExplosionSystemInterface.h"
#include "ECS/Systems/ParticleSystemInterface.h"
#include "FileSystem/FileSystemInterface.h"
#include "Locator.h"
#include "Particles/LightSheet.h"
#include "Resources/ResourcesInterface.h"

using namespace openblack;
using namespace openblack::ecs::systems;

namespace
{
/// The force field: a grey wall of light 250 units high, on up to 256 points
constexpr uint32_t k_ForceFieldRgb = 0x808080u;
constexpr float k_ForceFieldHeight = 250.0f;
constexpr size_t k_ForceFieldPoints = 256;
/// Hitting the fence lights the force field up within this distance of the camera
constexpr float k_PulseRadius = 200.0f;
/// And shakes the camera every way, at full strength within this distance, for this long
constexpr float k_ShakeRadius = 100.0f;
constexpr float k_ShakeStrength = 1.0f;
constexpr float k_ShakeSeconds = 0.4f;
} // namespace

CameraZoneSystem::CameraZoneSystem()
    : _forceField(std::make_shared<particles::LightSheet>())
{
	_forceField->StartPulsed(k_ForceFieldRgb, k_ForceFieldHeight, k_ForceFieldPoints);
	_forceField->SetHidden(true);
}

void CameraZoneSystem::PlaceForceField()
{
	// It runs all the way round, back to the fence's first corner
	std::vector<glm::vec3> points;
	if (_zones.fenceOn && !_zones.fence.empty())
	{
		points.assign(_zones.fence.begin(), _zones.fence.end());
		points.push_back(_zones.fence.front());
	}
	_forceField->SetPoints(std::move(points));
	// Without the fence it is neither drawn nor fades
	_forceField->SetHidden(!_zones.fenceOn);
	if (!_forceFieldShown && Locator::particleSystem::has_value())
	{
		Locator::particleSystem::value().AddLightSheet(_forceField);
		_forceFieldShown = true;
	}
}

void CameraZoneSystem::FenceHit(const glm::vec3& origin, const glm::vec3& listener)
{
	++_fenceHits;
	// The sound of going out of bounds, heard from where the camera is
	audio::PlayGameSoundEffect(static_cast<entt::id_type>(audio::SoundId::G_OutOfBounds), listener);
	if (Locator::explosionSystem::has_value())
	{
		Locator::explosionSystem::value().AddShake(origin, k_ShakeRadius, k_ShakeStrength, k_ShakeSeconds, false);
	}
	_forceField->Pulse(origin, k_PulseRadius);
}

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
	PlaceForceField();
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
	PlaceForceField();
	return true;
}
