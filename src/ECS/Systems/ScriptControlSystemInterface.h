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

namespace openblack
{
class Camera;
class ScriptCameraModel;
} // namespace openblack

namespace openblack::ecs::systems
{

/// Which script task has the camera and which has the game's speed. A task takes the camera for a cut scene: its model
/// replaces the player's, the script moves it, and the player gets a camera of their own back from where it is when the
/// task is done or stops. Inside the temple the temple's own scripts may take control without the camera changing.
class ScriptControlSystemInterface
{
public:
	using GroundHeight = std::function<float(float x, float z)>;

	/// What a task taking the camera is and where
	struct CameraRequest
	{
		uint32_t task;
		/// The task runs one of the temple's scripts, which alone take control inside the temple
		bool templeScript;
		/// The player is inside the temple
		bool insideTemple;
	};

	virtual ~ScriptControlSystemInterface() = default;

	/// A task asks for the camera: false when another task's camera can't be left, or inside the temple for any but its
	/// own scripts
	virtual bool StartCameraControl(Camera& camera, const CameraRequest& request, GroundHeight groundAt) = 0;
	/// The task with the camera gives it back; others are ignored. True when it was given back
	virtual bool EndCameraControl(Camera& camera, uint32_t task) = 0;
	/// The task with the camera, 0 for none
	[[nodiscard]] virtual uint32_t GetCameraOwner() const = 0;
	/// The script's camera, when it is the camera's model
	[[nodiscard]] virtual ScriptCameraModel* GetScriptCamera(Camera& camera) const = 0;

	/// A task takes the game's speed, unless another has it
	virtual void StartGameSpeed(uint32_t task) = 0;
	/// The game's speed is given back, by the task with it or when nobody has it; true when it goes back to normal
	virtual bool EndGameSpeed(uint32_t task) = 0;
	/// Whether the task may set the game's speed: only the one with it
	[[nodiscard]] virtual bool MaySetGameSpeed(uint32_t task) const = 0;

	/// What a stopped task gave back
	struct Released
	{
		bool camera {false};
		/// The game's speed goes back to normal
		bool gameSpeed {false};
	};

	/// A task has stopped: the camera goes back to the player and the game's speed to normal, for whichever it had
	virtual Released TaskStopped(Camera& camera, uint32_t task) = 0;

	/// As a new land opens: nobody has the camera or the speed
	virtual void Reset() = 0;
};

} // namespace openblack::ecs::systems
