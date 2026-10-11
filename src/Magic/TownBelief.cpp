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
		// Capped from above only: belief can fall below nothing
		Set(town, static_cast<PlayerNames>(p), town.belief.at(p) + amount);
		town.pending.at(p) = 0.0f;
	}
	return gained;
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

std::optional<PlayerNames> magic::town_belief::Ownership(Belief& town, PlayerNames owner, float claimedMultiplier)
{
	Set(town, PlayerNames::NEUTRAL, town.neutral);
	if (static_cast<size_t>(owner) >= k_PlayerCount)
	{
		return std::nullopt;
	}
	auto winner = static_cast<size_t>(owner);
	float most = town.belief.at(winner);
	for (size_t p = 0; p < k_PlayerCount; ++p)
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
