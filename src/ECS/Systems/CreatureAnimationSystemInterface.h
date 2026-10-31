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

#include <chrono>
#include <optional>
#include <span>

#include <entt/entity/fwd.hpp>
#include <glm/vec3.hpp>

#include "Creature/CreatureHandGlow.h"
#include "Creature/CreatureKiss.h"

namespace openblack::ecs
{
class Registry;
}

namespace openblack::ecs::systems
{

/// Brings the creatures' bodies to life: their shape follows what each creature has become, they are posed by their
/// animations, and their eyes look about and blink (see components::CreatureMorph, CreatureAnimation, CreatureEyes)
class CreatureAnimationSystemInterface
{
public:
	virtual ~CreatureAnimationSystemInterface() = default;

	/// Once a game turn: the fatness each body shows follows its creature's a step
	virtual void ProcessTurn() = 0;
	/// In the creature's own turn, before its leash and its mind: the breath goes back to the resting period of its size
	virtual void BreathTurn(entt::entity /*creature*/) {}
	/// In the creature's own turn, after its action: the fatness that its body shows follows its fatness a step
	virtual void BodyTurn(entt::entity /*creature*/) {}
	/// In the creature's own turn, as its body lives the turn: the motes or the smoke of its alignment
	virtual void MotesTurn(entt::entity /*creature*/) {}
	/// In the creature's own turn, as its body lives the turn: the body sways from what hit it
	virtual void SwayTurn(entt::entity /*creature*/) {}
	/// At the end of the creature's own turn, its body takes its pose for the turn if it has no pose yet
	virtual void EndPoseTurn(entt::entity /*creature*/) {}
	/// Once a frame, by the game time, which stops while the game is paused: the bodies are drawn between the poses of
	/// their last two game turns, and their eyes placed. The frame changes only what the draw reads.
	virtual void Update(std::chrono::duration<float, std::milli> gameTime) = 0;
	/// In the creature's own turn, as its body lives the turn: the shape that the body is drawn with follows its
	/// alignment, fatness and strength once they changed enough, and its skeleton follows the shape
	virtual void MorphTurn(entt::entity /*creature*/) {}
	/// At the end of each game turn: a creature with no skeleton for its shape gets one, and a creature with no pose yet
	/// takes its first pose
	virtual void ShapeBodies() {}
	/// Before the turn of the living: the system makes the blends of the first pose and the first plan of each creature
	/// with no pose yet, so that the creature's own turn finds them. It changes nothing in the world.
	virtual void PrepareFirstPoses() {}
	/// A creature in the world takes its pose for this game turn now, at the end of its turn: its animations move on by
	/// one turn, at the place and heading where its turn took it. A creature that has its pose for this turn keeps it.
	virtual void PoseTurn(entt::entity creature) = 0;
	/// At the end of each game turn, each creature in the world without a pose for this turn takes its pose
	virtual void PoseTurns() = 0;
	/// A creature that moves to a new place without a walk keeps its last pose as both poses. Thus the body does not
	/// move back to the pose from two turns before.
	virtual void HoldTurnPose(entt::entity creature) = 0;
	/// Poses a copy of a creature's body some milliseconds on (see components::CreatureCopy): shaped as its creature is,
	/// by its own animations, its eyes looking where its head does and blinking. Nothing happens to an entity that isn't
	/// a copy, or whose creature is gone. The copy is a thing of the registry of copies, which can be another registry
	/// than the land's, as the temple's rooms have. Its creature is a thing of the registry of bodies: the land, or the
	/// drawn registry for the creature of the tattoo menu.
	virtual void PoseCopy(Registry& copies, entt::entity copy, float milliseconds, const Registry& bodies) = 0;

	/// Where a bone of a creature is in one of its animations at a time, played left to right or not, in its mesh's space,
	/// the animation blended as the body is drawn; nothing when the creature has no such animation or bone, or hasn't been
	/// posed yet
	[[nodiscard]] virtual std::optional<glm::vec3> BoneInAnimation(entt::entity creature, size_t animation, float timeMs,
	                                                               uint32_t bone, bool mirrored) = 0;
	/// The middle of some bones of a creature in one of its animations at a time, in its mesh's space, the animation
	/// blended as the body is drawn; nothing when the creature has no such animation or bones, or hasn't been posed yet
	[[nodiscard]] virtual std::optional<glm::vec3> MiddleOfBonesInAnimation(entt::entity creature, size_t animation,
	                                                                        float timeMs, std::span<const uint32_t> bones) = 0;
	/// How long one of a creature's animations lasts, in milliseconds, if it has it
	[[nodiscard]] virtual std::optional<float> AnimationDuration(entt::entity creature, size_t animation) = 0;
	/// How far one of a creature's animations carries it, in its model's units and axes, if it has it
	[[nodiscard]] virtual std::optional<glm::vec3> AnimationTravel(entt::entity creature, size_t animation) = 0;
	/// Where the lips of a creature are at the moment of the kiss in its high and its low kiss animations, in its own space
	/// at its scale in the world, the animations blended as the body is drawn. Nothing when the creature has not been
	/// posed yet.
	[[nodiscard]] virtual std::optional<creature_kiss::KissPoints> KissPointsOf(entt::entity creature) = 0;
	/// A force kicks a creature's body at a point, swaying its upper body when the point is high on it and its lower
	/// body otherwise, unless it is striking with a destroying blow
	virtual void KickSway(entt::entity creature, glm::vec3 force, glm::vec3 point) = 0;
	/// The creature is told to hurry. The action that its body plays, the action that it does with a thing, or its
	/// moves in a fight continue at two times the usual speed, when they are of a kind that hurries.
	virtual void Hurry(entt::entity /*creature*/) {}

	/// Places the glows on a creature's body (see components::CreatureHandGlows) on its pose as it is now. Each glow's
	/// texture slides on by the real milliseconds of the frame, and the circling glow turns by the game's milliseconds.
	/// Nothing happens to a creature or copy without glows or a pose. `things` is the registry of the creature or the
	/// copy; the creature of a copy is a thing of the land.
	virtual void PlaceHandGlows(Registry& things, entt::entity creature, int32_t frameMs, int32_t gameMs) = 0;
	/// A script gives a creature a creed power: a glow of a scale and a power at a place on its body, a power of 0 to
	/// remove the glow. The creature hums while its glows have power (see creature_hand_glow::SetCreedPower).
	virtual void SetCreedPower(entt::entity creature, creature_hand_glow::Place place, float scale, float power) = 0;
	/// A saved game gives a loaded creature its creed powers, one for each place (see
	/// creature_hand_glow::LoadCreedPowers). The creature hums while its glows have power.
	virtual void LoadCreedPowers(entt::entity creature,
	                             std::span<const creature_hand_glow::SavedPower, creature_hand_glow::k_PlaceCount> powers) = 0;
	/// An animation event of a creature sets a creed glow, if the event is one that sets a glow. The hum does not
	/// change.
	virtual void PlayCreedEvent(entt::entity creature, int32_t event) = 0;
	/// The middle of a creature's or a copy's groin bone and its lips (the point that kisses, else its head bone), as its
	/// body is posed at the end of its newest game turn, in the world. Nothing when it has no pose or no such bones.
	/// `things` is the registry of the creature or the copy.
	[[nodiscard]] virtual std::optional<glm::vec3> MiddleOfGroinAndLips(const Registry& things, entt::entity creature) = 0;
	/// A copy of a creature makes its motes or its smoke once, as a creature does each game turn, from the bones of its
	/// newest turn pose and the alignment that its body shows. `copies` is the registry of the copy.
	virtual void AddAlignmentMotesOf(const Registry& copies, entt::entity copy) = 0;
};

} // namespace openblack::ecs::systems
