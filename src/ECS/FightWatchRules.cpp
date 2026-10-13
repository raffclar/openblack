/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "FightWatchRules.h"

#include <array>

#include "Common/GUtilsAngle.h"

using namespace openblack::ecs;
using namespace openblack::ecs::fight_watch;

namespace
{
/// The highest set bit of a byte, 0 for 0 and 1
uint32_t ByteLog2(uint32_t byte)
{
	uint32_t bits = 0;
	for (byte &= 0xFFu; (byte >>= 1u) != 0; ++bits)
	{
	}
	return bits;
}

/// The highest set bit of a number, as the game finds it a byte at a time from the top: the first byte whose own highest
/// bit is above its lowest decides, so a byte of just 1 is passed over for the bytes below it
uint32_t Log2(uint32_t value)
{
	for (const uint32_t shift : {24u, 16u, 8u})
	{
		if (const auto bits = ByteLog2(value >> shift); bits != 0)
		{
			return bits + shift;
		}
	}
	return ByteLog2(value);
}
} // namespace

uint16_t fight_watch::PlaceAngle(uint32_t index)
{
	constexpr uint32_t k_Circle = static_cast<uint32_t>(gutils::k_GameAngleCircle);
	// Each pair of places is half a turn apart; the pairs halve the gaps left between those before them
	const uint32_t pair = index >> 1u;
	uint32_t numerator = 0;
	uint32_t denominator = 1;
	if (pair != 0)
	{
		const auto bits = Log2(pair);
		numerator = ((pair - (1u << (bits & 31u))) * 2u) + 1u;
		denominator = 1u << ((bits + 1u) & 31u);
	}
	uint32_t angle = ((numerator * k_Circle) >> 1u) / denominator;
	if ((index & 1u) != 0)
	{
		angle += k_Circle >> 1u;
	}
	return static_cast<uint16_t>(angle);
}

glm::ivec2 fight_watch::PlaceOnRing(glm::ivec2 middle, float arenaRadius, uint16_t angle)
{
	constexpr float k_UnitsPerCell = 65536.0f;
	constexpr float k_CellMetres = 10.0f;
	constexpr float k_PerUnit = 1.0f / 65536.0f;
	const float radius = arenaRadius * k_RingPerRadius;
	// In metres the game's way, each step rounded to a float, then back to map units, truncated
	const auto axis = [radius](int32_t table, int32_t centre) {
		const float along = static_cast<float>(table) * radius * k_PerUnit;
		const float from = static_cast<float>(static_cast<double>(centre) * k_CellMetres) * k_PerUnit;
		const float metres = along + from;
		return static_cast<int32_t>((metres * k_UnitsPerCell) / k_CellMetres);
	};
	return {axis(gutils::Cos(angle), middle.x), axis(gutils::Sin(angle), middle.y)};
}

uint8_t fight_watch::Priority(bool fightOn, uint8_t tablePriority)
{
	return fightOn ? tablePriority : 0;
}

Side fight_watch::Supports(std::optional<TownFeelings> town, const IntRandom& random)
{
	using creature_attitude::Attitude;
	if (town.has_value())
	{
		const bool fearsFirst = town->first == Attitude::Fear;
		const bool fearsSecond = town->second == Attitude::Fear;
		// It supports the one its town doesn't fear
		if (fearsFirst != fearsSecond)
		{
			return fearsFirst ? Side::Second : Side::First;
		}
	}
	return random(2) != 0 ? Side::Second : Side::First;
}

uint32_t fight_watch::SecondsSince(uint32_t now, uint32_t then, uint32_t turnsPerSecond)
{
	return turnsPerSecond != 0 ? (now - then) / turnsPerSecond : 0;
}
