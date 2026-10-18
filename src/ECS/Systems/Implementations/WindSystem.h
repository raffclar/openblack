/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include "ECS/Systems/WindSystemInterface.h"

#if !defined(LOCATOR_IMPLEMENTATIONS)
#error "ECS System implementations should only be included in Locator.cpp"
#endif

namespace openblack::ecs::systems
{

class WindSystem final: public WindSystemInterface
{
public:
	void Update(std::chrono::milliseconds frameRealTime) override;
	[[nodiscard]] const wind::Wind& GetWind() const override { return _wind; }
	[[nodiscard]] float GetSeconds() const override { return _seconds; }
	[[nodiscard]] glm::vec2 VelocityAt(glm::vec2 point) const override { return wind::VelocityAt(_wind, point, _seconds); }
	void SetDirectionOverride(std::optional<float> angle) override { _directionOverride = angle; }

private:
	wind::Wind _wind;
	std::optional<float> _directionOverride;
	float _seconds {0.0f};
};

} // namespace openblack::ecs::systems
