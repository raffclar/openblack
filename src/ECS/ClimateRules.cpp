/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "ClimateRules.h"

#include "Common/X87.h"

namespace openblack::ecs::climate_rules
{

namespace
{
// The game keeps these constants as doubles
constexpr double k_RainPerDay = 0.01;
constexpr double k_HalfWayToRain = 0.5;
constexpr double k_WindSpread = 5.0;
constexpr double k_CloudiestTemperature = 30.0;
constexpr double k_CloudPerDegree = 1.0 / 15.0;
constexpr double k_SnowPerDegree = 0.2;
constexpr double k_Hundred = 100.0;
} // namespace

float RainedFor(int32_t rainingDays)
{
	return x87::Mul(static_cast<double>(rainingDays), k_RainPerDay);
}

glm::vec2 Wind(float minimum, float maximum, float angle, float rainDesire)
{
	// The weight is 1 only when e to the power of minus the square of the spread rounds to 1, else 0
	const float spread = x87::Mul(x87::Sub(rainDesire, k_HalfWayToRain), k_WindSpread);
	const auto weight = static_cast<float>(x87::ToInteger(x87::Exp(-(spread * spread))));

	const auto blend = [weight](float low, float high) { return high - ((high - low) * weight) + low; };
	const double cos = x87::Cos(angle);
	const double sin = x87::Sin(angle);
	return {
	    blend(x87::Mul(cos, minimum), x87::Mul(cos, maximum)),
	    blend(x87::Mul(sin, minimum), x87::Mul(sin, maximum)),
	};
}

float StormCloud(int8_t temperature)
{
	const float heat = x87::Mul(static_cast<double>(temperature) - k_CloudiestTemperature, k_CloudPerDegree);
	return x87::Exp(-(heat * heat));
}

int8_t Percent(float share)
{
	return static_cast<int8_t>(x87::ToInteger(x87::Mul(share, k_Hundred)));
}

int8_t StormSnow(int8_t temperature)
{
	const float cold = x87::Mul(static_cast<double>(temperature), k_SnowPerDegree);
	return Percent(x87::Exp(-(cold * cold)));
}

glm::ivec2 CellNearMiddle(float angle, float distance, uint16_t middleX, uint16_t middleZ)
{
	// The product with the wide sine or cosine rounds one time, then the sum with the middle cell rounds
	const float x = x87::Mul(x87::Cos(angle), distance) + static_cast<float>(middleX);
	const float z = x87::Mul(x87::Sin(angle), distance) + static_cast<float>(middleZ);
	return {x87::ToInteger(x), x87::ToInteger(z)};
}

} // namespace openblack::ecs::climate_rules
