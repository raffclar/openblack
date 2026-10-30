/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "ComputerMindConquestRules.h"

#include "3D/MapCoords.h"
#include "Common/GUtilsDistance.h"
#include "Common/X87.h"

using namespace openblack;

namespace
{

/// The search turns a map position into metres and back with these
constexpr float k_SearchMetresPerUnit = 10.0f;
constexpr float k_SearchFixedToCells = 1.52587890625e-05f;
constexpr float k_SearchCellsToFixed = 65536.0f;

int16_t CellOfSearch(int32_t fixed, float radius, bool high)
{
	const float metres =
	    x87::Mul(static_cast<double>(fixed), static_cast<double>(k_SearchMetresPerUnit)) * k_SearchFixedToCells;
	const float edge = high ? metres + radius : metres - radius;
	const int32_t value = map_coords::FtoL((edge * k_SearchCellsToFixed) / k_SearchMetresPerUnit);
	return static_cast<int16_t>(static_cast<uint16_t>(static_cast<uint32_t>(value) >> 16u));
}

} // namespace

float computer_mind_rules::TownHold(float rivalBelief, bool ownTown)
{
	const float share = !(rivalBelief * k_HeldBeliefShare >= 1.0f) ? rivalBelief * k_HeldBeliefShare : 1.0f;
	float hold = 1.0f - share;
	if (ownTown)
	{
		hold += k_OwnTownHold;
	}
	if (!(hold > k_LeastHold))
	{
		return k_LeastHold;
	}
	return !(hold >= 1.0f) ? hold : 1.0f;
}

float computer_mind_rules::RivalBelief(std::span<const float> belief, size_t god, size_t owner)
{
	if (owner != god)
	{
		return owner < belief.size() ? belief[owner] : 0.0f;
	}
	const float own = god < belief.size() ? belief[god] : 0.0f;
	float most = k_NoRivalBelief;
	float found = 0.0f;
	for (size_t player = 0; player < belief.size(); ++player)
	{
		if (player == god)
		{
			continue;
		}
		const float difference = belief[player] - own;
		if (difference > most)
		{
			most = difference;
			found = belief[player];
		}
	}
	return found;
}

float computer_mind_rules::TownMiracleWish(std::span<const TownMiracleFacts> miracles)
{
	float wish = 0.0f;
	for (const auto& miracle : miracles)
	{
		if (miracle.enabled)
		{
			continue;
		}
		wish += k_MissingMiracleWish;
		if (miracle.requested)
		{
			wish += k_MissingMiracleWish;
		}
	}
	return wish;
}

float computer_mind_rules::TownPeopleShare(uint32_t townPeople, float godPeople, float miracleWish)
{
	if (godPeople == 0.0f)
	{
		return miracleWish + 1.0f;
	}
	return x87::Div(static_cast<double>(townPeople), static_cast<double>(godPeople));
}

float computer_mind_rules::PeopleCount(uint32_t people)
{
	return x87::RoundToFloat(static_cast<double>(people));
}

float computer_mind_rules::RivalPowerCut(bool neutralOwner, float ownerPower, float godPower)
{
	if (neutralOwner || !(godPower < 0.0f || godPower > 0.0f))
	{
		return 0.0f;
	}
	const float ratio = ownerPower / godPower;
	if (ratio > 1.0f)
	{
		return x87::Mul(1.0, k_RivalPowerShare);
	}
	return x87::Mul(static_cast<double>(ratio), k_RivalPowerShare);
}

float computer_mind_rules::TownNearness(float distanceFromHand)
{
	const float near = gutils::DistanceChangeToBelief(distanceFromHand, k_TownNearDistance);
	return x87::Add(static_cast<double>(near * k_NearHalf), k_NearBase);
}

float computer_mind_rules::TownWorth(float peopleShare, float miracleWish, float hold, float powerCut, float nearness)
{
	return ((hold * (peopleShare + miracleWish)) * (1.0f - powerCut)) * nearness;
}

std::optional<size_t> computer_mind_rules::MostWantedTown(std::span<const float> worths)
{
	std::optional<size_t> best;
	float most = 0.0f;
	for (size_t i = 0; i < worths.size(); ++i)
	{
		if (worths[i] > most)
		{
			most = worths[i];
			best = i;
		}
	}
	return best;
}

float computer_mind_rules::CappedWorth(float worth)
{
	return !(worth >= 1.0f) ? worth : 1.0f;
}

float computer_mind_rules::RivalBeliefRatio(float strongestRival, float godBelief)
{
	// A belief that is 0, or that is not a number, gives 0
	if (!(godBelief < 0.0f || godBelief > 0.0f))
	{
		return 0.0f;
	}
	return strongestRival / godBelief;
}

float computer_mind_rules::FeedFactor(bool ownTown, float rivalRatio)
{
	if (!ownTown)
	{
		return 1.0f;
	}
	const float factor = rivalRatio * k_OwnTownFeedScale;
	return !(factor >= 1.0f) ? factor : 1.0f;
}

float computer_mind_rules::NeedWish(float rawDesire, float feedFactor)
{
	const float wish = feedFactor * rawDesire;
	return !(wish >= 1.0f) ? wish : 1.0f;
}

float computer_mind_rules::CreatureReach(float size)
{
	return ((size * k_CreatureHeightPerSize) * k_CreatureReachPerHeight) + k_CreatureLeastReach;
}

float computer_mind_rules::CreatureHelpWish(float freedom, float exhaustion, float distance, float size)
{
	const float near = gutils::GetDistanceModifier(distance, CreatureReach(size));
	return (freedom * (1.0f - exhaustion)) * near;
}

float computer_mind_rules::KnownMiracleShare(uint32_t miracles, uint32_t unknown)
{
	if (miracles == 0)
	{
		return 1.0f;
	}
	const float share = 1.0f - static_cast<float>(unknown / miracles);
	if (share < 0.0f)
	{
		return 0.0f;
	}
	return share > 1.0f ? 1.0f : share;
}

computer_mind_rules::NeedsMiracle computer_mind_rules::ChooseNeedsMiracle(float foodWish, float woodWish)
{
	if (foodWish > woodWish)
	{
		return NeedsMiracle::Food;
	}
	return woodWish == 0.0f ? NeedsMiracle::None : NeedsMiracle::Wood;
}

float computer_mind_rules::ThrowWish(float pull, float placeValue, float boredom)
{
	return pull * (placeValue * boredom);
}

float computer_mind_rules::ThrowScore(float impressiveness, float artefactModifier)
{
	return artefactModifier + impressiveness;
}

bool computer_mind_rules::CanBeThrownByPlayer(ThrowKind kind, float radius, uint32_t amount)
{
	switch (kind)
	{
	case ThrowKind::Always:
		return true;
	case ThrowKind::Pile:
		return amount != 0;
	case ThrowKind::RockLike:
		return !(radius > k_ThrowableRockRadius);
	case ThrowKind::None:
		break;
	}
	return false;
}

computer_mind_rules::CellSpan computer_mind_rules::SearchCells(int32_t fixed, float radius)
{
	return {.first = CellOfSearch(fixed, radius, false), .last = CellOfSearch(fixed, radius, true)};
}

std::optional<float> computer_mind_rules::SearchKeeps(float score, float distance, float radius, float best)
{
	if (!(score >= best))
	{
		return std::nullopt;
	}
	const float value = gutils::GetDistanceModifier(distance, radius) * score;
	return value > best ? std::optional(value) : std::nullopt;
}
