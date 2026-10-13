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
/// Taking or losing a seed cross-fades the hand from its last pose over this many seconds
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

/// What the hand is doing, as far as where it is drawn goes: every change from one to another cross-fades the drawn hand
enum class HandStage : uint8_t
{
	Free,
	/// Pulling a thing free of the land
	Pulling,
	/// Holding a thing or a seed
	Holding,
	/// Scooping food, wood or fish into a handful
	Scooping,
};
/// The hand's stage: a seed or a thing held is holding, unless the thing is still being scooped into the hand
[[nodiscard]] HandStage StageOf(bool holdsSeed, bool pulling, bool scooping, bool holdsThing);

/// A cross-fade of the drawn hand from where it was to where it now is, over k_FadeSeconds
class HandFade
{
public:
	/// The hand starts fading from where it was last drawn
	void Start(glm::vec3 position, const glm::mat3& rotation);
	/// Some seconds on, the drawn hand between where the fade began and where the hand now is
	void Step(float seconds, glm::vec3& position, glm::mat3& rotation);
	[[nodiscard]] bool Fading() const { return _time.has_value(); }

private:
	glm::vec3 _fromPosition {0.0f};
	glm::mat3 _fromRotation {1.0f};
	std::optional<float> _time;
};

/// The drawn hand's pose through its changes of stage, as the game blends it: for k_FadeSeconds after a change, every
/// bone of the drawn hand moves in a straight line, in the world, from where it was last drawn to where the newly placed
/// and posed hand has it. Blending the drawn bones themselves, rather than the hand's place and turn and its pose apart,
/// keeps the hand together: turning between two very different holds can't fold it up or swing it away from what it
/// holds.
class WorldPoseFade
{
public:
	/// A change of stage: the drawn hand fades from how it was last drawn
	void Start();
	/// The hand placed by a model matrix and posed by its bones this frame: some seconds on, the bones are made to draw
	/// it between how it was last drawn and how it is now placed and posed
	void Apply(float seconds, const glm::mat4& model, std::vector<glm::mat4>& bones);
	[[nodiscard]] bool Fading() const { return _time.has_value(); }

private:
	std::vector<glm::mat4> _lastDrawn;
	std::vector<glm::mat4> _from;
	std::optional<float> _time;
};

/// The drawn hand through the hand's changes of stage. When the stage changes, the hand is drawn moving in a straight
/// line over k_FadeSeconds from where it was last drawn to where it is now placed, turning from how it was last drawn,
/// so it never jumps as it starts or stops pulling, holding or scooping, even when a change comes part way through an
/// earlier fade. A thing held in it follows its own straight line from where the hand was last placed (before any
/// fade), so that during a fade begun part way through another it can be drawn a little apart from the hand.
class StageFade
{
public:
	/// The hand placed at a position and turn this frame, in a stage: they become where it is drawn. How far a thing held
	/// in it is drawn from where the hand was placed.
	glm::vec3 Step(HandStage stage, float seconds, glm::vec3& position, glm::mat3& rotation);
	[[nodiscard]] bool Fading() const { return _fade.Fading(); }

private:
	HandFade _fade;
	HandFade _heldFade;
	std::optional<HandStage> _stage;
	std::optional<glm::vec3> _placed;
	glm::vec3 _drawn {0.0f};
	glm::mat3 _drawnRotation {1.0f};
};

} // namespace openblack::magic::hand_hold
