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

#include <array>
#include <span>

/// The sky type of the original, a continuous 2 (night) .. 1 (dusk) .. 0 (day) of the visual hour, and everything
/// built on it. Wiki: docs/bw1-notes/day-night-weather.md, "Sky type".
///
/// The thresholds A..D are written only when the visual time cycle is set (DayNightClock::SetCycle) and, once, by the
/// sky's set-up (4.5 / 7 / 7.5 / 8.25, overwritten right after by the cycle). In the campaign they are always 0.786 /
/// 1.206 / 1.626 / 2.046 visual hours (the default cycle 1700 / 0.083 / 0.07; no CHL script calls
/// SET_GAME_TIME_PROPERTIES and Land2 / Land3 SET_NIGHTTIME repeat the defaults).
///
/// Once a frame the sky drawing samples the visual hour: that is Frame() / FrameHour(), what the sky dome, the land
/// light table and the sound map read. Code that computes the sky type of the visual time itself in the original (the
/// night check, the time update, the fireflies) uses At(visual hour).
///
/// Convention: this is the original's sky type. openblack's old Sky::GetCurrentSkyType ran the other way (0 night ..
/// 2 day, = 2 - sky type) on the script hour with made-up thresholds; it is gone (LandLightTable::Build takes T).
namespace openblack::sky_type
{

/// A..D
using Thresholds = std::array<float, 4>;

/// The thresholds the sky's set-up writes
constexpr Thresholds k_SetUpThresholds = {4.5f, 7.0f, 7.5f, 8.25f};

/// Sets the day/night thresholds A..D
void SetThresholds(float a, float b, float c, float d);
[[nodiscard]] const Thresholds& GetThresholds();

/// The sky type of an hour with the given thresholds (DayNightClock::SkyType forwards here): fold at 12 only when
/// hour > 12, then strict < in turn against A..D: 2, 2 - (h - A) / (B - A), 1, 1 - (h - C) / (D - C), 0.
/// No division by zero can happen: with A = B (or C = D) the ramp branch is unreachable.
[[nodiscard]] float At(float hour, const Thresholds& thresholds);
/// The same with the sky's thresholds
[[nodiscard]] float At(float hour);

/// The hour brought into [0, 24) is stored with its sky type. Called once a frame with the visual time:
/// Renderer::PreDraw does, before the land light table and the dome.
void SampleFrame(float visualHour);
/// The sky type of the last SampleFrame (0 before the first one)
[[nodiscard]] float Frame();
/// The hour of the last SampleFrame
[[nodiscard]] float FrameHour();

/// The hour jump (forced visual time, DayNightClock::SetScriptTime): SampleFrame(hour), then the dome is rebuilt
/// whole at once with that sky type (DomeBlend::Jump). The original also rebuilds the light table here; not needed:
/// openblack rebuilds the table every frame (Renderer::UpdateLandLight).
void Jump(float visualHour);

/// sky type > 1.2, compared as a double (exactly 1.2), so the float 1.2f (1.20000005) is already night. The child at
/// the creche has the same compare inline.
[[nodiscard]] bool IsVisualNight(float skyType);

/// The evening ramp of the villagers' Relaxation / Sleep desires: u = 24 - visual (stored as float), s = D + o
/// (stored); u < s -> 1; !((D + w) + o > u) -> 0; else 1 - (u - s) / w. Relaxation passes (0.5 of a game info value
/// twice) and Sleep (1, 0). No openblack caller yet (the town's desires).
[[nodiscard]] float EveningRamp(float visualHour, float width, float offset);

/// The time column of the land light table: x = (2 - T) * 6.0f, a fold x > 12 -> 24 - x that can never run, then
/// x * 2.5f = (2 - T) * 15; the caller takes the truncation of it as the column and the fraction * 256 as the weight.
[[nodiscard]] float LightColumn(float skyType);
/// The haze factor of the land light table: v = T > 1 ? 2 - T : T, v * v (0 by day and in full night, 1 in full dusk);
/// with v * v > 0 the near / far haze distances become v2 * 0.0075 + 0.0025 and v2 * 0.00013888883 + 0.0011111111
[[nodiscard]] float HazeFactor(float skyType);

/// One dome blend call: rows [firstRow, firstRow + rowCount) of the three dynamic dome textures (one per alignment) are
/// blended again with sky type `skyType`
struct DomeBlock
{
	float skyType;
	int firstRow;
	int rowCount;
};

/// The dome's two-texture blend weight: T <= 1 -> w = trunc(T * 255.0f) between the day (time of day 0) and the dusk
/// (1) textures, else w = trunc((T - 1) * 255) between the dusk and the night (2) ones. The lower texture gets 255 - w,
/// the upper w.
struct DomeWeight
{
	int lower;  ///< time-of-day index of the source with 255 - w: 0 _day, 1 _dusk, 2 _night
	int upper;  ///< the one with w
	int weight; ///< w
};
[[nodiscard]] DomeWeight DomeWeightOf(float skyType);

/// The 555 blend path (the 565 path is not ported): tables low[i] = (i (255 - w)) >> 8 and high[i] = (i w) >> 8, each
/// 5-bit channel out = low[lower] + high[upper]; bit 15 comes out 0. The sum of the weights is 255 / 256, so even with
/// an integer sky type a channel of 31 gives 30.
[[nodiscard]] uint16_t BlendTexel555(uint16_t lower, uint16_t upper, int weight);
/// The same over whole rows: dst[i] = BlendTexel555(lower[i], upper[i], weight)
void BlendRows555(std::span<uint16_t> dst, std::span<const uint16_t> lower, std::span<const uint16_t> upper, int weight);

/// The dome's slow follow of the sky type: the sky type the dome is being built with and the rows built so far. Once a
/// frame after SampleFrame: when the whole dome is built and |Frame() - built| > 0.03 (compared as the double of
/// 0.03f, strict), the new sky type is latched and the rows start again from 0; while rows are missing, 32 more are
/// blended with the latched sky type, the first block in the same frame as the latch. The original uses 128 rows
/// instead of 256 at the two lowest detail levels (graphics::DetailLevel::skyNoBlend): there it does not blend but
/// keeps a single copy of the day textures with a per-T tint of the dome colour. That path is not ported: openblack
/// always blends 256 rows, whatever the detail level.
class DomeBlend
{
public:
	static constexpr int k_Rows = 256; ///< 128 at the no-blend detail levels, the blend path only
	static constexpr int k_RowsPerFrame = 32;
	static constexpr double k_Hysteresis = static_cast<double>(0.03f);

	/// What a frame asks the dome textures to redo, in order: a pending whole rebuild of Jump and this frame's block
	struct Blocks
	{
		std::array<DomeBlock, 2> blocks {};
		int count {0};
	};

	/// The per-frame follow (without the land light table, which Renderer::UpdateLandLight does)
	[[nodiscard]] Blocks Advance(float frameSkyType);
	/// The jump: built = T, no rows done and a whole rebuild with T, handed out by the next Advance
	void Jump(float frameSkyType);

	[[nodiscard]] float Built() const { return _built; }
	[[nodiscard]] int RowsDone() const { return _rowsDone; }

private:
	float _built {0.0f};    ///< 0 at start and again at the sky's set-up
	int _rowsDone {k_Rows}; ///< The sky's set-up builds the whole dome
	bool _rebuildPending {false};
	float _rebuildSkyType {0.0f};
};

/// The sky's dome blend (one, like the original's)
[[nodiscard]] DomeBlend& Dome();

} // namespace openblack::sky_type
