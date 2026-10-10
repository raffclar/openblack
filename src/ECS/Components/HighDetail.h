/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <array>
#include <optional>

#include <entt/core/fwd.hpp>
#include <glm/vec3.hpp>

#include "ECS/HighDetailRules.h"
#include "ECS/VillagerEyes.h"

namespace openblack::ecs::components
{

/// A villager a script draws in high detail for its cinema
struct HighDetail
{
	/// The model it wears when it is drawn as usual again, when it changed into a detailed one, and its models at the
	/// three distances
	std::optional<entt::id_type> usualModel;
	std::optional<std::array<entt::id_type, 3>> usualDetailModels;
	/// The face its eyes are drawn with, when it wears a detailed model
	std::optional<high_detail_rules::Face> face;
	high_detail_rules::DrawOrders orders;
	/// Drawn here, facing as it does, rather than where it stands: while the opening's hand holds it
	std::optional<glm::vec3> heldAt;
	/// Its eyes, when its detailed model has places for them: only the opening's family has
	std::optional<villager_eyes::Eyes> eyes;
	/// Where its eyes are drawn this frame, while it is in view
	std::optional<villager_eyes::DrawnEyes> drawnEyes;
};

} // namespace openblack::ecs::components
