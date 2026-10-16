/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <cstddef>
#include <cstdint>

#include <array>
#include <optional>
#include <span>

#include <glm/mat4x4.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

/// EXPERIMENT (not for merge): the rules of the wakes, rings and splashes on the experiment's water. Things that move
/// in the water write into a field of small waves round the camera. The original game has no such field.
namespace openblack::graphics::underwater::wake
{

/// The field is a square of k_FieldSide texels a side, k_FieldSpacing units apart, centred near the camera
inline constexpr uint16_t k_FieldSide = 512;
inline constexpr float k_FieldSpacing = 0.25f;
/// The field steps at most this many times a second, the same on a fast or a slow screen
inline constexpr float k_StepsPerSecond = 60.0f;
/// The most steps in one frame, after a slow frame
inline constexpr uint32_t k_MostStepsPerFrame = 2;
/// How many texels the kernel of the waves reaches out on each side of a texel
inline constexpr int k_KernelReach = 6;
/// The kernel's weights for each distance along z and x, 0 to k_KernelReach: [z * (k_KernelReach + 1) + x]
using Kernel = std::array<float, (k_KernelReach + 1) * (k_KernelReach + 1)>;
/// How quickly the waves of the field die away, a second: the height falls by half in about 1.7 seconds
inline constexpr float k_Damping = 0.8f;
/// The gravity of the field's waves. It is the gravity that the experiment's waves use (their speed is 0.8 of the
/// speed of deep water waves), so the wakes move as the sea does.
inline constexpr float k_Gravity = 9.8f * 0.8f * 0.8f;
/// The time in seconds in which the foam in the field falls to about a third
inline constexpr float k_FoamLife = 1.6f;
/// A height lower than this is flat: the field does not step while it is flat everywhere
inline constexpr float k_FlatHeight = 0.004f;
/// Foam less than this does not show
inline constexpr float k_FlatFoam = 0.01f;

/// The most things that write into the field at once, the nearest to the field's centre first
inline constexpr size_t k_MostMovers = 16;
/// The most rings that start in the field in one frame
inline constexpr size_t k_MostImpulses = 16;
/// The most hulls that push the water down at once
inline constexpr size_t k_MostHulls = 8;
/// The most splashes at once
inline constexpr size_t k_MostSplashes = 8;

/// A thing in the water, as it writes into the field
struct Mover
{
	/// Where it is across the ground
	glm::vec2 position {0.0f};
	/// How fast it moves across the ground, in units a second
	glm::vec2 velocity {0.0f};
	/// How fast it moves up (more than 0) or down (less than 0), in units a second
	float verticalSpeed {0.0f};
	/// How wide it is in the water: its radius across the ground
	float radius {1.0f};
	/// How deep it is in the water, 0 when it is out of the water
	float submerged {0.0f};
};

/// What a thing writes into the field at each step
struct Write
{
	/// How far the thing pushes the water down under it, in units. A still thing pushes nothing, so the field settles.
	float depth {0.0f};
	/// How much foam it makes round it, each second, 0 to 1
	float foam {0.0f};
};

/// A thing writes more into the field when it moves faster, up to this speed
inline constexpr float k_FullWakeSpeed = 4.0f;
/// A thing slower than this writes nothing
inline constexpr float k_StillSpeed = 0.15f;
/// The deepest that a thing pushes the water down
inline constexpr float k_MostDepth = 0.8f;

/// What a thing in the water writes into the field: more when it is faster, bigger and deeper in the water. It writes
/// nothing when it is still or out of the water.
[[nodiscard]] Write WriteFor(const Mover& mover);

/// A splash where a thing falls into the water
struct Splash
{
	/// How high the crown of water round the hole goes
	float crownHeight {0.0f};
	/// How wide the crown is at its base
	float crownRadius {0.0f};
	/// How many drops it throws up
	uint32_t drops {0};
	/// How fast the drops leave the water
	float dropSpeed {0.0f};
	/// How deep the hole is that starts the rings in the field
	float ringDepth {0.0f};
	/// How long the splash shows, in seconds
	float seconds {0.0f};
};

/// A thing that falls slower than this makes no splash, only rings
inline constexpr float k_LeastSplashSpeed = 1.5f;
/// The most drops in one splash
inline constexpr uint32_t k_MostDrops = 80;
/// The fewest drops in a splash
inline constexpr uint32_t k_FewestDrops = 12;
/// The highest crown
inline constexpr float k_HighestCrown = 8.0f;
/// The deepest hole that a splash makes in the field
inline constexpr float k_DeepestRing = 1.2f;

/// The splash of a thing that falls into the water at a speed (units a second, down) with a radius. No splash when it is
/// too slow. A faster or a bigger thing throws a higher and wider crown and more drops.
[[nodiscard]] std::optional<Splash> SplashFor(float fallSpeed, float radius);

/// The rings that a thing makes as it falls in slowly or bobs, with no splash: the depth of the hole in the field
[[nodiscard]] float RingDepthFor(float fallSpeed, float radius);

/// The weights of the kernel that gives the waves of the field their speed: the long waves go faster than the short
/// waves, as on deep water, which gives a wake its V shape behind a thing. The weight at the middle is 1.
[[nodiscard]] Kernel WaveKernel();

/// How much the field's height pulls back towards flat in one step, for the step's time and the field's spacing, with
/// the kernel of WaveKernel. It makes a wave of k_ReferenceLength texels move at the speed of a deep water wave.
[[nodiscard]] float GravityTerm(const Kernel& kernel, float stepSeconds, float spacing);
/// The length of wave in texels that GravityTerm makes exact: the length of a wake behind a thing that walks
inline constexpr float k_ReferenceLength = 16.0f;
/// How strongly the kernel pulls back a wave across x of a number of radians a texel
[[nodiscard]] float KernelResponse(const Kernel& kernel, float radiansPerTexel);

/// The texel of the field that a point across the ground is in
[[nodiscard]] glm::ivec2 FieldCell(glm::vec2 point, float spacing = k_FieldSpacing);
/// The point across the ground at the low corner of the field when its centre is in a texel
[[nodiscard]] glm::vec2 FieldOrigin(glm::ivec2 centre, uint16_t side = k_FieldSide, float spacing = k_FieldSpacing);
/// Where the field is centred: the point that the camera looks at, but no farther than `reach` from the eye across the
/// ground, so that the field is under the camera when it looks to the horizon
[[nodiscard]] glm::vec2 FieldCentre(glm::vec3 eye, glm::vec3 focus, float reach);

/// How long the field takes to settle to flat after a write of a height, and the foam to clear
[[nodiscard]] float SettleSeconds(float height, float foam);

/// The plan for the field's steps in one frame
struct StepPlan
{
	/// How many steps to make this frame, 0 while the field is flat
	uint32_t steps {0};
	/// How many texels the field moves this frame, as its centre moves. The first step reads the last field there.
	glm::ivec2 shift {0};
	/// Whether the field starts again from flat this frame: the first step reads nothing
	bool reset {false};
};

/// The field's clock: it steps at most k_StepsPerSecond times a second, and stops when nothing has written into it for
/// long enough that it is flat
class FieldClock
{
public:
	/// The plan for this frame: the frame's time, the texel at the field's centre, and the most that any thing writes
	/// into the field this frame (a height or foam)
	[[nodiscard]] StepPlan Plan(float frameSeconds, glm::ivec2 centre, float height, float foam);
	/// Whether the field is flat and does not step
	[[nodiscard]] bool IsFlat() const { return _flat; }
	/// The texel at the field's centre now
	[[nodiscard]] glm::ivec2 Centre() const { return _centre; }

private:
	float _carry {0.0f};
	float _quiet {0.0f};
	float _settle {0.0f};
	bool _flat {true};
	glm::ivec2 _centre {0};
};

/// A hull in the water, as a mask seen from above: it pushes the water down and round it, and no water shows inside
/// a boat
struct Hull
{
	/// Where its middle is across the ground
	glm::vec2 centre {0.0f};
	/// The direction to its bow across the ground, a unit long
	glm::vec2 forward {1.0f, 0.0f};
	/// Half its length along forward, and half its width across
	glm::vec2 half {1.0f};
	/// How deep its keel is under the water
	float draft {0.0f};
	/// How fast it moves along forward, in units a second
	float speed {0.0f};
	/// Whether it is a boat: an open hull with no water inside it
	bool boat {false};
};

/// The hull of a thing drawn by a model matrix, with its model's box, on water at a height: none when the thing is out
/// of the water or all under it. The longer side across the ground is the hull's length.
[[nodiscard]] std::optional<Hull> HullFor(const glm::mat4& model, glm::vec3 boxLow, glm::vec3 boxHigh, float waterHeight,
                                          bool boat);
/// The box of a hull's points across the ground (x and z of its model) near its water line: the points of the model
/// within `band` of the height `waterline` in the model's space. None when no point is that near. A hull is narrower at
/// its water line than at its top, so its mask follows the water line.
struct WaterlineBox
{
	glm::vec2 low {0.0f};
	glm::vec2 high {0.0f};
};
[[nodiscard]] std::optional<WaterlineBox> WaterlineOf(std::span<const glm::vec3> points, float waterline, float band);

/// How far a point across the ground is outside the edge of a hull, less than 0 inside it. The bow is pointed and the
/// stern is round. The shader has the same shape (UwHullDistance).
[[nodiscard]] float HullDistance(const Hull& hull, glm::vec2 point);

} // namespace openblack::graphics::underwater::wake
