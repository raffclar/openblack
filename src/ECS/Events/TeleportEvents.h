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

namespace openblack::ecs::events
{
/// A thing was put somewhere outside its own turn (a script, the hand, the end of physics). Its position at the start
/// of the turn becomes the new one, so it is drawn there from the next frame instead of sliding from where it was.
/// Moves made in the thing's own turn are not teleports: they are drawn between the two turn positions
struct Teleported
{
	entt::entity thing {entt::null};
};
} // namespace openblack::ecs::events
