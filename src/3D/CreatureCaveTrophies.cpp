/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "CreatureCaveTrophies.h"

#include <algorithm>
#include <string_view>
#include <utility>

#include <fmt/format.h>

namespace openblack::CreatureCaveTrophies
{

namespace
{
constexpr std::array<std::string_view, k_BeltColours> k_Belts {"WHITE", "YELLOW", "BLUE", "GREEN", "RED", "PURPLE", "BLACK"};
constexpr std::array<std::string_view, 5> k_Medals {"WOOD", "BRONZE", "SILVER", "GOLD", "GEM"};
constexpr uint32_t k_BeltIcons = k_BeltColours * k_BeltLevels;
constexpr uint32_t k_MedalsPerMetal = 5;

/// The points of the room's mesh the two rows of belts hang at, and the medals stand at, every other point
constexpr uint32_t k_FirstBeltPoint = 16;
constexpr uint32_t k_FirstMedalPoint = 6;
/// A row of belts fills by this much of the fight balance's side, five to a colour
constexpr float k_BeltFill = 34.0f;
constexpr int32_t k_LevelsPerColour = 5;
/// The scroll multiplies the miracles' percentages added together by a 42nd, kept as a float
constexpr float k_OverMiracles = 1.0f / 42.0f;
/// A medal's level is a hundredth of its percentage, kept as a float, of its 25 levels: 20% falls just short of the
/// fifth level, and 100% reaches the last
constexpr float k_Hundredth = 0.01f;
constexpr float k_FullPercent = 100.0f;
} // namespace

std::string IconName(uint32_t icon)
{
	if (icon < k_BeltIcons)
	{
		return fmt::format("I_BELT_{}_0{}", k_Belts.at(icon / k_BeltLevels), (icon % k_BeltLevels) + 1);
	}
	const auto medal = icon - k_BeltIcons;
	return fmt::format("I_MEDAL_{}0{}", k_Medals.at(medal / k_MedalsPerMetal), (medal % k_MedalsPerMetal) + 1);
}

float PercentLearnt(uint32_t timesSeen, float timesNeeded)
{
	const float share = static_cast<float>(timesSeen) / timesNeeded;
	return (share > 1.0f ? 1.0f : share) * k_FullPercent;
}

MiracleLearning LearningOf(std::span<const MiracleLearnt> miracles)
{
	MiracleLearning learning;
	auto& best = learning.best;
	auto& which = learning.bestMiracles;
	for (const auto& [miracle, percent] : miracles)
	{
		learning.overall += percent;
		// A miracle takes the last place only by beating it, and moves up past each it beats
		if (best.back() < percent)
		{
			best.back() = percent;
			which.back() = miracle;
			for (size_t i = best.size() - 1; i > 0; --i)
			{
				if (best.at(i - 1) < best.at(i))
				{
					std::swap(best.at(i - 1), best.at(i));
					std::swap(which.at(i - 1), which.at(i));
				}
			}
		}
	}
	learning.overall *= k_OverMiracles;
	return learning;
}

int32_t MedalLevel(float percent)
{
	// Each step is rounded to a float, as the game's single-precision arithmetic rounds it
	const float hundredths = percent * k_Hundredth;
	const float levels = hundredths * static_cast<float>(k_MedalLevels);
	return std::min(static_cast<int32_t>(levels), static_cast<int32_t>(k_MedalLevels));
}

std::vector<Trophy> Choose(float fightBalance, const MiracleLearning& learning)
{
	std::vector<Trophy> trophies;
	// Two rows of seven belts: the first fills as the balance leans to attack, the second to defence
	const float side = (fightBalance + 1.0f) * 0.5f;
	for (uint32_t i = 0; i < 2 * k_BeltColours; ++i)
	{
		const float fill = i < k_BeltColours ? side : 1.0f - side;
		const float levels = fill * k_BeltFill;
		const auto colour = i % k_BeltColours;
		const int32_t level = static_cast<int32_t>(levels) - (static_cast<int32_t>(colour) * k_LevelsPerColour);
		if (level >= 1)
		{
			const auto icon =
			    (colour * k_BeltLevels) + static_cast<uint32_t>(std::min(level, static_cast<int32_t>(k_BeltLevels))) - 1;
			trophies.push_back({.point = k_FirstBeltPoint + i, .icon = icon, .medal = false, .environmentMapped = true});
		}
	}
	// A medal for all the miracles, then one for each of the best four
	for (uint32_t k = 0; k <= k_BestMiracles; ++k)
	{
		const auto level = MedalLevel(k == 0 ? learning.overall : learning.best.at(k - 1));
		if (level >= 1)
		{
			trophies.push_back({.point = k_FirstMedalPoint + (2 * k),
			                    .icon = k_BeltIcons + static_cast<uint32_t>(level) - 1,
			                    .medal = true,
			                    .environmentMapped = level > static_cast<int32_t>(k_MedalsPerMetal)});
		}
	}
	return trophies;
}

std::array<std::optional<SpellSeedType>, k_BestMiracles> SeedsToMake(const std::array<bool, k_BestMiracles>& shown,
                                                                     const std::array<SpellSeedType, k_BestMiracles>& seeds)
{
	std::array<std::optional<SpellSeedType>, k_BestMiracles> made {};
	for (size_t i = 0; i < k_BestMiracles; ++i)
	{
		if (!shown.at(i) && seeds.at(i) != SpellSeedType::None)
		{
			made.at(i) = seeds.at(i);
		}
	}
	return made;
}

} // namespace openblack::CreatureCaveTrophies
