/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <chrono>
#include <memory>
#include <optional>

#include <glm/mat4x4.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

#include "CameraModel.h"
#include "CameraZoomer.h"
#include "Common/ZoomInterpolator.h"
#include "DrawnPose.h"
#include "ECS/Components/Transform.h"
#include "KeyboardMoveSpeed.h"

namespace openblack
{

class Camera
{
public:
	enum class Interpolation : uint8_t
	{
		Current,
		Start,
		Target,
		/// The view that the camera kept after its last step, with the shake. The game drew this view, unless a command
		/// put the camera after the step. Before the first kept view, the view now.
		Drawn,
	};
	enum class Projection : uint8_t
	{
		Normal,
		ReversedZ,
	};
	explicit Camera(glm::vec3 focus = glm::vec3(1000.0f, 0.0f, 1000.0f));
	virtual ~Camera();

	/// The lens: the view across the width of a 4:3 screen, in radians, as the game sets it (70 degrees in the world)
	[[nodiscard]] float GetLens() const { return _xFov; }
	/// The view across the screen's width, in radians: the lens on 4:3 screens and narrower ones, wider on wider screens
	[[nodiscard]] float GetHorizontalFieldOfView() const;
	/// The view up and down the screen, in radians: a wider screen than 4:3 keeps the 4:3 screen's
	[[nodiscard]] float GetVerticalFieldOfView() const;
	[[nodiscard]] virtual glm::mat4 GetViewMatrix(Interpolation interpolation) const;
	[[nodiscard]] const glm::mat4& GetProjectionMatrix() const;
	[[nodiscard]] const glm::mat4& GetProjectionMatrix(Projection projection) const;
	[[nodiscard]] glm::mat4 GetViewProjectionMatrix(Interpolation interpolation = Camera::Interpolation::Current) const;
	[[nodiscard]] glm::mat4 GetViewProjectionMatrix(Projection projection,
	                                                Interpolation interpolation = Camera::Interpolation::Current) const;

	[[nodiscard]] std::optional<ecs::components::Transform>
	RaycastMouseToLand(bool includeWater = true, Interpolation interpolation = Camera::Interpolation::Current) const;
	[[nodiscard]] std::optional<ecs::components::Transform>
	RaycastScreenCoordToLand(glm::vec2 screenCoord, bool includeWater,
	                         Interpolation interpolation = Camera::Interpolation::Current) const;

	[[nodiscard]] glm::vec3 GetOrigin(Interpolation interpolation = Interpolation::Current) const;
	[[nodiscard]] glm::vec3 GetOriginVelocity(Interpolation interpolation = Interpolation::Current) const;
	[[nodiscard]] glm::vec3 GetFocus(Interpolation interpolation = Interpolation::Current) const;
	[[nodiscard]] glm::vec3 GetFocusVelocity(Interpolation interpolation = Interpolation::Current) const;

	/// Get rotation as euler angles in radians
	[[nodiscard]] glm::vec3 GetRotation() const;

	Camera& SetOrigin(const glm::vec3& position);
	Camera& SetFocus(const glm::vec3& position);
	/// Puts the camera here, still, and has what moves it forget where it was going, so that it stays here until the
	/// player or a script moves it. When the kept view has this eye and focus, nothing changes.
	Camera& Place(const glm::vec3& origin, const glm::vec3& focus);
	/// The eye and what it looks at jump up by these as the camera shakes, as it is drawn
	Camera& SetShake(const glm::vec3& eye, const glm::vec3& focus);

	Camera& SetOriginInterpolator(const glm::vec3& p0, const glm::vec3& p1, const glm::vec3& m0, const glm::vec3& m1);
	Camera& SetFocusInterpolator(const glm::vec3& p0, const glm::vec3& p1, const glm::vec3& m0, const glm::vec3& m1);

	/// The eye's and the focus's movements each run on their own clock: they are set off together, but holding the eye
	/// inside the world's ball sets the eye off again alone. These give the eye's clock, which the camera models time by.
	[[nodiscard]] std::chrono::microseconds GetInterpolatorTime() const { return _interpolatorTime; }
	[[nodiscard]] float GetInterpolatorT() const { return ShareOf(_interpolatorTime, _interpolatorDuration); }
	[[nodiscard]] std::chrono::microseconds GetInterpolatorDuration() const { return _interpolatorDuration; }
	[[nodiscard]] std::chrono::microseconds GetFocusInterpolatorDuration() const { return _focusInterpolatorDuration; }

	/// These set or move on both clocks
	Camera& SetInterpolatorTime(std::chrono::microseconds t);
	Camera& AddInterpolatorTime(std::chrono::microseconds t);
	Camera& SetInterpolatorT(float t);
	Camera& SetInterpolatorDuration(std::chrono::microseconds duration);

	/// A perspective with a near plane and no far one, as the game's: nothing is lost however far away it is, the sky's
	/// dome across the map and the sea to its horizon from as high as the camera goes
	Camera& SetProjectionMatrixPerspective(float xFov, float aspect, float nearClip);
	/// Moves the near plane of the perspective, keeping its field of view and aspect
	Camera& SetNearClip(float nearClip);
	[[nodiscard]] float GetNearClip() const { return _nearClip; }
	/// Gives the perspective the screen's new shape, keeping its field of view across the width and its clip planes
	Camera& SetAspect(float aspect);
	[[nodiscard]] float GetAspect() const { return _aspect; }
	Camera& SetProjectionMatrix(const glm::mat4& projection);

	/// The lens (the field of view across the width, in radians) glides to a new one over some seconds, starting from
	/// how it is changing now; under a thousandth of a second it changes at once
	Camera& SetLens(float radians, float seconds);
	/// The lens moves on by some seconds of its glide, and the perspective follows it
	void UpdateLens(float seconds);
	/// Where the lens is gliding to, or where it is when still
	[[nodiscard]] float GetLensDestination() const { return _lens.Destination(); }

	[[nodiscard]] glm::vec3 GetForward() const;
	[[nodiscard]] glm::vec3 GetRight() const;
	[[nodiscard]] glm::vec3 GetUp() const;

	[[nodiscard]] std::unique_ptr<Camera> Reflect() const;

	void DeprojectScreenToWorld(glm::vec2 screenCoord, glm::vec3& outWorldOrigin, glm::vec3& outWorldDirection,
	                            Interpolation interpolation = Camera::Interpolation::Current) const;
	/// The eye, and the point of the near plane a point of the screen (0 to 1 from the top left) shows
	struct NearPlanePoint
	{
		glm::vec3 eye;
		glm::vec3 point;
	};
	[[nodiscard]] NearPlanePoint OnNearPlane(glm::vec2 screenCoord,
	                                         Interpolation interpolation = Camera::Interpolation::Current) const;
	bool ProjectWorldToScreen(glm::vec3 worldPosition, glm::vec4 viewport, glm::vec3& outScreenPosition,
	                          Interpolation interpolation = Camera::Interpolation::Current) const;

	void Update(std::chrono::microseconds dt);
	void HandleActions(std::chrono::microseconds dt);
	/// Keeps the view after the step of the camera in this frame. The rays of the camera to the land on the next frame
	/// start from this view. A command that puts the camera after the step does not change this view.
	void KeepDrawnView();

	[[nodiscard]] glm::mat4 GetRotationMatrix() const;
	[[nodiscard]] Projection GetCameraProjection() const;

	CameraModel& GetModel() { return *_model; }
	[[nodiscard]] const CameraModel& GetModel() const { return *_model; }
	/// Hands the camera's control to another model, as the temple does inside, giving back the one it had
	std::unique_ptr<CameraModel> SetModel(std::unique_ptr<CameraModel> model);

	/// Where the camera is and how it is moving there, whole: a tool showing the camera elsewhere for a frame puts this
	/// back before the camera next moves, so that its movement carries on as if it hadn't
	struct Motion
	{
		ZoomInterpolator3f origin;
		ZoomInterpolator3f focus;
		std::chrono::microseconds time;
		std::chrono::microseconds duration;
		std::chrono::microseconds focusTime;
		std::chrono::microseconds focusDuration;
	};
	[[nodiscard]] Motion GetMotion() const
	{
		return {.origin = _originInterpolators,
		        .focus = _focusInterpolators,
		        .time = _interpolatorTime,
		        .duration = _interpolatorDuration,
		        .focusTime = _focusInterpolatorTime,
		        .focusDuration = _focusInterpolatorDuration};
	}
	void SetMotion(const Motion& motion)
	{
		_originInterpolators = motion.origin;
		_focusInterpolators = motion.focus;
		_interpolatorTime = motion.time;
		_interpolatorDuration = motion.duration;
		_focusInterpolatorTime = motion.focusTime;
		_focusInterpolatorDuration = motion.focusDuration;
	}

	/// How much faster than the game's own speed the movement keys move the camera, kept within the allowed range
	void SetKeyboardMoveSpeed(float speed);
	[[nodiscard]] float GetKeyboardMoveSpeed() const { return _keyboardMoveSpeed; }

protected:
	/// How far through its movement a clock is, 0 to 1; a movement of no length is over
	[[nodiscard]] static float ShareOf(std::chrono::microseconds time, std::chrono::microseconds duration)
	{
		return duration == std::chrono::microseconds::zero() ? 1.0f : static_cast<float>(time.count()) / duration.count();
	}
	[[nodiscard]] float GetFocusInterpolatorT() const { return ShareOf(_focusInterpolatorTime, _focusInterpolatorDuration); }
	/// Where the eye is going is kept within the world's ball, as for every camera but the editor's
	void KeepInsideTheWorld();

	ZoomInterpolator3f _originInterpolators;
	ZoomInterpolator3f _focusInterpolators;
	// As a value between 0 and _interpolatorDuration
	std::chrono::microseconds _interpolatorTime = std::chrono::microseconds::zero();
	std::chrono::microseconds _interpolatorDuration = std::chrono::microseconds::zero();
	std::chrono::microseconds _focusInterpolatorTime = std::chrono::microseconds::zero();
	std::chrono::microseconds _focusInterpolatorDuration = std::chrono::microseconds::zero();
	float _xFov = 0.0f;
	/// The lens as it glides; the field of view follows it while it moves
	camera::Zoomer _lens;
	float _aspect = 1.0f;
	float _nearClip = 1.0f;
	glm::mat4 _projectionMatrix = glm::mat4 {1.0f};
	glm::mat4 _projectionMatrixReversedZ = glm::mat4 {1.0f};
	std::unique_ptr<CameraModel> _model;
	Projection _cameraProjection = Projection::ReversedZ;
	glm::vec3 _shakeEye {0.0f};
	glm::vec3 _shakeFocus {0.0f};
	/// A kept view: the eye and the focus with the shake, and the perspective
	struct View
	{
		glm::vec3 eye;
		glm::vec3 focus;
		glm::mat4 projection;
		float nearClip;
	};
	std::optional<View> _drawnView;
	/// The kept view, or the view now when there is no kept view
	[[nodiscard]] View GetDrawnView() const;
	/// The eye and the focus of the view, with the shake. The view now also has the limits of the drawn eye.
	[[nodiscard]] camera_drawn::Pose DrawnPose(Interpolation interpolation) const;
	[[nodiscard]] glm::mat4 GetProjectionMatrix(Projection projection, Interpolation interpolation) const;
	float _keyboardMoveSpeed = k_KeyboardMoveSpeedDefault;
};

} // namespace openblack
