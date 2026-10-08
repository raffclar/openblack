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
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include <entt/entity/entity.hpp>
#include <glm/vec3.hpp>

#include "Enums.h"

// What the Vortices debug window works out before it acts: the names of the vortex types and states, the rows of the
// vortices on the land, and where each land's script opens the vortex that leads to the next land. Pure functions and
// small value types, tested with hand-made data.

namespace openblack::debug::vortices
{

/// The types the window can create, in the game's order
inline constexpr std::array k_Types {VortexType::In, VortexType::Out, VortexType::Volcano};

/// How a vortex type reads in the window
[[nodiscard]] std::string_view TypeName(VortexType type);

/// How a vortex state reads in the window
[[nodiscard]] std::string_view StateName(VortexStateType state);

/// A vortex on the land, as the window reads it
struct VortexAt
{
	entt::entity entity {entt::null};
	VortexType type {VortexType::In};
	VortexStateType state {VortexStateType::Inactive};
	glm::vec3 position {0.0f};
};

/// One vortex as the window lists it
struct Row
{
	entt::entity entity {entt::null};
	std::string_view type;
	std::string_view state;
	glm::vec3 position {0.0f};
	/// It is not fading out already
	bool canFadeOut {false};
};

/// The rows of the vortices, oldest entity first, so the list keeps its order from frame to frame
[[nodiscard]] std::vector<Row> Rows(std::span<const VortexAt> vortices);

/// What became of a vortex the window asked for
[[nodiscard]] std::string CreatedMessage(VortexType type, glm::vec3 position, entt::entity made);

/// Where a land's script opens the vortex that takes the player to the next land
struct ExitVortex
{
	int land {0};
	glm::vec3 position {0.0f};
};

/// The script opens that vortex as an In
inline constexpr VortexType k_ExitVortexType = VortexType::In;

/// The places the scripts of Lands 1, 2 and 4 give for it; Land 3's script is handed its place when it starts, so it
/// has none of its own
inline constexpr std::array k_ExitVortices {
    ExitVortex {.land = 1, .position = {1700.227539f, 11.400500f, 2520.182861f}},
    ExitVortex {.land = 2, .position = {1061.107056f, 147.785004f, 3597.965088f}},
    ExitVortex {.land = 4, .position = {2060.179932f, 11.390000f, 2591.697998f}},
};

} // namespace openblack::debug::vortices
