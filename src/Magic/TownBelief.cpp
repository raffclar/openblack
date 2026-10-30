/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "TownBelief.h"

#include <algorithm>

#include "Common/X87.h"

using namespace openblack;
using namespace openblack::magic::town_belief;

void magic::town_belief::Add(Belief& town, PlayerNames player, float amount)
{
	const auto p = static_cast<size_t>(player);
	if (p >= k_PlayerCount)
	{
		return;
	}
	town.pending.at(p) += amount;
	town.recent.at(p) += amount;
}

std::vector<Gained> magic::town_belief::Turn(Belief& town, float recentDecay)
{
	std::vector<Gained> gained;
	for (size_t p = 0; p < k_PlayerCount; ++p)
	{
		town.recent.at(p) *= recentDecay;
		const float amount = town.scale * town.pending.at(p);
		if (amount == 0.0f)
		{
			continue;
		}
		gained.push_back({.player = static_cast<PlayerNames>(p), .amount = amount});
		town.gainedSinceShown.at(p) += amount;
		// Capped from above only: belief can fall below nothing
		Set(town, static_cast<PlayerNames>(p), town.belief.at(p) + amount);
		town.pending.at(p) = 0.0f;
	}
	return gained;
}

std::optional<float> magic::town_belief::GainShown(float gained)
{
	if (!(gained > k_LeastGainShown))
	{
		return std::nullopt;
	}
	return gained * k_GainShownScale;
}

std::array<float, k_PlayerCount> magic::town_belief::TakeGainedSinceShown(Belief& town)
{
	std::array<float, k_PlayerCount> gained {};
	std::ranges::transform(town.gainedSinceShown, gained.begin(), [](float amount) { return amount > 0.0f ? amount : 0.0f; });
	town.gainedSinceShown.fill(0.0f);
	return gained;
}

void magic::town_belief::ResetDesireLevels(Belief& town, std::span<const DesireDecay, k_DesireCount> table)
{
	auto& levels = town.desireLevel.emplace();
	std::ranges::transform(table, levels.begin(), &DesireDecay::levelAfterGain);
}

void magic::town_belief::LoseBeliefToDesires(Belief& town, PlayerNames owner, std::span<const float, k_DesireCount> desire,
                                             std::span<const DesireDecay, k_DesireCount> table, float leastLevel)
{
	const auto o = static_cast<size_t>(owner);
	if (owner == PlayerNames::NEUTRAL || o >= k_PlayerCount)
	{
		return;
	}
	if (!town.desireLevel.has_value())
	{
		ResetDesireLevels(town, table);
	}
	auto& levels = *town.desireLevel;
	for (size_t d = 0; d < k_DesireCount; ++d)
	{
		const auto& row = table[d];
		if (desire[d] > levels.at(d))
		{
			// The difference and then the product are each rounded to a float
			const float loss = x87::Mul(x87::Sub(desire[d], levels.at(d)), row.toBelief);
			town.belief.at(o) = x87::Sub(town.belief.at(o), loss);
		}
		if (levels.at(d) > leastLevel)
		{
			levels.at(d) -= row.levelFall;
		}
	}
}

void magic::town_belief::Set(Belief& town, PlayerNames player, float value)
{
	const auto p = static_cast<size_t>(player);
	if (p >= k_PlayerCount)
	{
		return;
	}
	town.belief.at(p) = std::min(value, town.cap.at(p));
}

void magic::town_belief::SetInPlayer(Belief& town, PlayerNames player, float value)
{
	if (player == PlayerNames::NEUTRAL)
	{
		town.neutral = value;
	}
	Set(town, player, value);
}

std::optional<PlayerNames> magic::town_belief::Ownership(Belief& town, PlayerNames owner, float claimedMultiplier,
                                                         size_t players)
{
	Set(town, PlayerNames::NEUTRAL, town.neutral);
	const auto count = std::min(player_slots::PlayerNumbersOf(players), k_PlayerCount);
	if (static_cast<size_t>(owner) >= count)
	{
		return std::nullopt;
	}
	auto winner = static_cast<size_t>(owner);
	float most = town.belief.at(winner);
	for (size_t p = 0; p < count; ++p)
	{
		if (town.belief.at(p) >= most)
		{
			most = town.belief.at(p);
			winner = p;
		}
	}
	if (winner == static_cast<size_t>(owner))
	{
		return std::nullopt;
	}
	const auto taker = static_cast<PlayerNames>(winner);
	Set(town, taker, claimedMultiplier * town.belief.at(winner));
	return taker;
}

void magic::town_belief::LostTown(Belief& town, PlayerNames loser, float lostMultiplier, float lostTownScale)
{
	const auto p = static_cast<size_t>(loser);
	if (p >= k_PlayerCount)
	{
		return;
	}
	Set(town, loser, lostMultiplier * town.belief.at(p) * lostTownScale);
}

void magic::town_belief::SetRelativeToOwner(Belief& town, PlayerNames owner, PlayerNames player, float share)
{
	const auto o = static_cast<size_t>(owner);
	if (o >= k_PlayerCount)
	{
		return;
	}
	Set(town, player, town.belief.at(o) * share);
}

void magic::town_belief::TakeTown(Belief& town, PlayerNames player, float amount, std::span<const PlayerNames> playersInGame)
{
	if (player == PlayerNames::NEUTRAL)
	{
		for (const auto other : playersInGame)
		{
			Set(town, other, 0.0f);
		}
		return;
	}
	float most = 0.0f;
	std::optional<PlayerNames> believedMost;
	for (const auto other : playersInGame)
	{
		const auto p = static_cast<size_t>(other);
		if (p < k_PlayerCount && town.belief.at(p) > most)
		{
			most = town.belief.at(p);
			believedMost = other;
		}
	}
	if (believedMost.has_value())
	{
		Set(town, *believedMost, most * 0.5f);
	}
	Set(town, player, most + amount);
}

void magic::town_belief::ResetForEmptyTown(Belief& town, std::span<const DesireDecay, k_DesireCount> table)
{
	town.belief.fill(0.0f);
	town.pending.fill(0.0f);
	town.cap.fill(k_DefaultCap);
	town.belief.at(static_cast<size_t>(PlayerNames::NEUTRAL)) = town.neutral;
	ResetDesireLevels(town, table);
	for (size_t p = 0; p < k_PlayerCount; ++p)
	{
		Set(town, static_cast<PlayerNames>(p), town.neutral);
	}
}
