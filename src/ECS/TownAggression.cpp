/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "TownAggression.h"

#include <limits>

using namespace openblack;
using namespace openblack::ecs;

namespace
{
/// Each attack wears its multiplier down by a tenth
constexpr float k_MultiplierWear = 0.9f;
/// The aggression fades by a thousandth a turn
constexpr float k_AggressionFade = 0.999f;
/// A multiplier recovers by a thousandth a turn while its want is under this
constexpr float k_SmallWant = 0.1f;
constexpr float k_MultiplierRecovery = 1.001f;
/// A town that holds this much or less against a player gives no time since the attack. The game compares with a
/// double.
constexpr double k_StillHeld = 0.15;
/// The game changes milliseconds to seconds with this float
constexpr float k_SecondsPerMillisecond = 0.001f;

/// The game recovers a multiplier through a min that works its product out twice: under 1 it recovers twice over
float Recover(float multiplier)
{
	if (multiplier * k_MultiplierRecovery < 1.0f)
	{
		return multiplier * k_MultiplierRecovery * k_MultiplierRecovery;
	}
	return 1.0f;
}
} // namespace

void town_aggression::Attacked(Record& record, PlayerNames aggressor, bool aggressorIsOwner, float amount,
                               float firstTimeAddition, uint32_t turn, const Harm& harm)
{
	const auto index = static_cast<size_t>(aggressor);
	if (index >= record.aggression.size())
	{
		return;
	}
	if (record.aggression.at(index) == 0.0f)
	{
		amount += firstTimeAddition;
	}
	auto& multiplier = aggressorIsOwner ? record.mercyMultiplier : record.protectionMultiplier;
	amount *= multiplier;
	multiplier *= k_MultiplierWear;
	record.lastAggressor = aggressor;
	record.lastTurn = turn;
	record.lastTurns.at(index) = turn;
	record.lastHarms.at(index) = harm;
	record.aggression.at(index) += amount;
}

void town_aggression::Attacked(Record& record, PlayerNames aggressor, bool aggressorIsOwner, float amount,
                               float firstTimeAddition, uint32_t turn)
{
	Attacked(record, aggressor, aggressorIsOwner, amount, firstTimeAddition, turn, Harm {});
}

void town_aggression::ProcessTurn(Record& record, PlayerNames owner)
{
	record.protection = 0.0f;
	record.mercy = 0.0f;
	const bool neutralTown = owner == PlayerNames::NEUTRAL;
	for (size_t i = 0; i < record.aggression.size(); ++i)
	{
		auto& aggression = record.aggression.at(i);
		aggression *= k_AggressionFade;
		if (!neutralTown && static_cast<PlayerNames>(i) == owner)
		{
			record.mercy += aggression;
		}
		else
		{
			record.protection += aggression;
		}
	}
	if (record.protection < k_SmallWant)
	{
		record.protectionMultiplier = Recover(record.protectionMultiplier);
	}
	if (record.mercy < k_SmallWant)
	{
		record.mercyMultiplier = Recover(record.mercyMultiplier);
	}
}

float town_aggression::SecondsSinceAttacked(const Record& record, PlayerNames player, uint32_t turn,
                                            uint32_t millisecondsPerTurn)
{
	const auto index = static_cast<size_t>(player);
	if (index >= record.aggression.size() || !(static_cast<double>(record.aggression.at(index)) > k_StillHeld))
	{
		return std::numeric_limits<float>::max();
	}
	// The game multiplies the whole turns by the turn length and rounds the product to a float once
	const auto turns = static_cast<int32_t>(turn - record.lastTurns.at(index));
	const auto milliseconds = static_cast<int64_t>(turns) * static_cast<int64_t>(millisecondsPerTurn);
	return static_cast<float>(milliseconds) * k_SecondsPerMillisecond;
}
