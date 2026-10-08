/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "SkyType.h"

#include <cassert>
#include <cmath>

#include "ECS/Systems/SkyFrameSystemInterface.h"
#include "Locator.h"

using namespace openblack;

namespace
{
constexpr float k_Half = 12.0f;
constexpr float k_Day = 24.0f;
constexpr float k_Night = 2.0f;
constexpr float k_Dusk = 1.0f;
constexpr double k_NightAbove = 1.2; ///< compared as a double
constexpr float k_ColumnHours = 6.0f;
constexpr float k_ColumnScale = 2.5f;
constexpr float k_WeightSteps = 255.0f;

/// The sky of the frame (Locator::skyFrameSystem)
openblack::ecs::systems::SkyFrameSystemInterface& SkyFrame()
{
	return openblack::Locator::skyFrameSystem::value();
}
} // namespace

void sky_type::SetThresholds(float a, float b, float c, float d)
{
	SkyFrame().SetThresholds({a, b, c, d});
}

const sky_type::Thresholds& sky_type::GetThresholds()
{
	return SkyFrame().GetThresholds();
}

float sky_type::At(float hour, const Thresholds& thresholds)
{
	// Only hour > 12 folds (not 12 itself, not NaN)
	const float t = hour > k_Half ? k_Day - hour : hour;
	// Each compare takes the branch on "<" or unordered. Written as !(t >= x) so that a NaN hour gives 2 (night) like
	// the original.
	if (!(t >= thresholds[0]))
	{
		return k_Night;
	}
	if (!(t >= thresholds[1]))
	{
		return k_Night - (t - thresholds[0]) / (thresholds[1] - thresholds[0]);
	}
	if (!(t >= thresholds[2]))
	{
		return k_Dusk;
	}
	if (!(t >= thresholds[3]))
	{
		return k_Dusk - (t - thresholds[2]) / (thresholds[3] - thresholds[2]);
	}
	return 0.0f;
}

float sky_type::At(float hour)
{
	return At(hour, SkyFrame().GetThresholds());
}

void sky_type::SampleFrame(float visualHour)
{
	SkyFrame().SampleFrame(visualHour);
}

float sky_type::Frame()
{
	return SkyFrame().GetCurrentSkyType();
}

float sky_type::FrameHour()
{
	return SkyFrame().GetCurrentHour();
}

void sky_type::Jump(float visualHour)
{
	SkyFrame().Jump(visualHour);
}

bool sky_type::IsVisualNight(float skyType)
{
	// Strict, against the double 1.2
	return static_cast<double>(skyType) > k_NightAbove;
}

float sky_type::EveningRamp(float visualHour, float width, float offset)
{
	const float d = SkyFrame().GetThresholds()[3];
	const float u = k_Day - visualHour; // stored as float
	const float s = d + offset;         // stored as float
	if (u < s)
	{
		return 1.0f;
	}
	if (!((d + width) + offset > u))
	{
		return 0.0f;
	}
	return 1.0f - (u - s) / width;
}

float sky_type::LightColumn(float skyType)
{
	float x = (k_Night - skyType) * k_ColumnHours;
	if (x > k_Half)
	{
		x = k_Day - x; // never taken for a sky type in [0, 2]
	}
	return x * k_ColumnScale;
}

float sky_type::HazeFactor(float skyType)
{
	const float v = skyType > k_Dusk ? k_Night - skyType : skyType;
	return v * v;
}

sky_type::DomeWeight sky_type::DomeWeightOf(float skyType)
{
	// The float to int conversion truncates
	if (!(skyType > k_Dusk))
	{
		return {0, 1, static_cast<int>(skyType * k_WeightSteps)};
	}
	return {1, 2, static_cast<int>((skyType - k_Dusk) * k_WeightSteps)};
}

uint16_t sky_type::BlendTexel555(uint16_t lower, uint16_t upper, int weight)
{
	// The caller passes 255 - w, the callee masks it with 0xFF and takes 255 - that
	const int lowWeight = (255 - weight) & 0xFF;
	const int highWeight = 255 - lowWeight;
	auto channel = [&](int shift) {
		const int lo = (lower >> shift) & 0x1F;
		const int hi = (upper >> shift) & 0x1F;
		// low[i] = (i lowWeight) >> 8, high[i] = (i highWeight) >> 8
		return ((lo * lowWeight) >> 8) + ((hi * highWeight) >> 8);
	};
	// Red (>> 10), green (>> 5), blue; the shifts leave bit 15 at 0
	return static_cast<uint16_t>((channel(10) << 10) | (channel(5) << 5) | channel(0));
}

void sky_type::BlendRows555(std::span<uint16_t> dst, std::span<const uint16_t> lower, std::span<const uint16_t> upper,
                            int weight)
{
	assert(lower.size() >= dst.size() && upper.size() >= dst.size());
	for (size_t i = 0; i < dst.size(); ++i)
	{
		dst[i] = BlendTexel555(lower[i], upper[i], weight);
	}
}

sky_type::DomeBlend::Blocks sky_type::DomeBlend::Advance(float frameSkyType)
{
	Blocks out;
	if (_rebuildPending)
	{
		out.blocks[out.count++] = {_rebuildSkyType, 0, k_Rows};
		_rebuildPending = false;
	}
	// The difference is rounded to float under the 24-bit FPU hypothesis of DayNightClock::SetCycle, then compared
	// against the double hysteresis
	if (_rowsDone >= k_Rows && static_cast<double>(std::fabs(frameSkyType - _built)) > k_Hysteresis)
	{
		_built = frameSkyType;
		_rowsDone = 0;
	}
	if (_rowsDone < k_Rows)
	{
		out.blocks[out.count++] = {_built, _rowsDone, k_RowsPerFrame};
		_rowsDone += k_RowsPerFrame;
	}
	return out;
}

void sky_type::DomeBlend::Jump(float frameSkyType)
{
	_built = frameSkyType;
	_rowsDone = 0;
	_rebuildPending = true;
	_rebuildSkyType = frameSkyType; // the whole dome is blended with T
}

sky_type::DomeBlend& sky_type::Dome()
{
	return SkyFrame().GetDome();
}
