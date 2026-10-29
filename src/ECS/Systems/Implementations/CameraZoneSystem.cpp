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
#include <PackFile.h>
#include <entt/core/hashed_string.hpp>
#include <spdlog/spdlog.h>

#include "ECS/Reflection/BinaryArchive.h"
#include "ECS/Systems/EffectRequestSystemInterface.h"
#include "ECS/Systems/ExplosionSystemInterface.h"
#include "FileSystem/FileSystemInterface.h"
#include "Locator.h"
#include "Particles/LightSheet.h"
#include "Resources/ResourcesInterface.h"
#include "Sound/GameSoundEffects.h"
#include "Sound/SoundId.h"

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
constexpr int32_t k_ShakeMilliseconds = 400;
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
	// The particles keep the sheet until a new land or a load clears their list, so it is given to them again each time
	if (Locator::effectRequestSystem::has_value())
	{
		Locator::effectRequestSystem::value().AddLightSheet(_forceField);
	}
}

void CameraZoneSystem::FenceHit(const glm::vec3& origin, const glm::vec3& listener)
{
	++_fenceHits;
	// The sound of going out of bounds, heard from where the camera is. It has no owner, so it follows the listener
	// each turn.
	audio::StartGameSoundEffect(
	    static_cast<entt::id_type>(audio::SoundId::G_OutOfBounds),
	    {.position = listener, .voicePlayType = pack::AudioBankLoop::Once, .follows = audio::Follows::Listener});
	if (Locator::explosionSystem::has_value())
	{
		Locator::explosionSystem::value().AddShake(origin, k_ShakeRadius, k_ShakeStrength, k_ShakeMilliseconds, false);
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

void CameraZoneSystem::Reset()
{
	_zones = {};
	_fileName.clear();
	_fenceHits = 0;
	PlaceForceField();
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
	const auto id = camera_zones::FileId(name);
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
	// A file registered at the start that did not read is not in the cache
	const auto* file = cache.Find(id);
	if (file == nullptr)
	{
		return false;
	}
	_zones = FromFile(file->GetZones());
	_fileName = name;
	PlaceForceField();
	return true;
}

void CameraZoneSystem::SaveState(reflection::BinaryWriter& out) const
{
	reflection::Save(out, _fileName);
	reflection::Save(out, _zones.exclusionsOn);
	reflection::Save(out, _zones.fenceOn);
	reflection::Save(out, _zones.useMaxAltitude);
	reflection::Save(out, _zones.useHeightAboveLand);
	reflection::Save(out, _zones.maxAltitude);
	reflection::Save(out, _zones.heightAboveLand);
	reflection::Save(out, _zones.fence);
	reflection::Save(out, _zones.exclusions.size());
	for (const auto& exclusion : _zones.exclusions)
	{
		reflection::Save(out, exclusion.position);
		reflection::Save(out, exclusion.radius);
		reflection::Save(out, exclusion.height);
		reflection::Save(out, exclusion.kind);
	}
}

bool CameraZoneSystem::LoadState(reflection::BinaryReader& in)
{
	std::string fileName;
	camera_zones::Zones zones;
	reflection::Load(in, fileName);
	reflection::Load(in, zones.exclusionsOn);
	reflection::Load(in, zones.fenceOn);
	reflection::Load(in, zones.useMaxAltitude);
	reflection::Load(in, zones.useHeightAboveLand);
	reflection::Load(in, zones.maxAltitude);
	reflection::Load(in, zones.heightAboveLand);
	reflection::Load(in, zones.fence);
	size_t count = 0;
	reflection::Load(in, count);
	for (size_t i = 0; i < count && !in.Failed(); ++i)
	{
		auto& exclusion = zones.exclusions.emplace_back();
		reflection::Load(in, exclusion.position);
		reflection::Load(in, exclusion.radius);
		reflection::Load(in, exclusion.height);
		reflection::Load(in, exclusion.kind);
	}
	if (in.Failed())
	{
		return false;
	}
	_fileName = std::move(fileName);
	_zones = std::move(zones);
	PlaceForceField();
	return true;
}
