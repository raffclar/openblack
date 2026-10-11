/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "TownBeliefMaths.h"

#include <cmath>

#include <algorithm>
#include <numbers>

namespace openblack::particles::town_belief
{

namespace
{
constexpr float k_TwoPi = 2.0f * std::numbers::pi_v<float>;
/// A fight's next wait and length are its usual ones times a random share between these
constexpr float k_RandomShareMin = 0.5f;
constexpr float k_RandomShareMax = 1.5f;

float Lerp(float at0, float at1, float belief)
{
	return (at1 - at0) * belief + at0;
}

/// Rises from 0 to 1 halfway through and falls back to 0 at the end
float Swell(float t)
{
	const float centred = std::clamp(t, 0.0f, 1.0f) * 2.0f - 1.0f;
	return 1.0f - centred * centred;
}

float Wrapped(float angle)
{
	return std::fmod(angle, k_TwoPi);
}
} // namespace

float Shown(float belief)
{
	return std::clamp(belief, 0.0f, 1.0f);
}

int Rank(const std::array<float, k_PlayerCount>& beliefs, size_t player)
{
	const float mine = Shown(beliefs.at(player));
	int rank = 0;
	for (size_t other = 0; other < beliefs.size(); ++other)
	{
		if (other == player)
		{
			continue;
		}
		const float theirs = Shown(beliefs.at(other));
		if (mine < theirs || (mine == theirs && player < other))
		{
			++rank;
		}
	}
	return rank;
}

float DistanceScale(const Params& params, float distance)
{
	if (!params.useFixedScale)
	{
		return 1.0f;
	}
	const float scale = distance * params.fixedScale;
	if (scale <= 1.0f)
	{
		return 1.0f;
	}
	return std::min(scale, k_MaximumDistanceScale);
}

Look Step(Symbol& symbol, const Params& params, float belief, int rank, float dt, float distanceScale,
          const std::function<float(float, float)>& random)
{
	const float shown = Shown(belief);
	Look look;
	look.scale = shown == 0.0f ? 0.0f : Lerp(params.scaleAt0, params.scaleAt1, shown);
	// The most believed sits still over the totem; the others circle at a speed across the ground
	float radius = 0.0f;
	float speed = 0.0f;
	if (rank != 0)
	{
		radius = Lerp(params.radiusAt0, params.radiusAt1, shown);
		speed = Lerp(params.speedAt0, params.speedAt1, shown) / radius;
		// The second most believed fights the first now and then
		if (rank == 1)
		{
			if (symbol.untilFight > 0.0f)
			{
				symbol.untilFight -= dt;
			}
			else
			{
				symbol.intoFight += dt;
				if (symbol.intoFight > symbol.fightLength)
				{
					symbol.untilFight = random(k_RandomShareMin, k_RandomShareMax) *
					                    Lerp(params.timeBetweenFightsAt0, params.timeBetweenFightsAt1, shown);
					symbol.intoFight = 0.0f;
					symbol.fightLength = random(k_RandomShareMin, k_RandomShareMax) *
					                     Lerp(params.fightDurationAt0, params.fightDurationAt1, shown);
				}
				look.fight = Swell(symbol.intoFight / symbol.fightLength);
			}
			speed *= (params.speedUpDuringFight - 1.0f) * look.fight + 1.0f;
		}
	}
	const float level = static_cast<float>(rank) * params.heightPerLevel;
	symbol.sway = Wrapped(symbol.sway + dt * params.angleSpeedPhaseSpeed);
	const float swayed = (std::cos(symbol.sway) + 1.0f) * 0.25f + 0.5f;
	symbol.tilt = Wrapped(symbol.tilt + swayed * speed * dt * k_TiltShare);
	symbol.around = Wrapped(symbol.around + speed * dt);
	const float flat = std::cos(symbol.tilt) * radius;
	// Fighting, it comes down to the first's level
	look.offset = glm::vec3(std::cos(symbol.around) * flat, (1.0f - look.fight) * level + std::sin(symbol.tilt) * radius,
	                        std::sin(symbol.around) * flat) *
	              distanceScale;
	return look;
}

uint8_t SparkAlpha(float fight)
{
	constexpr float k_ByteMax = 255.0f;
	return static_cast<uint8_t>(Swell(fight) * k_ByteMax);
}

bool Sparking(const Params& params, float fight)
{
	return fight > params.fightFracWhenEmitting;
}

} // namespace openblack::particles::town_belief
