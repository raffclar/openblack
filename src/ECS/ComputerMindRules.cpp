/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "ComputerMindRules.h"

#include <cstdint>

#include <optional>
#include <span>

#include "Common/GUtilsDistance.h"

using namespace openblack;
using namespace openblack::computer_mind_rules;

float computer_mind_rules::ChargeShare(const IconFacts& icon)
{
	float have = icon.store + k_SmallChants;
	if (icon.site)
	{
		const float available = icon.infinite ? k_InfiniteChants : icon.available - icon.used;
		have = available + icon.battery + have;
		if (have < icon.cost && icon.capacity == 0.0f)
		{
			return 0.0f;
		}
	}
	return have / (icon.cost + k_SmallChants);
}

float computer_mind_rules::TurnsToCharge(const IconFacts& icon)
{
	if (!icon.site)
	{
		return -1.0f;
	}
	const float available = icon.infinite ? k_InfiniteChants : icon.available;
	const float rest = icon.cost - icon.store - available;
	if (rest <= 0.0f)
	{
		return 0.0f;
	}
	if (icon.capacity == 0.0f)
	{
		return -1.0f;
	}
	return rest / icon.capacity;
}

float computer_mind_rules::RequestShare(uint32_t requests)
{
	const float share = static_cast<float>(static_cast<uint64_t>(requests)) * k_RequestShare;
	return share < 1.0f ? share : 1.0f;
}

float computer_mind_rules::SpellValue(float charge, float turnsToCharge, float placePull)
{
	const float soon = gutils::GetDistanceModifier(turnsToCharge, k_MostChargeTurns);
	const float value = placePull * soon * charge;
	return value < 1.0f ? value : 1.0f;
}

std::optional<size_t> computer_mind_rules::MostWanting(std::span<const float> desires)
{
	std::optional<size_t> best;
	float most = 0.0f;
	for (size_t i = 0; i < desires.size(); ++i)
	{
		if (desires[i] > most)
		{
			most = desires[i];
			best = i;
		}
	}
	return best;
}

std::optional<ChargeChoice> computer_mind_rules::ChooseCharge(std::span<const ChargeCandidate> icons)
{
	std::optional<ChargeChoice> best;
	float most = 0.0f;
	for (size_t i = 0; i < icons.size(); ++i)
	{
		const auto& icon = icons[i];
		if (icon.charging || icon.store != 0.0f || icon.requests == 0)
		{
			continue;
		}
		const float value = RequestShare(icon.requests);
		if (value > most)
		{
			most = value;
			best = ChargeChoice {.index = i, .value = value};
		}
	}
	return best;
}
