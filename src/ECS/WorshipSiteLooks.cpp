/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "WorshipSiteLooks.h"

#include <cmath>

#include "Common/X87.h"

namespace openblack::ecs::worship_site_looks
{

namespace
{
/// Each worshipper that does not dance adds this to the hiding flag
constexpr float k_HidingPerWorshipper = 0.01f;
/// The ring's points are this far round from each other, as a share of a turn
constexpr float k_TurnShare = 1.0f / 9.0f;
constexpr float k_Turn = 6.2831855f;
/// The game keeps the turn that wraps the throb's phase as a double with the bits of the float turn
constexpr double k_WideTurn = static_cast<double>(k_Turn);
/// The throb runs from this many radians a second to the most
constexpr float k_SlowestThrob = 5.0f;
constexpr float k_FastestThrob = 10.0f;
constexpr float k_SecondsPerMillisecond = 0.001f;
constexpr float k_Opaque = 255.0f;
constexpr uint32_t k_ByteMask = 0xFFu;
} // namespace

float HidingIndicator(bool hasDance, uint32_t worshippers, uint32_t dancers)
{
	if (!hasDance)
	{
		return 0.0f;
	}
	// The count can run below none: it wraps round to a large number, and the flag is full
	const float indicator = static_cast<float>(worshippers - dancers) * k_HidingPerWorshipper;
	return indicator > 1.0f ? 1.0f : indicator;
}

std::vector<glm::vec3> RingPoints(glm::vec3 centre)
{
	std::vector<glm::vec3> points;
	points.reserve(k_RingPoints);
	for (size_t i = 0; i < k_RingPoints; ++i)
	{
		// The angle stays wide
		const double angle = static_cast<double>(i) * static_cast<double>(k_Turn) * static_cast<double>(k_TurnShare);
		// The game keeps the cosine and its product with the radius as floats, and the sine wide
		const auto cosine = x87::RoundToFloat(x87::Cos(angle));
		const float across = k_RingRadius * cosine;
		const double along = x87::Sin(angle) * k_RingRadius;
		points.emplace_back(x87::Add(across, centre.x), centre.y, x87::Add(along, centre.z));
	}
	return points;
}

float RingStrength(float battery, float maxBattery)
{
	const float strength = x87::Div(battery, maxBattery);
	if (strength <= 0.0f)
	{
		return 0.0f;
	}
	return strength >= 1.0f ? 1.0f : strength;
}

uint32_t RingRgb(uint32_t playerRgb, float strain, float pulse)
{
	if (!(strain > 0.0f))
	{
		return playerRgb & 0xFFFFFFu;
	}
	// Each channel times the pulse in 256ths
	const uint32_t light = static_cast<uint32_t>(static_cast<int64_t>(static_cast<double>(pulse) * k_Opaque)) & k_ByteMask;
	const uint32_t red = ((playerRgb & 0xFF0000u) * light) & 0xFF0000FFu;
	const uint32_t green = ((playerRgb & 0xFF00u) * light) & 0xFF0000u;
	const uint32_t blue = ((playerRgb & 0xFFu) * light) & 0xFF00u;
	return ((red | green | blue) >> 8u) & 0xFFFFFFu;
}

float FrameSeconds(int32_t frameMilliseconds)
{
	return static_cast<float>(frameMilliseconds) * k_SecondsPerMillisecond;
}

Throb NextThrob(Throb throb, float strain, int32_t frameMilliseconds)
{
	if (!(strain > 0.0f))
	{
		return throb;
	}
	const float share = strain >= 1.0f ? 1.0f : strain;
	// The sum stays wide until the phase is wrapped. The cosine is of the wide phase.
	const double speed = (static_cast<double>(k_FastestThrob - k_SlowestThrob) * share) + k_SlowestThrob;
	const double moved =
	    (speed * static_cast<double>(frameMilliseconds) * static_cast<double>(k_SecondsPerMillisecond)) + throb.phase;
	const double phase = std::fmod(moved, k_WideTurn);
	return {
	    .phase = x87::RoundToFloat(phase),
	    .pulse = x87::RoundToFloat((x87::Cos(phase) + 1.0) * 0.5),
	};
}

uint32_t MoteAlpha(float strain, float pulse)
{
	if (!(strain > 0.0f))
	{
		return static_cast<uint32_t>(k_Opaque);
	}
	return static_cast<uint32_t>(static_cast<int64_t>(static_cast<double>(pulse) * k_Opaque));
}

} // namespace openblack::ecs::worship_site_looks
