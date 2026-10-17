/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "RisingSparks.h"

#include <cmath>

#include <algorithm>

#include <glm/gtc/constants.hpp>

#include "Common/X87.h"

namespace openblack::particles::rising_sparks
{

Sparks MakeSparks(const std::function<float(float)>& randomFloat, const std::function<uint32_t(uint32_t)>& random)
{
	constexpr int32_t k_StartApart = k_SparkLifeMilliseconds / static_cast<int32_t>(k_Sparks);
	Sparks made;
	for (size_t i = 0; i < k_Sparks; ++i)
	{
		made.sparks.at(i) = {.ageMilliseconds = -static_cast<int32_t>(i) * k_StartApart,
		                     .angle = randomFloat(glm::two_pi<float>())};
	}
	made.firstPicture = random(32);
	return made;
}

void StepSparks(Sparks& sparks, uint32_t gameMilliseconds, const std::function<float(float)>& randomFloat)
{
	for (auto& spark : sparks.sparks)
	{
		spark.ageMilliseconds += static_cast<int32_t>(gameMilliseconds);
		if (spark.ageMilliseconds > k_SparkLifeMilliseconds)
		{
			spark.ageMilliseconds -= k_SparkLifeMilliseconds;
			spark.angle = randomFloat(glm::two_pi<float>());
		}
	}
}

std::optional<SparkLook> LookOf(const Sparks& sparks, size_t index, glm::vec3 from, uint8_t strength)
{
	if (index >= k_Sparks || sparks.sparks.at(index).ageMilliseconds < 0)
	{
		return std::nullopt;
	}
	/// The game subtracts this times the share from the height, so that the spark goes up
	constexpr float k_Fall = -20.0f;
	constexpr float k_StartHalfSize = 2.0f;
	constexpr float k_EndHalfSize = 1.2f;
	constexpr float k_FadeInPerMillisecond = 0.0002f;
	constexpr float k_FadeOutPerMillisecond = 1.0f / 3000.0f;
	constexpr int32_t k_FadeOutFrom = 5000;
	constexpr float k_LifeShareSquared = 1.5625e-08f;
	constexpr int32_t k_PictureMilliseconds = 50;
	const auto& spark = sparks.sparks.at(index);
	const int32_t age = spark.ageMilliseconds;
	// Its share of its life, squared. The square of the age is an exact integer, and the multiply rounds it
	const float share = x87::Mul(static_cast<double>(age * age), k_LifeShareSquared);
	const float fade = age < k_FadeOutFrom ? static_cast<float>(age) * k_FadeInPerMillisecond
	                                       : 1.0f - static_cast<float>(age - k_FadeOutFrom) * k_FadeOutPerMillisecond;
	// Cut down to a whole number, of which only the low byte is kept
	const auto alpha = static_cast<uint8_t>(static_cast<int32_t>(static_cast<float>(strength) * fade) & 0xFF);
	return SparkLook {
	    .position = {from.x, from.y - k_Fall * share, from.z},
	    .halfSize = (k_EndHalfSize - k_StartHalfSize) * share + k_StartHalfSize,
	    .alpha = alpha,
	    .picture =
	        (static_cast<uint32_t>(age / k_PictureMilliseconds) + static_cast<uint32_t>(index) + sparks.firstPicture) & 31u,
	    .angle = spark.angle,
	};
}

uint32_t FrameMilliseconds::Take(float frameMilliseconds)
{
	carried += std::max(frameMilliseconds, 0.0f);
	const float whole = std::floor(carried);
	carried -= whole;
	return static_cast<uint32_t>(whole);
}

} // namespace openblack::particles::rising_sparks
