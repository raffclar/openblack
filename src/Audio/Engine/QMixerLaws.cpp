/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "QMixerLaws.h"

#include <cmath>
#include <cstdlib>

#include <algorithm>

using namespace openblack::audio;

float qmixer::Gain(int volume, int mainVolume)
{
	const int v = std::clamp(volume, 0, k_MaxVolume);
	const int m = std::clamp(mainVolume, 0, k_MaxVolume);
	// floor(m * v / 127) * 258, / 32767
	return static_cast<float>(m * v / 127 * 258) / 32767.0f;
}

float qmixer::DistanceGain(float minDistance, float maxDistance, float scale, float distance)
{
	if (distance > maxDistance)
	{
		return 0.0f;
	}
	if (distance <= minDistance || scale == 0.0f)
	{
		return 1.0f;
	}
	if (scale == 1.0f)
	{
		return minDistance / distance;
	}
	return minDistance / ((distance - minDistance) * scale + minDistance);
}

glm::vec3 qmixer::PolarRelative(glm::vec3 position)
{
	// On the game's thread, so with the FPU at 24 bits: every add, subtract, multiply, divide and square root rounds to a
	// float's 24-bit mantissa, while the loads of the doubles (and the compares against them) are exact and the arc
	// tangent keeps the full precision. The library's doubles: 0, 180, 90, 270 and 0.31847133757961782 = 1 / 3.14 (its
	// "1 / pi"). Each angle is atan(..) * 180 * that double, in that order; the polar point it sends (azimuth, range,
	// elevation) holds floats. Here each FPU step is a double operation rounded to a float (R).
	const auto R = [](double v) { return static_cast<float>(v); };
	constexpr double k_HalfTurnDegrees = 180.0;
	constexpr double k_InvPi = 0.31847133757961782; // 1 / 3.14
	const auto degrees = [&R](double atanValue) { return R(R(atanValue * k_HalfTurnDegrees) * k_InvPi); };
	const float x = position.x; // kept exact as a double
	const float y = position.y;
	const float z = position.z;
	const float xx = x * x;
	const float yy = y * y;
	const float range = std::sqrt((z * z + xx) + yy);
	float elevation = 0.0f;
	if (z != 0.0f)
	{
		if (y == 0.0f && x == 0.0f)
		{
			elevation = z < 0.0f ? -90.0f : 90.0f;
		}
		else
		{
			// the add, square root and divide at 24 bits each, the arc tangent, the two multiplies
			elevation = degrees(std::atan(static_cast<double>(z / std::sqrt(xx + yy))));
		}
	}
	// truncated to an integer and its absolute value; back to a float exactly
	const auto ftolAbs = [](float v) { return static_cast<float>(std::abs(static_cast<int32_t>(v))); };
	float azimuth = 0.0f;
	if (x == 0.0f)
	{
		azimuth = y < 0.0f ? 180.0f : 0.0f;
	}
	else if (y == 0.0f)
	{
		azimuth = x > 0.0f ? 90.0f : 270.0f; // (x is not 0 here)
	}
	else if (x > 0.0f)
	{
		azimuth = y > 0.0f ? R(90.0 - degrees(std::atan(static_cast<double>(y / x))))
		                   : R(90.0 + degrees(std::atan(static_cast<double>(ftolAbs(y) / x))));
	}
	else
	{
		azimuth = y < 0.0f ? R(270.0 - degrees(std::atan(static_cast<double>(ftolAbs(y) / ftolAbs(x)))))
		                   : R(270.0 + degrees(std::atan(static_cast<double>(y / ftolAbs(x)))));
	}
	// QMixer's half is run by its pump on the multimedia timer's thread (every 20 ms), so at 53 bits, not at the game's
	// 24 (setting the polar point only stores it; (inferred) the timer thread keeps the default control word): k = pi
	// (a double) * the float 1 / 180 (0.0055555557) in double; az = k * azimuth stored as a float; el = k * elevation
	// stays on the FPU (a double); flat = cos(el) * range and up = sin(el) * range stored as floats; right = sin(az) *
	// flat and ahead = cos(az) * flat stored as floats (sine and cosine at full precision). Emulated at 53 bits:
	// identical on the 9 points of test_audio_laws RelativeAxesExact.
	constexpr double k_Pi = 3.1415926535897931;
	constexpr float k_InvDegrees = 0.0055555556900799274f; // 1 / 180 as a float
	constexpr double k = k_Pi * static_cast<double>(k_InvDegrees);
	const float az = R(k * static_cast<double>(azimuth));
	const double el = k * static_cast<double>(elevation);
	const float flat = R(std::cos(el) * static_cast<double>(range));
	const float up = R(std::sin(el) * static_cast<double>(range));
	const float right = R(std::sin(static_cast<double>(az)) * static_cast<double>(flat));
	const float ahead = R(std::cos(static_cast<double>(az)) * static_cast<double>(flat));
	return {right, up, ahead};
}

float qmixer::FrequencyRatio(int sampleRate, int percent)
{
	if (sampleRate <= 0)
	{
		return static_cast<float>(percent) / 100.0f;
	}
	const auto frequency = static_cast<uint32_t>(sampleRate) * static_cast<uint32_t>(std::max(percent, 0)) / 100u;
	return static_cast<float>(frequency) / static_cast<float>(sampleRate);
}

int qmixer::StartPitch(int pitch, int deviation, int rand15)
{
	auto p = static_cast<uint32_t>(pitch > 0 ? pitch : 100);
	const uint32_t d = static_cast<uint32_t>(std::max(deviation, 0)) * p / 100;
	p -= d;
	p += static_cast<uint32_t>(std::clamp(rand15, 0, 32767)) * (2 * d) / 32767;
	return p == 0 ? 100 : static_cast<int>(p);
}
