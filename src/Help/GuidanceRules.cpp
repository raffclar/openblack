/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "GuidanceRules.h"

#include <cmath>

#include <numbers>

using namespace openblack;
using namespace openblack::guidance;

namespace
{
// The waits, help levels and first-land switches of the 33 kinds, in kind order
constexpr std::array<KindRule, k_KindCount> k_Rules {{
    {.leastGap = 50, .helpLevel = 1, .onFirstLand = false},    // town desire sound
    {.leastGap = 25, .helpLevel = 1, .onFirstLand = false},    // resource drop sound
    {.leastGap = 40, .helpLevel = 1, .onFirstLand = true},     // town attacked sound
    {.leastGap = 50, .helpLevel = 1, .onFirstLand = false},    // totem raised sound
    {.leastGap = 0, .helpLevel = 0, .onFirstLand = true},      // new disciple sound
    {.leastGap = 30, .helpLevel = 1, .onFirstLand = true},     // belief sound
    {.leastGap = 0, .helpLevel = 1, .onFirstLand = true},      // sound 6
    {.leastGap = 0, .helpLevel = 1, .onFirstLand = true},      // death in the village
    {.leastGap = 100, .helpLevel = 1, .onFirstLand = true},    // any remark
    {.leastGap = 1000, .helpLevel = 2, .onFirstLand = false},  // our town attacked
    {.leastGap = 1500, .helpLevel = 2, .onFirstLand = false},  // our creature attacked
    {.leastGap = 2000, .helpLevel = 3, .onFirstLand = false},  // our creature attacking them
    {.leastGap = 1000, .helpLevel = 2, .onFirstLand = false},  // losing villagers
    {.leastGap = 1000, .helpLevel = 3, .onFirstLand = true},   // attacking a town
    {.leastGap = 2500, .helpLevel = 3, .onFirstLand = false},  // low on food
    {.leastGap = 2500, .helpLevel = 3, .onFirstLand = false},  // low on wood
    {.leastGap = 1000, .helpLevel = 4, .onFirstLand = false},  // injured people
    {.leastGap = 2500, .helpLevel = 2, .onFirstLand = false},  // low on people
    {.leastGap = 3000, .helpLevel = 3, .onFirstLand = false},  // villagers unhappy
    {.leastGap = 1000, .helpLevel = 2, .onFirstLand = false},  // losing belief
    {.leastGap = 1000, .helpLevel = 2, .onFirstLand = false},  // other villages
    {.leastGap = 200, .helpLevel = 1, .onFirstLand = true},    // creature fight
    {.leastGap = 1000, .helpLevel = 2, .onFirstLand = false},  // general bad
    {.leastGap = 1000, .helpLevel = 2, .onFirstLand = false},  // general good
    {.leastGap = 500, .helpLevel = 2, .onFirstLand = false},   // killing people
    {.leastGap = 5000, .helpLevel = 2, .onFirstLand = true},   // good being evil
    {.leastGap = 5000, .helpLevel = 2, .onFirstLand = true},   // evil being good
    {.leastGap = 600, .helpLevel = 1, .onFirstLand = false},   // worshippers dying
    {.leastGap = 10000, .helpLevel = 2, .onFirstLand = false}, // very good
    {.leastGap = 10000, .helpLevel = 2, .onFirstLand = false}, // very evil
    {.leastGap = 1000, .helpLevel = 3, .onFirstLand = true},   // destroying a building
    {.leastGap = 0, .helpLevel = 0, .onFirstLand = true},      // no text
    {.leastGap = 0, .helpLevel = 0, .onFirstLand = true},      // one-off
}};

constexpr std::array<OneOffRule, k_OneOffCount> k_OneOffs {{
    {.text = 3308, .chance = 0.02f},
    {.text = 3317, .chance = 0.0002f},
    {.text = 3318, .chance = 0.01f},
    {.text = 3321, .chance = 0.01f},
    {.text = 3325, .chance = 0.05f},
    {.text = 3326, .chance = 0.1f},
    {.text = 3328, .chance = 0.025f},
}};

// The first kind with a row of remarks, the last, and the rows they take in order
constexpr auto k_FirstRemarkKind = static_cast<size_t>(Kind::OurTownAttacked);
constexpr auto k_LastRemarkKind = static_cast<size_t>(Kind::DestroyBuilding);

// Past this the alignment's drift is remarked on, in turns' worth of the most it can change a turn
constexpr float k_DriftTurns = 2.0f;
constexpr float k_SameWayThreshold = 0.75f;
constexpr float k_OtherWayThreshold = 0.4f;

constexpr double k_MoonCloseToFull = 0.15000000596046448;
constexpr double k_MoonWaitScale = 12000.0;
} // namespace

const KindRule& guidance::Rule(Kind kind)
{
	return k_Rules.at(static_cast<size_t>(kind));
}

const OneOffRule& guidance::Rule(OneOff oneOff)
{
	return k_OneOffs.at(static_cast<size_t>(oneOff));
}

uint32_t guidance::GapSpread(uint32_t leastGap, float draw)
{
	// Worked in double, as the game works it at more than float precision, and cut towards zero
	const double cube = static_cast<double>(draw) * draw * draw;
	return static_cast<uint32_t>(static_cast<double>(leastGap) * 5.0 * (1.0 - cube));
}

bool guidance::MayPlay(const KindRule& rule, uint32_t helpLevel, bool firstLandOfStory, uint32_t wait, uint32_t turnsSinceHeard)
{
	if (firstLandOfStory && !rule.onFirstLand)
	{
		return false;
	}
	return rule.helpLevel <= helpLevel && wait < turnsSinceHeard;
}

std::optional<size_t> guidance::RemarkRow(Kind kind)
{
	const auto index = static_cast<size_t>(kind);
	if (index < k_FirstRemarkKind || index > k_LastRemarkKind)
	{
		return std::nullopt;
	}
	return index - k_FirstRemarkKind;
}

size_t guidance::RemarkChoices(const RemarkTexts& texts)
{
	size_t count = 0;
	// The count stops one short of the row's end, so a full row's last text is never picked
	while (count + 1 < k_RemarkSlots && texts.at(count) != 0)
	{
		++count;
	}
	return count;
}

std::optional<Kind> guidance::AlignmentRemark(float drift, float alignment, float maxChangePerTurn)
{
	if (!(k_DriftTurns * maxChangePerTurn < std::abs(drift)))
	{
		return std::nullopt;
	}
	const bool sameWay = alignment * drift > 0.0f;
	const float threshold = sameWay ? k_SameWayThreshold : k_OtherWayThreshold;
	if (!(threshold < std::abs(alignment)))
	{
		return std::nullopt;
	}
	const bool good = alignment > 0.0f;
	if (!sameWay)
	{
		return good ? Kind::GoodBeingEvil : Kind::EvilBeingGood;
	}
	return good ? Kind::VeryEvil : Kind::VeryGood;
}

bool guidance::ClockSaysNight(int hour, int minute)
{
	const bool day = hour < 23 && hour > 5 && (hour < 20 || minute < 45 || hour > 20);
	return !day;
}

MoonLook guidance::LookAtMoon(float phase, bool clockSaysNight)
{
	const float distance = phase - std::numbers::pi_v<float>;
	if (clockSaysNight && std::abs(static_cast<double>(distance)) < k_MoonCloseToFull)
	{
		return {.remark = true, .wait = k_MoonRemarkWait};
	}
	const double cube = static_cast<double>(distance) * distance * distance;
	return {.remark = false, .wait = static_cast<int32_t>(cube * k_MoonWaitScale)};
}
