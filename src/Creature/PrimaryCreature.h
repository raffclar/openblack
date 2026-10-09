/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <algorithm>
#include <optional>
#include <span>
#include <vector>

#include <entt/entity/entity.hpp>

/// Which of a player's creatures is their primary one: the earliest they got that is still theirs
namespace openblack::primary_creature
{

/// Remembers that a player got a creature, after those they got before; a creature already listed keeps its place
void Acquire(std::vector<entt::entity>& acquired, entt::entity creature);

/// The earliest creature got that is still the player's, by a test of whether a creature still exists and is theirs
template <typename StillOwned>
[[nodiscard]] std::optional<entt::entity> Primary(std::span<const entt::entity> acquired, StillOwned&& stillOwned)
{
	const auto found = std::ranges::find_if(acquired, stillOwned);
	return found != acquired.end() ? std::optional(*found) : std::nullopt;
}

} // namespace openblack::primary_creature
