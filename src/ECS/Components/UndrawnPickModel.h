/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <entt/core/fwd.hpp>
#include <entt/entity/entity.hpp>

namespace openblack::ecs::components
{

/// A thing the cursor picks by a model that is never drawn, such as the way into a temple. Its model's triangles are
/// tested where the thing's own transform puts them, each frame the thing it goes with is drawn, just before that thing
/// is tested, so on a tie of distance the undrawn model keeps the pick. Nothing is tested while it doesn't take the
/// cursor.
struct UndrawnPickModel
{
	/// The model whose triangles are tested
	entt::id_type mesh;
	/// The drawn thing it is tested with: only on frames that thing is drawn, and just before it
	entt::entity drawnWith {entt::null};
	/// Whether it takes the cursor now
	bool takesCursor {false};
};

} // namespace openblack::ecs::components
