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

/// Whether a building of this kind goes up as a miracle dispenser, when a plan or a scaffold makes it
[[nodiscard]] constexpr bool MadeAsDispenser(AbodeType type)
{
	return type == AbodeType::SpellDispenser;
}

} // namespace openblack::ecs::building_replan
