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

#include <optional>
#include <span>
#include <vector>

#include <entt/entity/entity.hpp>

/// The things the cursor picks by a model that is never drawn: where each is tested among the frame's drawn things,
/// and when the way into a temple takes the cursor
namespace openblack::ecs::undrawn_picks
{

/// An undrawn model that takes the cursor now, and the drawn thing it is tested with
struct Shape
{
	entt::entity entity {entt::null};
	entt::entity drawnWith {entt::null};
};

/// One test of the cursor pick, in order: a drawn thing (its place in the frame's drawn list), or an undrawn model
struct Turn
{
	std::size_t drawnIndex {0};
	std::optional<entt::entity> undrawn;
};

/// The order the pick tests things in: the drawn things in the order they are drawn, each undrawn model just before the
/// drawn thing it goes with. An undrawn model whose thing isn't drawn this frame isn't tested.
void TestOrder(std::span<const entt::entity> drawn, std::span<const Shape> shapes, std::vector<Turn>& order);

/// Whether a player's temple entrances take the cursor: from the first turn a temple of theirs stands fully built on
/// the land, for as long as the land lasts, whatever happens to that temple afterwards
[[nodiscard]] constexpr bool EntranceTakesCursor(bool alreadyTaking, float templeBuilt)
{
	return alreadyTaking || templeBuilt >= 1.0f;
}

} // namespace openblack::ecs::undrawn_picks
