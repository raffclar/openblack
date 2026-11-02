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

#include <chrono>
#include <memory>

#include <entt/fwd.hpp>
#include <glm/mat4x4.hpp>

#include "ECS/Systems/SavedStateInterface.h"

namespace openblack
{
class CameraPath;
}

namespace openblack::particles
{
struct CameraPathLink;
}

namespace openblack::ecs::systems
{

class CameraPathSystemInterface: public SavedStateInterface
{
public:
	virtual void Start(entt::id_type id) = 0;
	virtual void Stop() = 0;
	virtual void Play() = 0;
	virtual void Pause() = 0;
	/// Once a frame. The camera glides by the camera's time of the frame (dt). A placed path takes its time from the frame
	/// of its atom's animation that the last draw showed: while the game is paused that frame stays, so the camera glides
	/// to where the path stopped and stays there.
	virtual void Update(const std::chrono::microseconds& dt, std::chrono::milliseconds frameGameTime) = 0;
	virtual bool IsPathing() = 0;
	virtual bool IsPaused() = 0;
	/// The camera follows a path placed in the world by a matrix, as a miracle's effect takes its caster's camera along:
	/// from where it is it glides onto the path's start over the pause and a little more, then follows the path a little
	/// behind, at the time that the atom's animation frame gives (see particles::CameraPathLink), until the animation
	/// comes to its end or the atom goes. Meanwhile the player's other camera controls do nothing, while the hand stays
	/// free, drawn and following the mouse: the path never hides it
	virtual void FollowPlaced(std::unique_ptr<CameraPath> path, const glm::mat4& placement, float pauseSeconds,
	                          const std::shared_ptr<particles::CameraPathLink>& link) = 0;
	/// Once a frame, as the world is drawn: the draw shows the frame of the atom's animation between its last two steps,
	/// by the fraction of the game turn that has gone by. The path takes its time from that frame in the next frame, and
	/// the last frame ends the path.
	virtual void ShowPlaced(float turnFraction) = 0;
	/// Once a game turn, at its end: a placed path whose animation came to its end, or whose atom has gone, lets go of the
	/// camera, and the player's camera flies back to where it was going
	virtual void CheckPlaced() = 0;
	/// Whether a placed path has the camera, so that the player's camera controls are left alone
	[[nodiscard]] virtual bool HoldsCamera() const = 0;
	/// What the player is doing that may take the camera back from a placed path: a key moving the camera left, right,
	/// forwards or backwards held through a frame of so many whole milliseconds, or the hand gripping the land to drag
	/// it. Rotating, tilting and zooming don't (see Camera/CameraPathControl.h)
	struct PlayerControl
	{
		bool movementKey {false};
		uint32_t frameMilliseconds {0};
		bool grippingLand {false};
	};
	/// Once a frame, before the camera handles the player's controls: a movement key that would move the camera this
	/// frame, or the hand gripping the land, gives the camera back to the player at once, from where it is
	virtual void HandlePlayerControl(const PlayerControl& control) = 0;
};
} // namespace openblack::ecs::systems
