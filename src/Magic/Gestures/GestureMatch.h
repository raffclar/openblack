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
#include <optional>
#include <vector>

#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

#include "GestureBuffer.h"
#include "GestureTemplates.h"

// Recognition: the buffer's keypoints against the templates. Only the turn sequence, the first
// direction and the aspect class are compared. Wiki: docs/bw1-notes/magic.md, "Gestures".

namespace openblack::magic::gestures
{
/// 21 pi / 128: a "small" turn, which may be absorbed
constexpr float k_SmallTurn = 0.51541936f;
/// 3 pi / 16: the largest accumulated turn error
constexpr float k_MaxError = 0.58904862f;
/// The aspect class limits: thin below the first, wide above the second
constexpr float k_AspectThin = 0.15f;
constexpr float k_AspectWide = 4.0f;

/// The game's last recognition
struct Result
{
	Gesture gesture {k_None};
	bool reversed {false};     ///< Matched mirrored
	uint8_t start {0};         ///< First matched keypoint
	uint8_t end {0};           ///< Last matched keypoint
	uint8_t templateIndex {0}; ///< The template's index in the list
};

/// What an apply sends (the circle's centre and world radius)
struct Packet
{
	Gesture gesture {k_None};
	bool reversed {false};
	glm::vec3 position {0.0f}; ///< World point
	float size {0.0f};
};

/// The camera services PacketFromResult needs (the engine's screen/land functions)
struct Projection
{
	/// The land point under a pixel (nullopt off the land)
	std::function<std::optional<glm::vec3>(glm::vec2 pixel)> screenToLand;
	/// The unit direction of the ray through a pixel
	std::function<glm::vec3(glm::vec2 pixel)> rayDirection;
	glm::vec3 cameraPosition {0.0f}; ///< The camera's position
	/// (cos yaw, -sin yaw) of the camera in x, z (inferred: the camera's right on the ground)
	glm::vec2 yawAxis {1.0f, 0.0f};
};

/// (W + 1) / max(1, (H + 1) * screenRatio), screenRatio = screen width / height
[[nodiscard]] float Aspect(const BoundingBox& box, float screenRatio);

/// The keypoints (flags & 0xB, and the newest sample), then their aspect
[[nodiscard]] GestureData BuildFromSystem(const GestureSystem& system, float screenRatio);

/// Forward, then mirrored if the template allows it; result.templateIndex = index
bool Match(const GestureData& tpl, uint8_t index, const GestureData& input, Result& result, float screenRatio);

/// Every template of that gesture, in file order
bool MatchGesture(const std::vector<GestureData>& list, Gesture gesture, const GestureData& input, Result& result,
                  float screenRatio);

/// The packet of a result. PositionMode 2 (every template): the land under the centre of the matched
/// samples' box, and size = 1.05 x the world half-width of the box at that distance; other modes: the first matched
/// sample's land point and size 1
[[nodiscard]] Packet PacketFromResult(const std::vector<GestureData>& list, const GestureSystem& system, const Result& result,
                                      const Projection& projection);
} // namespace openblack::magic::gestures
