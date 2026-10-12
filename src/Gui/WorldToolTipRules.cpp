/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "WorldToolTipRules.h"

#include <algorithm>
#include <array>

#include <glm/vec4.hpp>

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

std::optional<uint32_t> ToolTipOfText(uint32_t text)
{
	if (text < k_FirstToolTipText || text - k_FirstToolTipText >= k_ToolTipCount)
	{
		return std::nullopt;
	}
	return text - k_FirstToolTipText;
}

std::optional<uint32_t> ScaffoldDrop(std::optional<uint32_t> plannedBuildText)
{
	return plannedBuildText.has_value() ? ToolTipOfText(*plannedBuildText) : std::optional(k_Build);
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

std::optional<glm::ivec2> HandOnScreen(const glm::mat4& worldToClip, float nearClip, glm::vec3 hand, glm::ivec2 screen)
{
	const auto clip = worldToClip * glm::vec4(hand, 1.0f);
	if (clip.w < nearClip)
	{
		return std::nullopt;
	}
	// Whole pixels, the fractions cut off
	return glm::ivec2 {static_cast<int>((clip.x / clip.w + 1.0f) * static_cast<float>(screen.x) * 0.5f),
	                   static_cast<int>((1.0f - clip.y / clip.w) * static_cast<float>(screen.y) * 0.5f)};
}

glm::ivec2 KeptOnScreen(glm::ivec2 hand, glm::ivec2 screen, int size)
{
	return {std::clamp(hand.x, 0, std::max(screen.x - size, 0)), std::clamp(hand.y, 0, std::max(screen.y - size, 0))};
}

} // namespace openblack::gui::world_tooltip
