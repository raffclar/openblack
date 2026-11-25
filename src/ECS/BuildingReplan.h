/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <cstdint>

#include <optional>

#include <entt/entity/entity.hpp>
#include <glm/vec3.hpp>

#include "Enums.h"

namespace openblack::ecs
{
class Registry;
}

/// The plan that a town makes when one of its buildings goes from the land. A miracle dispenser is one of the buildings
/// of its town, so its town plans it again as it plans a house.
namespace openblack::ecs::building_replan
{

/// What the plan of a building that goes keeps from the building
struct Replan
{
	/// The number of the town that plans it
	uint32_t townId {0};
	/// The row of the building in the buildings' table
	AbodeInfo info {AbodeInfo::None};
	/// Where the building stood
	glm::vec3 position {0.0f};
	/// How far round the building was turned, in radians
	float yAngle {0.0f};
	float scale {1.0f};
	/// The building was finished before it went
	bool wasBuilt {false};
};

/// The plan that the town of a building makes when the building goes. There is no plan for a thing that is not a
/// building of a valid town, for a building with no row, and for a building that its town must not plan again.
[[nodiscard]] std::optional<Replan> Of(const Registry& registry, entt::entity building);

/// Whether a plan or a building of a town takes the place of a building that went: it is of the same town and row, and it
/// stands at the same place across the ground
[[nodiscard]] bool TakesPlaceOf(const Replan& gone, uint32_t townId, AbodeInfo info, glm::vec3 position);

/// The plan that the town of a building made when a building of the same row went from the same place. The editor makes a
/// building again there when it undoes a removal. entt::null when the town has no such plan.
[[nodiscard]] entt::entity PlanLeftAt(const Registry& registry, entt::entity building);

/// Whether a building of this kind goes up as a miracle dispenser, when a plan or a scaffold makes it
[[nodiscard]] constexpr bool MadeAsDispenser(AbodeType type)
{
	return type == AbodeType::SpellDispenser;
}

} // namespace openblack::ecs::building_replan
