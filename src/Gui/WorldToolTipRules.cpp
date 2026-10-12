/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "WorldToolTipRules.h"

#include <array>

#include "Common/FixedFormat.h"

namespace openblack::gui::world_tooltip
{

Numbered Belief(double margin)
{
	return {.index = k_BeliefLeft, .first = margin * k_BeliefScale, .second = std::nullopt};
}

Wording Worded(uint32_t index, double number)
{
	if (index == k_BeliefLeft && number < 0.0)
	{
		return {.words = k_BeliefNeeded, .number = -number};
	}
	return {.words = index, .number = number};
}

Numbered TempleHealth(float life)
{
	return {.index = k_TempleHealth, .first = static_cast<double>(life) * k_PercentScale, .second = std::nullopt};
}

Numbered Population(uint32_t people, uint32_t places)
{
	return {.index = k_Population, .first = static_cast<double>(people), .second = static_cast<double>(places)};
}

Numbered Stored(uint32_t food, uint32_t wood)
{
	return {.index = k_Stored, .first = static_cast<double>(food), .second = static_cast<double>(wood)};
}

Numbered PileAmount(bool food, uint32_t amount)
{
	return {.index = food ? k_FoodAmount : k_WoodAmount, .first = static_cast<double>(amount), .second = std::nullopt};
}

Numbered HeldAmount(uint32_t amount)
{
	return {.index = k_HeldAmount, .first = static_cast<double>(amount), .second = std::nullopt};
}

std::u16string Compose(uint32_t index, std::u16string_view words, double first, std::optional<double> second)
{
	if (!second.has_value())
	{
		return fixed_format::WithNumber(words, first);
	}
	if (index == k_Population)
	{
		// The people as the words write them, then "/" and the places as a whole number
		auto text = fixed_format::WithNumber(words, first);
		text.push_back(u'/');
		for (const char c : std::to_string(static_cast<int64_t>(*second)))
		{
			text.push_back(static_cast<char16_t>(c));
		}
		return text;
	}
	const std::array<double, 2> values {first, *second};
	return fixed_format::WithNumbers(words, values);
}

} // namespace openblack::gui::world_tooltip
