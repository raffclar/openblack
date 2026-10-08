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
#include "Common/Zoomer.h"
#include "ECS/Components/Transform.h"

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
	};
	enum class Projection : uint8_t
	{
		Normal,
		ReversedZ,
	};
	explicit Camera(glm::vec3 focus = glm::vec3(1000.0f, 0.0f, 1000.0f));
	virtual ~Camera();

	[[nodiscard]] float GetHorizontalFieldOfView() const;
	/// The W / H SetProjectionMatrixPerspective was given (the viewport's aspect)
	[[nodiscard]] float GetAspect() const { return _aspect; }
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

	/// The camera's Zoomer3 of the position and of the focus
	[[nodiscard]] Zoomer3& GetOriginZoomer() { return _origin; }
	[[nodiscard]] Zoomer3& GetFocusZoomer() { return _focus; }
	[[nodiscard]] const Zoomer3& GetOriginZoomer() const { return _origin; }
	[[nodiscard]] const Zoomer3& GetFocusZoomer() const { return _focus; }
	/// The camera shake: a world-space translation of the DRAWN position and focus only, applied when the engine
	/// camera is updated; the Zoomer3 are never touched, so it does not build up. GetOrigin / GetFocus (Current) and
	/// GetViewMatrix add it (GET_CAMERA_POSITION sees the shaking camera). Set every frame (camera_shake::Adjust), zero
	/// when there is none
	void SetDrawOffset(const glm::vec3& origin, const glm::vec3& focus)
	{
		_originDrawOffset = origin;
		_focusDrawOffset = focus;
	}
	/// A drawn view of its own (only the falling spell sets it: magic::falling_spell::WorldToCamera). GetViewMatrix
	/// gives it while it is set; the zoomers are not touched (the game camera is not pushed to the engine meanwhile),
	/// so clearing it draws the game camera again, as the first frame in mode 0 does
	void SetDrawnView(const std::optional<glm::mat4>& view) { _drawnView = view; }
	/// The time of the zoomers since their last destination (the position's x Zoomer's current time)
	[[nodiscard]] std::chrono::microseconds GetInterpolatorTime() const;

	Camera& SetProjectionMatrixPerspective(float xFov, float aspect, float nearClip, float farClip);
	Camera& SetProjectionMatrix(const glm::mat4& projection);

	[[nodiscard]] glm::vec3 GetForward() const;
	[[nodiscard]] glm::vec3 GetRight() const;
	[[nodiscard]] glm::vec3 GetUp() const;

	[[nodiscard]] std::unique_ptr<Camera> Reflect() const;

	void DeprojectScreenToWorld(glm::vec2 screenCoord, glm::vec3& outWorldOrigin, glm::vec3& outWorldDirection,
	                            Interpolation interpolation = Camera::Interpolation::Current) const;
	bool ProjectWorldToScreen(glm::vec3 worldPosition, glm::vec4 viewport, glm::vec3& outScreenPosition,
	                          Interpolation interpolation = Camera::Interpolation::Current) const;

	void Update(std::chrono::microseconds dt);
	/// The zoomers' part of the camera update, after the mode's Update: the mode's new destinations (the default mode
	/// sets them every frame with Zoomer3::SetDestinationWithTime), then each Zoomer::Update(min(dt, 0.1))
	void UpdateZoomers(const std::optional<CameraModel::CameraInterpolationUpdateInfo>& updateInfo, float seconds);
	void HandleActions(std::chrono::microseconds dt);

	[[nodiscard]] glm::mat4 GetRotationMatrix() const;
	[[nodiscard]] Projection GetCameraProjection() const;

	CameraModel& GetModel() { return *_model; }
	[[nodiscard]] const CameraModel& GetModel() const { return *_model; }
	/// Hands the camera's control to another model, as the temple does inside, giving back the one it had
	std::unique_ptr<CameraModel> SetModel(std::unique_ptr<CameraModel> model);
	/// (openblack engine) Every member the const getters read, into `out`: the zoomers, the shake, the drawn
	/// view, the field of view, both projections and the one used; not the model. The copy of the camera the draw
	/// reads (Game::Run, at the end of the frame's logic)
	void CopyViewTo(Camera& out) const;

protected:
	Zoomer3 _origin;
	Zoomer3 _focus;
	glm::vec3 _originDrawOffset {0.0f};  ///< the shake added to the drawn position
	glm::vec3 _focusDrawOffset {0.0f};   ///< the shake added to the drawn focus
	std::optional<glm::mat4> _drawnView; ///< the falling spell's view, while it (mode 2) draws
	float _xFov = 0.0f;                  // TODO(#707): This should be a zoomer for animations
	float _aspect = 1.0f;                ///< SetProjectionMatrixPerspective's aspect (W / H)
	glm::mat4 _projectionMatrix = glm::mat4 {1.0f};
	glm::mat4 _projectionMatrixReversedZ = glm::mat4 {1.0f};
	std::unique_ptr<CameraModel> _model;
	Projection _cameraProjection = Projection::ReversedZ;
};

} // namespace openblack
