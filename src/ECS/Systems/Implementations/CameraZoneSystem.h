/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <memory>
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
	CameraZoneSystem();

	bool SetZones(std::string_view fileName) override;
	void Reset() override;
	void ClearZones() override;
	[[nodiscard]] const camera_zones::Zones& GetZones() const override { return _zones; }
	[[nodiscard]] const std::string& GetFileName() const override { return _fileName; }
	/// A save keeps the zones as the scripts last set them: the fence, the height limits, the places the camera is kept
	/// out of and their switches
	void SaveState(reflection::BinaryWriter& out) const override;
	bool LoadState(reflection::BinaryReader& in) override;
	void FenceHit(const glm::vec3& origin, const glm::vec3& listener) override;
	[[nodiscard]] const particles::LightSheet& GetForceField() const override { return *_forceField; }
	[[nodiscard]] uint32_t GetFenceHits() const override { return _fenceHits; }
	void SetInclusionDistance(float distance) override { _inclusionDistance = distance; }
	[[nodiscard]] float GetInclusionDistance() const override { return _inclusionDistance; }

	/// The zones a file holds, switched on as a script's are
	[[nodiscard]] static camera_zones::Zones FromFile(const exc::EXCZones& file);

private:
	/// The force field stands along the fence while the fence is kept to, and is drawn with the game's other sheets
	void PlaceForceField();

	camera_zones::Zones _zones;
	std::string _fileName;
	std::shared_ptr<particles::LightSheet> _forceField;
	uint32_t _fenceHits {0};
	float _inclusionDistance {0.0f};
};

} // namespace openblack::ecs::systems
