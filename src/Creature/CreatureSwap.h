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

/// Swapping creatures: a script can move the player's creature's mind into another creature's body, and that creature's
/// mind into the player's old one. What it has learnt, how it feels, its physique and its tattoos go with the mind; each
/// body keeps its name and species. The new body becomes the player's creature, and over the swap each body grows or
/// shrinks to the other's size and takes on the other's alignment, so those go with the mind too, little by little.
/// The Creature Breeder offers the five bonus creatures, which are only there once they have been unlocked.
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

/// A body's size and alignment, which it takes from the other body over a swap
struct Shape
{
	float size {1.0f};
	float alignment {0.0f};
};

/// Each turn a body closes this share of the gap to the shape it takes on
inline constexpr float k_EaseShare = 0.04f;
/// It has taken on the shape once both its size and its alignment are nearer than this
inline constexpr double k_Settled = 0.01;

/// Whether the body has taken on the shape: both its size and its alignment within the settled distance of it
[[nodiscard]] bool IsSettled(const Shape& now, const Shape& target);

/// One turn of a body taking on the shape: a share of each gap closed, the alignment kept from -1 to 1
[[nodiscard]] Shape Ease(const Shape& now, const Shape& target);

/// The sightings of a miracle a mind keeps as it moves into a body of another species, so that it is as far towards
/// learning it as before: scaled by the times the new species must see it over the times the old one must, rounded
/// down. Kept as they are when the old species never needs to see it.
[[nodiscard]] uint32_t RescaledSightings(uint32_t seen, float neededBefore, float neededAfter);

} // namespace openblack::creature_swap
