/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#define LOCATOR_IMPLEMENTATIONS

#include "CreatureAnimationSystem.h"

#include <cmath>

#include <algorithm>
#include <array>
#include <chrono>
#include <functional>
#include <numbers>
#include <optional>
#include <span>
#include <tuple>
#include <utility>
#include <vector>

#include <glm/gtx/transform.hpp>

#include "3D/CreatureBody.h"
#include "3D/L3DMesh.h"
#include "3D/LandIslandInterface.h"
#include "Common/FramePace.h"
#include "Common/GameRandom.h"
#include "Creature/CreatureAlignmentMotes.h"
#include "Creature/CreatureAnimation.h"
#include "Creature/CreatureEyes.h"
#include "Creature/CreatureFight.h"
#include "Creature/CreatureHandGlow.h"
#include "Creature/CreatureHeadLook.h"
#include "Creature/CreatureLayers.h"
#include "Creature/CreatureLocomotion.h"
#include "Creature/CreatureLook.h"
#include "Creature/CreatureMorph.h"
#include "Creature/CreatureObjectActions.h"
#include "Creature/CreatureProportions.h"
#include "Creature/CreatureRig.h"
#include "Creature/CreatureSway.h"
#include "ECS/Archetypes/CreatureArchetype.h"
#include "ECS/Components/Creature.h"
#include "ECS/Components/CreatureBody.h"
#include "ECS/Components/CreatureCopy.h"
#include "ECS/Components/CreatureFight.h"
#include "ECS/Components/CreatureHandGlows.h"
#include "ECS/Components/CreatureLocomotion.h"
#include "ECS/Components/CreatureObjectAction.h"
#include "ECS/Components/Transform.h"
#include "ECS/Registry.h"
#include "ECS/Systems/DynamicsSystemInterface.h"
#include "ECS/Systems/TimeSystemInterface.h"
#include "ECS/Systems/VideoSystemInterface.h"
#include "InfoConstants.h"
#include "Locator.h"
#include "Physics/LivingRules.h"
#include "Resources/ResourcesInterface.h"

using namespace openblack;
using namespace openblack::ecs::systems;
using namespace openblack::ecs::components;
using openblack::creature::CreatureRig;
using openblack::skeletal_animation::Animation;

namespace
{
/// The meshes look back along their z axis, so the eyes look ahead along -z when the creature looks nowhere in
/// particular
constexpr glm::vec3 k_MeshBack {0.0f, 0.0f, 1.0f};
/// The rate at which a head speeds up as it turns when no species table gives one, in radians a second each second
constexpr float k_DefaultHeadTurnRate = 15.0f * std::numbers::pi_v<float>;

/// How heavy a creature is, by its size and how fat and strong its body is drawn
float MassOf(const Creature& creature, const CreatureMorph* morph)
{
	return physics::living::CreatureMass(ShownSize(creature), morph != nullptr ? morph->drawn.thinFat : 0.0f,
	                                     morph != nullptr ? morph->drawn.weakStrong : 0.0f);
}

float LandHeightAt(glm::vec3 point)
{
	return Locator::terrainSystem::has_value() ? Locator::terrainSystem::value().GetHeightAt(glm::vec2(point.x, point.z))
	                                           : 0.0f;
}

/// What the body of a creature does, as far as a kick of its sway is concerned
creature_sway::BodyFacts SwayFactsOf(const ecs::Registry& registry, entt::entity creature, const CreatureAnimation& animation)
{
	creature_sway::BodyFacts facts;
	// A fight shows its moves as loops, but only a fighter that is down keeps still
	if (const auto* fighting = registry.TryGet<const CreatureFighting>(creature);
	    fighting != nullptr && fighting->fighter.state != creature_fight::State::Idle)
	{
		const auto state = fighting->fighter.state;
		facts.downInFight = state == creature_fight::State::Faint || state == creature_fight::State::GetUp ||
		                    state == creature_fight::State::Lying;
		return facts;
	}
	facts.playsStartLoopEnd = creature_layers::IsPlaying(animation.body) &&
	                          animation.body.kind == creature_layers::BodyAction::Kind::Sequence &&
	                          !animation.body.timedByPlayer;
	const auto* walk = registry.TryGet<const CreatureLocomotion>(creature);
	facts.turnsOnTheSpot = walk != nullptr && walk->motion == CreatureLocomotion::Motion::Turning;
	return facts;
}

/// The sway kicked at a point, by the force over the last game turn of the creature
void Kick(const ecs::Registry& registry, entt::entity entity, CreatureAnimation& animation, glm::vec3 force, glm::vec3 point)
{
	if (!creature_sway::TakesKick(SwayFactsOf(registry, entity, animation)))
	{
		return;
	}
	const auto& creature = registry.Get<const Creature>(entity);
	creature_sway::Kick(animation.sway, force, point.y - LandHeightAt(point),
	                    creature_sway::k_UpperHeightPerSize * ShownSize(creature),
	                    MassOf(creature, registry.TryGet<const CreatureMorph>(entity)));
}

creature_morph::Morph TargetMorph(const Creature& creature, const CreatureMorph& morph)
{
	auto target = creature_morph::FromAttributes(creature.alignment, morph.shownFatness, creature.strength,
	                                             ecs::archetypes::CreatureArchetype::SpeciesStrength(creature.species));
	target.weakStrong = morph.strengthLook.value_or(target.weakStrong);
	return target;
}

CreatureRig::Mesh EvilGoodMesh(float value)
{
	return value < 0.0f ? CreatureRig::Mesh::Evil : CreatureRig::Mesh::Good;
}

CreatureRig::Mesh ThinFatMesh(float value)
{
	return value < 0.0f ? CreatureRig::Mesh::Thin : CreatureRig::Mesh::Fat;
}

/// A variant's animation, the base's adjusted to the variant's stand where the variant has none of its own, and the
/// variant's stand
struct VariantAnimation
{
	std::optional<Animation> adjusted;
	const Animation* animation {nullptr};
	const Animation* stand {nullptr};
};

VariantAnimation VariantOf(const CreatureRig& rig, CreatureRig::Mesh mesh, size_t index)
{
	VariantAnimation variant;
	const auto slot = static_cast<size_t>(mesh);
	if (!rig.hasMesh.at(slot) || slot >= rig.animations.size())
	{
		return variant;
	}
	const auto* base = rig.GetAnimation(CreatureRig::Mesh::Base, index);
	const auto* baseStand = rig.GetAnimation(CreatureRig::Mesh::Base, creature_animation::k_StandAnimation);
	variant.stand = rig.GetAnimation(mesh, creature_animation::k_StandAnimation);
	const auto& own = rig.animations.at(slot);
	if (index < own.size() && own[index].has_value())
	{
		variant.animation = &*own[index];
	}
	else if (index == creature_animation::k_StandAnimation)
	{
		variant.animation = variant.stand;
	}
	else if (base != nullptr && baseStand != nullptr && variant.stand != nullptr)
	{
		variant.adjusted = creature_animation::AdjustFromStand(*base, *baseStand, *variant.stand);
		variant.animation = &*variant.adjusted;
	}
	return variant;
}

/// The species' animation blended as the body is drawn
std::optional<Animation> BlendAnimation(const CreatureRig& rig, const creature_morph::Morph& morph, size_t index)
{
	const auto* base = rig.GetAnimation(CreatureRig::Mesh::Base, index);
	if (base == nullptr)
	{
		return std::nullopt;
	}
	const auto evilGood = VariantOf(rig, EvilGoodMesh(morph.evilGood), index);
	const auto thinFat = VariantOf(rig, ThinFatMesh(morph.thinFat), index);
	return creature_animation::Blend(*base,
	                                 {.animation = evilGood.animation,
	                                  .stand = evilGood.stand,
	                                  .weight = evilGood.animation != nullptr ? std::abs(morph.evilGood) : 0.0f},
	                                 {.animation = thinFat.animation,
	                                  .stand = thinFat.stand,
	                                  .weight = thinFat.animation != nullptr ? std::abs(morph.thinFat) : 0.0f});
}

/// An eye's point on the body as drawn: each vertex blended as the body is, placed by its bone, in the world
std::optional<creature_eyes::SurfacePoint> EyePointOf(const CreatureRig::EyePoint& point, const creature_morph::Morph& morph,
                                                      const std::vector<glm::mat4>& bones, const glm::mat4& model)
{
	if (!point.enabled)
	{
		return std::nullopt;
	}
	const auto world = creature::PosedTriangle(point.vertices, point.bones, morph, bones, model, creature::k_SkinPointOrder);
	return creature_eyes::PointOnTriangle(world[0], world[1], world[2], point.u, point.v);
}

template <typename Random>
void PlaceEyes(CreatureEyes& eyes, const CreatureRig::Eyes& rig, const creature_morph::Morph& morph,
               const std::vector<glm::mat4>& bones, const Transform& transform, float size, int32_t milliseconds,
               Random&& random)
{
	const auto model = creature::PlacementMatrix(transform.position, transform.rotation, transform.scale);
	const auto eyeSize = creature_eyes::EyeSize(size, rig.scale, morph.evilGood, eyes.mode);
	// The model's back axis, which is the way the creature faces across the land
	const auto back = transform.rotation * k_MeshBack;
	creature_eyes::Look look {
	    .mode = eyes.mode,
	    .openness = eyes.openness,
	    .blink = eyes.blink,
	    .lookAt = eyes.lookAt,
	    .ahead = glm::vec3(back.x, 0.0f, back.z),
	    .darts = {},
	};
	bool lidColourSet = false;
	for (size_t i = 0; i < eyes.drawn.size(); ++i)
	{
		auto& drawn = eyes.drawn.at(i);
		drawn = {};
		const auto point = EyePointOf(rig.points.at(i), morph, bones, model);
		if (!point)
		{
			continue;
		}
		if (look.mode == creature_eyes::Mode::Stoned)
		{
			for (auto& dart : look.darts)
			{
				dart = static_cast<int32_t>(random(201)) - 100;
			}
		}
		auto& turn = eyes.look.at(i);
		if (!eyes.lookStarted.at(i))
		{
			turn.Reset(creature_eyes::LookDestination(*point, look));
			eyes.lookStarted.at(i) = true;
		}
		const auto anchor = EyePointOf(rig.points.at(i + 2), morph, bones, model);
		const auto placed =
		    creature_eyes::PlaceEye(*point, anchor ? std::optional<glm::vec3>(anchor->position) : std::nullopt,
		                            rig.points.at(i).depth, i == 1, look, rig.lidAngles, eyeSize, turn, milliseconds);
		drawn.eyeball = placed.eyeball;
		drawn.eyelid = placed.eyelid;
		// Both lids take the skin's colour under the first one drawn
		if (anchor && !lidColourSet)
		{
			eyes.lidColour = rig.points.at(i + 2).skinColour;
			lidColourSet = true;
		}
	}
}
/// A blended animation of the creature's, blended the first time it is wanted, or nothing when the species has none
const Animation* AnimationOf(CreatureAnimation& animation, const CreatureRig& rig, const creature_morph::Morph& morph,
                             size_t index)
{
	auto found = animation.animations.find(index);
	if (found == animation.animations.end())
	{
		// A species without the animation keeps an empty one, so as not to look for it again
		auto blended = BlendAnimation(rig, morph, index).value_or(Animation {});
		found = animation.animations.emplace(index, std::move(blended)).first;
	}
	return found->second.frames.empty() ? nullptr : &found->second;
}

/// A turn's or step's pair of the creature's animations blended into one, kept while the same pair plays, or nothing
/// when the species lacks either
const Animation* PairAnimationOf(CreatureAnimation& animation, const CreatureRig& rig, const creature_morph::Morph& morph,
                                 size_t from, size_t to, float weight)
{
	auto& pair = animation.pair;
	if (!pair.has_value() || pair->from != from || pair->to != to || pair->weight != weight)
	{
		const auto* first = AnimationOf(animation, rig, morph, from);
		const auto* second = AnimationOf(animation, rig, morph, to);
		const auto* stand = AnimationOf(animation, rig, morph, creature_layers::animations::k_Stand);
		if (first == nullptr || second == nullptr || stand == nullptr)
		{
			return first;
		}
		pair = CreatureAnimation::PairBlend {.from = from,
		                                     .to = to,
		                                     .weight = weight,
		                                     .animation = creature_animation::BlendPair(*first, *second, *stand, weight)};
	}
	return &pair->animation;
}

std::optional<uint32_t> DurationOf(const Animation* animation)
{
	return animation != nullptr ? std::optional(animation->duration) : std::nullopt;
}

/// Plays a head turning animation on top of the pose for how far the head is turned. When the head is straight, the
/// animation plays only if the creature has a point to look at.
void AddLook(std::vector<skeletal_animation::Pose>& poses, const Animation* look, float angle, float limit,
             const CreatureAnimation& animation, std::span<const uint32_t> mirror)
{
	if (look == nullptr || !creature_head_look::PlaysHeadClip(angle, animation.lookAt.has_value()))
	{
		return;
	}
	const auto reference = (look->frames.size() - 1) / 2;
	skeletal_animation::AddLayer(poses, *look, creature_layers::LookTime(angle, limit, look->duration), reference,
	                             animation.skeleton, mirror);
}

/// The swaying body leaning on top of the pose: for the lower body then the upper, its offset across the creature's own
/// axes picks how far through the sideways and the front to back lean to draw, each relative to its middle frame
void AddSway(std::vector<skeletal_animation::Pose>& poses, const CreatureAnimation& animation, const Transform& transform,
             const auto& animationOf)
{
	if (!animation.sway.active)
	{
		return;
	}
	auto side = glm::normalize(transform.rotation[0]);
	auto front = glm::normalize(transform.rotation[2]);
	// A body that turns on the spot leans along the heading that the turn has reached
	if (animation.sway.turnHeading.has_value())
	{
		const float heading = *animation.sway.turnHeading;
		side = glm::vec3(std::cos(heading), 0.0f, -std::sin(heading));
		front = glm::vec3(std::sin(heading), 0.0f, std::cos(heading));
	}
	const auto lean = [&](uint32_t clip, float amount) {
		if (std::abs(amount) <= creature_sway::k_DrawnLean)
		{
			return;
		}
		if (const auto* played = animationOf(clip))
		{
			skeletal_animation::AddLayer(poses, *played, creature_sway::LeanTime(amount, played->duration),
			                             (played->frames.size() - 1) / 2, animation.skeleton);
		}
	};
	for (const auto& [offset, sideClip, frontClip] :
	     {std::tuple {animation.sway.lowerOffset, creature_sway::k_LowerSide, creature_sway::k_LowerFrontBack},
	      std::tuple {animation.sway.upperOffset, creature_sway::k_UpperSide, creature_sway::k_UpperFrontBack}})
	{
		lean(sideClip, glm::dot(offset, side));
		lean(frontClip, glm::dot(offset, front));
	}
}

/// What the head turns towards this frame, and how
struct HeadLook
{
	creature_head_look::Pose pose {creature_head_look::Pose::Standing};
	/// The point that the head turns to, or none to turn back to straight ahead
	std::optional<glm::vec3> target;
	/// The heading that the body shows
	float facing {0.0f};
	/// How fast the head speeds up as it turns, in radians a second each second
	float acceleration {k_DefaultHeadTurnRate};
};

/// The bones of a species that change with its size, as its file names them
creature_proportions::Bones ProportionBonesOf(const CreatureRig& rig)
{
	creature_proportions::Bones bones {.head = rig.leashBone, .rightEye = rig.rightEye};
	if (rig.actionPoints.has_value())
	{
		bones.rightHand = rig.actionPoints->rightHand;
		bones.rightFoot = rig.actionPoints->rightFoot;
	}
	return bones;
}

/// The hand that holds a thing, and whether it is the left hand
struct HoldingHand
{
	uint32_t bone;
	bool mirrored;
};

/// The hand of a creature that shows the hold pose this frame: the hand that holds a thing, unless the body plays an
/// action whose animation moves the held thing itself
std::optional<HoldingHand> HoldingHandOf(const ecs::Registry& registry, entt::entity creature)
{
	const auto* held = registry.TryGet<const CreatureHeldObject>(creature);
	if (held == nullptr)
	{
		return std::nullopt;
	}
	std::optional<creature_object_actions::Kind> playing;
	if (const auto* action = registry.TryGet<const CreatureObjectAction>(creature);
	    action != nullptr && action->phase == CreatureObjectAction::Phase::Playing &&
	    (action->status == creature_object_actions::Status::Running ||
	     action->status == creature_object_actions::Status::Contact))
	{
		// Getting ready to catch and stepping across play before the catch itself
		const bool readyToCatch = action->kind == creature_object_actions::Kind::Catch &&
		                          (action->catching == CreatureObjectAction::Catching::Ready ||
		                           action->catching == CreatureObjectAction::Catching::Stepping);
		if (!readyToCatch)
		{
			playing = action->kind;
		}
	}
	if (!creature_object_actions::ShowsHoldPose(playing))
	{
		return std::nullopt;
	}
	return HoldingHand {.bone = held->bone, .mirrored = held->mirrored};
}

/// The body posed for this frame: its action or breathing, or its slots blended, then the arm that holds a thing, the
/// head turned, the face and any gesture on top
void PoseBody(CreatureAnimation& animation, const CreatureRig& rig, const creature_morph::Morph& morph,
              const Transform& transform, float size, float milliseconds, float seconds, const HeadLook& look,
              const std::function<uint32_t(uint32_t)>& random, std::optional<HoldingHand> holding = std::nullopt)
{
	const auto* stand = AnimationOf(animation, rig, morph, creature_layers::animations::k_Stand);
	if (stand == nullptr)
	{
		return;
	}
	const auto playbackMs = milliseconds * creature_layers::BodyRate(size, animation.realTime);
	const auto animationOf = [&](size_t index) { return AnimationOf(animation, rig, morph, index); };

	// A species without the start, the first move or the finish of the dance does not dance
	if (animation.body.kind == creature_layers::BodyAction::Kind::Dance &&
	    (animationOf(creature_layers::animations::k_DanceStart) == nullptr ||
	     animationOf(creature_layers::animations::k_FirstDanceMove) == nullptr ||
	     animationOf(creature_layers::animations::k_DanceFinish) == nullptr))
	{
		animation.body = {};
	}
	const auto bodyDuration = DurationOf(animationOf(creature_layers::CurrentAnimation(animation.body)));
	animation.actionMoment =
	    animation.actionMoment || creature_layers::ReachesActionMoment(animation.body, playbackMs, bodyDuration,
	                                                                   static_cast<uint32_t>(rig.kissMs.value_or(0.0f)));
	animation.body = creature_layers::AdvanceBody(animation.body, playbackMs, bodyDuration, milliseconds);
	// A dance that needs its next move chooses one of the moves that the species has
	if (animation.body.chooseDanceMove)
	{
		const auto move = creature_layers::ChooseDanceMove(animation.body.firstDanceMove, random,
		                                                   [&](size_t index) { return animationOf(index) != nullptr; });
		animation.body.animations[1] = move.value_or(creature_layers::animations::k_Stand);
		animation.body.chooseDanceMove = false;
	}
	animation.face = creature_layers::AdvanceFace(
	    animation.face, milliseconds,
	    DurationOf(animation.face.current.has_value() ? animationOf(*animation.face.current) : nullptr));
	animation.gesture = creature_layers::AdvanceGesture(
	    animation.gesture, playbackMs,
	    DurationOf(animation.gesture.animation.has_value() ? animationOf(*animation.gesture.animation) : nullptr));
	animation.wobble = creature_layers::AdvanceGesture(
	    animation.wobble, playbackMs,
	    DurationOf(animation.wobble.animation.has_value() ? animationOf(*animation.wobble.animation) : nullptr));

	// The head turns towards its target from the height of the eyes, or back to straight ahead
	const auto eyes = transform.position + glm::vec3(0.0f, creature_look::k_HeadHeight * size, 0.0f);
	const auto angles = look.target.has_value() ? creature_head_look::AnglesTowards(eyes, look.facing, *look.target)
	                                            : creature_head_look::Angles {};
	animation.yaw =
	    creature_layers::TurnHead(animation.yaw, angles.yaw, look.acceleration, seconds, creature_layers::k_YawLimit);
	animation.pitch =
	    creature_layers::TurnHead(animation.pitch, angles.pitch, look.acceleration, seconds, creature_layers::k_PitchLimit);

	const std::span<const uint32_t> mirror = animation.body.mirrored ? animation.mirror : std::span<const uint32_t> {};
	std::vector<skeletal_animation::Pose> poses;
	if (!animation.slots.empty())
	{
		std::vector<std::vector<skeletal_animation::Pose>> sampled;
		std::vector<float> weights;
		for (const auto& slot : animation.slots)
		{
			const auto* played = slot.blendedWith.has_value() ? PairAnimationOf(animation, rig, morph, slot.animation,
			                                                                    *slot.blendedWith, slot.blendWeight)
			                                                  : animationOf(slot.animation);
			sampled.push_back(skeletal_animation::SampleCycle(
			    played != nullptr ? *played : *stand, *stand, static_cast<uint32_t>(std::max(slot.timeMs, 0.0f)),
			    animation.skeleton, slot.mirrored ? animation.mirror : std::span<const uint32_t> {}));
			weights.push_back(slot.weight);
		}
		poses = skeletal_animation::WeightedSumInMeshSpace(sampled, weights, animation.skeleton.parents);
	}
	else if (const auto* played =
	             !creature_layers::IsPlaying(animation.body) ? nullptr
	             : animation.body.blendedWith.has_value()
	                 ? PairAnimationOf(animation, rig, morph, creature_layers::CurrentAnimation(animation.body),
	                                   *animation.body.blendedWith, animation.body.blendWeight)
	                 : animationOf(creature_layers::CurrentAnimation(animation.body));
	         played != nullptr)
	{
		const auto time = creature_layers::DanceMoveTime(animation.body, played->duration);
		poses = skeletal_animation::SampleCycle(*played, *stand, static_cast<uint32_t>(std::max(time, 0.0f)),
		                                        animation.skeleton, mirror);
	}
	else
	{
		const auto time = creature_animation::BreathTime(animation.breathPhase, stand->duration);
		poses = skeletal_animation::SampleCycle(*stand, *stand, time, animation.skeleton);
	}

	// A small creature has a large head, large hands and feet and large eyes for its size
	creature_proportions::Apply(poses, creature_proportions::ScaledBones(ProportionBonesOf(rig), animation.mirror, size));
	// The arm that holds a thing takes the last frame of the hold animation, on the side of the hand that holds it. The
	// hand that holds the thing gets its size for the size of the creature again.
	if (const auto* hold = holding.has_value() ? animationOf(creature_layers::animations::k_PickUpHold) : nullptr)
	{
		skeletal_animation::SetLayer(poses, *hold, hold->duration > 0 ? hold->duration - 1 : 0, animation.skeleton, nullptr,
		                             holding->mirrored ? std::span<const uint32_t>(animation.mirror)
		                                               : std::span<const uint32_t> {});
		const std::array hand {creature_proportions::BoneScale {
		    .bone = holding->bone, .factor = creature_proportions::Factor(creature_proportions::k_HandsAndFeet, size)}};
		creature_proportions::Apply(poses, hand);
	}

	// During the start, the loop and the end of the sit, the head turns by the sitting versions. The head turns the other way
	// when the body plays mirrored.
	const bool sitting = creature_head_look::UsesSittingClips(look.pose);
	AddLook(poses,
	        animationOf(sitting ? creature_layers::animations::k_SitLookDownUp : creature_layers::animations::k_LookDownUp),
	        animation.pitch.angle, creature_layers::k_PitchLimit, animation, mirror);
	AddLook(
	    poses,
	    animationOf(sitting ? creature_layers::animations::k_SitLookRightLeft : creature_layers::animations::k_LookRightLeft),
	    mirror.empty() ? animation.yaw.angle : -animation.yaw.angle, creature_layers::k_YawLimit, animation, mirror);
	// The sway leans the body between the head's turning and the face
	AddSway(poses, animation, transform, animationOf);
	// The face and gestures, relative to their first frames
	if (animation.face.current.has_value())
	{
		if (const auto* face = animationOf(*animation.face.current))
		{
			skeletal_animation::AddLayer(poses, *face, static_cast<uint32_t>(std::max(animation.face.timeMs, 0.0f)), 0,
			                             animation.skeleton);
		}
	}
	if (animation.gesture.animation.has_value())
	{
		if (const auto* gesture = animationOf(*animation.gesture.animation))
		{
			skeletal_animation::AddLayer(poses, *gesture, static_cast<uint32_t>(std::max(animation.gesture.timeMs, 0.0f)), 0,
			                             animation.skeleton);
		}
	}
	// A blow's wobble, relative to its first frame, the way the body plays
	if (animation.wobble.animation.has_value())
	{
		if (const auto* wobble = animationOf(*animation.wobble.animation))
		{
			skeletal_animation::AddLayer(poses, *wobble, static_cast<uint32_t>(std::max(animation.wobble.timeMs, 0.0f)), 0,
			                             animation.skeleton, mirror);
		}
	}
	animation.boneMatrices = skeletal_animation::ComposeBoneMatrices(poses, animation.skeleton.parents);
}
/// The pose of a body that plays an action, a start, loop and end, or nothing
creature_head_look::Pose PoseOfBody(const creature_layers::BodyAction& body)
{
	using creature_head_look::Pose;
	if (!creature_layers::IsPlaying(body))
	{
		return Pose::Standing;
	}
	if (body.kind != creature_layers::BodyAction::Kind::Sequence || body.animations[1] != creature_layers::animations::k_Sit)
	{
		return Pose::Other;
	}
	switch (body.phase)
	{
	case creature_layers::BodyAction::Phase::Start:
		return Pose::SittingDown;
	case creature_layers::BodyAction::Phase::Loop:
		return Pose::Sitting;
	case creature_layers::BodyAction::Phase::End:
		return Pose::GettingUp;
	}
	return Pose::Other;
}

/// Where the head of a creature is as posed in its last frame: its head bone, else the height of its eyes over its feet
glm::vec3 PosedHead(const ecs::Registry& registry, entt::entity creature)
{
	const auto& transform = registry.Get<const Transform>(creature);
	const auto& body = registry.Get<const Creature>(creature);
	const auto eyes = transform.position + glm::vec3(0.0f, creature_look::k_HeadHeight * ShownSize(body), 0.0f);
	const auto* animation = registry.TryGet<const CreatureAnimation>(creature);
	const auto& rigs = Locator::resources::value().GetCreatureRigs();
	const auto rigId = creature::GetRigId(body.species);
	if (animation == nullptr || animation->boneMatrices.empty() || !rigs.Contains(rigId))
	{
		return eyes;
	}
	const auto& points = rigs.Handle(rigId)->actionPoints;
	if (!points.has_value() || points->head >= animation->boneMatrices.size())
	{
		return eyes;
	}
	const auto placement = creature::PlacementMatrix(transform.position, transform.rotation, transform.scale);
	return glm::vec3(creature::PosedBone(points->head, animation->boneMatrices, placement)[3]);
}

/// How fast the head of a species speeds up as it turns, at a size
float HeadAcceleration(CreatureType species, float size)
{
	auto rate = k_DefaultHeadTurnRate;
	if (Locator::infoConstants::has_value())
	{
		const auto& creatures = Locator::infoConstants::value().creature;
		if (const auto row = creature::InfoRow(species); row < creatures.size())
		{
			rate = creatures.at(row).headTurnRate;
		}
	}
	return creature_head_look::Acceleration(size, rate);
}

/// What the head of a creature in the world turns towards this frame. A fight, a catch or a point, the walk and then
/// the body decide the pose, in that order.
HeadLook HeadLookOf(ecs::Registry& registry, entt::entity entity, CreatureAnimation& animation, const CreatureRig* rig,
                    const creature_morph::Morph& morph, const Transform& transform, const Creature& creature,
                    int32_t milliseconds)
{
	using creature_head_look::Pose;
	using Motion = CreatureLocomotion::Motion;
	creature_head_look::AimFacts facts {.hasLookPoint = animation.lookAt.has_value()};
	std::optional<entt::entity> opponent;
	std::optional<glm::vec3> thing;
	std::optional<Pose> pose;

	const auto* fighting = registry.TryGet<const CreatureFighting>(entity);
	if (fighting != nullptr && fighting->fighter.state != creature_fight::State::Idle)
	{
		pose = fighting->fighter.state == creature_fight::State::Stance ? Pose::FightStance : Pose::Other;
		if (registry.Valid(fighting->opponent) && registry.AllOf<Creature, Transform>(fighting->opponent))
		{
			opponent = fighting->opponent;
		}
	}
	const auto* action = registry.TryGet<const CreatureObjectAction>(entity);
	if (!pose.has_value() && action != nullptr)
	{
		using Catching = CreatureObjectAction::Catching;
		if (action->kind == creature_object_actions::Kind::Catch)
		{
			const bool waiting = action->catching == Catching::Ready || action->catching == Catching::Stepping;
			pose = waiting ? Pose::WaitingToCatch : Pose::Other;
			const auto* at = action->target.has_value() ? registry.TryGet<const Transform>(*action->target) : nullptr;
			if (waiting && at != nullptr)
			{
				thing = at->position;
			}
		}
		else if (action->phase == CreatureObjectAction::Phase::Playing)
		{
			const bool pointing = action->kind == creature_object_actions::Kind::Point;
			pose = pointing ? Pose::Pointing : Pose::Other;
			const auto* clip = pointing && rig != nullptr
			                       ? AnimationOf(animation, *rig, morph, creature_head_look::k_PointTimingClip)
			                       : nullptr;
			if (clip != nullptr)
			{
				facts.pointTimeMs = static_cast<int32_t>(action->timeMs);
				facts.pointDurationMs = static_cast<int32_t>(clip->duration);
			}
		}
	}
	auto* walk = registry.TryGet<CreatureLocomotion>(entity);
	// A fidget while it waits for its route is part of the walk, not an action of the body
	if (!pose.has_value() && walk != nullptr && walk->motion == Motion::Confused)
	{
		pose = Pose::Moving;
	}
	if (!pose.has_value() && creature_layers::IsPlaying(animation.body))
	{
		pose = PoseOfBody(animation.body);
	}
	if (!pose.has_value() && walk != nullptr && walk->motion != Motion::Standing)
	{
		pose = walk->motion != Motion::Turning ? Pose::Moving : walk->facingOnly ? Pose::TurningToFace : Pose::TurningToWalk;
	}
	facts.pose = pose.value_or(Pose::Standing);
	facts.hasOpponent = opponent.has_value();
	facts.hasThing = thing.has_value();

	const auto ahead = -(transform.rotation * k_MeshBack);
	HeadLook look {
	    .pose = facts.pose,
	    .facing = creature_locomotion::HeadingOf(glm::vec2(ahead.x, ahead.z)),
	    .acceleration = HeadAcceleration(creature.species, ShownSize(creature)),
	};
	// Part way through a turn to face a point, the body shows a heading between its start and its end
	if (facts.pose == Pose::TurningToFace && walk != nullptr && walk->move.has_value())
	{
		look.facing = creature_locomotion::LerpHeading(walk->move->startHeading, walk->targetHeading, walk->move->timeMs,
		                                               walk->move->durationMs);
	}
	switch (creature_head_look::AimOf(facts))
	{
	case creature_head_look::Aim::Ahead:
		break;
	case creature_head_look::Aim::LookPoint:
		look.target = animation.lookAt;
		// While it waits for its route, the head looks ahead along the search
		if (facts.pose == Pose::Moving && walk != nullptr && walk->destination.has_value() &&
		    (walk->motion == Motion::Planning || walk->motion == Motion::Confused))
		{
			const auto ground = [](glm::vec2 at) {
				return Locator::terrainSystem::has_value() ? Locator::terrainSystem::value().GetHeightAt(at) : 0.0f;
			};
			const auto rate = animation.body.hurried ? 2 : 1;
			const auto stepMs = static_cast<int32_t>(static_cast<float>(milliseconds) *
			                                         creature_layers::BodyRate(ShownSize(creature), animation.realTime)) *
			                    rate;
			walk->routeLook = creature_head_look::StepRouteLook(
			    walk->routeLook, stepMs, {.searchHead = walk->follower.GetSearchHead(), .destination = *walk->destination},
			    ground);
			look.target = walk->routeLook.point;
		}
		break;
	case creature_head_look::Aim::OpponentHead:
		look.target = PosedHead(registry, *opponent);
		break;
	case creature_head_look::Aim::Thing:
		// The thing that it waits to catch becomes the point that it looks at
		animation.lookAt = thing;
		look.target = thing;
		break;
	}
	return look;
}

/// What the head of a copy of a creature turns towards: a copy only stands or plays its body
HeadLook HeadLookOfCopy(const CreatureAnimation& animation, const Transform& transform, CreatureType species, float size)
{
	const auto ahead = -(transform.rotation * k_MeshBack);
	HeadLook look {
	    .pose = PoseOfBody(animation.body),
	    .facing = creature_locomotion::HeadingOf(glm::vec2(ahead.x, ahead.z)),
	    .acceleration = HeadAcceleration(species, size),
	};
	if (creature_head_look::AimOf({.pose = look.pose, .hasLookPoint = animation.lookAt.has_value()}) ==
	    creature_head_look::Aim::LookPoint)
	{
		look.target = animation.lookAt;
	}
	return look;
}
} // namespace

void CreatureAnimationSystem::ProcessTurn()
{
	auto& registry = Locator::entitiesRegistry::value();
	registry.Each<const Creature, CreatureMorph>([](const Creature& creature, CreatureMorph& morph) {
		morph.shownFatness = creature_morph::EaseFatness(morph.shownFatness, creature.fatness);
	});
	// Each game turn the breath goes back to the resting period of the size that the creature shows, before its mind
	// starts a walk that makes it breathe faster
	registry.Each<const Creature, CreatureAnimation>([](const Creature& creature, CreatureAnimation& animation) {
		const auto resting = creature_animation::BreathPeriod(ShownSize(creature));
		animation.breathPeriod = resting;
		animation.breathTarget = resting;
	});
	StepSway();
	AddAlignmentMotes();
}

void CreatureAnimationSystem::StepSway()
{
	auto& registry = Locator::entitiesRegistry::value();
	// Each game turn the body sways from what kicked it, by the seconds that the creature lived in the turn
	registry.Each<const Creature, const CreatureMorph, CreatureAnimation>(
	    [&registry](entt::entity entity, const Creature& creature, const CreatureMorph& morph, CreatureAnimation& animation) {
		    auto& sway = animation.sway;
		    sway.turnSeconds = creature_sway::TurnSeconds(animation.playbackScale);
		    creature_sway::Step(sway, sway.turnSeconds, MassOf(creature, &morph));
		    sway.turnHeading.reset();
		    const auto* walk = registry.TryGet<const CreatureLocomotion>(entity);
		    if (walk != nullptr && walk->motion == CreatureLocomotion::Motion::Turning && walk->move.has_value() &&
		        walk->move->animations.has_value() && walk->move->durationMs > 0.0f)
		    {
			    sway.turnHeading =
			        creature_sway::TurnHeading(walk->heading, walk->targetHeading, walk->move->timeMs, walk->move->durationMs);
		    }
	    });
}

void CreatureAnimationSystem::AddAlignmentMotes()
{
	if (!Locator::dynamicsSystem::has_value() || !Locator::gameRandom::has_value())
	{
		return;
	}
	auto& registry = Locator::entitiesRegistry::value();
	auto& dynamics = Locator::dynamicsSystem::value();
	auto& random = Locator::gameRandom::value();
	const creature_alignment_motes::LocalRandom localRandom = [&random](int32_t n) { return random.LocalRand(n); };
	// While the falling spell plays, no creature in the world makes motes
	const bool fallingSpell = Locator::videoSystem::has_value() && Locator::videoSystem::value().HidesWorld();
	// Each game turn, a creature whose body is to look good or evil makes motes or smoke near the bones of the pose of the
	// turn before. Then it keeps the bones of this turn for the next turn.
	registry.Each<const Creature, const CreatureMorph, CreatureAnimation, const Transform>(
	    [&](const Creature& creature, const CreatureMorph& morph, CreatureAnimation& animation, const Transform& transform) {
		    const auto placement = creature::PlacementMatrix(transform.position, transform.rotation, transform.scale);
		    std::vector<glm::vec3> bones(animation.boneMatrices.size());
		    for (uint32_t bone = 0; bone < bones.size(); ++bone)
		    {
			    bones[bone] = glm::vec3(creature::PosedBone(bone, animation.boneMatrices, placement)[3]);
		    }
		    // Before the first turn ends, the bones of this frame are the only pose there is
		    const auto& last = animation.lastTurnBones.size() == bones.size() ? animation.lastTurnBones : bones;
		    const auto alignment = TargetMorph(creature, morph).evilGood;
		    if (!fallingSpell && creature_alignment_motes::MakesAny(alignment))
		    {
			    const creature_alignment_motes::BonePoint bonePoint = [&last](uint32_t bone) { return last.at(bone); };
			    for (const auto& puff : creature_alignment_motes::PuffsOf(
			             alignment, ShownSize(creature), static_cast<uint32_t>(last.size()), bonePoint, localRandom))
			    {
				    dynamics.AddPuff(puff.position, puff.velocity, puff.size, puff.argb, puff.kind);
			    }
		    }
		    animation.lastTurnBones = std::move(bones);
	    });
}

void CreatureAnimationSystem::Update(std::chrono::duration<float, std::milli> gameTime)
{
	auto& registry = Locator::entitiesRegistry::value();
	auto& resources = Locator::resources::value();
	const auto& meshes = resources.GetMeshes();
	const auto& rigs = resources.GetCreatureRigs();
	const auto seconds = std::chrono::duration<float>(gameTime).count();
	const auto milliseconds = static_cast<int32_t>(std::lround(gameTime.count()));
	// The breath moves towards its target period one step a frame, at most 60 times a second
	const bool breathSteps = frame_pace::Due(_breathPace, std::chrono::milliseconds {milliseconds});
	constexpr auto k_TurnSeconds = std::chrono::duration<float>(TimeSystemInterface::k_TurnDuration).count();

	registry.Each<const Creature, CreatureMorph, CreatureAnimation, const Transform>(
	    [&](entt::entity entity, const Creature& creature, CreatureMorph& morph, CreatureAnimation& animation,
	        const Transform& transform) {
		    const auto refresh = creature_morph::RefreshDrawn(morph.drawn, TargetMorph(creature, morph));
		    morph.drawn = refresh.drawn;
		    if (refresh.animations)
		    {
			    ++morph.revision;
		    }

		    const auto bodyMeshes = creature_morph::MeshesOf(creature.species, morph.drawn,
		                                                     [&meshes](entt::id_type id) { return meshes.Contains(id); });
		    if (!meshes.Contains(bodyMeshes.base))
		    {
			    return;
		    }
		    const auto base = meshes.Handle(bodyMeshes.base);
		    const auto rigId = creature::GetRigId(creature.species);
		    const CreatureRig* rig = rigs.Contains(rigId) ? &*rigs.Handle(rigId) : nullptr;

		    // The rest pose and the animations follow the evil to good and thin to fat axes
		    std::vector<glm::mat4> rest;
		    if (animation.builtRevision != morph.revision || animation.skeleton.Empty())
		    {
			    const auto evilGood = meshes.Handle(bodyMeshes.evilGood);
			    const auto thinFat = meshes.Handle(bodyMeshes.thinFat);
			    rest = creature_animation::BlendRest(base->GetBoneMatrices(), evilGood->GetBoneMatrices(),
			                                         std::abs(morph.drawn.evilGood), thinFat->GetBoneMatrices(),
			                                         std::abs(morph.drawn.thinFat));
			    animation.skeleton = skeletal_animation::Skeleton::FromRestMatrices(base->GetBoneParents(), rest);
			    animation.animations.clear();
			    animation.pair.reset();
			    animation.builtRevision = morph.revision;
			    animation.boneMatrices = rest;
			    // Each bone's mirror as the species' file has it
			    auto mirror = rig != nullptr ? rig->MirrorBones(rest.size()) : std::nullopt;
			    animation.mirror = mirror.has_value() ? std::move(*mirror) : skeletal_animation::MirrorJoints(rest);
		    }

		    // Standing, the creature breathes in and out over its stand animation, from its first frame at its resting
		    // period
		    const auto resting = creature_animation::BreathPeriod(ShownSize(creature));
		    if (animation.breathPeriod <= 0.0f)
		    {
			    animation.breathPeriod = resting;
		    }
		    if (animation.breathTarget <= 0.0f)
		    {
			    animation.breathTarget = animation.breathPeriod;
		    }
		    if (breathSteps)
		    {
			    animation.breathPeriod = creature_animation::EaseBreathPeriod(animation.breathPeriod, animation.breathTarget,
			                                                                  resting, k_TurnSeconds);
		    }
		    // A frozen creature plays slower, down to not at all
		    const float scale = animation.playbackScale;
		    animation.breathPhase =
		        creature_animation::AdvanceBreath(animation.breathPhase, seconds * scale, animation.breathPeriod);

		    if (rig != nullptr)
		    {
			    const auto look = HeadLookOf(registry, entity, animation, rig, morph.drawn, transform, creature, milliseconds);
			    PoseBody(
			        animation, *rig, morph.drawn, transform, ShownSize(creature), gameTime.count() * scale, seconds * scale,
			        look,
			        [this](uint32_t range) {
				        return range == 0 ? 0u : std::uniform_int_distribution<uint32_t>(0, range - 1)(_random);
			        },
			        HoldingHandOf(registry, entity));
		    }
		    if (animation.boneMatrices.size() != base->GetBoneMatrices().size())
		    {
			    animation.boneMatrices = base->GetBoneMatrices();
		    }

		    auto* eyes = registry.TryGet<CreatureEyes>(entity);
		    if (eyes == nullptr)
		    {
			    return;
		    }
		    eyes->blink = creature_eyes::AdvanceBlink(eyes->blink, milliseconds, [this](uint32_t range) {
			    return range == 0 ? 0u : std::uniform_int_distribution<uint32_t>(0, range - 1)(_random);
		    });
		    eyes->drawn = {};
		    if (rig != nullptr && rig->eyes.has_value())
		    {
			    PlaceEyes(*eyes, *rig->eyes, morph.drawn, animation.boneMatrices, transform, ShownSize(creature), milliseconds,
			              [this](uint32_t range) { return std::uniform_int_distribution<uint32_t>(0, range - 1)(_random); });
		    }
	    });

	// The glows on the creatures' bodies follow their poses. Their textures slide by the real time and the circling glow
	// turns by the game's time, at most once a frame of the screen's refresh.
	const auto realTime = Locator::time::has_value() ? Locator::time::value().GetFrameRealTime() : std::chrono::milliseconds(0);
	std::vector<entt::entity> glowing;
	registry.Each<const Creature, CreatureHandGlows>([&glowing](entt::entity entity, const Creature& /*creature*/,
	                                                            CreatureHandGlows& /*glows*/) { glowing.push_back(entity); });
	for (const auto entity : glowing)
	{
		auto& glows = registry.Get<CreatureHandGlows>(entity);
		const auto slideMs = frame_pace::Step(glows.slidePace, realTime, realTime);
		const auto turnMs = frame_pace::Step(glows.turnPace, realTime, std::chrono::milliseconds(milliseconds));
		PlaceHandGlows(entity, static_cast<int32_t>(slideMs.count()), static_cast<int32_t>(turnMs.count()));
	}
}

void CreatureAnimationSystem::PoseCopy(entt::entity copy, float milliseconds)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto* self = registry.TryGet<const CreatureCopy>(copy);
	if (self == nullptr || !registry.Valid(self->creature))
	{
		return;
	}
	const auto* creature = registry.TryGet<const Creature>(self->creature);
	const auto* morph = registry.TryGet<const CreatureMorph>(self->creature);
	auto* animation = registry.TryGet<CreatureAnimation>(copy);
	const auto* transform = registry.TryGet<const Transform>(copy);
	if (creature == nullptr || morph == nullptr || animation == nullptr || transform == nullptr)
	{
		return;
	}
	auto& resources = Locator::resources::value();
	const auto& meshes = resources.GetMeshes();
	const auto& rigs = resources.GetCreatureRigs();
	const auto bodyMeshes =
	    creature_morph::MeshesOf(creature->species, morph->drawn, [&meshes](entt::id_type id) { return meshes.Contains(id); });
	if (!meshes.Contains(bodyMeshes.base))
	{
		return;
	}
	const auto base = meshes.Handle(bodyMeshes.base);
	const auto rigId = creature::GetRigId(creature->species);
	const CreatureRig* rig = rigs.Contains(rigId) ? &*rigs.Handle(rigId) : nullptr;
	const float seconds = milliseconds / 1000.0f;

	// The copy is shaped as the creature is: its rest pose and animations follow the creature's body as it changes
	if (animation->builtRevision != morph->revision || animation->skeleton.Empty())
	{
		const auto rest = creature_animation::BlendRest(
		    base->GetBoneMatrices(), meshes.Handle(bodyMeshes.evilGood)->GetBoneMatrices(), std::abs(morph->drawn.evilGood),
		    meshes.Handle(bodyMeshes.thinFat)->GetBoneMatrices(), std::abs(morph->drawn.thinFat));
		animation->skeleton = skeletal_animation::Skeleton::FromRestMatrices(base->GetBoneParents(), rest);
		animation->animations.clear();
		animation->pair.reset();
		animation->builtRevision = morph->revision;
		animation->boneMatrices = rest;
		auto mirror = rig != nullptr ? rig->MirrorBones(rest.size()) : std::nullopt;
		animation->mirror = mirror.has_value() ? std::move(*mirror) : skeletal_animation::MirrorJoints(rest);
	}
	// It breathes at the pace of its own size
	if (animation->breathPeriod <= 0.0f)
	{
		animation->breathPeriod = creature_animation::BreathPeriod(self->size);
	}
	animation->breathPhase = creature_animation::AdvanceBreath(animation->breathPhase, seconds, animation->breathPeriod);
	if (rig != nullptr)
	{
		PoseBody(*animation, *rig, morph->drawn, *transform, self->size, milliseconds, seconds,
		         HeadLookOfCopy(*animation, *transform, creature->species, self->size), [this](uint32_t range) {
			         return range == 0 ? 0u : std::uniform_int_distribution<uint32_t>(0, range - 1)(_random);
		         });
	}
	if (animation->boneMatrices.size() != base->GetBoneMatrices().size())
	{
		animation->boneMatrices = base->GetBoneMatrices();
	}

	// Its eyes are the creature's, looking where the copy's head looks
	auto* eyes = registry.TryGet<CreatureEyes>(copy);
	if (eyes == nullptr)
	{
		return;
	}
	if (const auto* own = registry.TryGet<const CreatureEyes>(self->creature))
	{
		eyes->mode = own->mode;
		eyes->openness = own->openness;
	}
	eyes->lookAt = animation->lookAt;
	eyes->blink =
	    creature_eyes::AdvanceBlink(eyes->blink, static_cast<int32_t>(std::lround(milliseconds)), [this](uint32_t range) {
		    return range == 0 ? 0u : std::uniform_int_distribution<uint32_t>(0, range - 1)(_random);
	    });
	eyes->drawn = {};
	if (rig != nullptr && rig->eyes.has_value())
	{
		PlaceEyes(*eyes, *rig->eyes, morph->drawn, animation->boneMatrices, *transform, self->size,
		          static_cast<int32_t>(std::lround(milliseconds)),
		          [this](uint32_t range) { return std::uniform_int_distribution<uint32_t>(0, range - 1)(_random); });
	}
}

namespace
{
/// A creature's animation blended as its body is drawn, if it has the animation and has been posed
const Animation* PosedAnimationOf(entt::entity creature, size_t index)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto* body = registry.TryGet<const Creature>(creature);
	const auto* morph = registry.TryGet<const CreatureMorph>(creature);
	auto* animation = registry.TryGet<CreatureAnimation>(creature);
	if (body == nullptr || morph == nullptr || animation == nullptr || animation->skeleton.Empty())
	{
		return nullptr;
	}
	const auto& rigs = Locator::resources::value().GetCreatureRigs();
	const auto rigId = creature::GetRigId(body->species);
	if (!rigs.Contains(rigId))
	{
		return nullptr;
	}
	return AnimationOf(*animation, *rigs.Handle(rigId), morph->drawn, index);
}
} // namespace

std::optional<glm::vec3> CreatureAnimationSystem::BoneInAnimation(entt::entity creature, size_t animation, float timeMs,
                                                                  uint32_t bone, bool mirrored)
{
	const auto* played = PosedAnimationOf(creature, animation);
	const auto* stand = PosedAnimationOf(creature, creature_layers::animations::k_Stand);
	if (played == nullptr || stand == nullptr)
	{
		return std::nullopt;
	}
	const auto& body = Locator::entitiesRegistry::value().Get<CreatureAnimation>(creature);
	if (bone >= body.skeleton.parents.size())
	{
		return std::nullopt;
	}
	const auto time = static_cast<uint32_t>(std::clamp(timeMs, 0.0f, static_cast<float>(std::max(played->duration, 1u) - 1)));
	const auto poses = skeletal_animation::SampleCycle(
	    *played, *stand, time, body.skeleton, mirrored ? std::span<const uint32_t>(body.mirror) : std::span<const uint32_t> {});
	const auto matrices = skeletal_animation::ComposeBoneMatrices(poses, body.skeleton.parents);
	return bone < matrices.size() ? std::optional(glm::vec3(matrices[bone][3])) : std::nullopt;
}

std::optional<glm::vec3> CreatureAnimationSystem::MiddleOfBonesInAnimation(entt::entity creature, size_t animation,
                                                                           float timeMs, std::span<const uint32_t> bones)
{
	const auto* played = PosedAnimationOf(creature, animation);
	const auto* stand = PosedAnimationOf(creature, creature_layers::animations::k_Stand);
	if (played == nullptr || stand == nullptr || bones.empty())
	{
		return std::nullopt;
	}
	const auto& body = Locator::entitiesRegistry::value().Get<CreatureAnimation>(creature);
	const auto time = static_cast<uint32_t>(std::clamp(timeMs, 0.0f, static_cast<float>(std::max(played->duration, 1u) - 1)));
	const auto poses = skeletal_animation::SampleCycle(*played, *stand, time, body.skeleton, {});
	const auto matrices = skeletal_animation::ComposeBoneMatrices(poses, body.skeleton.parents);
	if (std::ranges::any_of(bones, [&matrices](uint32_t bone) { return bone >= matrices.size(); }))
	{
		return std::nullopt;
	}
	glm::vec3 sum(0.0f);
	for (const auto bone : bones)
	{
		sum += glm::vec3(matrices[bone][3]);
	}
	return sum / static_cast<float>(bones.size());
}

std::optional<float> CreatureAnimationSystem::AnimationDuration(entt::entity creature, size_t animation)
{
	const auto* played = PosedAnimationOf(creature, animation);
	return played != nullptr ? std::optional(static_cast<float>(played->duration)) : std::nullopt;
}

std::optional<glm::vec3> CreatureAnimationSystem::AnimationTravel(entt::entity creature, size_t animation)
{
	const auto* played = PosedAnimationOf(creature, animation);
	return played != nullptr ? std::optional(played->displacement) : std::nullopt;
}

std::optional<creature_kiss::KissPoints> CreatureAnimationSystem::KissPointsOf(entt::entity creature)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto* self = registry.TryGet<const Creature>(creature);
	const auto* morph = registry.TryGet<const CreatureMorph>(creature);
	const auto* transform = registry.TryGet<const Transform>(creature);
	const auto& rigs = Locator::resources::value().GetCreatureRigs();
	if (self == nullptr || morph == nullptr || transform == nullptr || !rigs.Contains(creature::GetRigId(self->species)))
	{
		return std::nullopt;
	}
	const auto& rig = *rigs.Handle(creature::GetRigId(self->species));
	const auto* stand = PosedAnimationOf(creature, creature_layers::animations::k_Stand);
	if (stand == nullptr)
	{
		return std::nullopt;
	}
	const auto& body = registry.Get<CreatureAnimation>(creature);
	// The lips are the point on the body that kisses, or else the head bone. Without the animation they stay at the
	// origin of the creature.
	const auto lipsIn = [&](size_t index) {
		const auto* played = PosedAnimationOf(creature, index);
		if (played == nullptr)
		{
			return glm::vec3(0.0f);
		}
		const auto time = static_cast<uint32_t>(
		    std::clamp(rig.kissMs.value_or(0.0f), 0.0f, static_cast<float>(std::max(played->duration, 1u) - 1)));
		const auto poses = skeletal_animation::SampleCycle(*played, *stand, time, body.skeleton, {});
		const auto matrices = skeletal_animation::ComposeBoneMatrices(poses, body.skeleton.parents);
		glm::vec3 lips(0.0f);
		if (rig.kissPoint.has_value())
		{
			const auto& point = *rig.kissPoint;
			const auto triangle = creature::PosedTriangle(point.vertices, point.bones, morph->drawn, matrices, glm::mat4(1.0f),
			                                              creature::k_SkinPointOrder);
			lips = creature_eyes::PointOnTriangle(triangle[0], triangle[1], triangle[2], point.u, point.v).position;
		}
		else if (rig.actionPoints.has_value() && rig.actionPoints->head < matrices.size())
		{
			lips = glm::vec3(matrices[rig.actionPoints->head][3]);
		}
		return lips * transform->scale;
	};
	return creature_kiss::KissPoints {.high = lipsIn(creature_kiss::k_HighKiss), .low = lipsIn(creature_kiss::k_LowKiss)};
}

void CreatureAnimationSystem::KickSway(entt::entity creature, glm::vec3 force, glm::vec3 point)
{
	auto& registry = Locator::entitiesRegistry::value();
	if (auto* animation = registry.TryGet<CreatureAnimation>(creature);
	    animation != nullptr && registry.AllOf<Creature>(creature))
	{
		Kick(registry, creature, *animation, force, point);
	}
}

namespace
{
/// What a creature or a copy of one is drawn from: its species, the morph of its body and the size it shows
struct BodyOf
{
	CreatureType species;
	const CreatureMorph* morph;
	float size;
};

std::optional<BodyOf> FindBody(const ecs::Registry& registry, entt::entity entity)
{
	if (const auto* creature = registry.TryGet<const Creature>(entity); creature != nullptr)
	{
		const auto* morph = registry.TryGet<const CreatureMorph>(entity);
		return morph != nullptr ? std::optional(BodyOf {creature->species, morph, ShownSize(*creature)}) : std::nullopt;
	}
	const auto* copy = registry.TryGet<const CreatureCopy>(entity);
	if (copy == nullptr || !registry.Valid(copy->creature))
	{
		return std::nullopt;
	}
	const auto* creature = registry.TryGet<const Creature>(copy->creature);
	const auto* morph = registry.TryGet<const CreatureMorph>(copy->creature);
	if (creature == nullptr || morph == nullptr)
	{
		return std::nullopt;
	}
	return BodyOf {creature->species, morph, copy->size};
}

const CreatureRig* RigOf(CreatureType species)
{
	const auto& rigs = Locator::resources::value().GetCreatureRigs();
	const auto id = creature::GetRigId(species);
	return rigs.Contains(id) ? &*rigs.Handle(id) : nullptr;
}
} // namespace

void CreatureAnimationSystem::PlaceHandGlows(entt::entity creature, int32_t frameMs, int32_t gameMs)
{
	using creature_hand_glow::Place;
	auto& registry = Locator::entitiesRegistry::value();
	auto* hand = registry.TryGet<CreatureHandGlows>(creature);
	const auto* animation = registry.TryGet<const CreatureAnimation>(creature);
	const auto* transform = registry.TryGet<const Transform>(creature);
	const auto body = FindBody(registry, creature);
	if (hand == nullptr || animation == nullptr || transform == nullptr || !body.has_value())
	{
		return;
	}
	const auto* rig = RigOf(body->species);
	const auto placement = creature::PlacementMatrix(transform->position, transform->rotation, transform->scale);
	const std::span<const glm::mat4> bones = animation->boneMatrices;
	const auto posed = [&](uint32_t bone) { return creature::PosedBone(bone, bones, placement); };
	for (size_t i = 0; i < creature_hand_glow::k_PlaceCount; ++i)
	{
		auto& glow = hand->glows.at(i);
		auto& placed = hand->placed.at(i);
		placed.reset();
		if (!glow.has_value())
		{
			continue;
		}
		const auto place = static_cast<Place>(i);
		glm::mat3 axes(1.0f);
		glm::vec3 point(0.0f);
		if (place == Place::Circling)
		{
			// It turns about the up axis above the body
			creature_hand_glow::Turn(*glow, gameMs);
			axes = creature_hand_glow::CirclingAxes(glow->turn);
			point = creature_hand_glow::CirclingPoint(transform->position, body->size);
		}
		else
		{
			if (rig == nullptr || !rig->actionPoints.has_value())
			{
				continue;
			}
			auto bone = place == Place::Belly ? rig->actionPoints->belly : rig->actionPoints->rightHand;
			if (place == Place::LeftHand)
			{
				bone = bone < animation->mirror.size() ? animation->mirror[bone] : static_cast<uint32_t>(bones.size());
			}
			if (bone >= bones.size())
			{
				continue;
			}
			const auto world = posed(bone);
			axes = creature_hand_glow::UnitAxes(glm::mat3(world));
			point = glm::vec3(world[3]);
			// A hand glow is at the middle of the ends of the fingers
			if (place != Place::Belly)
			{
				std::vector<glm::vec3> ends;
				for (const auto end : creature_hand_glow::BranchEnds(animation->skeleton.parents, bone))
				{
					if (end < bones.size())
					{
						ends.emplace_back(posed(end)[3]);
					}
				}
				point = creature_hand_glow::Middle(ends);
			}
		}
		// The shell reaches out by the power it had when it was drawn, and its texture slides on
		const float zoom = creature_hand_glow::Zoom(*glow);
		creature_hand_glow::Slide(*glow, frameMs);
		placed = CreatureHandGlows::Placed {
		    .model = creature_hand_glow::ModelMatrix(axes, point, transform->scale.x, glow->scale),
		    .zoom = zoom,
		    .slide = glow->slide,
		};
	}
}

std::optional<glm::vec3> CreatureAnimationSystem::MiddleOfGroinAndLips(entt::entity creature)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto* animation = registry.TryGet<const CreatureAnimation>(creature);
	const auto* transform = registry.TryGet<const Transform>(creature);
	const auto body = FindBody(registry, creature);
	if (animation == nullptr || transform == nullptr || !body.has_value())
	{
		return std::nullopt;
	}
	const auto* rig = RigOf(body->species);
	const std::span<const glm::mat4> bones = animation->boneMatrices;
	if (rig == nullptr || !rig->actionPoints.has_value() || rig->actionPoints->groin >= bones.size())
	{
		return std::nullopt;
	}
	const auto placement = creature::PlacementMatrix(transform->position, transform->rotation, transform->scale);
	const auto groin = glm::vec3(creature::PosedBone(rig->actionPoints->groin, bones, placement)[3]);
	// The lips are the point on the body that kisses, else the head bone
	glm::vec3 lips(0.0f);
	if (rig->kissPoint.has_value())
	{
		const auto& kiss = *rig->kissPoint;
		if (std::ranges::any_of(kiss.bones, [&bones](uint32_t bone) { return bone >= bones.size(); }))
		{
			return std::nullopt;
		}
		const auto triangle = creature::PosedTriangle(kiss.vertices, kiss.bones, body->morph->drawn, bones, placement,
		                                              creature::k_SkinPointOrder);
		lips = creature_eyes::PointOnTriangle(triangle[0], triangle[1], triangle[2], kiss.u, kiss.v).position;
	}
	else if (rig->actionPoints->head < bones.size())
	{
		lips = glm::vec3(creature::PosedBone(rig->actionPoints->head, bones, placement)[3]);
	}
	else
	{
		return std::nullopt;
	}
	const float half = 0.5f;
	return glm::vec3((lips.x + groin.x) * half, (groin.y + lips.y) * half, (groin.z + lips.z) * half);
}

void CreatureAnimationSystem::AddAlignmentMotesOf(entt::entity copy)
{
	if (!Locator::dynamicsSystem::has_value() || !Locator::gameRandom::has_value())
	{
		return;
	}
	auto& registry = Locator::entitiesRegistry::value();
	const auto* animation = registry.TryGet<const CreatureAnimation>(copy);
	const auto* transform = registry.TryGet<const Transform>(copy);
	const auto body = FindBody(registry, copy);
	if (animation == nullptr || transform == nullptr || !body.has_value() || !registry.AllOf<CreatureCopy>(copy) ||
	    !creature_alignment_motes::MakesAny(body->morph->drawn.evilGood))
	{
		return;
	}
	auto& random = Locator::gameRandom::value();
	const creature_alignment_motes::LocalRandom localRandom = [&random](int32_t n) { return random.LocalRand(n); };
	const auto placement = creature::PlacementMatrix(transform->position, transform->rotation, transform->scale);
	const creature_alignment_motes::BonePoint bonePoint = [animation, &placement](uint32_t bone) {
		return glm::vec3(creature::PosedBone(bone, animation->boneMatrices, placement)[3]);
	};
	auto& dynamics = Locator::dynamicsSystem::value();
	const auto bones = static_cast<uint32_t>(animation->boneMatrices.size());
	for (const auto& puff :
	     creature_alignment_motes::PuffsOf(body->morph->drawn.evilGood, body->size, bones, bonePoint, localRandom))
	{
		dynamics.AddPuff(puff.position, puff.velocity, puff.size, puff.argb, puff.kind);
	}
}
