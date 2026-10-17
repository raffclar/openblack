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

#include <array>
#include <functional>
#include <optional>

#include <glm/mat4x4.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

/// EXPERIMENT (not for merge): the rules of the shadows of the sun and the moon. The original game bakes its shadows
/// for a sun that does not move. With the experiment on, the shadows follow the sun and the moon:
/// - the land shades itself from a map of the sun's visibility over the height map, made again as the light moves;
/// - the trees, the buildings and the other things cast shadows from maps seen from the light, in two cascades round
///   the camera's focus, made again about every 3 seconds and blended from the old maps to the new;
/// - the things that move cast shadows from a third map, made each frame.
namespace openblack::graphics::sun_shadows
{

/// Each map of the environment's shadows is made again after this many seconds
inline constexpr float k_UpdateSeconds = 3.0f;
/// The old maps blend into the new over this many seconds
inline constexpr float k_BlendSeconds = 0.8f;
/// The light must turn by more than this, in radians, for the map of the sun's visibility to be made again early
inline constexpr float k_TurnRadians = 0.0087f;
/// The side of each map of the shadows, in texels
inline constexpr uint16_t k_MapTexels = 2048;
/// The width that each cascade covers round the camera's focus, in world units
inline constexpr std::array<float, 2> k_CascadeWidths {480.0f, 1920.0f};
/// How far the light's camera stands back from the focus, and the depth of its view, in world units
inline constexpr float k_LightDistance = 2500.0f;
inline constexpr float k_LightDepth = 5000.0f;
/// The side of the map of the sun's visibility, a texel a corner of the land (as the shore map)
inline constexpr uint16_t k_VisibilityTexels = 513;

/// When the maps of the environment are made, and how far the old maps have blended into the new
class ShadowClock
{
public:
	/// What a frame must do
	struct Frame
	{
		/// Make the maps of the environment again, into the other set of targets
		bool update {false};
		/// The set of targets that has the newest maps, 0 or 1
		uint8_t newest {0};
		/// How much of the newest maps shows, 0 to 1; the rest is the set before
		float blend {1.0f};
	};

	/// One frame: the seconds since the start, the direction towards the light and the camera's focus. The maps are
	/// made again every k_UpdateSeconds, when the light turns by more than k_TurnRadians, or when the focus leaves the
	/// middle half of the nearest cascade.
	Frame Advance(float seconds, glm::vec3 towardsLight, glm::vec2 focus);
	/// Makes the maps again on the next frame, as after a new land opens
	void Reset() { _last.reset(); }
	/// The focus and the direction towards the light that the newest maps were made with
	[[nodiscard]] glm::vec2 Focus() const { return _focus; }
	[[nodiscard]] glm::vec3 Light() const { return _light; }

private:
	std::optional<float> _last;
	uint8_t _newest {0};
	glm::vec2 _focus {0.0f};
	glm::vec3 _light {0.0f, 1.0f, 0.0f};
};

/// The middle of a cascade round the focus, moved to whole texels of the light's view so that the shadows do not
/// crawl as the focus moves. `right` and `up` are the light camera's axes across the map.
[[nodiscard]] glm::vec3 SnapToTexels(glm::vec3 middle, glm::vec3 right, glm::vec3 up, float width);

/// The view and the projection of a cascade: from the light, round its middle, with the far depth at 0 and the near
/// depth at 1 (as the scene's reversed depth). `homogeneousDepth` is true when the depth goes from -1 to 1.
struct LightView
{
	glm::mat4 view;
	glm::mat4 projection;
};
[[nodiscard]] LightView CascadeView(glm::vec3 towardsLight, glm::vec3 middle, float width, bool homogeneousDepth);

/// The CPU twin of the shader that makes the map of the sun's visibility: how much of the light reaches a point on
/// the ground, 0 to 1, from the heights along the way towards the light. `heightAt` gives the ground's height at a
/// point across the ground. The edge of the shadow is soft, wider farther from what casts it.
[[nodiscard]] float VisibilityAt(glm::vec2 point, glm::vec3 towardsLight, const std::function<float(glm::vec2)>& heightAt);

} // namespace openblack::graphics::sun_shadows
