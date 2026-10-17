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

#include <entt/core/fwd.hpp>
#include <entt/entity/fwd.hpp>

namespace openblack::ecs
{
class Registry;
} // namespace openblack::ecs

/// The model that gives a thing its size and shape on the map. Usually this is the model that the game shows. A worship
/// site's altar has its model from when the game makes the site, also while the game does not show the altar.
namespace openblack::ecs::thing_model
{

/// The model of a thing for its size and shape, none for a thing without a model
[[nodiscard]] std::optional<entt::id_type> ModelOf(const Registry& registry, entt::entity thing);

} // namespace openblack::ecs::thing_model
