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

#include <functional>
#include <vector>

#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

#include "Help/TipBubble.h"
#include "Temple/ScrollPictureLine.h"

/// The pictures of gestures and controls on the scroll that the camera is close to. The game then draws the scroll's
/// text in front of the scroll. Each line that names a gesture or a control gets its picture, flat on the screen where
/// the line shows. The world in front of a gesture's picture hides it. A control's picture goes over all of the world.
namespace openblack::temple::scroll_pictures
{

/// A point of the world on the screen
struct ScreenPoint
{
	enum class Seen : uint8_t
	{
		/// Nearer than the near plane, or behind the camera
		Behind,
		/// In front of the camera but outside the screen
		OffScreen,
		OnScreen,
	};
	Seen seen {Seen::Behind};
	/// Pixels from the top left of the screen
	glm::vec2 pixel {0.0f};
	/// The distance in front of the camera
	float depth {0.0f};
};
using ProjectFn = std::function<ScreenPoint(glm::vec3 point)>;

/// A gesture's picture and its distance in front of the camera, so that the world in front of it hides it
struct DepthGesture
{
	help::tip_bubble::GestureIcon icon;
	float depth {0.0f};
};

/// The pictures to draw this frame
struct Pictures
{
	/// Each shadow before its picture
	std::vector<DepthGesture> gestures;
	std::vector<help::tip_bubble::ControlIcon> controls;
};

/// The pictures go this far in front of the line. A gesture's shadow goes one more, and its picture two more.
constexpr float k_InFront = 0.7f;
/// The gesture's shadow is two pixels of the default screen down and to the right
constexpr float k_ShadowOffset = 2.0f;

/// Adds the picture of one line. It adds nothing unless these conditions are true:
/// - The two corners of the texture are in front of the camera.
/// - The top of the line and the point one line lower are on the screen.
/// The picture is two lines high on the screen, and the corners of the texture on the screen cut it. A gesture's
/// picture starts at the top of the line, and a control's picture one line lower. A control's picture shows only while
/// it is fully inside the texture and its action has a binding (`controlShown`). `pixelScale` is the number of pixels
/// of the screen for each pixel of the default screen.
void Place(Pictures& out, const WorldPicture& picture, const ProjectFn& project,
           const help::tip_bubble::ControlShownFn& controlShown, float pixelScale);

} // namespace openblack::temple::scroll_pictures
