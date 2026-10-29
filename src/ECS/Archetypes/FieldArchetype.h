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
#include <glm/fwd.hpp>

#include "3D/FieldCrop.h"
#include "Enums.h"

namespace openblack::ecs::archetypes
{
class FieldArchetype
{
public:
	static entt::entity Create(int townId, const glm::vec3& position, FieldTypeInfo type, float yAngleRadians);
	/// A field of a saved game: its building row, its crop and its turn of growth come from the file, with no random
	/// draw
	static entt::entity CreateLoaded(int townId, const glm::vec3& position, AbodeInfo row, FieldTypeInfo type,
	                                 float yAngleRadians, float scale, float built, const field_crop::Crop& crop,
	                                 uint32_t growthTurn);
	FieldArchetype() = delete;
};
} // namespace openblack::ecs::archetypes
