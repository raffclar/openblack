/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "TreeSway.h"

#include <bit>

#include "Common/X87.h"

using namespace openblack::nature;

namespace
{
/// Radians that a sway moves on for each millisecond at a speed of 1. The game holds it as a float.
constexpr float k_PhasePerMillisecond = std::bit_cast<float>(0x3A8B0409u);
/// The lean for each unit of height. The game holds it as a double.
constexpr double k_Lean = 0.029999999329447746;
/// A turn, and the turns in a radian, as the game holds them in floats
constexpr float k_Turn = std::bit_cast<float>(0x40C90FDBu);
constexpr float k_TurnsPerRadian = std::bit_cast<float>(0x3E22F983u);
/// The sixteenths of a turn in a radian, as the game holds it in a float
constexpr float k_SlotsPerRadian = std::bit_cast<float>(0x4022F983u);
} // namespace

float tree_sway::StepPhase(float phase, float speed, uint32_t frameMilliseconds)
{
	return ((x87::RoundToFloat(static_cast<double>(frameMilliseconds)) * speed) * k_PhasePerMillisecond) + phase;
}

float tree_sway::Lean(float phase)
{
	// The sway leans towards minus z at the start of its phase. Its lean along x is always 0.
	return -x87::Mul(x87::Cos(phase), k_Lean);
}

float tree_sway::Facing(float yaw)
{
	const auto turns = static_cast<int32_t>(yaw * k_TurnsPerRadian);
	return yaw - (static_cast<float>(turns) * k_Turn);
}

uint8_t tree_sway::SlotOf(float facing)
{
	const auto slot = static_cast<int32_t>((facing * k_SlotsPerRadian) + 0.5f);
	return static_cast<uint8_t>(static_cast<uint32_t>(slot) & (k_Count - 1));
}
