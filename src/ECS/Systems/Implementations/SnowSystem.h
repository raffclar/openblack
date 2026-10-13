/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <vector>

#include <entt/entity/fwd.hpp>

#include "3D/SnowCover.h"
#include "3D/WaterRings.h"
#include "ECS/Systems/SnowSystemInterface.h"

#if !defined(LOCATOR_IMPLEMENTATIONS)
#error "ECS System implementations should only be included in Locator.cpp"
#endif

namespace openblack::ecs::systems
{

class SnowSystem final: public SnowSystemInterface
{
public:
	SnowSystem();

	void Reset() override;
	void ProcessTurn(std::span<const components::Storm> storms) override;
	void Cover(float depth) override;
	void StepObjects(Registry& registry, const ObjectDrawListSystemInterface& drawList, std::chrono::milliseconds frameRealTime,
	                 std::chrono::milliseconds frameGameTime) override;

	[[nodiscard]] std::span<const float> GetDepths() const override { return _depths; }
	[[nodiscard]] float GetDepth(glm::vec2 xz) const override { return snow_cover::DepthAt(_depths, xz); }
	[[nodiscard]] uint32_t GetRevision() const override { return _revision; }

private:
	std::vector<float> _depths;
	snow_cover::Melting _melting;
	uint32_t _revision {0};
	/// The things show their snow at the same pace as the rings on the water: 60 steps a second at most
	water_rings::Pace _objectPace;
	/// The things that the land drew on the last step
	std::vector<entt::entity> _drawn;
};

} // namespace openblack::ecs::systems
