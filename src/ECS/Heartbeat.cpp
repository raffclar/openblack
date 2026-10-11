/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "Heartbeat.h"

#include <cmath>
#include <cstdlib>

#include <algorithm>
#include <array>
#include <numbers>

namespace openblack::ecs::heartbeat
{

namespace
{
/// The destruction's clock and the beats a second it holds the heart at there, straight between
constexpr std::array<float, 3> k_ClockPoints {0.0f, 19.0f, 25.0f};
constexpr std::array<float, 3> k_BeatPoints {2.0f / std::numbers::pi_v<float>, 4.0f / std::numbers::pi_v<float>,
                                             12.5f / std::numbers::pi_v<float>};

/// How far a share has fallen from its smoothed value, as a fraction of the new share
float Fall(float smoothed, float now)
{
	return (smoothed + k_ShareFloor) / (now + k_ShareFloor) - 1.0f;
}
} // namespace

bool IsMeasureTurn(uint32_t turn)
{
	return turn % k_MeasureEveryTurns == 0;
}

uint32_t CellDistance(glm::ivec2 a, glm::ivec2 b)
{
	const auto across = static_cast<uint32_t>(std::abs(a.x - b.x));
	const auto along = static_cast<uint32_t>(std::abs(a.y - b.y));
	return across < along ? (across >> 1u) + along : (along >> 1u) + across;
}

float CreatureDanger(uint32_t cells)
{
	const auto distance = static_cast<float>(cells);
	if (distance >= static_cast<float>(k_CreatureFarCells))
	{
		return 0.0f;
	}
	const float beyondNear = std::max(distance - static_cast<float>(k_CreatureNearCells), 0.0f);
	return 1.0f - beyondNear / static_cast<float>(k_CreatureFarCells);
}

void Measure(State& state, const Danger& danger)
{
	float stress = danger.protectionDesire;
	stress += Fall(state.believerShare, danger.believerShare) + Fall(state.influenceShare, danger.influenceShare);
	state.believerShare += (danger.believerShare - state.believerShare) * k_ShareEase;
	state.influenceShare += (danger.influenceShare - state.influenceShare) * k_ShareEase;
	for (const auto cells : danger.creatureDistances)
	{
		stress += CreatureDanger(cells);
	}
	state.stress = std::clamp(stress, 0.0f, 1.0f);
}

void Step(State& state, float secondsPerTurn)
{
	if (state.destructionBeats == 0.0f)
	{
		const float wanted = state.stress * k_DangerRate + k_CalmRate;
		state.rate += (wanted - state.rate) * k_RateEase;
	}
	else
	{
		state.rate = state.destructionBeats * k_OverrideRate;
	}
	state.phase += state.rate * k_BeatsPerRate * secondsPerTurn;
	while (state.phase > 1.0f)
	{
		state.phase -= 1.0f;
	}
	state.previousPulse = state.pulse;
	state.pulse = (1.0f - std::cos(state.phase * 2.0f * std::numbers::pi_v<float>)) * 0.5f;
}

float Pulse(const State& state, float turnFraction)
{
	return (state.pulse - state.previousPulse) * turnFraction + state.previousPulse;
}

float DestructionBeats(float clock)
{
	if (clock < k_ClockPoints.front())
	{
		return k_BeatPoints.front();
	}
	if (clock >= k_ClockPoints.back())
	{
		return k_BeatPoints.back();
	}
	const auto upper = static_cast<size_t>(clock < k_ClockPoints[1] ? 1 : 2);
	const float t = (clock - k_ClockPoints.at(upper - 1)) / (k_ClockPoints.at(upper) - k_ClockPoints.at(upper - 1));
	return k_BeatPoints.at(upper - 1) + (k_BeatPoints.at(upper) - k_BeatPoints.at(upper - 1)) * t;
}

} // namespace openblack::ecs::heartbeat
