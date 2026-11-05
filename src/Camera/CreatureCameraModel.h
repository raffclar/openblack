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

#include <optional>

#include "CameraModel.h"
#include "CreatureFollow.h"

namespace openblack
{

/// Creature Mode's camera: locked onto a creature, it follows it round the land, easing after it. The follow camera of a
/// saved game of the original game can also follow a thing that is not a creature. The player turns it
/// with the keys (see creature_follow), and the cursor keys alone give the camera back. Creature Mode takes the camera
/// over from the player's own camera with it and hands it back afterwards; it tells it each frame where the creature is.
class CreatureCameraModel final: public CameraModel
{
public:
	/// Starts on a creature, by its middle and height, from where the camera is
	CreatureCameraModel(glm::vec3 cameraOrigin, glm::vec3 cameraFocus, glm::vec3 creature, float height,
	                    creature_follow::StartDistance start = creature_follow::StartDistance::KeepWhenClose);
	/// Starts on a creature from a camera that glides: the angles come from the points that it glides to
	CreatureCameraModel(const creature_follow::StartPoints& camera, glm::vec3 creature, float height,
	                    creature_follow::StartDistance start);
	/// Starts on a creature with the view and values of a saved game of the original game
	CreatureCameraModel(const creature_follow::Loaded& loaded, glm::vec3 creature, float height);

	/// Where the thing now is, and how tall it is. A thing that walks also gives its game angle, which a camera that
	/// turns with its thing reads.
	void SetTarget(glm::vec3 position, float height, std::optional<uint16_t> gameAngle = std::nullopt);
	/// Where the thing now is, as the camera reads it. On a thing with no model, the offsets give only the distance.
	void SetTarget(const creature_follow::Target& target);
	/// Asks for a swing round and a tilt for a clear view of the creature, as Ctrl and Shift do together. The camera
	/// swings in its next update.
	void ClearView(const camera_limits::Ground& ground);
	/// Turns it by keys, as if held this frame, on a screen so many pixels wide; returns whether they give the camera
	/// back
	creature_follow::KeyOutcome Turn(const creature_follow::Keys& keys, float screenWidth);

	[[nodiscard]] const creature_follow::View& GetView() const { return _view; }
	/// The share of the follow's own glide time that the glides take
	[[nodiscard]] float GetTimeScale() const { return _timeScale; }
	/// Whether Ctrl and Shift together swing the camera clear
	[[nodiscard]] bool AutoRepositions() const { return _autoReposition; }
	[[nodiscard]] float GetSecondsInMode() const { return _seconds; }
	/// Whether the heading turns with the thing
	[[nodiscard]] bool TurnsWithThing() const { return _relativeHeading; }
	/// The offsets of the camera from its thing, until the cursor keys stop them
	[[nodiscard]] const std::optional<creature_follow::Offsets>& GetOffsets() const { return _offsets; }
	/// Whether the player has asked for their camera back with the cursor keys
	[[nodiscard]] bool WantsToLeave() const { return _leaveRequested; }
	/// Whether it gave the camera back because it would have gone outside the fence: the player's camera then starts
	/// afresh from where it stopped
	[[nodiscard]] bool StoppedAtFence() const { return _stoppedAtFence; }

	std::optional<CameraInterpolationUpdateInfo> Update(std::chrono::microseconds dt, const Camera& camera) override;
	/// The mouse does not move this camera
	void HandleActions(std::chrono::microseconds /*dt*/) override {}
	/// Reads the keys after the step of the camera: the cursor keys with Shift or Ctrl, the wheel, and Ctrl and Shift
	/// together for a clear view. They move the camera in the next frame.
	void HandleKeys(std::chrono::microseconds dt) override;
	void SetFlight(glm::vec3 origin, glm::vec3 focus, float rise) override;
	[[nodiscard]] glm::vec3 GetTargetOrigin() const override { return Aim().origin; }
	[[nodiscard]] glm::vec3 GetTargetFocus() const override { return Aim().focus; }

private:
	struct Points
	{
		glm::vec3 origin;
		glm::vec3 focus;
	};
	/// Where the camera wants to stand and look: at its offsets from the place of the thing, or else at its view of the
	/// middle of the thing, with the heading turned with the thing when it must turn
	[[nodiscard]] Points Aim() const;
	/// Whether the offsets keep the eye and the point that the camera looks at on the thing
	[[nodiscard]] bool KeepsOffsets() const;

	creature_follow::View _view;
	glm::vec3 _focus;
	/// The place of the thing, with no height added
	glm::vec3 _position;
	std::optional<uint16_t> _gameAngle;
	bool _relativeHeading {false};
	std::optional<creature_follow::Offsets> _offsets;
	/// Whether the thing has a model in the world, which the offsets keep the camera on
	bool _thingHasModel {true};
	float _seconds {0.0f};
	float _timeScale {1.0f};
	bool _autoReposition {true};
	bool _leaveRequested {false};
	bool _stoppedAtFence {false};
	/// The land for the clear view swing that the next update does
	std::optional<camera_limits::Ground> _clearView;
};

} // namespace openblack
