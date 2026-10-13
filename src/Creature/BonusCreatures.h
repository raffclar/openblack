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

#include <bitset>
#include <optional>
#include <string_view>

/// The Creature Breeder's five bonus creatures, which scripts may offer only once they have been unlocked
namespace openblack::creature_swap
{

/// The bonus creatures the breeder can offer, in the order of the unlocks
enum class Bonus : uint8_t
{
	Leopard,
	Horse,
	Mandrill,
	Gorilla,
	Rhino,

	_Count
};
inline constexpr size_t k_BonusCount = static_cast<size_t>(Bonus::_Count);
using Unlocked = std::bitset<k_BonusCount>;

/// The bonus creature a script's species number is, none for any other species
[[nodiscard]] std::optional<Bonus> BonusOf(int32_t species);

/// Whether a species can be offered, as scripts ask: a bonus creature only once unlocked, any other species always
[[nodiscard]] bool IsAvailable(int32_t species, const Unlocked& unlocked);

/// A bonus creature's name, as the debug option spells it: leopard, horse, mandrill, gorilla or rhino
[[nodiscard]] std::string_view NameOf(Bonus bonus);

/// The bonus creatures a debug option unlocks: "all", "none", or names joined by commas (in any case); none when the
/// text names something else
[[nodiscard]] std::optional<Unlocked> ParseUnlocked(std::string_view text);

} // namespace openblack::creature_swap
