/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <cstdint>

#include "3D/SkyType.h"
#include "ECS/Systems/SkyFrameSystemInterface.h"

#if !defined(LOCATOR_IMPLEMENTATIONS)
#error "Locator interface implementations should only be included in Locator.cpp, use interface instead."
#endif

namespace openblack::ecs::systems
{
/// The sky of the frame, as the game starts it
class SkyFrameSystem final: public SkyFrameSystemInterface
{
public:
	void SetThresholds(const sky_type::Thresholds& thresholds) noexcept override { _thresholds = thresholds; }
	[[nodiscard]] const sky_type::Thresholds& GetThresholds() const noexcept override { return _thresholds; }

	void SampleFrame(float visualHour) noexcept override;
	[[nodiscard]] float GetCurrentSkyType() const noexcept override { return _skyType; }
	[[nodiscard]] float GetCurrentHour() const noexcept override { return _hour; }
	void Jump(float visualHour) noexcept override;
	[[nodiscard]] sky_type::DomeBlend& GetDome() noexcept override { return _dome; }

	void OnLandscapeOpened() noexcept override { ++_landscapeGeneration; }
	[[nodiscard]] uint32_t GetLandscapeGeneration() const noexcept override { return _landscapeGeneration; }

private:
	sky_type::Thresholds _thresholds {};
	float _skyType {0.0f};
	float _hour {0.0f};
	sky_type::DomeBlend _dome;
	uint32_t _landscapeGeneration {0};
};
} // namespace openblack::ecs::systems
