/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <optional>
#include <string>

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
/// doing once its body has been looked after. It also has the values that the body got when the game made it, the
/// environment map word and the counts of its life. Its species and name aren't taken.
[[nodiscard]] creature_mind_body::Body Capture(const Registry& registry, entt::entity creature);

/// A creature takes up the body a file kept: its alignment, strength, fatness and size, the fatness its body shows, its
/// tattoos and marks, how its body was doing, which its body starts from, the values that the body got when the game
/// made it, the environment map word and the counts of its life. While a nice or nasty spell is on the creature,
/// the alignment of the file goes to the value that the spell puts back as it ends, and the alignment now stays.
/// The pending alignment stays as it is.
void Apply(Registry& registry, entt::entity creature, const creature_mind_body::Body& body);

/// What a body keeps for itself in a swap of minds
struct OwnBody
{
	std::u16string name;
	float size {1.0f};
	float alignment {0.0f};
	float pendingAlignment {0.0f};
};

/// A body takes up the other mind in a swap of minds. The steps are:
/// 1. The body takes up the body of the other mind's file, with the alignment of that mind. Thus the load paints the
///    skin and the hair with the alignment of the other mind.
/// 2. The body gets its own name, size and alignment back.
/// 3. The body takes its looks once more and does one morph step. The step paints the skin and the hair again only when
///    the evil to good target is far enough from the drawn axis.
/// Without a body in the file, only steps 2 and 3 occur.
void TakeUpSwappedMind(Registry& registry, entt::entity creature, const std::optional<creature_mind_body::Body>& body,
                       const OwnBody& own);

} // namespace openblack::ecs::creature_body_file
