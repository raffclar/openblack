/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "CreatureSwap.h"

#include <cmath>

#include <algorithm>

using namespace openblack;
using namespace openblack::creature_swap;

std::optional<Bonus> creature_swap::BonusOf(int32_t species)
{
	switch (species)
	{
	case 3:
		return Bonus::Leopard;
	case 6:
		return Bonus::Horse;
	case 14:
		return Bonus::Mandrill;
	case 15:
		return Bonus::Rhino;
	case 16:
		return Bonus::Gorilla;
	default:
		return std::nullopt;
	}
}

bool creature_swap::IsAvailable(int32_t species, const Unlocked& unlocked)
{
	const auto bonus = BonusOf(species);
	return !bonus.has_value() || unlocked.test(static_cast<size_t>(*bonus));
}

bool creature_swap::IsSettled(const Shape& now, const Shape& target)
{
	const auto sizeGap = static_cast<double>(target.size) - static_cast<double>(now.size);
	const auto alignmentGap = static_cast<float>(static_cast<double>(target.alignment) - static_cast<double>(now.alignment));
	return std::abs(sizeGap) < k_Settled && std::abs(static_cast<double>(alignmentGap)) < k_Settled;
}

Shape creature_swap::Ease(const Shape& now, const Shape& target)
{
	// The size's gap is worked out at full precision; the alignment's is kept as a float first
	const auto sizeGap = static_cast<double>(target.size) - static_cast<double>(now.size);
	const auto alignmentGap = static_cast<float>(static_cast<double>(target.alignment) - static_cast<double>(now.alignment));
	const auto size = static_cast<float>(sizeGap * static_cast<double>(k_EaseShare) + static_cast<double>(now.size));
	const auto alignment = static_cast<float>(static_cast<double>(alignmentGap) * static_cast<double>(k_EaseShare) +
	                                          static_cast<double>(now.alignment));
	return {.size = size, .alignment = std::clamp(alignment, -1.0f, 1.0f)};
}

uint32_t creature_swap::RescaledSightings(uint32_t seen, float neededBefore, float neededAfter)
{
	const auto factor = neededBefore > 0.0f ? static_cast<double>(neededAfter) / static_cast<double>(neededBefore) : 1.0;
	const auto scaled = static_cast<double>(seen) * factor;
	return scaled <= 0.0 ? 0u : static_cast<uint32_t>(scaled);
}
