/*******************************************************************************
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
#include <vector>

#include <entt/entity/entity.hpp>
#include <glm/vec2.hpp>

#include "Animals/FishShoal.h"

namespace openblack::ecs::components
{

/// A stretch of sea a town fishes (see fish_farm), and the shoal that shows its fish (see fish_shoal)
struct FishFarm
{
	/// The town it belongs to, and so its player; none when there was no town
	entt::entity town {entt::null};
	/// Its place on the map, x and z in 1/65536 of a cell: the middle of its cell
	glm::ivec2 place {0};
	/// The fish in it
	float fish {0.0f};
	/// The villagers fishing it
	std::vector<entt::entity> fishermen;
	/// Its shoal, none where there was no open sea near it
	std::optional<fish_shoal::Shoal> shoal;
	/// How opaque its shoal is drawn this frame, none while it is too far from the camera to show
	std::optional<uint8_t> shownAlpha;
};

} // namespace openblack::ecs::components
