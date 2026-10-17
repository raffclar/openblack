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
#include <optional>
#include <unordered_map>
#include <vector>

#include <entt/core/hashed_string.hpp>
#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>

#include "3D/SkeletalAnimation.h"
#include "Common/Zoomer.h"
#include "Creature/CreatureEyes.h"
#include "Creature/CreatureLayers.h"
#include "Creature/CreatureMorph.h"
#include "Creature/CreatureSway.h"

namespace openblack::ecs::components
{

/// How a creature's body is drawn: its base mesh pulled towards its evil or good, thin or fat and weak or strong
/// meshes, as the creature has become
struct CreatureMorph
{
	/// The fatness the body shows, which follows the creature's own a little each turn
	float shownFatness {0.5f};
	/// How far along each axis the body is drawn
	creature_morph::Morph drawn {};
	/// The weak to strong look the body shows, -1 to 1. It takes the look of the strength only when the strength is set
	/// or changed by an amount (a spell, a script, carrying, an action), not as the strength slowly goes down each turn.
	/// A creature that a script makes from another takes the other's look. Without it, the body shows its strength.
	std::optional<float> strengthLook {};
	/// Goes up each time the animations and rest pose have to follow the body anew
	uint32_t revision {0};
};

/// How a creature's body is posed. The animations follow the body's evil to good and thin to fat axes, built again
/// when they change.
struct CreatureAnimation
{
	/// What the body plays: standing and breathing, an action, or sitting
	creature_layers::BodyAction body {};
	/// Animations played together, each by its weight, in place of the body's own when there are any (as walking
	/// blends into running)
	struct Slot
	{
		size_t animation;
		float timeMs;
		float weight;
		bool mirrored;
		/// A turn's or step's pair: the animation blended keyframe by keyframe towards this one by blendWeight
		std::optional<size_t> blendedWith {};
		float blendWeight {0.0f};
	};
	std::vector<Slot> slots;
	/// The face's expression and a gesture such as a nod, played on top of the body
	creature_layers::FaceLayer face {};
	creature_layers::GestureLayer gesture {};
	/// The wobble a blow sends through the body, played once on top of it
	creature_layers::GestureLayer wobble {};
	/// The upper and lower body swaying from things thrown at it and the leash dragging it, drawn leaning on top
	creature_sway::Sway sway {};
	/// Where the head turns to look, if anywhere, and how far it is turned right to left and down to up
	std::optional<glm::vec3> lookAt;
	creature_layers::LookAxis yaw {};
	creature_layers::LookAxis pitch {};

	/// How fast the body plays its animations and breathes, 1 as it should and 0 frozen still
	float playbackScale {1.0f};
	/// A script set real time: the body plays its animations at the usual speed, not at the rate of its size, and each
	/// static action and cast pose starts on the right side
	bool realTime {false};
	/// How far through a breath the creature is, 0 to 1, and the seconds a breath takes now
	float breathPhase {0.0f};
	float breathPeriod {0.0f};
	/// The seconds a breath moves towards each frame. Each game turn sets it and the period to the resting period. A walk
	/// that starts sets a shorter one, and a stop sets the resting period again.
	float breathTarget {0.0f};

	/// The CreatureMorph revision the animations below were built for
	std::optional<uint32_t> builtRevision;
	/// The rest pose, blended as the body is, and the animations blended so far, by their index in the creature spec
	skeletal_animation::Skeleton skeleton;
	std::unordered_map<size_t, skeletal_animation::Animation> animations;
	/// The pair a turn or step plays, blended from the animations above, kept while the same pair plays
	struct PairBlend
	{
		size_t from;
		size_t to;
		float weight;
		skeletal_animation::Animation animation;
	};
	std::optional<PairBlend> pair;
	/// Each bone's mirror bone, for playing actions left to right
	std::vector<uint32_t> mirror;

	/// The bones' matrices in the mesh's space as posed this frame, which the body is drawn with
	std::vector<glm::mat4> boneMatrices;
	/// Where each bone was in the world at the end of the last game turn. The alignment motes of a turn start at the
	/// bones of the pose of the turn before.
	std::vector<glm::vec3> lastTurnBones;
};

/// A creature's eyes: where they look and blink, and where each eyeball and eyelid is drawn this frame
struct CreatureEyes
{
	/// The eyeball and eyelid every creature's eyes are drawn with, Data/Eyeball.l3d and Data/Eyelid.l3d
	static constexpr entt::id_type k_EyeballMeshId = entt::hashed_string("creature/eyeball");
	static constexpr entt::id_type k_EyelidMeshId = entt::hashed_string("creature/eyelid");

	creature_eyes::Mode mode {creature_eyes::Mode::Calm};
	/// How open the eyes are, -1 to about 0.75: sleepy eyes turn slowly and droop
	float openness {0.0f};
	creature_eyes::Blink blink {};
	/// Where the creature looks, when it looks at something rather than ahead
	std::optional<glm::vec3> lookAt;
	/// Which way the back of each eye faces as it eases round to where it looks
	std::array<Zoomer3, 2> look;
	std::array<bool, 2> lookStarted {};

	struct Eye
	{
		std::optional<glm::mat4> eyeball;
		std::optional<glm::mat4> eyelid;
	};
	std::array<Eye, 2> drawn {};
	/// The skin's colour under the eyelids, which they are drawn in
	glm::vec3 lidColour {1.0f};
};

} // namespace openblack::ecs::components
