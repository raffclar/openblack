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

#include "Physics/ObjectRules.h"

namespace openblack::ecs
{
class Registry;
}

/// The roots drawn under a tree that is out of the land: pulled at by the hand, held, carried off or moving in the
/// physics. A tree standing in the land has its roots hidden in the ground and none are drawn.
namespace openblack::ecs::tree_roots
{

/// Where a tree is, for its roots: out of the land while a hand pulls at it, holds it, a creature holds it or a tornado
/// carries it; moving or still while it is in the physics; in the land otherwise
[[nodiscard]] physics::objects::TreePlace PlaceOf(const Registry& registry, entt::entity tree);

/// Has every tree out of the land drawn with its roots this frame, and no other, as each now is
void Show(Registry& registry);

} // namespace openblack::ecs::tree_roots
