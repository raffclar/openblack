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

#include <functional>
#include <optional>
#include <string>
#include <string_view>

/// Every creature has a name of its own, shown in the fight panel and on the Creature Cave's scroll. The player's
/// creature takes the creature name of the player's profile, which starts as the player's own name. A creature that
/// belongs to nobody or to a computer player is named after one of four help texts, and a script can name any creature
/// with a help text's words. A creature saved with its mind keeps its name in the file, and takes it back up when its
/// mind is loaded, unless a computer player owns it.
namespace openblack::creature_names
{

/// Who a creature belongs to, as far as its name goes
enum class Owner : uint8_t
{
	/// No one: the creatures the scripts make before a player takes one
	Nobody,
	/// A god the computer plays
	Computer,
	/// The player at this machine
	Player,
};

/// The first of the four help texts a creature that isn't the player's is named with: Rootle, Chugworth, Samson and
/// Brinsley
constexpr uint32_t k_FirstStrayNameText = 5008;
constexpr uint32_t k_StrayNameCount = 4;

/// The help text a new creature that isn't the player's is named with, by how many creatures there are, it counted
[[nodiscard]] constexpr uint32_t StrayNameText(size_t creatures)
{
	return k_FirstStrayNameText + static_cast<uint32_t>(creatures % k_StrayNameCount);
}

/// The name a new creature is given. The player's creature takes their profile's creature name (none without a
/// profile); any other takes the words of the help text StrayNameText gives.
[[nodiscard]] std::u16string NewName(Owner owner, size_t creatures, std::optional<std::u16string_view> profileCreatureName,
                                     const std::function<std::u16string(uint32_t)>& helpWords);

/// The most characters a creature's name can have. A script that names a creature with a longer help text gives it
/// only the first characters.
constexpr size_t k_LongestName = 63;

/// The name cut to k_LongestName characters. The cut does not keep half of a character pair: a name that would end
/// with the first half of a pair ends before it.
[[nodiscard]] std::u16string FitName(std::u16string_view name);

/// Whether a creature whose mind is loaded from a file takes the name the file keeps: all but a computer player's do
[[nodiscard]] constexpr bool TakesNameFromFile(Owner owner)
{
	return owner != Owner::Computer;
}

/// Whether a change of the profile, or an edit of its creature name, gives the player's creature the creature name of the
/// profile. Only a game alone does this. In a network game, the profile of one computer does not change a creature of
/// the world.
[[nodiscard]] constexpr bool ProfileRenamesCreature(bool networkGame)
{
	return !networkGame;
}

} // namespace openblack::creature_names
