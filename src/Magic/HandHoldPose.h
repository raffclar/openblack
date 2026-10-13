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
#include <vector>

#include <glm/mat3x3.hpp>
#include <glm/mat4x4.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

#include "3D/HandAnimation.h"
#include "Enums.h"

// How the god hand holds a miracle's seed. The hand has no animation of its own for miracles: it takes a still frame of
// one of its holding poses, chosen by how the seed is held, rises by an amount that depends on that too, and sways with
// the seed as the cursor moves. The seed's model hangs below the hand point, turned with the hand. Pure functions, tested
// on their own; HandHoldPoser applies them to the hand and the seed each frame.

namespace openblack::magic::hand_hold
{

/// The hand's standard height, from which the poses and the lift are measured
inline constexpr float k_StandardHandHeight = 3.2f;
/// A seed held above the hand lifts it by this much
inline constexpr float k_AboveLift = 0.2f;
/// A seed held at the side lifts the hand by at least this much
inline constexpr float k_MinimumSideLift = 1.9f;
/// A seed held above opens the hand against a reach this many times the hand's height
inline constexpr float k_AboveReachShare = 1.2f;
/// The hand sways by up to this many radians...
inline constexpr float k_MaxSway = 0.3f;
/// ...when the cursor runs this many pixels ahead of where the hand has caught up to
inline constexpr float k_SwayLag = 80.0f;
/// A change of the hand blends the drawn hand for this number of seconds
inline constexpr float k_FadeSeconds = 0.13f;

/// How a seed is held: as a miracle not yet ready until it is ready, then as its record says
[[nodiscard]] HoldType HoldTypeOf(bool ready, HoldType recorded);

/// The still frame of the hand's animations that holds a seed
struct HoldFrame
{
	HandAnimation::Cycle cycle {HandAnimation::Cycle::Wiggle};
	uint32_t timeMs {0};
};
/// The animation a hold uses, none for a hold that has none
[[nodiscard]] std::optional<HandAnimation::Cycle> HoldCycle(HoldType hold);
/// The frame of a hold's animation (lasting some milliseconds) for a seed of some reach (its scale times its hold
/// radius), with the hand at a size: held above, the hand closes as the reach grows; at the side it opens; a miracle
/// not yet ready takes the middle of the rest pose
[[nodiscard]] uint32_t HoldTimeMs(HoldType hold, uint32_t durationMs, float reach, float handSize);
/// The same moment while the hand pulls a thing free: a thing held above opens the hand by its reach against the hand's
/// own height alone; every other hold opens it as when holding
[[nodiscard]] uint32_t TugTimeMs(HoldType hold, uint32_t durationMs, float reach, float handSize);

/// How far below the hand point a seed's model hangs: its hold lowering times its height for a seed held above or at
/// the side, nothing for a miracle not yet ready (or one that shows no model)
[[nodiscard]] float SeedHang(HoldType hold, float lowering, float height);
/// How far the hand rises for what it holds: a little for a seed held above, its own height times its size for a
/// miracle with no model, and the seed's hang (at least a minimum) for one held at the side
[[nodiscard]] float HoldLift(HoldType hold, float hang, float handSize);

/// The hand's sway from how far the cursor runs ahead of the hand, in pixels (positive right and down): a roll about
/// the line from the hand to the camera, and a pitch
[[nodiscard]] glm::vec2 CursorSway(glm::vec2 cursorLag);
/// The held seed's up, and the hand's: straight up, rolled back by the roll about the line from the hand to the camera,
/// then pitched back by the pitch about the level line across it
[[nodiscard]] glm::vec3 HeldUp(glm::vec3 camera, glm::vec3 hand, float roll, float pitch);
/// The seed's axes for an up: its forward kept as level as it can be along the way the hand faces (the hand's level
/// turn, about the vertical), its side across the two. The hand's own turn is these axes with its model turned upright.
[[nodiscard]] glm::mat3 HeldBasis(const glm::mat3& levelTurn, glm::vec3 up);
/// The seed model's turn in the hand: its axes, half a turn about its up for a right hand, then its own extra turn
/// about its up
[[nodiscard]] glm::mat3 SeedTurn(const glm::mat3& basis, float yRotate, bool rightHanded);

/// The blend of the drawn hand. The blend keeps two poses of the hand: the drawn pose and a kept pose. Each pose has
/// the world matrices of all the bones. For k_FadeSeconds after a change, each bone moves in a straight line in the
/// world, from the kept pose to the pose of the frame. This keeps the hand together. A turn between two very different
/// holds cannot fold the hand or move it away from what it holds.
class WorldPoseFade
{
public:
	/// A change starts the blend again. The last drawn pose becomes the kept pose. The kept pose of the last change becomes
	/// the drawn pose until the frame gives its pose.
	void Change();
	/// The model matrix and the bones place and pose the hand in this frame. The bones change to draw the blend. Usually
	/// the pose of the frame replaces the drawn pose. When poseIntoKept is true, the pose of the frame replaces the kept
	/// pose, and the blend goes from the pose of the frame to the old drawn pose.
	void Apply(float seconds, const glm::mat4& model, std::vector<glm::mat4>& bones, bool poseIntoKept = false);
	[[nodiscard]] bool Fading() const { return _time.has_value(); }

private:
	std::vector<glm::mat4> _drawn;
	std::vector<glm::mat4> _kept;
	std::optional<float> _time;
};

/// The position of a thing in the hand during the blend. Each change keeps the position where the hand was last placed.
/// For k_FadeSeconds, the thing moves in a straight line from that position to the place of the hand.
class HeldFade
{
public:
	/// A change starts the blend again from the last place of the hand
	void Change();
	/// The hand is at a place in this frame. The result is the distance from that place to the position of the thing.
	glm::vec3 Step(float seconds, glm::vec3 placed);
	[[nodiscard]] bool Fading() const { return _time.has_value(); }

private:
	std::optional<glm::vec3> _placed;
	glm::vec3 _from {0.0f};
	std::optional<float> _time;
};

} // namespace openblack::magic::hand_hold
