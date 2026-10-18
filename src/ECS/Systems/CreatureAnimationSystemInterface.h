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
	/// Once a frame, by the game time, which stops while the game is paused: the bodies are reshaped where they have
	/// changed enough, posed, and their eyes placed
	virtual void Update(std::chrono::duration<float, std::milli> gameTime) = 0;
	/// Poses a copy of a creature's body some milliseconds on (see components::CreatureCopy): shaped as its creature is,
	/// by its own animations, its eyes looking where its head does and blinking. Nothing happens to an entity that isn't
	/// a copy, or whose creature is gone.
	virtual void PoseCopy(entt::entity copy, float milliseconds) = 0;

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

	/// Places the glows on a creature's body (see components::CreatureHandGlows) on its pose as it is now. Each glow's
	/// texture slides on by the real milliseconds of the frame, and the circling glow turns by the game's milliseconds.
	/// Nothing happens to a creature or copy without glows or a pose.
	virtual void PlaceHandGlows(entt::entity creature, int32_t frameMs, int32_t gameMs) = 0;
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
	/// body is posed now, in the world. Nothing when it has no pose or no such bones.
	[[nodiscard]] virtual std::optional<glm::vec3> MiddleOfGroinAndLips(entt::entity creature) = 0;
	/// A copy of a creature makes its motes or its smoke once, as a creature does each game turn, from the bones of its
	/// pose now and the alignment that its body shows
	virtual void AddAlignmentMotesOf(entt::entity copy) = 0;
};

} // namespace openblack::ecs::systems
