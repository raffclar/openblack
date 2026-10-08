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
#include <functional>
#include <optional>

#include <glm/vec3.hpp>

#include "CameraModel.h"
#include "CameraZoomer.h"

namespace openblack
{

namespace script_camera
{

/// The camera moves on by no more than a tenth of a second a frame
inline constexpr float k_MaxFrameSeconds = 0.1f;
/// Closer than this, squared, to where it is going, the camera has arrived
inline constexpr float k_ArrivedDistanceSquared = 0.001f;
/// The camera is kept within a sphere of 3500 around the middle of the world, at (2560, 0, 2560)
inline constexpr float k_WorldCentre = 2560.0f;
inline constexpr float k_WorldRadiusSquared = 1.225e7f;
/// Sent beyond it, it is sent back onto it instead, over 3 seconds
inline constexpr float k_WorldScale = 0.000285795948f;
inline constexpr float k_WorldReturnSeconds = 3.0f;
/// The camera is drawn at least a metre above the land
inline constexpr float k_GroundClearance = 1.0f;

/// Where the camera is drawn from and looks at
struct View
{
	glm::vec3 origin;
	glm::vec3 focus;
};

/// The point a camera sent beyond the world's sphere is sent to instead: back onto the sphere along the same line from
/// its middle; nothing when it is within it
[[nodiscard]] std::optional<glm::vec3> PullIntoWorld(const glm::vec3& destination);
/// The camera as it is drawn: a metre aside and up when it is where it looks, then lifted, with what it looks at, to stay
/// a metre above the land under it
[[nodiscard]] View Drawn(const View& camera, const std::function<float(float x, float z)>& groundAt);

} // namespace script_camera

/// The camera while a script has taken control of it: each of the six coordinates of where it is and what it looks at
/// glides to where the script sends it, and the player can't move it. The script hands it back when it is done, or
/// when its task stops.
class ScriptCameraModel final: public CameraModel
{
public:
	using GroundHeight = std::function<float(float x, float z)>;

	/// It takes over from where the camera is
	ScriptCameraModel(const glm::vec3& origin, const glm::vec3& focus, GroundHeight groundAt);

	/// The camera is put somewhere, still
	void SetOrigin(const glm::vec3& origin);
	void SetFocus(const glm::vec3& focus);
	/// The camera glides somewhere from where it is, arriving still after some seconds; at once in under a thousandth
	void MoveOrigin(const glm::vec3& origin, float seconds);
	void MoveFocus(const glm::vec3& focus, float seconds);
	/// Whether both where it is and what it looks at have arrived where they were sent
	[[nodiscard]] bool Arrived() const;

	std::optional<CameraInterpolationUpdateInfo> Update(std::chrono::microseconds dt, const Camera& camera) override;
	void HandleActions(std::chrono::microseconds dt) override;
	void SetFlight(glm::vec3 origin, glm::vec3 focus) override;
	[[nodiscard]] glm::vec3 GetTargetOrigin() const override;
	[[nodiscard]] glm::vec3 GetTargetFocus() const override;
	[[nodiscard]] std::chrono::seconds GetIdleTime() const override;

private:
	using Zoomers = std::array<camera::Zoomer, 3>;

	Zoomers _origin;
	Zoomers _focus;
	GroundHeight _groundAt;
};

} // namespace openblack
