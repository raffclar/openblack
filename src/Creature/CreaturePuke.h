/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <cstdint>

#include <array>
#include <bit>
#include <functional>

#include <glm/vec3.hpp>

/// The sick that a creature brings up. Each game turn that its body plays the loop of the puke animation, twelve drops
/// of liquid come out of its lips. Each drop falls with a random speed, down and to the sides. Each drop has a random
/// shade of the land's light where the creature stands.
namespace openblack::creature_puke
{

/// The number of drops in each game turn
inline constexpr int32_t k_DropsPerTurn = 12;
/// Each axis of a drop's speed is a draw of 0 to 99 times this part of the shown size. Along x and z, the speed is less
/// this other part of the shown size, and the speed down has no such part.
inline constexpr float k_SpeedStep = std::bit_cast<float>(0x3C03126Fu);
inline constexpr float k_SpeedLeast = std::bit_cast<float>(0x3ECCCCCDu);
inline constexpr int32_t k_SpeedDraws = 100;
/// The size of a drop for each unit of the shown size
inline constexpr float k_SizePerSize = std::bit_cast<float>(0x3E4CCCCDu);
/// The red, green and blue of a drop are those of the light, times a draw of 0 to less than the range plus the least,
/// divided by 255
struct Shade
{
	int32_t range;
	int32_t least;
};
inline constexpr Shade k_Red {.range = 100, .least = 80};
inline constexpr Shade k_Green {.range = 80, .least = 60};
inline constexpr Shade k_Blue {.range = 30, .least = 20};

/// Gives a random whole number from 0 to less than a limit, from the local random numbers of the game
using LocalRandom = std::function<uint32_t(int32_t)>;

/// One drop: its speed and its colour (0xAARRGGBB)
struct Drop
{
	glm::vec3 velocity {0.0f};
	uint32_t argb {0};
};

/// The size of each drop of a creature of a shown size
[[nodiscard]] float DropSize(float shownSize);

/// A channel of the light times a factor, divided by 255 and cut towards zero
[[nodiscard]] uint32_t ShadeChannel(uint32_t channel, int32_t factor);

/// The twelve drops of one game turn, for the shown size and the light (0xAARRGGBB) where the creature stands. For each
/// drop, the random numbers come in this order: the speed along z, the speed down, the speed along x, then the red,
/// the green and the blue.
[[nodiscard]] std::array<Drop, k_DropsPerTurn> DropsOf(float shownSize, uint32_t light, const LocalRandom& random);

} // namespace openblack::creature_puke
