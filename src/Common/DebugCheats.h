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

/// The debug cheats of a player. The editor turns them on and off for the player of this computer. In a network game
/// only a staff player can open the editor, and the game server refuses a cheat from every other player. A cheat goes
/// to every computer as a command of a turn, so that all the computers do the same turn. These rules keep no state, so
/// that the game and the tests use them both.
///
/// To add a cheat:
/// 1. Add its value before Count, and its name in k_CheatNames.
/// 2. Make the rule of the game ask Has for the player.
namespace openblack::debug_cheats
{

enum class Cheat : uint8_t
{
	/// The influence of the player covers all the land: the hand acts at each point, also under a shield
	InfiniteInfluence,
	/// The worship sites and the prayer power of the player never run dry, and the upkeep of the miracles costs nothing
	InfiniteMiracleCharge,
	Count,
};

inline constexpr size_t k_CheatCount = static_cast<size_t>(Cheat::Count);

/// The name of each cheat, for the editor and the logs
inline constexpr std::array<std::string_view, k_CheatCount> k_CheatNames {
    "infinite influence",
    "infinite miracle charge",
};

/// The cheat of a number on the wire, or none for a number that no cheat has
[[nodiscard]] constexpr std::optional<Cheat> CheatFrom(uint8_t number) noexcept
{
	if (number >= k_CheatCount)
	{
		return std::nullopt;
	}
	return static_cast<Cheat>(number);
}

[[nodiscard]] constexpr std::string_view NameOf(Cheat cheat) noexcept
{
	const auto index = static_cast<size_t>(cheat);
	return index < k_CheatCount ? k_CheatNames.at(index) : std::string_view {"unknown cheat"};
}

/// The bit of a cheat in the set of the cheats of a player
[[nodiscard]] constexpr uint32_t BitOf(Cheat cheat) noexcept
{
	return uint32_t {1} << static_cast<uint32_t>(cheat);
}

/// Whether the set of the cheats of a player has the cheat
[[nodiscard]] constexpr bool Has(uint32_t cheats, Cheat cheat) noexcept
{
	return (cheats & BitOf(cheat)) != 0;
}

/// The set of the cheats of a player after a cheat goes on or off
[[nodiscard]] constexpr uint32_t With(uint32_t cheats, Cheat cheat, bool on) noexcept
{
	return on ? (cheats | BitOf(cheat)) : (cheats & ~BitOf(cheat));
}

/// The flags of a worship site that a cheat of its player forces on
struct SiteFlags
{
	/// The site never runs dry
	bool infinite {false};
	/// The upkeep of the miracles of the site costs nothing
	bool freeMaintenance {false};

	friend bool operator==(const SiteFlags&, const SiteFlags&) = default;
};

/// The flags of a worship site while its player has the cheats: the infinite miracle charge forces both flags on. Without
/// the cheat the site has the flags that it had before the cheat.
[[nodiscard]] constexpr SiteFlags SiteFlagsWith(uint32_t cheats, SiteFlags own) noexcept
{
	if (Has(cheats, Cheat::InfiniteMiracleCharge))
	{
		return {.infinite = true, .freeMaintenance = true};
	}
	return own;
}

} // namespace openblack::debug_cheats
