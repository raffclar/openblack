/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "CreatureTownCompassion.h"

#include <algorithm>

using namespace openblack;
using namespace openblack::creature_town_compassion;

namespace
{
/// The town's people counted as a small town, and as a middling one
constexpr uint32_t k_SmallTown = 20;
constexpr uint32_t k_MiddlingTown = 40;
/// How much a town believes, at the steps between its levels of belief
constexpr std::array k_BeliefSteps {0.2f, 0.4f, 0.6f};

void TakeUp(State& state, std::span<const uint32_t> desires, std::span<const float, k_TownDesireCount> felt, uint32_t index)
{
	state.index = index;
	state.desire = desires[index];
	state.remembered = felt[desires[index]] * k_RememberedShare;
}
} // namespace

std::vector<uint32_t> creature_town_compassion::DesiresToHelp(std::span<const uint32_t> order,
                                                              std::span<const float, k_TownDesireCount> felt,
                                                              const DesireActions& actions)
{
	std::vector<uint32_t> desires;
	for (const auto desire : order)
	{
		if (desire >= k_TownDesireCount || actions.at(desire).empty())
		{
			continue;
		}
		if (felt[desire] == 0.0f)
		{
			break;
		}
		desires.push_back(desire);
	}
	return desires;
}

void creature_town_compassion::Settle(State& state, std::span<const uint32_t> desires,
                                      std::span<const float, k_TownDesireCount> felt)
{
	if (desires.empty() || (state.desire.has_value() && std::ranges::find(desires, *state.desire) != desires.end()))
	{
		return;
	}
	TakeUp(state, desires, felt, 0);
}

void creature_town_compassion::MoveOn(State& state, std::span<const uint32_t> desires,
                                      std::span<const float, k_TownDesireCount> felt)
{
	const auto next = state.index + 1;
	if (next >= desires.size())
	{
		state.index = 0;
		if (desires.empty())
		{
			state.desire.reset();
			state.remembered = 0.0f;
			return;
		}
		TakeUp(state, desires, felt, 0);
		return;
	}
	TakeUp(state, desires, felt, next);
}

bool creature_town_compassion::MovesOnWhilePlanning(const State& state, bool compassionMostWanted, bool beingCompassionate,
                                                    uint32_t turn)
{
	return state.choosesFreely && compassionMostWanted && !beingCompassionate && turn - state.lastTurn > k_TurnsBeforeMovingOn;
}

void creature_town_compassion::MoveOnWhilePlanning(State& state, std::span<const uint32_t> desires,
                                                   std::span<const float, k_TownDesireCount> felt, uint32_t turn)
{
	state.lastTurn = turn;
	state.timesKept = 0;
	MoveOn(state, desires, felt);
}

void creature_town_compassion::FinishedHelping(State& state, std::span<const uint32_t> desires,
                                               std::span<const float, k_TownDesireCount> felt, uint32_t turn,
                                               const std::function<uint32_t(uint32_t)>& random)
{
	if (!state.choosesFreely || !state.desire.has_value() || *state.desire >= k_TownDesireCount)
	{
		return;
	}
	state.lastTurn = turn;
	if (state.remembered < felt[*state.desire])
	{
		const auto kept = state.timesKept;
		if (kept <= random(2) + k_MostTimesKept && *state.desire != k_Relaxation)
		{
			state.timesKept = kept + 1;
			return;
		}
	}
	state.timesKept = 0;
	MoveOn(state, desires, felt);
}

bool creature_town_compassion::HealFirst(uint32_t hurt, bool leashedToTownDesire, float sightings, float sightingsNeeded,
                                         const std::function<uint32_t(uint32_t)>& random)
{
	if (hurt == 0 || leashedToTownDesire || sightingsNeeded <= 0.0f || sightings / sightingsNeeded <= k_HealSightingsShare)
	{
		return false;
	}
	return random(2) == 0;
}

std::vector<uint32_t> creature_town_compassion::Actions(const State& state, const DesireActions& actions,
                                                        std::optional<uint32_t> healFirst)
{
	std::vector<uint32_t> chosen;
	if (healFirst.has_value())
	{
		chosen.push_back(*healFirst);
	}
	if (!state.desire.has_value() || *state.desire >= k_TownDesireCount)
	{
		return chosen;
	}
	const auto& row = actions.at(*state.desire);
	for (size_t slot = healFirst.has_value() ? 1 : 0; slot < row.size(); ++slot)
	{
		chosen.push_back(row[slot]);
	}
	return chosen;
}

uint32_t creature_town_compassion::ReligiousBelief(float belief)
{
	return static_cast<uint32_t>(std::ranges::count_if(k_BeliefSteps, [belief](float step) { return belief >= step; }));
}

uint32_t creature_town_compassion::NeedsMost(int mostDesired)
{
	return mostDesired < 0 || mostDesired >= static_cast<int>(k_TownDesireCount) ? static_cast<uint32_t>(k_TownDesireCount - 1)
	                                                                             : static_cast<uint32_t>(mostDesired);
}

uint32_t creature_town_compassion::TownSize(uint32_t people)
{
	return people < k_SmallTown ? 0 : people < k_MiddlingTown ? 1 : 2;
}
