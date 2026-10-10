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

#include <memory>
#include <optional>
#include <string>

#include <InspectorProvider.h>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

namespace openblack::inspector
{

struct CameraState
{
	glm::vec3 origin {0.0f};
	glm::vec3 focus {0.0f};
	/// Its angles in degrees
	glm::vec3 rotation {0.0f};
	glm::vec3 forward {0.0f};
	float horizontalFieldOfView {0.0f};
	float nearClip {0.0f};
	/// What moves the camera: the world's camera, Creature Mode's, a fight's, the temple's, the editor's
	std::string model;
	/// A miracle's or a script's camera path has it, and the player's controls do nothing meanwhile
	bool heldByPath {false};
};

/// Where a camera is put: where it is and what it looks at
struct CameraPose
{
	glm::vec3 origin {0.0f};
	glm::vec3 focus {0.0f};
};

/// The camera's angles from where it is to what it looks at, in degrees: yaw about the up axis (0 looking along +z, 90
/// along +x), pitch below the horizon, and the distance between them
struct CameraAngles
{
	float yaw {0.0f};
	float pitch {0.0f};
	float distance {0.0f};
};

[[nodiscard]] CameraAngles AnglesOf(const CameraPose& pose);
/// Where the camera stands to look at a point from these angles
[[nodiscard]] glm::vec3 OriginFor(glm::vec3 focus, const CameraAngles& angles);

/// What the inspector moves the camera through
class CameraControlInterface
{
public:
	virtual ~CameraControlInterface() = default;
	[[nodiscard]] virtual std::optional<CameraState> State() const = 0;
	/// Puts the camera there at once, as the scripts and the testbed place it; why not, if it can't be
	virtual std::string Set(const CameraPose& pose) = 0;
	/// Flies the camera there as the bookmarks and the shortcuts fly it; why not, if it can't
	virtual std::string Fly(const CameraPose& pose) = 0;
	[[nodiscard]] virtual float GroundHeight(glm::vec2 point) const = 0;
	/// Where an entity is now, for the camera to frame it; none if there is no such entity or it has no place
	[[nodiscard]] virtual std::optional<glm::vec3> EntityPosition(uint32_t id) const = 0;
};

/// An entity for the camera to look at, and from where: the angles and distance not given are the camera's own
struct FrameRequest
{
	uint32_t id {0};
	std::optional<float> yaw;
	std::optional<float> pitch;
	std::optional<float> distance;
};

/// A frame request from an entity's id, or from {id, yaw?, pitch?, distance?}; none, with why not, if it doesn't make sense
[[nodiscard]] std::optional<FrameRequest> ParseFrameRequest(const Json& value, std::string& error);
/// Where the camera goes to look at a point, the entity's place, as the request asks and otherwise from the angles and
/// distance it has now
[[nodiscard]] CameraPose FramePose(glm::vec3 target, const FrameRequest& request, const CameraState& now);

/// Where a request ({position?, focus?, yaw?, pitch?, distance?}) puts the camera, from where it is now: what isn't given
/// is kept. None, with why not, when the request doesn't make sense
[[nodiscard]] std::optional<CameraPose> ResolveCameraPose(const Json& params, const CameraState& now,
                                                          const CameraControlInterface& camera, std::string& error);

///   camera.state                                  where the camera is, what it looks at, its angles and what moves it
///   camera.set {position?, focus?, yaw?, pitch?, distance?}   puts it there at once
///   camera.fly {position?, focus?, yaw?, pitch?, distance?}   flies it there
///   camera.frame {id, yaw?, pitch?, distance?}                puts it to look at an entity, where it is now
/// What isn't given is kept: a focus alone keeps the angles and distance, angles alone turn about the focus, a position
/// alone keeps the direction it looks.
[[nodiscard]] std::unique_ptr<ProviderInterface> MakeCameraProvider(CameraControlInterface& camera);

} // namespace openblack::inspector
