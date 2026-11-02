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
#include <span>
#include <string_view>
#include <vector>

#include <entt/core/fwd.hpp>
#include <glm/vec3.hpp>

/// The camera zones a land's scripts set: a fence the world camera is kept inside, limits on its height and places it
/// is kept out of. Pure rules, tested without the game.
namespace openblack::camera_zones
{

/// Without a limit of its own the camera may go this high, and this high above the land
inline constexpr float k_NoLimit = 30000.0f;
/// The heights the zones hold until a file gives others
inline constexpr float k_DefaultLimit = 500.0f;

/// A place the camera is kept out of
struct Exclusion
{
	enum class Kind : uint8_t
	{
		Dome,
		Cylinder,
	};
	glm::vec3 position {0.0f};
	float radius {0.0f};
	float height {0.0f};
	Kind kind {Kind::Dome};
};

struct Zones
{
	/// Whether the camera is kept out of the exclusions and under its height limits
	bool exclusionsOn {true};
	/// Whether the camera is kept inside the fence
	bool fenceOn {false};
	/// Whether the highest the camera may go is `maxAltitude`, and the highest above the land `heightAboveLand`
	bool useMaxAltitude {false};
	bool useHeightAboveLand {false};
	float maxAltitude {k_DefaultLimit};
	float heightAboveLand {k_DefaultLimit};
	/// The fence's corners in order; only their ground's directions count
	std::vector<glm::vec3> fence;
	std::vector<Exclusion> exclusions;
};

/// Where a line from a point crosses the fence
struct Crossing
{
	/// Whether the point is inside: the line crosses the fence an odd number of times ahead of it. On the fence, or
	/// with fewer than three corners or the fence off, it is.
	bool inside {true};
	/// The nearest crossing ahead (or else behind) the point, or a corner nearer the point than that, or the nearest
	/// corner when the line crosses nowhere
	glm::vec3 closest {0.0f};
	/// Across the side crossed, flat on the ground
	glm::vec3 normal {0.0f, 0.0f, 1.0f};
};
[[nodiscard]] Crossing CrossFence(std::span<const glm::vec3> fence, bool fenceOn, const glm::vec3& point,
                                  const glm::vec3& direction);

/// How high the camera over a point of the land may go: its height above the land, unless that is above the highest
[[nodiscard]] float HeightLimit(const Zones& zones, float ground);

/// How far the camera and what it looks at slide together along the line between them, so that the camera comes down to
/// a height limit it is above; nothing when it isn't above it, or the line is too flat for sliding to bring it down
[[nodiscard]] std::optional<glm::vec3> SlideUnderLimit(const glm::vec3& origin, const glm::vec3& focus, float limit);

/// Where across the ground a camera outside the fence is put back inside it: tested along the line towards where it
/// started the frame, or towards what it looks at when it has hardly moved; it goes past the fence where that line
/// crosses it by `searchRadius`, or the first of the points round there, turning a 64th of a turn at a time either
/// way, that is inside; onto the crossing itself when none is. The test also gives the inclusion distance that the
/// scripts read.
inline constexpr float k_SearchRadius = 20.0f;
/// While the player turns the camera by dragging round the edge of the screen it is put back further in
inline constexpr float k_EdgeTurnSearchRadius = 50.0f;
[[nodiscard]] constexpr float SearchRadius(bool edgeTurning)
{
	return edgeTurning ? k_EdgeTurnSearchRadius : k_SearchRadius;
}
/// The controls that keep a camera put back inside the fence held back. The camera takes no control until none of them
/// is held. The mouse wheel is not one of them: it zooms the camera while it is held back.
struct HoldingControls
{
	/// The hand grips the land to drag it
	bool grippingLand {false};
	/// The action button (the right button) is held
	bool actionButton {false};
	/// Both buttons are held
	bool bothButtons {false};
	/// The camera turns round the mouse
	bool rotateAroundMouse {false};
	/// The camera keys of the last frame asked to turn, tilt, zoom or move the camera. Opposite keys cancel.
	bool keyStep {false};
};
[[nodiscard]] constexpr bool HoldsBack(const HoldingControls& controls)
{
	return controls.grippingLand || controls.actionButton || controls.bothButtons || controls.rotateAroundMouse ||
	       controls.keyStep;
}

struct FenceKeep
{
	/// The point that the camera goes to across the ground (its x and z), none when the camera is inside
	std::optional<glm::vec3> target;
	/// Inside the fence, the distance across the ground from the camera to the fence along the line of the test.
	/// Nothing when the fence puts the camera back, and k_NoFenceDistance when the fence is off.
	float inclusionDistance {0.0f};
};
/// The inclusion distance when the fence is off
inline constexpr float k_NoFenceDistance = 1e10f;
[[nodiscard]] FenceKeep PushInsideFence(std::span<const glm::vec3> fence, bool fenceOn, const glm::vec3& originAtStart,
                                        const glm::vec3& origin, const glm::vec3& focus, float searchRadius);

/// What the fence does to the camera when it puts the camera back to `target`. The camera moves to the target across
/// the ground and keeps its height. Each point that moves with the camera keeps its place from the camera, one axis at
/// a time.
[[nodiscard]] constexpr glm::vec3 MovedWithOrigin(const glm::vec3& point, const glm::vec3& origin, const glm::vec3& newOrigin)
{
	return (point - origin) + newOrigin;
}

/// Where a camera flight that would end outside the fence ends instead: where the line from its end towards what it will
/// look at crosses the fence (or the corner nearer than that); unchanged when the end is inside
[[nodiscard]] glm::vec3 FlightOriginInsideFence(std::span<const glm::vec3> fence, bool fenceOn, const glm::vec3& origin,
                                                const glm::vec3& focus);

/// A flight wooshes only when it starts more than this far from where it ends: one and a half times the distance of
/// the flight sound
inline constexpr float k_FlightSoundDistance = 100.0f;
inline constexpr float k_FlightWooshDistance = k_FlightSoundDistance * 1.5f;
/// Whether a flight from where the camera is now to where it ends wooshes
[[nodiscard]] bool FlightWooshes(const glm::vec3& from, const glm::vec3& to);

/// Whether the influence the scripts give the player counts at a point: only inside the fence, tested along the
/// ground's first axis. The player's temple and towns give influence on both sides of it.
[[nodiscard]] bool ScriptInfluenceCounts(std::span<const glm::vec3> fence, bool fenceOn, const glm::vec3& point);

/// The id of a zone file in the resource cache, from its name in Data/Zones. Names that differ only in case give the
/// same id, as the file system finds the same file for them.
[[nodiscard]] entt::id_type FileId(std::string_view fileName);

} // namespace openblack::camera_zones
