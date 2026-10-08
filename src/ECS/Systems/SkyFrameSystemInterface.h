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

namespace openblack::ecs::systems
{
/// The sky of the frame: the sky type's thresholds, the sky type sampled once a frame from the visual hour, the dome's
/// blend that follows it, and the count of landscapes opened, which the clouds and the land light watch
/// (Locator::skyFrameSystem)
class SkyFrameSystemInterface
{
public:
	virtual ~SkyFrameSystemInterface() = default;

	/// The day and night thresholds A to D, in visual hours; 0 until set
	virtual void SetThresholds(const sky_type::Thresholds& thresholds) noexcept = 0;
	[[nodiscard]] virtual const sky_type::Thresholds& GetThresholds() const noexcept = 0;

	/// Brings the visual hour into [0, 24) and keeps it with its sky type, once a frame before the land light and the
	/// dome
	virtual void SampleFrame(float visualHour) noexcept = 0;
	/// The sky type of the last SampleFrame, 0 before the first one. It runs the game's way, 0 by day, 1 at dusk and 2 at
	/// night: the other way round from a sky type that counts from 0 at night
	[[nodiscard]] virtual float GetCurrentSkyType() const noexcept = 0;
	/// The visual hour of the last SampleFrame
	[[nodiscard]] virtual float GetCurrentHour() const noexcept = 0;
	/// A jump of the visual time: SampleFrame, then the whole dome is blended again at once for that sky type
	virtual void Jump(float visualHour) noexcept = 0;
	/// The dome's slow follow of the sky type
	[[nodiscard]] virtual sky_type::DomeBlend& GetDome() noexcept = 0;

	/// Counts a landscape opened
	virtual void OnLandscapeOpened() noexcept = 0;
	[[nodiscard]] virtual uint32_t GetLandscapeGeneration() const noexcept = 0;
};
} // namespace openblack::ecs::systems
