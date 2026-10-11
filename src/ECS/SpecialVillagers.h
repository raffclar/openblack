/*******************************************************************************
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
#include <optional>
#include <span>

#include "Enums.h"

/// Named villagers: now and then a villager the game makes is given a real person's name from the game's table of
/// named people, and that person's age. Each name is given to one villager at most on a land.
namespace openblack::ecs::special_villager
{

/// The table holds this many named people
constexpr size_t k_NameCount = 0x30;
/// A villager is made a named one on a draw below ten that is 0 or 1: two in ten
constexpr uint32_t k_Draw = 10;
constexpr uint32_t k_DrawsNamed = 1;

/// A row of the table as the choice sees it
struct Row
{
	SexType sex {SexType::None};
	bool hasName {false};
};

/// Whether a draw below ten makes the new villager a named one
[[nodiscard]] constexpr bool TriesNamed(uint32_t draw)
{
	return draw <= k_DrawsNamed;
}

/// The name a villager of a sex gets, starting at a row drawn below the table's size and going round the table once:
/// the first row of that sex (any sex for None) with a name and not yet given on this land. None when every one is.
[[nodiscard]] std::optional<uint8_t> PickName(std::span<const Row> rows, const std::array<bool, k_NameCount>& used, SexType sex,
                                              uint32_t start);

/// The age a named villager is made at: its name's age, unless it is being born
[[nodiscard]] constexpr uint32_t NamedAge(uint32_t age, uint32_t nameAge)
{
	return age == 1 ? 1 : nameAge;
}

} // namespace openblack::ecs::special_villager
