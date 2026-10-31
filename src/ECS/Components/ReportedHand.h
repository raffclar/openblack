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

#include <bitset>

#include <entt/entity/entity.hpp>
#include <glm/vec3.hpp>

namespace openblack::ecs::components
{

/// Where a player's hand and camera are, as the last report that a turn did tells it (player_commands::HandReport). The
/// world reads the hand and the camera of a player only from here, never from the hand that the frames draw. All the
/// values are zero until the first report.
struct ReportedHand
{
	/// The point under the cursor that the interface picks
	glm::vec3 collidePoint {0.0f};
	/// The place of the hand
	glm::vec3 handPoint {0.0f};
	/// The place of the camera
	glm::vec3 camera {0.0f};
	/// The point that the camera looks at
	glm::vec3 focus {0.0f};
	/// The thing under the cursor as the last pick that a turn did tells it (player_commands::HandPick). None until the
	/// first pick.
	entt::entity pickedObject {entt::null};
	/// The view of the player as the last view report that a turn did tells it (player_commands::ViewReport). False
	/// until the first view report.
	bool viewReported {false};
	/// The eye and the point that the camera looks at, as the frame draws them
	glm::vec3 viewEye {0.0f};
	glm::vec3 viewFocus {0.0f};
	/// The lens across a 4:3 screen, in radians, and the distance of the near plane
	float viewLens {0.0f};
	float viewNearClip {0.0f};
	/// The size of the window of the player in pixels, and the shape of the camera's picture
	uint16_t viewScreenWidth {0};
	uint16_t viewScreenHeight {0};
	float viewAspect {0.0f};
	/// How far the camera is from the fence of the camera zones
	float inclusionDistance {0.0f};
	/// How many camera orders of the scripts the camera has got, and whether it has arrived where it goes
	uint32_t cameraOrders {0};
	bool cameraArrived {false};
	/// The hand grips the land with the select button, and the other button is held too
	bool gripsLand {false};
	bool otherButton {false};
	/// The mouse has a wheel
	bool mouseWheel {false};
	/// The camera icons by the hand of the world camera's last frame, with their offered bits (hand_tricons::icon)
	uint16_t cameraIcons {0};
	/// The keys held, by the game's key number
	std::bitset<255> keysHeld;
};

} // namespace openblack::ecs::components
