/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <entt/fwd.hpp>

#include "Creature/CreatureMindFileBody.h"

namespace openblack::ecs
{
class Registry;
}

/// A creature's body as its file keeps it, taken from the creature as it is and given to a creature made from the file
namespace openblack::ecs::creature_body_file
{

/// The creature's body as it is now: its alignment, strength, fatness, size, tattoos and marks, and how its body is
/// doing once its body has been looked after. Its species and name aren't taken.
[[nodiscard]] creature_mind_body::Body Capture(const Registry& registry, entt::entity creature);

/// A creature takes up the body a file kept: its alignment, strength, fatness and size, the fatness its body shows, its
/// tattoos and marks, and how its body was doing, which its body starts from
void Apply(Registry& registry, entt::entity creature, const creature_mind_body::Body& body);

} // namespace openblack::ecs::creature_body_file
