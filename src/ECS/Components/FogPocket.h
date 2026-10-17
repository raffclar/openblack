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

#include <glm/vec3.hpp>

namespace openblack::ecs::components
{

/// EXPERIMENT (not for merge): a pocket of fog in the volumetric fog, as a ball or a box, for example over a swamp at
/// dawn. Its density is full inside and goes to none over a soft edge.
struct FogPocket
{
	enum class Shape : uint8_t
	{
		Ball,
		Box,
	};

	Shape shape {Shape::Box};
	/// Its middle in the world
	glm::vec3 centre {0.0f};
	/// Half of its size along each axis; a ball's radius is x
	glm::vec3 halfSize {10.0f};
	/// The fog for each world unit inside it
	float density {0.02f};
	/// How far in from its surface the density goes from none to full, in world units
	float edge {8.0f};
	/// Whether it is thickest round dawn and goes as the morning warms, as the ground fog does; else it is there always
	bool atDawn {true};
};

} // namespace openblack::ecs::components
