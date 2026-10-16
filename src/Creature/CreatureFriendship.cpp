/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "CreatureFriendship.h"

#include <algorithm>
#include <array>

using namespace openblack;
using namespace openblack::creature_friendship;
using creature_desires::Desire;

namespace
{
/// The desire that the asked creature wants most about the asker, of a list in order, none when none is worth more
/// than nothing
std::optional<Desire> MostWanted(std::span<const Desire> desires, const std::function<float(Desire)>& worthAbout)
{
	std::optional<Desire> best;
	float bestWorth = 0.0f;
	for (const auto desire : desires)
	{
		const float worth = worthAbout ? worthAbout(desire) : 0.0f;
		if (bestWorth < worth)
		{
			bestWorth = worth;
			best = desire;
		}
	}
	return best;
}
} // namespace

Availability creature_friendship::AvailabilityOf(const AvailabilityFacts& facts)
{
	if (facts.dead || facts.scriptControlled != facts.askerScriptControlled)
	{
		return Availability::Refuses;
	}
	if (facts.partner == facts.asker)
	{
		return Availability::AlreadyPartner;
	}
	return facts.partner.has_value() ? Availability::Refuses : Availability::Free;
}

Answer creature_friendship::AnswerOf(const AnswerFacts& facts)
{
	if (facts.kind == RequestKind::Fight)
	{
		const bool nearInHeight = facts.heightRatio >= k_LeastFightHeightRatio && facts.heightRatio <= k_MostFightHeightRatio;
		return {.agrees = nearInHeight && facts.availability != Availability::Refuses};
	}
	if (facts.availability != Availability::Free)
	{
		return {.agrees = facts.availability == Availability::AlreadyPartner};
	}
	if (facts.alwaysAgree)
	{
		return {.agrees = true};
	}
	switch (facts.kind)
	{
	case RequestKind::Play:
	{
		constexpr std::array k_Order {Desire::Play, Desire::BeFriends, Desire::Fear, Desire::Anger};
		const auto best = MostWanted(k_Order, facts.worthAbout);
		const bool agrees = best == Desire::Play || best == Desire::BeFriends;
		return {.agrees = agrees, .explanation = agrees ? std::nullopt : std::optional(k_RefusedToPlay)};
	}
	case RequestKind::BeShown:
	{
		constexpr std::array k_Order {Desire::Curiosity, Desire::Fear, Desire::Anger};
		const bool agrees = MostWanted(k_Order, facts.worthAbout) == Desire::Curiosity;
		return {.agrees = agrees, .explanation = agrees ? std::nullopt : std::optional(k_RefusedToBeFriends)};
	}
	case RequestKind::BeFriends:
	{
		constexpr std::array k_Order {Desire::BeFriends, Desire::Fear, Desire::Anger};
		const bool agrees = MostWanted(k_Order, facts.worthAbout) == Desire::BeFriends;
		return {.agrees = agrees, .explanation = agrees ? std::nullopt : std::optional(k_RefusedToBeFriends)};
	}
	case RequestKind::Fight:
		break;
	}
	return {};
}

bool creature_friendship::AttentionRises(bool scriptControlled, bool sameCreature, bool otherAgendaUsesIt)
{
	return !scriptControlled && !sameCreature && otherAgendaUsesIt;
}

bool creature_friendship::AgendaUses(const AgendaUse& use, std::optional<uint32_t> thing)
{
	if (!use.running)
	{
		return false;
	}
	return std::ranges::find(use.plan, thing) != use.plan.end() || std::ranges::find(use.steps, thing) != use.steps.end();
}

void creature_friendship::Fade(std::span<creature_learning::CreatureAttitude> attitudes)
{
	for (auto& attitude : attitudes)
	{
		attitude.howImpressive *= k_ImpressiveFade;
		attitude.attention *= k_AttentionFade;
	}
}

void creature_friendship::ChangeAttention(creature_learning::CreatureAttitude& attitude, float change)
{
	attitude.attention = std::clamp(attitude.attention + change, 0.0f, 1.0f);
}

float creature_friendship::MissedFriends(std::span<const creature_learning::CreatureAttitude> attitudes)
{
	float missed = 0.0f;
	for (const auto& attitude : attitudes)
	{
		if (attitude.howNice > k_MissedNiceness && attitude.attention < k_MissedAttention)
		{
			missed += 1.0f - attitude.attention;
		}
	}
	return std::clamp(missed, 0.0f, 1.0f);
}
