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

#include <glm/ext/vector_uint4_sized.hpp>
#include <glm/vec3.hpp>

#include "Graphics/LightBeams.h"

/// The white light at the main room's way out. When the player goes through the door out of the temple, a sheet of
/// white light grows on the floor from the door towards the middle of the room. It is added over all of the room, with
/// no depth test, in the atmosphere's texture, and its texture drifts as the room's spot lights' textures do.
namespace openblack::graphics::temple_exit_glow
{

/// The glow starts this long after the camera starts its walk to the door
inline constexpr double k_StartSeconds = 2.2;
/// The door out of the temple is the third of the main room's eight doors, a quarter turn round from the first
inline constexpr int32_t k_ExitDoor = 2;

/// The sheet of light between two points on the floor of the room, in the room's own space
struct Sheet
{
	/// The sheet's near end, at the door, and its far end, towards the middle of the room
	glm::vec3 near {0.0f};
	glm::vec3 far {0.0f};
	/// How high the sheet rises at its near end and at its far end
	float nearHeight {0.0f};
	float farHeight {0.0f};
	/// How wide the sheet is at each end
	float nearWidth {0.0f};
	float farWidth {0.0f};
	/// The colour at the near end; the far end has none
	glm::u8vec4 colour {0};
};

/// The sheet of light a time after the camera starts its walk through the door out, none before it shows
[[nodiscard]] std::optional<Sheet> At(float stateSeconds);

/// The sheet's 32 points and 34 triangles, the texture's coordinates drifting by phase as the room's cones' do
void Append(const Sheet& sheet, float phase, BeamMesh& mesh);

} // namespace openblack::graphics::temple_exit_glow
