/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "VillageVoices.h"

#include <cmath>

#include <algorithm>
#include <array>

#include "Common/X87.h"
#include "Magic/SpellRules.h"

using namespace openblack;
using namespace openblack::guidance;

namespace
{
/// The samples of the guidance sound bank
constexpr uint32_t k_GeneralCries = 130;
constexpr uint32_t k_FireCries = 177;
constexpr uint32_t k_LightningCries = 190;
constexpr uint32_t k_RockCries = 218;
/// The game has no sample in the ninth slot of the creature cries
constexpr std::array<uint32_t, 10> k_CreatureCries {200, 201, 202, 203, 204, 205, 206, 207, 0, 208};
constexpr uint32_t k_CryCount = 10;
constexpr uint32_t k_AttackerRepeats = 10;

constexpr uint32_t k_PleasedFood = 209;
constexpr uint32_t k_DispleasedFood = 165;
constexpr uint32_t k_PleasedRain = 212;
constexpr uint32_t k_PleasedWood = 215;
constexpr uint32_t k_AnswerCount = 3;
constexpr float k_PleasedDesire = 0.5f;
constexpr float k_DispleasedDesire = 0.25f;

constexpr float k_BurnShare = 0.001f;
constexpr float k_CryReach = 200.0f;
constexpr float k_ProtectionShare = 0.2f;
/// The voices of each row of desire voices, as samples of the guidance sound bank: the 17 town desires, then the food
/// and the worshippers of a worship site. A row of zeros has no voices.
constexpr std::array<std::array<uint32_t, 3>, 19> k_DesireVoices {{
    {144, 145, 146},                  // food
    {162, 163, 164},                  // wood
    {0, 0, 0},       {153, 154, 155}, // protection
    {147, 148, 149},                  // mercy
    {140, 140, 140},                  // build
    {141, 142, 143},                  // expand
    {0, 0, 0},       {150, 151, 152}, // children
    {0, 0, 0},       {0, 0, 0},       {0, 0, 0}, {0, 0, 0},    {0, 0, 0},
    {0, 0, 0},       {0, 0, 0},       {0, 0, 0}, {23, 24, 25}, // food for the worship site
    {19, 20, 21},                                              // worshippers
}};
/// The least values (less the draw) for the first, the second and the third voice of a row
constexpr std::array<float, 3> k_DesireVoiceSteps {0.85f, 0.65f, 0.45f};
constexpr float k_DesireDrawShare = 0.33333334f;
/// The scores of the desire voices reach their full size after these numbers of turns and at this distance
constexpr float k_DesireHeardTurns = 50.0f;
constexpr float k_DesireSeenTurns = 300.0f;
/// The time since the first look starts again after this many turns
constexpr uint32_t k_SeenRestartTurns = 600;
constexpr float k_ReachDrawShare = 0.5f;
} // namespace

std::optional<bool> guidance::FireLeads(const magic::EffectValues& values)
{
	using magic::EffectKind;
	std::optional<bool> fire;
	float most = 0.0f;
	for (const auto kind : {EffectKind::Burn, EffectKind::Crush, EffectKind::Hit, EffectKind::FlyAway})
	{
		const float value = kind == EffectKind::Burn ? std::min(values[kind] * k_BurnShare, 1.0f) : values[kind];
		if (most < value)
		{
			most = value;
			fire = kind == EffectKind::Burn;
		}
	}
	return fire;
}

uint32_t guidance::AttackerCry(Attacker attacker, uint32_t draw)
{
	if (draw >= k_CryCount)
	{
		return 0;
	}
	switch (attacker)
	{
	case Attacker::Rock:
		return k_RockCries + draw;
	case Attacker::Lightning:
		return k_LightningCries + draw;
	case Attacker::Creature:
		return k_CreatureCries.at(draw);
	case Attacker::Other:
		break;
	}
	return 0;
}

std::vector<uint32_t> guidance::AttackCries(bool fire, uint32_t attackerCry)
{
	std::vector<uint32_t> cries;
	for (uint32_t i = 0; i < k_CryCount; ++i)
	{
		cries.push_back(k_GeneralCries + i);
	}
	if (fire)
	{
		for (uint32_t i = 0; i < k_CryCount; ++i)
		{
			cries.push_back(k_FireCries + i);
		}
	}
	if (attackerCry != 0)
	{
		cries.insert(cries.end(), k_AttackerRepeats, attackerCry);
	}
	return cries;
}

uint32_t guidance::PickCry(const std::vector<uint32_t>& cries, uint32_t draw)
{
	return draw < cries.size() ? cries.at(cries.size() - 1 - draw) : 0;
}

float guidance::AttackCryReach(float protection)
{
	return k_CryReach * (1.0f + std::min(protection * k_ProtectionShare, 1.0f));
}

std::optional<uint32_t> guidance::GiftAnswer(Gift gift, float rawDesire, uint32_t draw)
{
	const bool pleased = !(rawDesire < k_PleasedDesire);
	const bool displeased = rawDesire < k_DispleasedDesire;
	if ((!pleased && !displeased) || draw >= k_AnswerCount)
	{
		return std::nullopt;
	}
	switch (gift)
	{
	case Gift::Food:
		return (pleased ? k_PleasedFood : k_DispleasedFood) + draw;
	case Gift::Wood:
		// The game has displeased answers for wood in the bank, but it plays the pleased ones
		return k_PleasedWood + draw;
	case Gift::Rain:
		return k_PleasedRain + draw;
	}
	return std::nullopt;
}

float guidance::DesireVoiceDrawLimit(float value)
{
	return value * k_DesireDrawShare;
}

uint32_t guidance::DesireVoice(uint32_t row, float value, float draw)
{
	if (row >= k_DesireVoices.size())
	{
		return 0;
	}
	const float left = value - draw;
	for (size_t step = 0; step < k_DesireVoiceSteps.size(); ++step)
	{
		if (!(left < k_DesireVoiceSteps.at(step)))
		{
			return k_DesireVoices.at(row).at(step);
		}
	}
	return 0;
}

float guidance::DesireVoiceScore(const DesireVoiceScoreInput& input)
{
	// Each step rounds to a float, in the order that the game uses
	const float heard = x87::Div(static_cast<double>(input.turnsSinceHeard), k_DesireHeardTurns);
	const float sinceHeard = heard < 1.0f ? heard : 1.0f;
	const float seenShare = x87::Div(static_cast<double>(input.turnsSinceSeen), k_DesireSeenTurns);
	const float seen = seenShare < 1.0f ? seenShare : 1.0f;
	const float notSeenLong = 1.0f - seen * seen * seen;
	const float far = input.distance / k_DesireVoiceRange;
	const float distance = far < 1.0f ? far : 1.0f;
	const float near = 1.0f - distance * distance;
	// A value that is not a number goes through as it is
	const float desire = std::isnan(input.desire) || input.desire < 1.0f ? input.desire : 1.0f;
	const float strength = desire * desire * desire;
	const float repeat = input.sameAsLast ? sinceHeard * sinceHeard * sinceHeard : 1.0f;
	const float score = strength * repeat * near * notSeenLong * sinceHeard;
	return score + score;
}

uint32_t guidance::TurnsSinceSeen(std::vector<ecs::components::GuidanceHeard::SeenThing>& seen, entt::entity thing,
                                  uint32_t turn)
{
	const auto found = std::ranges::find(seen, thing, &ecs::components::GuidanceHeard::SeenThing::thing);
	if (found == seen.end())
	{
		seen.insert(seen.begin(), {.thing = thing, .since = turn});
		return 0;
	}
	if (turn - found->since > k_SeenRestartTurns)
	{
		found->since = turn;
	}
	return turn - found->since;
}

std::optional<DesireVoicePick> guidance::WorshipSiteVoice(DesireVoicePick food, DesireVoicePick worshippers)
{
	std::optional<DesireVoicePick> pick;
	if (food.score > worshippers.score)
	{
		pick = food;
	}
	if (worshippers.score != 0.0f)
	{
		pick = worshippers;
	}
	return pick;
}

float guidance::DesireVoiceReachDrawLimit(float best)
{
	return best * k_ReachDrawShare;
}

float guidance::DesireVoiceReach(float best, float draw)
{
	return k_DesireVoiceRange * (best - draw);
}
