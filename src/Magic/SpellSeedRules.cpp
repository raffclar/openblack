/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "SpellSeedRules.h"

#include <algorithm>

#include <glm/geometric.hpp>

#include "InfoConstants.h"

using namespace openblack;
using namespace openblack::magic;

CastStyle magic::CastStyleOf(SpellCastType castType)
{
	switch (castType)
	{
	case SpellCastType::SpellCastInHand:
		return CastStyle::Held;
	case SpellCastType::SpellCastHandGesture:
		return CastStyle::OnRelease;
	case SpellCastType::SpellCastHandPosition:
		break;
	}
	return CastStyle::OnPress;
}

CastStyle magic::CastStyleOf(const GSpellSeedInfo& seed)
{
	return CastStyleOf(seed.castType);
}

float magic::ChantNeeded(float costToCreate, float store)
{
	return costToCreate - store;
}

float magic::SeedPower(float store, float costToCreate)
{
	return costToCreate > 0.0f ? std::min(store / costToCreate, 1.0f) : 1.0f;
}

bool magic::SeedReadyAfter(uint32_t turnsInHand, float turnSeconds, float delaySeconds)
{
	return static_cast<float>(turnsInHand) * turnSeconds > delaySeconds;
}

bool magic::ShownWithSpell(const SeedWithSpell& seed)
{
	return !seed.castInHand && !seed.keptInHand && seed.followsSpell && !seed.spellClosedDown && seed.hasIcon;
}

bool magic::ClosesOutOfInfluence(bool shown, bool inInfluence, bool closedDown)
{
	return shown && !inInfluence && !closedDown;
}

float magic::TopUnderSeed(float seedRadius, std::span<const ThingUnderSeed> things)
{
	float top = 0.0f;
	for (const auto& thing : things)
	{
		if (thing.living || thing.moving || !(thing.top > top))
		{
			continue;
		}
		if (glm::dot(thing.offset, thing.offset) < (seedRadius * seedRadius) + (thing.radius * thing.radius))
		{
			top = thing.top;
		}
	}
	return top;
}

float magic::ForestSeedHeight(float topUnder, std::optional<float> tallestTree)
{
	if (!tallestTree.has_value())
	{
		return k_UnplantedForestSeedHeight;
	}
	return std::max(*tallestTree, topUnder);
}
