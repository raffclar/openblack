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
#include <cstdint>

#include <optional>

#include <entt/entity/fwd.hpp>
#include <glm/vec2.hpp>

/// Towns' crèches in the world: which building a town sends its children to, and the paths marked out in its model for
/// them to stroll along. The rules are creche::'s (CrecheRules.h).
namespace openblack::creches
{

/// A building is made: a crèche standing built works at once
void Made(entt::entity building);

/// A crèche starts working (built, or finished by its builders): its town takes it as its own when it has none
void StartsWorking(entt::entity building);

/// A building goes. A crèche that was its town's own hands over to the first other working civic building of the town,
/// and any crèche going leaves its town forgetting the one it had. A building standing in as the town's crèche going
/// leaves the town with none.
void Lost(entt::entity building);

/// The town's crèche, none when it has none
[[nodiscard]] entt::entity CrecheOf(entt::entity town);

/// How many paths the building's model marks out for children to stroll along
[[nodiscard]] uint32_t PathsOf(entt::entity building);

/// A point of the building's paths on the land, none past its points
[[nodiscard]] std::optional<glm::vec2> PathPoint(entt::entity building, size_t point);

} // namespace openblack::creches
