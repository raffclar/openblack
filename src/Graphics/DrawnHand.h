/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <entt/entity/entity.hpp>

/// Which hands the frame draws. The game draws one hand only: the left hand of the god of this computer. The hands of the
/// other gods, of the computer gods and the right hands have no model on the screen. Another god shows only as the
/// sparkles at its camera and the thing that its hand holds.
namespace openblack::graphics::drawn_hand
{

/// Whether the frame draws an entity. It draws each entity that is not a hand, and of the hands only the drawn hand.
[[nodiscard]] constexpr bool Drawn(bool isHand, entt::entity entity, entt::entity drawnHand) noexcept
{
	return !isHand || (entity != entt::null && entity == drawnHand);
}

} // namespace openblack::graphics::drawn_hand
