/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "MenuBackdrop.h"

#include <cmath>

#include <algorithm>
#include <numbers>

#include "Common/X87.h"

namespace openblack::gui::menu_backdrop
{
namespace
{
constexpr float k_HalfTurn = std::numbers::pi_v<float>;
constexpr float k_Turn = 2.0f * std::numbers::pi_v<float>;
/// A swirl's place: (sin a + 1) and (cos b + 1) times 20 grid squares
constexpr float k_Reach = 20.0f;
/// Its strength, (2 cos c + 3) * 3, over its distance plus 10
constexpr float k_StrengthBase = 3.0f;
constexpr float k_StrengthScale = 3.0f;
constexpr float k_DistanceBase = 10.0f;
constexpr float k_SpeedRange = 1.0f;
constexpr float k_AngleRange = 8.0f;
/// The grid's squares across and down
constexpr int k_Across = k_Columns - 1;
constexpr int k_Down = k_Rows - 1;
constexpr float k_RefreshMilliseconds = 1000.0f / 60.0f;
constexpr int k_FadeMilliseconds = 1000;
constexpr int k_FadeLast = 999;
constexpr int k_Opaque = 255;
} // namespace

Swirls Start(const std::function<float(float, float)>& random)
{
	Swirls swirls {};
	for (auto& swirl : swirls)
	{
		const auto speedC = random(-k_SpeedRange, k_SpeedRange);
		const auto speedB = random(-k_SpeedRange, k_SpeedRange);
		const auto speedA = random(-k_SpeedRange, k_SpeedRange);
		swirl.speeds = {speedA, speedB, speedC};
		const auto angleC = random(-k_AngleRange, k_AngleRange);
		const auto angleB = random(-k_AngleRange, k_AngleRange);
		const auto angleA = random(-k_AngleRange, k_AngleRange);
		swirl.angles = {angleA, angleB, angleC};
	}
	return swirls;
}

float Wrap(float angle)
{
	if (angle > k_HalfTurn)
	{
		return angle - k_Turn;
	}
	if (angle < -k_HalfTurn)
	{
		return angle + k_Turn;
	}
	return angle;
}

void PlaceLayer(Swirls& swirls, int layer, float turn, std::span<glm::vec2, k_Points> coordinates)
{
	const auto first = static_cast<size_t>(layer) * k_SwirlsPerLayer;
	for (int column = 0; column < k_Columns; ++column)
	{
		for (int row = 0; row < k_Rows; ++row)
		{
			// The odd swirls pull the first coordinate about, the even ones the second
			float odd = 0.0f;
			float even = 0.0f;
			for (size_t k = first; k < first + k_SwirlsPerLayer; ++k)
			{
				auto& swirl = swirls.at(k);
				// Each sine and cosine stays wide until the next add or multiply
				const float across = x87::Add(x87::Sin(swirl.angles.x), 1.0) * k_Reach;
				const float down = x87::Add(x87::Cos(swirl.angles.y), 1.0) * k_Reach;
				const float dy = static_cast<float>(row) - down;
				const float dx = static_cast<float>(column) - across;
				const float strength = (x87::Mul(x87::Cos(swirl.angles.z), 2.0) + k_StrengthBase) * k_StrengthScale;
				const float pull = strength / (std::sqrt((dx * dx) + (dy * dy)) + k_DistanceBase);
				if (k % 2 == 1)
				{
					odd = pull + odd;
				}
				else
				{
					even = pull + even;
				}
				for (int axis = 0; axis < 3; ++axis)
				{
					swirl.angles[axis] = (turn * swirl.speeds[axis]) + swirl.angles[axis];
				}
				for (int axis = 0; axis < 3; ++axis)
				{
					swirl.angles[axis] = Wrap(swirl.angles[axis]);
				}
			}
			coordinates[(static_cast<size_t>(column) * k_Rows) + static_cast<size_t>(row)] = {odd, even};
		}
	}
}

glm::ivec2 PointOnScreen(int column, int row, glm::ivec2 size)
{
	return {(size.x * column) / k_Across, (size.y * row) / k_Down};
}

void StepFade(Fade& fade, bool boxGone, int frameMilliseconds)
{
	fade.boxGone = boxGone;
	fade.milliseconds = boxGone ? fade.milliseconds + frameMilliseconds : std::max(fade.milliseconds - frameMilliseconds, 0);
	// The black is set again every frame; at 0 there is none
	fade.alpha = static_cast<uint8_t>(std::clamp((fade.milliseconds * k_Opaque) / k_FadeMilliseconds, 0, k_Opaque));
}

uint8_t SixteenBitLevel(uint8_t level)
{
	constexpr uint8_t k_Levels = 17;
	return static_cast<uint8_t>((level >> 4u) * k_Levels);
}

float FrameMilliseconds(std::optional<uint32_t> last, uint32_t now)
{
	return last.has_value() ? static_cast<float>(now - *last) : 0.0f;
}

bool FadeOver(const Fade& fade)
{
	return fade.boxGone && fade.milliseconds > k_FadeLast;
}

std::optional<int> StepPace(Pace& pace, float frameMilliseconds)
{
	pace.sinceStep += frameMilliseconds;
	if (pace.sinceStep < k_RefreshMilliseconds)
	{
		return std::nullopt;
	}
	const auto milliseconds = static_cast<int>(pace.sinceStep);
	pace.sinceStep -= static_cast<float>(milliseconds);
	return milliseconds;
}

} // namespace openblack::gui::menu_backdrop
