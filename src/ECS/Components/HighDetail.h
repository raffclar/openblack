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

#include "ECS/HighDetailRules.h"

namespace openblack::ecs::components
{

/// A villager a script draws in high detail for its cinema
struct HighDetail
{
	/// The model it wears when it is drawn as usual again, when it changed into a detailed one
	std::optional<entt::id_type> usualModel;
	/// The face its eyes are drawn with, when it wears a detailed model
	std::optional<high_detail_rules::Face> face;
	high_detail_rules::DrawOrders orders;
};

} // namespace openblack::ecs::components
