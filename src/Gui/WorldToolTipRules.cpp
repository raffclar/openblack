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

Numbered Belief(float margin)
{
	return {.index = k_BeliefLeft, .first = static_cast<double>(margin * k_BeliefScale), .second = std::nullopt};
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
	return {.index = k_TempleHealth, .first = static_cast<double>(life * k_PercentScale), .second = std::nullopt};
}

Numbered PrayerEnergy(float stored)
{
	return {.index = k_PrayerEnergy, .first = static_cast<double>(stored), .second = std::nullopt};
}

std::optional<Numbered> ProductionCompleted(bool repaired, int32_t makingTurns, float productionTime)
{
	if (!repaired)
	{
		return std::nullopt;
	}
	constexpr int32_t k_Percent = 100;
	// The turns that are left go in hundreds, and the share of the time is a float, as the game calculates it. The
	// whole number is cut toward zero, and then only the part below 100 stays.
	const float left = static_cast<float>(static_cast<int64_t>(makingTurns) * k_Percent) / productionTime;
	const auto made = static_cast<int32_t>(k_PercentScale - left);
	return Numbered {.index = k_ProductionCompleted, .first = static_cast<double>(made % k_Percent), .second = std::nullopt};
}

Numbered NeedsSign(NeedsRow row, float level, uint32_t woodRequired)
{
	switch (row)
	{
	case NeedsRow::Hunger:
		return {.index = k_FoodDesire, .first = static_cast<double>(level * k_PercentScale), .second = std::nullopt};
	case NeedsRow::Life:
		return {.index = k_RestDesire, .first = static_cast<double>(level * k_PercentScale), .second = std::nullopt};
	case NeedsRow::WorkshopWood:
		return {
		    .index = k_WoodRequired, .first = static_cast<double>(static_cast<float>(woodRequired)), .second = std::nullopt};
	case NeedsRow::Hiding:
		break;
	}
	return {.index = k_PickUp, .first = 0.0, .second = std::nullopt};
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
	if (index == k_ScaffoldValue)
	{
		// The words write the worth of the scaffold as a whole number
		std::u16string text(words);
		if (const auto at = text.find(u"%d"); at != std::u16string::npos)
		{
			std::u16string number;
			for (const char c : std::to_string(static_cast<int64_t>(first)))
			{
				number.push_back(static_cast<char16_t>(c));
			}
			text.replace(at, 2, number);
		}
		return text;
	}
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

std::u16string WithCharge(std::u16string_view words, double charge)
{
	constexpr int k_Width = 3;
	std::u16string text(words);
	text.push_back(u' ');
	for (const char c : fixed_format::Fixed(charge, k_Width, 0))
	{
		text.push_back(static_cast<char16_t>(c));
	}
	text.push_back(u'%');
	return text;
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

std::optional<uint32_t> HeldMiracle(magic::HeldCastOffer offer)
{
	switch (offer)
	{
	case magic::HeldCastOffer::Cast:
		return k_Cast;
	case magic::HeldCastOffer::CastCircle:
		return k_CastCircle;
	case magic::HeldCastOffer::None:
		break;
	}
	return std::nullopt;
}

bool OffersOverToolTips(bool miracleInHand)
{
	return !miracleInHand;
}

} // namespace openblack::gui::world_tooltip
