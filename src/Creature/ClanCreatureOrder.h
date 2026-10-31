/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <cstddef>
#include <cstdint>

#include <optional>
#include <span>
#include <string>
#include <string_view>

/// The order to make a new clan creature. Before the first game of a clan, the clan has no creature. The start data of
/// the god that plays for the clan then holds the text "CLANCREATECREATURE", two digits that give the row of the creature
/// table, and the name of the new creature. Every computer makes a new creature of that species with that name.
///
/// The game looks only at the start of the data. Thus data from any god that starts with this text makes a new creature.
namespace openblack::clan_creature_order
{

/// The text at the start of the order. The game compares it with no attention to upper and lower case.
constexpr std::string_view k_Text = "CLANCREATECREATURE";
/// The number of digits that give the row of the creature table
constexpr size_t k_RowDigits = 2;
/// The most characters that the name of a creature holds. The game does not stop at this number and writes a longer
/// name past the name of the creature (vanilla-bugs row 793): openblack keeps the first characters.
constexpr size_t k_MostNameCharacters = 63;
/// The new creature takes each stage up to this stage, one after the other, before it gets its leashes
constexpr uint32_t k_StagesBeforeLeashes = 13;

/// What the order asks for
struct Order
{
	/// The number of the two digits, as the C function atoi reads them: spaces before the number, a sign, then digits
	int32_t row {0};
	/// The name of the new creature
	std::u16string name;
};

/// The order in the start data of a god, or none when the data does not start with the text of the order. The name is
/// the text after the two digits, up to the first zero byte or the end of the data, in code page 1252.
[[nodiscard]] std::optional<Order> Read(std::span<const uint8_t> data);

} // namespace openblack::clan_creature_order
