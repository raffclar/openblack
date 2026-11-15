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

#include <array>
#include <optional>
#include <string_view>

/// The values of a creature that the editor changes in a network game. Each change goes to every computer as a command
/// of a turn, so that all the computers do the same turn. These rules keep no state, so that the game and the tests use
/// them both.
///
/// To add a value:
/// 1. Add the value before Count, and its name in k_ValueNames.
/// 2. Give its limits in editor::LimitsOf, and make editor::SetCreatureValue set it.
namespace openblack::creature_edit_values
{

enum class Value : uint8_t
{
	/// From -1 (evil) to 1 (good)
	Alignment,
	Fatness,
	Strength,
	/// How big the creature is. The size of its body follows it.
	Size,
	/// How full the creature is. The top limit is the size that the body shows, or 1 if the body is smaller.
	Energy,
	Exhaustion,
	Thirst,
	Poo,
	Life,
	/// From -1 (cold) to 1 (hot)
	Warmth,
	Itchiness,
	/// The stage of growing up. The top limit is the last stage of the info table.
	GrownUp,
	Count,
};

inline constexpr size_t k_ValueCount = static_cast<size_t>(Value::Count);

/// The name of each value, for the editor, the inspector and the logs
inline constexpr std::array<std::string_view, k_ValueCount> k_ValueNames {
    "alignment", "fatness", "strength", "size",   "energy",    "exhaustion",
    "thirst",    "poo",     "life",     "warmth", "itchiness", "grown up",
};

/// The value of a number on the wire, or none for a number that no value has
[[nodiscard]] constexpr std::optional<Value> ValueFrom(uint8_t number) noexcept
{
	if (number >= k_ValueCount)
	{
		return std::nullopt;
	}
	return static_cast<Value>(number);
}

[[nodiscard]] constexpr std::string_view NameOf(Value value) noexcept
{
	const auto index = static_cast<size_t>(value);
	return index < k_ValueCount ? k_ValueNames.at(index) : std::string_view {"unknown value"};
}

/// The value of a name, or none for a name that no value has
[[nodiscard]] constexpr std::optional<Value> ValueNamed(std::string_view name) noexcept
{
	for (size_t index = 0; index < k_ValueCount; ++index)
	{
		if (k_ValueNames.at(index) == name)
		{
			return static_cast<Value>(index);
		}
	}
	return std::nullopt;
}

} // namespace openblack::creature_edit_values
