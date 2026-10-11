/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "DiscipleWorkRules.h"

#include <algorithm>

#include "Common/GUtilsDistance.h"

namespace openblack::disciple_work
{

namespace
{
/// A trader weighs the two stores by their distance out to this many metres
constexpr float k_TradeReach = 5000.0f;
/// A waiting disciple looks again within this many turns of its town's wants changing
constexpr uint32_t k_SoonTurns = 10;
/// Kept off zero so that a workshop needing no wood for a scaffold isn't divided by nothing
constexpr float k_TinyWood = 0.0001f;
} // namespace

std::optional<ResourceType> TradeResource(float foodWanted, float woodWanted)
{
	if (foodWanted > woodWanted)
	{
		return ResourceType::Food;
	}
	if (woodWanted == 0.0f)
	{
		return std::nullopt;
	}
	return ResourceType::Wood;
}

TradeLeg ChooseTradeLeg(uint32_t held, uint32_t carryLimit, float metresToOwnStore, float metresToTradeStore)
{
	const float full = static_cast<float>(held) / static_cast<float>(carryLimit);
	const float fetch = gutils::GetDistanceModifier(metresToOwnStore, k_TradeReach) * (1.0f - full);
	const float deliver = gutils::GetDistanceModifier(metresToTradeStore, k_TradeReach) * full;
	return fetch <= deliver ? TradeLeg::Deliver : TradeLeg::Fetch;
}

ResourceType TradeFetchResource(ResourceType carried, ResourceType wanted)
{
	// Deviation #30: vanilla asked for the resource already carried
	return carried == ResourceType::None ? wanted : carried;
}

uint32_t TradeFetchAmount(uint32_t carryLimit, uint32_t held)
{
	return held < carryLimit ? carryLimit - held : 0;
}

void MarkDesiresChanged(bool& changed, bool& lastTurn)
{
	changed = true;
	lastTurn = false;
}

void EndTownTurn(bool& changed, bool& lastTurn)
{
	if (lastTurn)
	{
		changed = false;
	}
	lastTurn = changed;
}

int32_t IdleTurnsLeft(int32_t turnsLeft, bool townWantsChanged, const std::function<uint32_t(uint32_t)>& random)
{
	if (townWantsChanged)
	{
		turnsLeft = static_cast<int32_t>(random(k_SoonTurns));
	}
	return turnsLeft - 1;
}

uint32_t WorkshopRoom(bool makingScaffold, uint32_t scaffoldsHeld)
{
	const uint32_t taken = scaffoldsHeld + (makingScaffold ? 1 : 0);
	return taken < k_WorkshopScaffoldPlaces ? k_WorkshopScaffoldPlaces - taken : 0;
}

float WorkshopWoodShortfall(uint32_t wood, uint32_t scaffoldWood)
{
	return 1.0f - std::min(1.0f, (static_cast<float>(wood) + k_TinyWood) / (static_cast<float>(scaffoldWood) + k_TinyWood));
}

float WorkshopWantsWood(uint32_t room, uint32_t wood, uint32_t scaffoldWood)
{
	return room == 0 && WorkshopWoodShortfall(wood, scaffoldWood) <= 0.0f ? 0.0f : 1.0f;
}

std::optional<size_t> BestWorkshop(std::span<const WorkshopFacts> workshops)
{
	std::optional<size_t> best;
	float bestScore = 0.0f;
	for (size_t i = 0; i < workshops.size(); ++i)
	{
		const auto& workshop = workshops[i];
		const float score = gutils::GetDistanceModifier(workshop.metresAway, k_WorkshopReach) * workshop.wantsWood;
		if (bestScore < score && workshop.functional)
		{
			best = i;
			bestScore = score;
		}
	}
	return best;
}

int32_t CarryRoom(uint32_t maxCarried, uint32_t held)
{
	return static_cast<int16_t>(static_cast<uint16_t>(maxCarried - held));
}

} // namespace openblack::disciple_work
