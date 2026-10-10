/*******************************************************************************
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

#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

/// The shoal of fish that shows where a fish farm is: fifteen flat fish swimming under the sea's surface after a point
/// that wanders about the shoal's centre. As many of them show as the farm is full. A splash or a step nearby sends the
/// fish close to it darting away, and the shoal off somewhere else.
namespace openblack::fish_shoal
{

/// A shoal is fifteen fish
inline constexpr size_t k_FishCount = 15;
/// The shoal's point wanders this far about its centre, along each of x and z
inline constexpr float k_WanderRadius = 7.0f;
/// The sea is searched for a shoal's centre in rings round the farm this far apart, out to this far
inline constexpr float k_RingStep = 2.0f;
inline constexpr float k_RingLimit = 50.0f;
/// ...in this many directions
inline constexpr uint32_t k_RingDirections = 32;
/// The shoal is drawn, and swims, only while its centre is this near the camera (squared), and fades out beyond the
/// nearer distance
inline constexpr float k_ShownDistanceSquared = 90000.0f;
inline constexpr float k_OpaqueDistanceSquared = 40000.0f;
inline constexpr float k_FadePerDistanceSquared = 0.0001f;
/// A scare reaches the fish this near it (squared), panicking them this long
inline constexpr float k_ScareReachSquared = 64.0f;
inline constexpr float k_PanicSeconds = 2.0f;
/// A scared shoal heads for a point this far from its centre
inline constexpr float k_ScaredTargetDistance = 2.0f;
/// The hand finds a fish this near it across the sea (squared)
inline constexpr float k_HandReachSquared = 4.0f;
/// A fish moves no further in a frame than in this long
inline constexpr float k_LongestStep = 0.1f;
/// The fish picture's frames in the sprite atlas: sixteen, from the ninth, of an atlas eight frames across
inline constexpr uint32_t k_FirstFrame = 8;
inline constexpr uint32_t k_FrameCount = 16;
inline constexpr uint32_t k_AtlasColumns = 8;

struct Fish
{
	glm::vec3 position {0.0f};
	/// Which way it swims, in radians from the x axis towards z
	float heading {0.0f};
	float speed {0.0f};
	/// How fast it turns towards its shoal's point, in radians a second
	float turnRate {0.0f};
	/// How far through its swimming picture it is, in frames
	float phase {0.0f};
	/// How long it still darts, in seconds: four times as fast while over a second, slowing to its own speed
	float panic {0.0f};
	/// Half its length and width
	float size {1.0f};
	/// The atlas frame it is drawn with
	uint32_t frame {k_FirstFrame};
};

struct Shoal
{
	glm::vec3 centre {0.0f};
	/// The point the fish swim after, and how long until it moves again
	glm::vec3 target {0.0f};
	float retargetSeconds {0.0f};
	/// How full the farm is, 0 to 1: the share of the fish that show
	float fullness {1.0f};
	std::array<Fish, k_FishCount> fish {};
};

/// A uniform draw from the first to the second
using Random = std::function<float(float, float)>;

/// Where the shoal of a farm at a point lies: going out from the farm in rings, the first point in open sea whose
/// direction was open sea on the ring before too. None where there isn't any near.
[[nodiscard]] std::optional<glm::vec2> FindCentre(glm::vec2 farm, const std::function<bool(glm::vec2)>& isOpenSea);

/// A new fish of a shoal at a centre, and a new shoal of fifteen
[[nodiscard]] Fish MakeFish(glm::vec3 centre, const Random& random);
[[nodiscard]] Shoal MakeShoal(glm::vec3 centre, const Random& random);

/// How many of the shoal's fish show at its fullness
[[nodiscard]] uint32_t ShownCount(float fullness);
/// How opaque the shoal is drawn at its distance from the camera (squared), none beyond where it shows. It fades out
/// long before that, and further off its opacity wraps round to show it again, as the game's does
[[nodiscard]] std::optional<uint8_t> AlphaAt(float distanceSquared);

/// A fish swims for a while towards a point, darting while it panics
void StepFish(Fish& fish, glm::vec3 target, float seconds);
/// A shoal near enough to the camera swims for a frame: its point moves on when its time is up, and goes off another way
/// when something scares the fish, which then dart away from it if they are near it
void Step(Shoal& shoal, float seconds, std::optional<glm::vec3> scare, const Random& random);

/// Whether one of the shoal's fish that show is near a point across the sea
[[nodiscard]] bool HasShownFishNear(const Shoal& shoal, glm::vec2 point);

/// A fish drawn: a square lying flat, turned the way it swims, and where its frame is in the atlas, corner by corner
[[nodiscard]] std::array<glm::vec3, 4> Corners(const Fish& fish);
[[nodiscard]] std::array<glm::vec2, 4> AtlasCorners(uint32_t frame);

} // namespace openblack::fish_shoal
