/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <glm/mat3x3.hpp>
#include <glm/vec3.hpp>

namespace openblack::ecs::components
{

/// Where a villager or animal is drawn this frame (ECS/MobileDrawing.h), instead of its Transform: between its
/// positions at the start and the end of the last turn, turned smoothly, sheared along the slope. Only for drawing.
struct DrawPosition
{
	/// The position at the start of the last processed turn (each living's turn copies its position into it)
	glm::vec3 turnStart {0.0f};
	bool started {false};
	/// The drawn yaw, turning towards the real one every frame
	float yaw {0.0f};
	bool hasYaw {false};
	/// this frame's result
	glm::vec3 position {0.0f};
	glm::mat3 rotation {1.0f};
	/// The slope shear, the land's rise along the object's x and z axes (row0 += a row1, row2 += b row1)
	float shearX {0.0f};
	float shearZ {0.0f};
	/// The SuperVillager's second yaw stage (ECS/SuperVillager.h): with followRate > 0 (rad/s, 3.92699) followYaw
	/// chases the object's yaw (the drawn yaw in the object placement convention, `yaw` + 90 degrees) and the body is
	/// drawn turned about its own Y by their difference, followDrawnTurn (on a local copy of the sheared object
	/// matrix: ecs::DrawnBodyModel; `rotation` is not turned). followSnap snaps this stage (keeps them equal) and the
	/// cross-fade (not drawn, ECS/Animations.h); followTurn false (a swimmer) moves followYaw but does not turn;
	/// followFrozen (off screen) moves nothing. followYaw is set when the SuperVillager is made (hasFollowYaw). Set by
	/// ECS/SuperVillager
	float followRate {0.0f};
	bool followSnap {false};
	bool followTurn {true};
	bool followFrozen {false};
	float followYaw {0.0f};
	bool hasFollowYaw {false};
	/// this frame's turn of the SuperVillager's drawn copy (followYaw - Wrap(target)), 0 for none
	float followDrawnTurn {0.0f};
};

} // namespace openblack::ecs::components
