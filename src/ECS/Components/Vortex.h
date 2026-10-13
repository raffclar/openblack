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

#include <array>
#include <string_view>
#include <vector>

#include <entt/core/hashed_string.hpp>
#include <entt/entity/entity.hpp>
#include <glm/vec3.hpp>

#include "Enums.h"

namespace openblack::ecs::components
{

/// A swirling vortex between the lands: the way out of a land, the way into the next, or the volcano's glowing mouth
struct Vortex
{
	/// The textures, each with its alpha beside it, of the marks a vortex leaves on the ground: the one whose alpha opens
	/// its hole in the land and the ring laid around it
	struct GroundTextures
	{
		entt::id_type hole;
		entt::id_type ring;
		std::string_view holeFile;
		std::string_view ringFile;
	};
	/// The land vortices share theirs; the volcano's mouth has its own
	static constexpr std::array<GroundTextures, 2> k_GroundTextures {{
	    {entt::hashed_string("vortex/land_hole").value(), entt::hashed_string("vortex/land_ring").value(),
	     "S_VortexBaseAlphacopy", "S_VortexBaseMultiRing"},
	    {entt::hashed_string("vortex/volcano_hole").value(), entt::hashed_string("vortex/volcano_ring").value(),
	     "S_Volcano_Base_Alpha", "S_Volcano_Base"},
	}};
	[[nodiscard]] static constexpr const GroundTextures& GroundTexturesOf(VortexType type)
	{
		return k_GroundTextures.at(type == VortexType::Volcano ? 1 : 0);
	}

	/// What a script tells a vortex that leads into the land: the town those who come out join (none: the nearest town
	/// when it starts throwing), and where the animals that come out gather, as one flock kept within a distance and a
	/// radius, and the flock the people who come out follow, if any. Until told, the animals gather at the vortex
	struct Arrival
	{
		entt::entity town {entt::null};
		glm::vec3 gatheringPlace {0.0f};
		float flockDistance {0.0f};
		float flockRadius {0.0f};
		entt::entity flock {entt::null};
	};

	VortexType type {VortexType::In};
	VortexStateType state {VortexStateType::FadeIn};
	/// The game turn its state began on
	uint32_t stateStartTurn {0};
	/// Its middle: where it was made, on the land
	glm::vec3 centre {0.0f};
	/// How high above the land it was made
	float altitude {0.0f};
	/// How far it has levelled the ground so far; below 0 before it first does
	float levelApplied {-1.0f};
	/// The levelled square's heights as they were when it first levelled them, x major, and their average
	std::vector<uint8_t> groundHeights;
	float groundAverage {0.0f};
	/// Its particle effects (0 for none): the swirl drawn before the land, the effect drawn after it, what carries
	/// things in, and the glow it lays on the ground
	uint32_t beforeLandEffect {0};
	uint32_t afterLandEffect {0};
	uint32_t objectMoverEffect {0};
	uint32_t lightMapEffect {0};
	/// Only a vortex that leads into the land keeps one
	Arrival arrival;
};

/// A thing a vortex that leads away has handed to its effect this turn, to be drawn in once the effect next steps
struct DrawnIntoVortex
{
	entt::entity vortex {entt::null};
};

} // namespace openblack::ecs::components
