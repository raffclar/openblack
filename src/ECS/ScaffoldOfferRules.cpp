/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "ScaffoldOfferRules.h"

#include <algorithm>
#include <ranges>

namespace openblack::ecs
{

using components::ScaffoldOffer;
using components::ScaffoldPhantom;

namespace
{

/// Whether a building can come from a scaffold of a value
[[nodiscard]] bool Affordable(uint32_t scaffoldsRequired, uint32_t value)
{
	return scaffoldsRequired != 0 && scaffoldsRequired <= value;
}

/// The phantom changes its size: it becomes small when it grows, or grows when it is small and its change is complete
void ChangeSize(ScaffoldPhantom& phantom)
{
	if (phantom.grows)
	{
		phantom.grows = false;
		phantom.changeMs = scaffold_offer::k_ChangeMs - phantom.changeMs;
		return;
	}
	if (phantom.changeMs < 1)
	{
		phantom.grows = true;
		phantom.changeMs = scaffold_offer::k_ChangeMs;
	}
}

} // namespace

bool scaffold_offer::NearWorkshop(const WorkshopDistances& distances)
{
	if (distances.workshop > k_WorkshopReach)
	{
		return false;
	}
	return distances.flag < k_FlagReach ||
	       std::ranges::any_of(distances.places, [](float distance) { return distance < k_PlaceReach; });
}

std::optional<ScaffoldOffer> scaffold_offer::BestAtTurn(std::span<const Candidate> candidates, uint32_t value, float yAngle,
                                                        const Fits& fits)
{
	std::optional<ScaffoldOffer> best;
	float bestDesire = 0.0f;
	for (const auto& candidate : candidates)
	{
		if (!Affordable(candidate.scaffoldsRequired, value))
		{
			continue;
		}
		if (fits && !fits(candidate.offer, yAngle))
		{
			continue;
		}
		if (candidate.desire > bestDesire)
		{
			bestDesire = candidate.desire;
			best = candidate.offer;
			best->yAngle = yAngle;
		}
	}
	return best;
}

std::optional<ScaffoldOffer> scaffold_offer::SearchTown(std::span<const Candidate> candidates, uint32_t value,
                                                        float startYAngle, const Fits& fits)
{
	float yAngle = startYAngle;
	for (size_t i = 0; i < k_Tries; ++i)
	{
		if (auto best = BestAtTurn(candidates, value, yAngle, fits); best.has_value())
		{
			return best;
		}
		yAngle += k_TryStep;
	}
	return std::nullopt;
}

std::optional<ScaffoldOffer> scaffold_offer::SearchNewTown(const Candidate& centre, uint32_t value, const Fits& fits)
{
	if (!Affordable(centre.scaffoldsRequired, value))
	{
		return std::nullopt;
	}
	float yAngle = 0.0f;
	for (size_t i = 0; i < k_Tries; ++i)
	{
		if (!fits || fits(centre.offer, yAngle))
		{
			auto offer = centre.offer;
			offer.yAngle = yAngle;
			offer.scale = 1.0f;
			return offer;
		}
		yAngle += k_TryStep;
	}
	return std::nullopt;
}

void scaffold_offer::StepTurn(ScaffoldPhantom& phantom, float turnPerGameTurn)
{
	if (phantom.noOfferTurns != 0)
	{
		phantom.noOfferTurns = static_cast<uint8_t>((phantom.noOfferTurns + 1) % k_NoOfferTurnsWrap);
	}
	// A fixed phantom adds no turn, but the check of the full turn still applies
	phantom.yAngle += phantom.fixedAngle ? 0.0f : turnPerGameTurn;
	if (phantom.yAngle > k_FullTurn)
	{
		phantom.yAngle -= k_FullTurn;
	}
	phantom.changeMs -= std::min(phantom.changeMs, k_TurnMs);
	if (phantom.changeMs < 1 && !phantom.grows)
	{
		phantom.shown = phantom.next;
		phantom.next.reset();
		if (phantom.shown.has_value())
		{
			phantom.scale = phantom.shown->scale;
			if (!phantom.fixedAngle)
			{
				phantom.yAngle = phantom.shown->yAngle;
			}
			phantom.grows = true;
			phantom.changeMs = k_ChangeMs;
		}
	}
}

void scaffold_offer::TakeNextOffer(ScaffoldPhantom& phantom)
{
	if (!phantom.shown.has_value())
	{
		if (phantom.next.has_value())
		{
			phantom.changeMs = k_ChangeMs;
			phantom.shown = phantom.next;
			phantom.scale = phantom.shown->scale;
			phantom.grows = true;
		}
		phantom.next.reset();
		return;
	}
	if (phantom.next.has_value())
	{
		if (phantom.next->mesh == phantom.shown->mesh)
		{
			// The same phantom shows: only a wonder takes the size of the new offer
			phantom.shown = phantom.next;
			phantom.next.reset();
			if (phantom.shown->number == AbodeNumber::Wonder)
			{
				phantom.scale = phantom.shown->scale;
			}
			return;
		}
		if (phantom.next->number != AbodeNumber::TownCentre)
		{
			phantom.foundsTown = false;
		}
	}
	else
	{
		phantom.foundsTown = false;
	}
	ChangeSize(phantom);
}

scaffold_offer::HeldEvent scaffold_offer::HeldTurn(ScaffoldPhantom& phantom, const HeldFacts& facts, const Search& search,
                                                   const KeepsOffer& keeps)
{
	StepTurn(phantom, facts.turnPerGameTurn);
	if (!facts.handStill)
	{
		phantom.looked = false;
		if ((!keeps || !keeps(phantom)) && phantom.changeMs < 1)
		{
			TakeNextOffer(phantom);
		}
		return HeldEvent::None;
	}
	if (phantom.shown.has_value() && phantom.looked)
	{
		return HeldEvent::None;
	}
	// A search that finds nothing keeps the next offer as it was
	const auto found = search ? search() : std::nullopt;
	if (found.has_value())
	{
		phantom.next = found->offer;
		if (found->foundsTown)
		{
			phantom.foundsTown = true;
		}
	}
	TakeNextOffer(phantom);
	auto event = HeldEvent::None;
	if (!found.has_value() && phantom.noOfferTurns == 0 && !phantom.looked)
	{
		event = HeldEvent::ShowsNoOffer;
		phantom.noOfferTurns = 1;
	}
	phantom.looked = true;
	return event;
}

} // namespace openblack::ecs
