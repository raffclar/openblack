/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "VillageVoices.h"

#include <algorithm>
#include <array>

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
