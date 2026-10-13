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

#include <entt/entity/entity.hpp>
#include <glm/vec3.hpp>

#include "Help/HelpQuery.h"

/// What the Help key sees of the world: a thing's help texts and who it is, and the ground under the hand
namespace openblack::ecs::help_query_facts
{

/// A thing's help texts, by its kind and what it is doing; none (a first text of 0) for things the help has nothing
/// to say about
[[nodiscard]] help_query::Texts TextsOf(entt::entity thing);

/// What the help sees of a thing: its texts, and who it is for a person, a wonder, a town's centre or a town's need
[[nodiscard]] help_query::Thing ThingOf(entt::entity thing);

/// What the help sees of the ground at a point of the land or sea; none for no point
[[nodiscard]] help_query::Ground GroundAt(std::optional<glm::vec3> point);

} // namespace openblack::ecs::help_query_facts
