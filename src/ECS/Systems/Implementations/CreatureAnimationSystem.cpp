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
#include <string>
#include <string_view>
#include <tuple>
#include <utility>
#include <vector>

#include <entt/entity/entity.hpp>
#include <glm/gtx/transform.hpp>
#include <spdlog/spdlog.h>

#include "3D/CreatureBody.h"
#include "3D/L3DMesh.h"
#include "3D/LandIslandInterface.h"
#include "3D/LandLightFrame.h"
#include "Common/FramePace.h"
#include "Common/GameRandom.h"
#include "Common/X87.h"
#include "Creature/CreatureAlignmentMotes.h"
#include "Creature/CreatureAnimation.h"
#include "Creature/CreatureAnimationLog.h"
#include "Creature/CreatureBlendStore.h"
#include "Creature/CreatureEyes.h"
#include "Creature/CreatureFight.h"
#include "Creature/CreatureHandGlow.h"
#include "Creature/CreatureHeadLook.h"
#include "Creature/CreatureLayers.h"
#include "Creature/CreatureLocomotion.h"
#include "Creature/CreatureLook.h"
#include "Creature/CreatureMindTables.h"
#include "Creature/CreatureMorph.h"
#include "Creature/CreatureObjectActions.h"
#include "Creature/CreatureProportions.h"
#include "Creature/CreaturePuke.h"
#include "Creature/CreatureReach.h"
#include "Creature/CreatureRig.h"
#include "Creature/CreatureShownFace.h"
#include "Creature/CreatureSway.h"
#include "Creature/CreatureTurnPose.h"
#include "Creature/CreatureWalkPlanner.h"
#include "ECS/Archetypes/CreatureArchetype.h"
#include "ECS/Components/Creature.h"
#include "ECS/Components/CreatureBody.h"
#include "ECS/Components/CreatureCopy.h"
#include "ECS/Components/CreatureFight.h"
#include "ECS/Components/CreatureHandGlows.h"
#include "ECS/Components/CreatureLocomotion.h"
#include "ECS/Components/CreatureMind.h"
#include "ECS/Components/CreatureObjectAction.h"
#include "ECS/Components/ShownCreatureFace.h"
#include "ECS/Components/Transform.h"
#include "ECS/CreaturePlacement.h"
#include "ECS/Registry.h"
#include "ECS/Systems/CreatureAnimationLogSystemInterface.h"
#include "ECS/Systems/CreatureHandSystemInterface.h"
#include "ECS/Systems/CreatureLocomotionSystemInterface.h"
#include "ECS/Systems/CreatureMindSystemInterface.h"
#include "ECS/Systems/LiquidParticleSystemInterface.h"
#include "ECS/Systems/SoundTagSystemInterface.h"
#include "ECS/Systems/TimeSystemInterface.h"
#include "ECS/Systems/VideoSystemInterface.h"
#include "InfoConstants.h"
#include "Locator.h"
#include "Physics/LivingRules.h"
#include "Resources/ResourcesInterface.h"
#include "Sound/SoundId.h"

using namespace openblack;
using namespace openblack::ecs::systems;
using namespace openblack::ecs::components;
using openblack::creature::CreatureRig;
using openblack::skeletal_animation::Animation;
using AnimationStores = creature_blends::Stores;

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
	target.evilGood = morph.alignmentLook.value_or(target.evilGood);
	target.weakStrong = morph.strengthLook.value_or(target.weakStrong);
	return target;
}

/// The animations of the first pose of a body and of its first plan: its stand and the two turns of its head. A body
/// that did not measure its reach yet also blends the four animations that measure how far its hand reaches.
constexpr std::array k_FirstPoseBlends = {creature_animation::k_StandAnimation, creature_layers::animations::k_LookRightLeft,
                                          creature_layers::animations::k_LookDownUp};

/// An eye's point on the body as drawn: each vertex blended as the body is, placed by its bone, in the world
std::optional<creature_eyes::SurfacePoint> EyePointOf(const CreatureRig::EyePoint& point, const creature_morph::Morph& morph,
                                                      creature::PosedBones& posed)
{
	if (!point.enabled)
	{
		return std::nullopt;
	}
	const auto world = creature::PosedTriangle(point.vertices, point.bones, morph, posed, creature::k_SkinPointOrder);
	return creature_eyes::PointOnTriangle(world[0], world[1], world[2], point.u, point.v);
}

template <typename Random>
void PlaceEyes(CreatureEyes& eyes, const CreatureRig::Eyes& rig, const creature_morph::Morph& morph,
               const std::vector<glm::mat4>& bones, const Transform& transform, float size, int32_t milliseconds,
               Random&& random)
{
	// The points of both eyes share bones, so each bone is placed in the world one time
	creature::PosedBones posed(bones, creature::PlacementMatrix(transform.position, transform.rotation, transform.scale));
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
	for (size_t i = 0; i < eyes.drawn.size(); ++i)
	{
		auto& drawn = eyes.drawn.at(i);
		drawn = {};
		const auto point = EyePointOf(rig.points.at(i), morph, posed);
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
		const auto anchor = EyePointOf(rig.points.at(i + 2), morph, posed);
		const auto placed =
		    creature_eyes::PlaceEye(*point, anchor ? std::optional<glm::vec3>(anchor->position) : std::nullopt,
		                            rig.points.at(i).depth, i == 1, look, rig.lidAngles, eyeSize, turn, milliseconds);
		drawn.eyeball = placed.eyeball;
		drawn.eyelid = placed.eyelid;
	}
}
/// A blended animation of the creature's, blended the first time it is wanted, or nothing when the species has none
const Animation* AnimationOf(AnimationStores& stores, CreatureAnimation& animation, const CreatureRig& rig,
                             const creature_morph::Morph& morph, size_t index)
{
	auto found = animation.animations.find(index);
	if (found == animation.animations.end())
	{
		// A species without the animation keeps an empty one, so as not to look for it again. A blend that the system
		// made before the turn asked for it, or that another body of the same shape made, is the same blend.
		auto blended = creature_blends::KeptBlend(stores, rig, morph, index);
		if (!blended.has_value())
		{
			blended = creature_blends::Blend(stores, rig, morph, index);
			if (blended.has_value())
			{
				creature_blends::Keep(stores, rig, morph, index, *blended);
			}
		}
		found = animation.animations.emplace(index, std::move(blended).value_or(Animation {})).first;
	}
	return found->second.frames.empty() ? nullptr : &found->second;
}

/// A turn's or step's pair of the creature's animations blended into one, kept while the same pair plays, or nothing
/// when the species lacks either
const Animation* PairAnimationOf(AnimationStores& stores, CreatureAnimation& animation, const CreatureRig& rig,
                                 const creature_morph::Morph& morph, size_t from, size_t to, float weight)
{
	auto& pair = animation.pair;
	if (!pair.has_value() || pair->from != from || pair->to != to || pair->weight != weight)
	{
		const auto* first = AnimationOf(stores, animation, rig, morph, from);
		const auto* second = AnimationOf(stores, animation, rig, morph, to);
		const auto* stand = AnimationOf(stores, animation, rig, morph, creature_layers::animations::k_Stand);
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
             const CreatureAnimation& animation, std::span<const uint32_t> mirror, skeletal_animation::KeyframeTurns& turns)
{
	if (look == nullptr || !creature_head_look::PlaysHeadClip(angle, animation.lookAt.has_value()))
	{
		return;
	}
	const auto reference = (look->frames.size() - 1) / 2;
	skeletal_animation::AddLayer(poses, *look, creature_layers::LookTime(angle, limit, look->duration), reference,
	                             animation.skeleton, mirror, &turns);
}

/// The swaying body leaning on top of the pose: for the lower body then the upper, its offset across the creature's own
/// axes picks how far through the sideways and the front to back lean to draw, each relative to its middle frame
void AddSway(std::vector<skeletal_animation::Pose>& poses, const CreatureAnimation& animation, const Transform& transform,
             const auto& animationOf, skeletal_animation::KeyframeTurns& turns)
{
	if (!animation.sway.active)
	{
		return;
	}
	auto side = glm::normalize(transform.rotation[0]);
	auto front = glm::normalize(transform.rotation[2]);
	// The body leans along its heading, or the heading that a turn on the spot has reached: the FPU's cosine and sine,
	// each rounded to a float
	if (animation.sway.heading.has_value())
	{
		const auto heading = static_cast<double>(animation.sway.heading->Radians());
		const auto cosine = x87::RoundToFloat(x87::Cos(heading));
		const auto sine = x87::RoundToFloat(x87::Sin(heading));
		side = glm::vec3(cosine, 0.0f, sine);
		front = glm::vec3(-sine, 0.0f, cosine);
	}
	const auto lean = [&](uint32_t clip, float amount) {
		if (std::abs(amount) <= creature_sway::k_DrawnLean)
		{
			return;
		}
		if (const auto* played = animationOf(clip))
		{
			skeletal_animation::AddLayer(poses, *played, creature_sway::LeanTime(amount, played->duration),
			                             (played->frames.size() - 1) / 2, animation.skeleton, {}, &turns);
		}
	};
	for (const auto& [offset, sideClip, frontClip] :
	     {std::tuple {animation.sway.lowerOffset, creature_sway::k_LowerSide, creature_sway::k_LowerFrontBack},
	      std::tuple {animation.sway.upperOffset, creature_sway::k_UpperSide, creature_sway::k_UpperFrontBack}})
	{
		// Only the parts on the ground count, each lean the two products added
		lean(sideClip, (side.x * offset.x) + (side.z * offset.z));
		lean(frontClip, (front.z * offset.z) + (front.x * offset.x));
	}
}

/// What the head turns towards this frame, and how
struct HeadLook
{
	creature_head_look::Pose pose {creature_head_look::Pose::Standing};
	/// The point that the head turns to, or none to turn back to straight ahead
	std::optional<glm::vec3> target;
	/// The heading that the body shows
	GameHeading facing {};
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

/// The clips that a pose used, for the developer log of the clips (see creature_animation_log)
struct PoseReport
{
	std::vector<creature_animation_log::Clip> clips;
	/// The clip of the body's own action that the species does not have. The action ended at once.
	std::optional<size_t> missingBody;
};

/// A face that only this computer shows, and the bones that the pose gives with it
struct ShownFacePose
{
	creature_layers::FaceLayer& face;
	std::vector<glm::mat4>& boneMatrices;
};

/// A clip of the log, from the animation that the pose sampled, or from none when the species does not have it
creature_animation_log::Clip LogClip(creature_animation_log::Layer layer, size_t index, float timeMs, const Animation* played)
{
	return {
	    .layer = layer,
	    .animation = index,
	    .timeMs = timeMs,
	    .found = played != nullptr,
	    .looping = played != nullptr && played->looping,
	    .durationMs = played != nullptr ? std::optional<uint32_t>(played->duration) : std::nullopt,
	};
}

/// The body posed for this frame: its action or breathing, or its slots blended, then the arm that holds a thing, the
/// head turned, the face and any gesture on top. The body's animations move on by bodyStepMs, the face by milliseconds.
void PoseBody(AnimationStores& stores, skeletal_animation::KeyframeTurns& turns, CreatureAnimation& animation,
              const CreatureRig& rig, const creature_morph::Morph& morph, const Transform& transform, float size,
              float milliseconds, float seconds, float bodyStepMs, const HeadLook& look,
              const std::function<uint32_t(uint32_t)>& random, std::optional<HoldingHand> holding = std::nullopt,
              PoseReport* report = nullptr, const ShownFacePose* shown = nullptr)
{
	const auto* stand = AnimationOf(stores, animation, rig, morph, creature_layers::animations::k_Stand);
	if (stand == nullptr)
	{
		return;
	}
	const auto playbackMs = bodyStepMs;
	const auto animationOf = [&](size_t index) { return AnimationOf(stores, animation, rig, morph, index); };

	// A species without the start, the first move or the finish of the dance does not dance
	if (animation.body.kind == creature_layers::BodyAction::Kind::Dance &&
	    (animationOf(creature_layers::animations::k_DanceStart) == nullptr ||
	     animationOf(creature_layers::animations::k_FirstDanceMove) == nullptr ||
	     animationOf(creature_layers::animations::k_DanceFinish) == nullptr))
	{
		if (report != nullptr)
		{
			report->missingBody = creature_layers::CurrentAnimation(animation.body);
		}
		animation.body = {};
	}
	const auto bodyDuration = DurationOf(animationOf(creature_layers::TimingAnimation(animation.body)));
	if (report != nullptr && creature_layers::IsPlaying(animation.body) && !bodyDuration.has_value())
	{
		report->missingBody = creature_layers::TimingAnimation(animation.body);
	}
	animation.actionMoment = animation.actionMoment || creature_layers::ReachesActionMoment(
	                                                       animation.body, playbackMs, bodyDuration,
	                                                       static_cast<uint32_t>(rig.kissMs.value_or(0.0f)), milliseconds);
	animation.body = creature_layers::AdvanceBody(animation.body, playbackMs, bodyDuration, milliseconds);
	// A dance that needs its next move chooses one of the moves that the species has
	if (animation.body.chooseDanceMove)
	{
		const auto move = creature_layers::ChooseDanceMove(animation.body.firstDanceMove, random,
		                                                   [&](size_t index) { return animationOf(index) != nullptr; });
		animation.body.animations[1] = move.value_or(creature_layers::animations::k_Stand);
		animation.body.chooseDanceMove = false;
	}
	const auto advanceFace = [&](const creature_layers::FaceLayer& face) {
		return creature_layers::AdvanceFace(face, milliseconds,
		                                    DurationOf(face.current.has_value() ? animationOf(*face.current) : nullptr));
	};
	animation.face = advanceFace(animation.face);
	// The face that only this computer shows moves on as the face of the world does
	if (shown != nullptr)
	{
		shown->face = advanceFace(shown->face);
	}
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
			const auto* played = slot.blendedWith.has_value() ? PairAnimationOf(stores, animation, rig, morph, slot.animation,
			                                                                    *slot.blendedWith, slot.blendWeight)
			                                                  : animationOf(slot.animation);
			sampled.push_back(skeletal_animation::SampleCycle(
			    played != nullptr ? *played : *stand, *stand, static_cast<uint32_t>(std::max(slot.timeMs, 0.0f)),
			    animation.skeleton, slot.mirrored ? animation.mirror : std::span<const uint32_t> {}, &turns));
			weights.push_back(slot.weight);
			if (report != nullptr)
			{
				auto clip = LogClip(creature_animation_log::Layer::Blend, slot.animation, slot.timeMs, played);
				clip.weight = slot.weight;
				clip.mirrored = slot.mirrored;
				clip.blendedWith = slot.blendedWith;
				clip.blendWeight = slot.blendWeight;
				report->clips.push_back(clip);
			}
		}
		poses = skeletal_animation::WeightedSumInMeshSpace(sampled, weights, animation.skeleton.parents);
	}
	else if (const auto* played =
	             !creature_layers::IsPlaying(animation.body) ? nullptr
	             : animation.body.blendedWith.has_value()
	                 ? PairAnimationOf(stores, animation, rig, morph, creature_layers::CurrentAnimation(animation.body),
	                                   *animation.body.blendedWith, animation.body.blendWeight)
	                 : animationOf(creature_layers::CurrentAnimation(animation.body));
	         played != nullptr)
	{
		const auto time = creature_layers::DanceMoveTime(animation.body, played->duration);
		poses = skeletal_animation::SampleCycle(*played, *stand, static_cast<uint32_t>(std::max(time, 0.0f)),
		                                        animation.skeleton, mirror, &turns);
		if (report != nullptr)
		{
			auto clip =
			    LogClip(creature_animation_log::Layer::Body, creature_layers::CurrentAnimation(animation.body), time, played);
			clip.mirrored = animation.body.mirrored;
			clip.blendedWith = animation.body.blendedWith;
			clip.blendWeight = animation.body.blendWeight;
			report->clips.push_back(clip);
		}
	}
	else
	{
		const auto time = creature_animation::BreathTime(animation.breathPhase, stand->duration);
		poses = skeletal_animation::SampleCycle(*stand, *stand, time, animation.skeleton, {}, &turns);
		if (report != nullptr)
		{
			report->clips.push_back(LogClip(creature_animation_log::Layer::Body, creature_layers::animations::k_Stand,
			                                static_cast<float>(time), stand));
		}
	}

	// A small creature has a large head, large hands and feet and large eyes for its size
	creature_proportions::Apply(poses, creature_proportions::ScaledBones(ProportionBonesOf(rig), animation.mirror, size));
	// The arm that holds a thing takes the last frame of the hold animation, on the side of the hand that holds it. The
	// hand that holds the thing gets its size for the size of the creature again.
	if (const auto* hold = holding.has_value() ? animationOf(creature_layers::animations::k_PickUpHold) : nullptr)
	{
		skeletal_animation::SetLayer(
		    poses, *hold, hold->duration > 0 ? hold->duration - 1 : 0, animation.skeleton, nullptr,
		    holding->mirrored ? std::span<const uint32_t>(animation.mirror) : std::span<const uint32_t> {}, &turns);
		const std::array hand {creature_proportions::BoneScale {
		    .bone = holding->bone, .factor = creature_proportions::Factor(creature_proportions::k_HandsAndFeet, size)}};
		creature_proportions::Apply(poses, hand);
	}

	// During the start, the loop and the end of the sit, the head turns by the sitting versions. The head turns the other way
	// when the body plays mirrored.
	const bool sitting = creature_head_look::UsesSittingClips(look.pose);
	AddLook(poses,
	        animationOf(sitting ? creature_layers::animations::k_SitLookDownUp : creature_layers::animations::k_LookDownUp),
	        animation.pitch.angle, creature_layers::k_PitchLimit, animation, mirror, turns);
	AddLook(
	    poses,
	    animationOf(sitting ? creature_layers::animations::k_SitLookRightLeft : creature_layers::animations::k_LookRightLeft),
	    mirror.empty() ? animation.yaw.angle : -animation.yaw.angle, creature_layers::k_YawLimit, animation, mirror, turns);
	// The sway leans the body between the head's turning and the face
	AddSway(poses, animation, transform, animationOf, turns);
	// The face, relative to its first frame
	const auto addFace = [&](std::vector<skeletal_animation::Pose>& to, const creature_layers::FaceLayer& face,
	                         PoseReport* log) {
		if (!face.current.has_value())
		{
			return;
		}
		if (log != nullptr)
		{
			log->clips.push_back(
			    LogClip(creature_animation_log::Layer::Face, *face.current, face.timeMs, animationOf(*face.current)));
		}
		if (const auto* played = animationOf(*face.current))
		{
			skeletal_animation::AddLayer(to, *played, static_cast<uint32_t>(std::max(face.timeMs, 0.0f)), 0, animation.skeleton,
			                             {}, &turns);
		}
	};
	// The gestures and a blow's wobble, relative to their first frames, on top of the face
	const auto addGestures = [&](std::vector<skeletal_animation::Pose>& to, PoseReport* log) {
		if (animation.gesture.animation.has_value())
		{
			if (log != nullptr)
			{
				log->clips.push_back(LogClip(creature_animation_log::Layer::Gesture, *animation.gesture.animation,
				                             animation.gesture.timeMs, animationOf(*animation.gesture.animation)));
			}
			if (const auto* gesture = animationOf(*animation.gesture.animation))
			{
				skeletal_animation::AddLayer(to, *gesture, static_cast<uint32_t>(std::max(animation.gesture.timeMs, 0.0f)), 0,
				                             animation.skeleton, {}, &turns);
			}
		}
		// The wobble plays the way the body plays
		if (animation.wobble.animation.has_value())
		{
			if (log != nullptr)
			{
				log->clips.push_back(LogClip(creature_animation_log::Layer::Wobble, *animation.wobble.animation,
				                             animation.wobble.timeMs, animationOf(*animation.wobble.animation)));
			}
			if (const auto* wobble = animationOf(*animation.wobble.animation))
			{
				skeletal_animation::AddLayer(to, *wobble, static_cast<uint32_t>(std::max(animation.wobble.timeMs, 0.0f)), 0,
				                             animation.skeleton, mirror, &turns);
			}
		}
	};
	// The bones with the face that only this computer shows go to their own matrices. The bones of the world do not
	// change.
	if (shown != nullptr)
	{
		auto drawn = poses;
		addFace(drawn, shown->face, nullptr);
		addGestures(drawn, nullptr);
		skeletal_animation::ComposeBoneMatrices(drawn, animation.skeleton.parents, shown->boneMatrices);
	}
	addFace(poses, animation.face, report);
	addGestures(poses, report);
	skeletal_animation::ComposeBoneMatrices(poses, animation.skeleton.parents, animation.boneMatrices);
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
	const auto pose = creature::TurnPose(animation->newerTurnPose, animation->boneMatrices,
	                                     creature::PlacementMatrix(transform.position, transform.rotation, transform.scale));
	return glm::vec3(creature::PosedBone(points->head, pose.bones, pose.placement)[3]);
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
HeadLook HeadLookOf(AnimationStores& stores, ecs::Registry& registry, entt::entity entity, CreatureAnimation& animation,
                    const CreatureRig* rig, const creature_morph::Morph& morph, const Transform& transform,
                    const Creature& creature, int32_t milliseconds)
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
	// An action with a thing that ended stays on the creature until the next one starts
	const auto* action = registry.TryGet<const CreatureObjectAction>(entity);
	if (action != nullptr && action->status != creature_object_actions::Status::Running)
	{
		action = nullptr;
	}
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
			                       ? AnimationOf(stores, animation, *rig, morph, creature_head_look::k_PointTimingClip)
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
	// Part way through a spin to face a point, the head turns from the heading that the spin has reached
	if (facts.pose == Pose::TurningToFace && walk != nullptr && walk->move.has_value() && walk->move->animations.has_value() &&
	    walk->move->durationMs > 0.0f)
	{
		look.facing =
		    creature_sway::TurnHeading(walk->heading, walk->targetHeading, walk->move->timeMs, walk->move->durationMs);
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
	std::vector<entt::entity> creatures;
	registry.Each<const Creature>(
	    [&creatures](entt::entity entity, const Creature& /*creature*/) { creatures.push_back(entity); });
	for (const auto creature : creatures)
	{
		BodyTurn(creature);
	}
	for (const auto creature : creatures)
	{
		BreathTurn(creature);
	}
	for (const auto creature : creatures)
	{
		SwayTurn(creature);
	}
	for (const auto creature : creatures)
	{
		MotesTurn(creature);
	}
}

void CreatureAnimationSystem::BodyTurn(entt::entity creature)
{
	// While the player's hand holds the creature to interact with it, the body keeps its looks
	if (Locator::creatureHandSystem::has_value() && Locator::creatureHandSystem::value().IsLocked(creature))
	{
		return;
	}
	// At the end of the turn the body takes the looks of what the creature is now, also of a strength that only fades
	ecs::archetypes::CreatureArchetype::TakeLooks(Locator::entitiesRegistry::value(), creature);
}

void CreatureAnimationSystem::BreathTurn(entt::entity creature)
{
	auto& registry = Locator::entitiesRegistry::value();
	// Each game turn the breath goes back to the resting period of the size that the creature shows, before its mind
	// starts a walk that makes it breathe faster
	const auto* body = registry.TryGet<const Creature>(creature);
	auto* animation = registry.TryGet<CreatureAnimation>(creature);
	if (body != nullptr && animation != nullptr)
	{
		const auto resting = creature_animation::BreathPeriod(ShownSize(*body));
		animation->breathPeriod = resting;
		animation->breathTarget = resting;
	}
}

void CreatureAnimationSystem::SwayTurn(entt::entity entity)
{
	auto& registry = Locator::entitiesRegistry::value();
	// Each game turn the body sways from what kicked it, by the seconds that the creature lived in the turn
	const auto* creature = registry.TryGet<const Creature>(entity);
	const auto* morph = registry.TryGet<const CreatureMorph>(entity);
	auto* animation = registry.TryGet<CreatureAnimation>(entity);
	if (creature == nullptr || morph == nullptr || animation == nullptr)
	{
		return;
	}
	auto& sway = animation->sway;
	sway.turnSeconds = creature_sway::TurnSeconds(animation->playbackScale);
	creature_sway::Step(sway, sway.turnSeconds, MassOf(*creature, morph));
	sway.heading.reset();
	const auto* walk = registry.TryGet<const CreatureLocomotion>(entity);
	if (walk != nullptr && walk->motion == CreatureLocomotion::Motion::Turning && walk->move.has_value() &&
	    walk->move->animations.has_value() && walk->move->durationMs > 0.0f)
	{
		sway.heading =
		    creature_sway::TurnHeading(walk->heading, walk->targetHeading, walk->move->timeMs, walk->move->durationMs);
	}
	else if (walk != nullptr)
	{
		sway.heading = walk->heading;
	}
}

void CreatureAnimationSystem::MotesTurn(entt::entity entity)
{
	if (!Locator::liquidParticleSystem::has_value() || !Locator::gameRandom::has_value())
	{
		return;
	}
	auto& registry = Locator::entitiesRegistry::value();
	const auto* creature = registry.TryGet<const Creature>(entity);
	const auto* morph = registry.TryGet<const CreatureMorph>(entity);
	auto* animation = registry.TryGet<CreatureAnimation>(entity);
	const auto* transform = registry.TryGet<const Transform>(entity);
	if (creature == nullptr || morph == nullptr || animation == nullptr || transform == nullptr)
	{
		return;
	}
	auto& liquids = Locator::liquidParticleSystem::value();
	auto& random = Locator::gameRandom::value();
	const creature_alignment_motes::LocalRandom localRandom = [&random](int32_t n) { return random.LocalRand(n); };
	// While the falling spell plays, no creature in the world makes motes
	const bool fallingSpell = Locator::videoSystem::has_value() && Locator::videoSystem::value().HidesWorld();
	// Each game turn, a creature whose body is to look good or evil makes motes or smoke near the bones of the pose of the
	// turn before. The body takes the pose of this turn after the motes. Then a creature that pukes brings up its sick.
	const auto placement = creature::PlacementMatrix(transform->position, transform->rotation, transform->scale);
	std::vector<glm::vec3> bones(animation->boneMatrices.size());
	for (uint32_t bone = 0; bone < bones.size(); ++bone)
	{
		bones[bone] = glm::vec3(creature::PosedBone(bone, animation->boneMatrices, placement)[3]);
	}
	// Before the first turn ends, the bones of this frame are the only pose there is
	const auto& last = animation->lastTurnBones.size() == bones.size() ? animation->lastTurnBones : bones;
	const auto alignment = TargetMorph(*creature, *morph).evilGood;
	if (!fallingSpell && creature_alignment_motes::MakesAny(alignment))
	{
		const creature_alignment_motes::BonePoint bonePoint = [&last](uint32_t bone) { return last.at(bone); };
		for (const auto& puff : creature_alignment_motes::PuffsOf(alignment, ShownSize(*creature),
		                                                          static_cast<uint32_t>(last.size()), bonePoint, localRandom))
		{
			liquids.Add(puff.position, puff.velocity, puff.size, puff.argb, puff.kind);
		}
	}
	AddPukeDrops(entity);
}

namespace
{
/// Builds the skeleton and the rest pose of a body for the shape that it is drawn with, when the shape changed since
/// they were built or there are none yet. It is true when the body has a base mesh.
bool BuildSkeleton(const Creature& creature, const CreatureMorph& morph, CreatureAnimation& animation)
{
	auto& resources = Locator::resources::value();
	const auto& meshes = resources.GetMeshes();
	const auto& rigs = resources.GetCreatureRigs();
	const auto bodyMeshes =
	    creature_morph::MeshesOf(creature.species, morph.drawn, [&meshes](entt::id_type id) { return meshes.Contains(id); });
	if (!meshes.Contains(bodyMeshes.base))
	{
		return false;
	}
	if (animation.builtRevision == morph.revision && !animation.skeleton.Empty())
	{
		return true;
	}
	const auto base = meshes.Handle(bodyMeshes.base);
	const auto rigId = creature::GetRigId(creature.species);
	const CreatureRig* rig = rigs.Contains(rigId) ? &*rigs.Handle(rigId) : nullptr;
	// The rest pose and the animations follow the evil to good and thin to fat axes
	const auto evilGood = meshes.Handle(bodyMeshes.evilGood);
	const auto thinFat = meshes.Handle(bodyMeshes.thinFat);
	auto rest =
	    creature_animation::BlendRest(base->GetBoneMatrices(), evilGood->GetBoneMatrices(), std::abs(morph.drawn.evilGood),
	                                  thinFat->GetBoneMatrices(), std::abs(morph.drawn.thinFat));
	animation.skeleton = skeletal_animation::Skeleton::FromRestMatrices(base->GetBoneParents(), rest);
	animation.animations.clear();
	animation.pair.reset();
	animation.builtRevision = morph.revision;
	// Each bone's mirror as the species' file has it
	auto mirror = rig != nullptr ? rig->MirrorBones(rest.size()) : std::nullopt;
	animation.mirror = mirror.has_value() ? std::move(*mirror) : skeletal_animation::MirrorJoints(rest);
	animation.boneMatrices = std::move(rest);
	return true;
}
} // namespace

void CreatureAnimationSystem::MorphTurn(entt::entity entity)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto* creature = registry.TryGet<const Creature>(entity);
	auto* morph = registry.TryGet<CreatureMorph>(entity);
	auto* animation = registry.TryGet<CreatureAnimation>(entity);
	if (creature == nullptr || morph == nullptr || animation == nullptr || !registry.AllOf<Transform>(entity))
	{
		return;
	}
	const auto refresh = creature_morph::RefreshDrawn(morph->drawn, TargetMorph(*creature, *morph));
	morph->drawn = refresh.drawn;
	if (refresh.animations)
	{
		++morph->revision;
	}
	BuildSkeleton(*creature, *morph, *animation);
}

void CreatureAnimationSystem::ShapeBodies()
{
	auto& registry = Locator::entitiesRegistry::value();
	// A blend that no turn took after two shapes goes, so that the store does not grow
	auto& stores = BlendStores();
	++stores.shaping;
	creature_blends::ForgetOld(stores, stores.shaping);
	std::vector<entt::entity> unposed;
	registry.Each<const Creature, const CreatureMorph, CreatureAnimation, const Transform>(
	    [&](entt::entity entity, const Creature& creature, const CreatureMorph& morph, CreatureAnimation& animation,
	        const Transform& /*transform*/) {
		    if (!BuildSkeleton(creature, morph, animation))
		    {
			    return;
		    }
		    // Before its first game turn, the body takes its first pose, and the animations do not move on
		    if (animation.newerTurnPose.empty() || animation.newerTurnPose.size() != animation.skeleton.parents.size())
		    {
			    unposed.push_back(entity);
		    }
	    });
	PrepareFirstBlends(unposed);
	for (const auto entity : unposed)
	{
		auto& animation = registry.Get<CreatureAnimation>(entity);
		animation.olderTurnPose.clear();
		animation.newerTurnPose.clear();
		// The first pose stands for the pose of the next turn
		PoseForTurn(entity, 0.0f);
	}
}

void CreatureAnimationSystem::PrepareFirstPoses()
{
	PrepareFirstBlends(Unposed());
}

void CreatureAnimationSystem::StartFirstBlends()
{
	auto wanted = FirstBlendsWanted(Unposed());
	// One blend is made when the turn asks for it, as the threads do not make it sooner
	if (wanted.size() < 2)
	{
		return;
	}
	auto& stores = BlendStores();
	_background.Start(stores, std::move(wanted), Workers(), stores.shaping);
}

std::vector<entt::entity> CreatureAnimationSystem::Unposed()
{
	std::vector<entt::entity> unposed;
	Locator::entitiesRegistry::value().Each<const Creature, const CreatureAnimation>(
	    [&unposed](entt::entity entity, const Creature& /*creature*/, const CreatureAnimation& animation) {
		    if (animation.newerTurnPose.empty())
		    {
			    unposed.push_back(entity);
		    }
	    });
	return unposed;
}

std::vector<creature_blends::Wanted> CreatureAnimationSystem::FirstBlendsWanted(std::span<const entt::entity> creatures)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto& rigs = Locator::resources::value().GetCreatureRigs();
	std::vector<creature_blends::Wanted> wanted;
	for (const auto entity : creatures)
	{
		const auto* body = registry.TryGet<const Creature>(entity);
		const auto* morph = registry.TryGet<const CreatureMorph>(entity);
		const auto* animation = registry.TryGet<const CreatureAnimation>(entity);
		if (body == nullptr || morph == nullptr || animation == nullptr || !rigs.Contains(creature::GetRigId(body->species)))
		{
			continue;
		}
		const auto& rig = *rigs.Handle(creature::GetRigId(body->species));
		// A blend that the creature keeps already is not made again
		const auto want = [&](size_t index) {
			if (!animation->animations.contains(index))
			{
				wanted.push_back({.rig = &rig, .morph = morph->drawn, .index = index});
			}
		};
		std::ranges::for_each(k_FirstPoseBlends, want);
		if (!registry.AllOf<PickUpReach>(entity))
		{
			std::ranges::for_each(creature_reach::k_PickUpAnimations, want);
		}
	}
	return wanted;
}

void CreatureAnimationSystem::PrepareFirstBlends(std::span<const entt::entity> creatures)
{
	const auto wanted = FirstBlendsWanted(creatures);
	// One blend is made when the turn asks for it, as the threads do not make it sooner
	if (wanted.size() < 2)
	{
		return;
	}
	auto& stores = BlendStores();
	creature_blends::Prepare(stores, wanted, Workers(), stores.shaping);
}

creature_blends::Stores& CreatureAnimationSystem::BlendStores()
{
	// The blends that the threads make while the turn does other steps go into the stores first
	_background.Finish();
	return _stores;
}

WorkerPool& CreatureAnimationSystem::Workers()
{
	if (_workers == nullptr)
	{
		_workers = std::make_unique<WorkerPool>(WorkerPool::DefaultThreadCount());
	}
	return *_workers;
}

void CreatureAnimationSystem::Update(std::chrono::duration<float, std::milli> gameTime)
{
	auto& registry = Locator::entitiesRegistry::value();
	auto& resources = Locator::resources::value();
	const auto& meshes = resources.GetMeshes();
	const auto& rigs = resources.GetCreatureRigs();
	const auto milliseconds = static_cast<int32_t>(std::lround(gameTime.count()));
	// The body is drawn between the poses of the last two game turns, by the part of the turn that has passed
	const auto weight =
	    creature_turn_pose::DrawWeight(Locator::time::has_value() ? Locator::time::value().GetTurnFraction() : 1.0f);

	registry.Each<const Creature, const CreatureMorph, CreatureAnimation, const Transform>(
	    [&](entt::entity entity, const Creature& creature, const CreatureMorph& morph, CreatureAnimation& animation,
	        const Transform& turnTransform) {
		    const auto bodyMeshes = creature_morph::MeshesOf(creature.species, morph.drawn,
		                                                     [&meshes](entt::id_type id) { return meshes.Contains(id); });
		    if (!meshes.Contains(bodyMeshes.base))
		    {
			    return;
		    }
		    const auto base = meshes.Handle(bodyMeshes.base);
		    const auto rigId = creature::GetRigId(creature.species);
		    const CreatureRig* rig = rigs.Contains(rigId) ? &*rigs.Handle(rigId) : nullptr;
		    // A walking body is drawn between the places of its last two turns
		    const auto transform = DrawnTransform(turnTransform, &animation);

		    // A face that only this computer shows is drawn with the bones that its own pose gave
		    const auto* shown = registry.TryGet<const ShownCreatureFace>(entity);
		    const bool drawsShown =
		        shown != nullptr && !shown->newerTurnPose.empty() && shown->olderTurnPose.size() == shown->newerTurnPose.size();
		    const auto& older = drawsShown ? shown->olderTurnPose : animation.olderTurnPose;
		    const auto& newer = drawsShown ? shown->newerTurnPose : animation.newerTurnPose;
		    if (!newer.empty() && older.size() == newer.size() && transform.scale.x != 0.0f && transform.scale.y != 0.0f &&
		        transform.scale.z != 0.0f)
		    {
			    animation.boneMatrices.resize(newer.size());
			    creature_turn_pose::BlendToMesh(
			        older, newer, weight, creature::PlacementMatrix(transform.position, transform.rotation, transform.scale),
			        animation.boneMatrices);
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
		    eyes->blink = creature_eyes::AdvanceBlink(
		        eyes->blink, milliseconds, [this](uint32_t range) { return _random.LocalRand(static_cast<int32_t>(range)); });
		    eyes->drawn = {};
		    if (rig != nullptr && rig->eyes.has_value())
		    {
			    PlaceEyes(*eyes, *rig->eyes, morph.drawn, animation.boneMatrices, transform, ShownSize(creature), milliseconds,
			              [this](uint32_t range) { return _random.LocalRand(static_cast<int32_t>(range)); });
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
		PlaceHandGlows(registry, entity, static_cast<int32_t>(slideMs.count()), static_cast<int32_t>(turnMs.count()));
	}
}

namespace
{
std::string_view StageName(CreatureFighting::Stage stage)
{
	using Stage = CreatureFighting::Stage;
	switch (stage)
	{
	case Stage::FaceOff:
		return "face off";
	case Stage::Taunt:
		return "taunt";
	case Stage::Point:
		return "point";
	case Stage::Approach:
		return "approach";
	case Stage::Ready:
		return "ready";
	case Stage::Square:
		return "square";
	case Stage::Duel:
		return "duel";
	case Stage::Celebrate:
		return "celebrate";
	case Stage::Respond:
		return "respond";
	case Stage::Stop:
		return "stop";
	}
	return "fight";
}

std::string_view BodyKindName(creature_layers::BodyAction::Kind kind)
{
	using Kind = creature_layers::BodyAction::Kind;
	switch (kind)
	{
	case Kind::Stand:
		return "stand";
	case Kind::Once:
		return "once";
	case Kind::Sequence:
		return "sequence";
	case Kind::Dance:
		return "dance";
	}
	return "stand";
}

std::string_view PhaseName(creature_layers::BodyAction::Phase phase)
{
	using Phase = creature_layers::BodyAction::Phase;
	switch (phase)
	{
	case Phase::Start:
		return "start";
	case Phase::Loop:
		return "loop";
	case Phase::End:
		return "end";
	}
	return "start";
}

/// What asked a creature's body for the clips of its pose: its mind, the action with a thing, the fight or the walk
creature_animation_log::Cause CauseOf(const ecs::Registry& registry, entt::entity entity, const CreatureAnimation& animation)
{
	creature_animation_log::Cause cause;
	// An action with a thing that ended stays on the creature until the next one starts
	const auto* action = registry.TryGet<const CreatureObjectAction>(entity);
	if (action != nullptr && action->status != creature_object_actions::Status::Running)
	{
		action = nullptr;
	}
	const auto* fighting = registry.TryGet<const CreatureFighting>(entity);
	if (action != nullptr)
	{
		cause.objectAction = std::string(creature_object_actions::Name(action->kind));
		cause.objectAction += action->phase == CreatureObjectAction::Phase::Playing ? " (playing)" : " (approach)";
	}
	if (fighting != nullptr)
	{
		cause.fightStage =
		    std::string(StageName(fighting->stage)) + ", " + std::string(creature_fight::Name(fighting->fighter.state));
	}
	if (animation.slots.empty())
	{
		cause.by = "body";
	}
	else if (action != nullptr && action->phase == CreatureObjectAction::Phase::Playing)
	{
		cause.by = "object action";
	}
	else if (fighting != nullptr)
	{
		cause.by = "fight";
	}
	else
	{
		cause.by = "walk";
	}
	cause.body = std::string(BodyKindName(animation.body.kind));
	if (animation.body.kind == creature_layers::BodyAction::Kind::Sequence ||
	    animation.body.kind == creature_layers::BodyAction::Kind::Dance)
	{
		cause.body += " " + std::string(PhaseName(animation.body.phase));
	}
	if (const auto* mind = registry.TryGet<const CreatureMindState>(entity); mind != nullptr)
	{
		cause.activity = std::string(creature_mind::Name(mind->idle.activity));
		if (mind->planner.current.has_value())
		{
			cause.planAction = mind->planner.current->action;
			const auto* tables =
			    Locator::creatureMindSystem::has_value() ? Locator::creatureMindSystem::value().GetTables() : nullptr;
			if (tables != nullptr && *cause.planAction < tables->actions.size())
			{
				cause.planActionName = tables->actions[*cause.planAction].name;
			}
		}
		if (mind->idle.step < mind->idle.agenda.size())
		{
			const auto& step = mind->idle.agenda[mind->idle.step];
			cause.agendaStep = mind->idle.step;
			cause.agendaStepKind = static_cast<uint8_t>(step.kind);
			cause.agendaStepClip = step.animation;
		}
	}
	return cause;
}
} // namespace

void CreatureAnimationSystem::PoseForTurn(entt::entity entity, float milliseconds)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto* creature = registry.TryGet<const Creature>(entity);
	const auto* morph = registry.TryGet<const CreatureMorph>(entity);
	auto* animation = registry.TryGet<CreatureAnimation>(entity);
	const auto* transform = registry.TryGet<const Transform>(entity);
	if (creature == nullptr || morph == nullptr || animation == nullptr || transform == nullptr || animation->skeleton.Empty())
	{
		return;
	}
	const auto& rigs = Locator::resources::value().GetCreatureRigs();
	const auto rigId = creature::GetRigId(creature->species);
	if (rigs.Contains(rigId))
	{
		const auto& rig = *rigs.Handle(rigId);
		// A frozen creature lives fewer whole milliseconds in the turn, down to none. The body's animations move on by
		// these milliseconds at the rate of its size, cut to whole milliseconds.
		const auto turnMs = creature_walk_planner::TurnMs(animation->playbackScale, static_cast<int32_t>(milliseconds));
		const auto stepMs = creature_layers::TurnStepMs(turnMs, ShownSize(*creature), animation->realTime);
		const auto look = HeadLookOf(BlendStores(), registry, entity, *animation, &rig, morph->drawn, *transform, *creature,
		                             static_cast<int32_t>(std::lround(milliseconds)));
		// The developer log of the clips reads what the pose used, and changes nothing that the pose does
		std::optional<PoseReport> report;
		if (Locator::creatureAnimationLog::has_value())
		{
			report.emplace();
		}
		// The world changes a face that only this computer shows when it changes its own face. If the face of the world
		// changed in a different way since the last pose, the shown face becomes the face of the world, as the one face
		// of the creature does.
		auto* shown = registry.TryGet<ShownCreatureFace>(entity);
		if (shown != nullptr)
		{
			shown->face = creature_shown_face::AfterWorldWrites(shown->face, shown->worldFace, animation->face);
		}
		std::vector<glm::mat4> shownMatrices;
		const auto shownPose =
		    shown != nullptr ? std::optional<ShownFacePose>(ShownFacePose {.face = shown->face, .boneMatrices = shownMatrices})
		                     : std::nullopt;
		// The store of keyframe turns forgets its turns only here, when no pose uses them
		_keyframeTurns.Trim();
		PoseBody(
		    BlendStores(), _keyframeTurns, *animation, rig, morph->drawn, *transform, ShownSize(*creature),
		    static_cast<float>(turnMs), static_cast<float>(turnMs) / 1000.0f, static_cast<float>(stepMs), look,
		    [this](uint32_t range) { return _random.GameRand(range); }, HoldingHandOf(registry, entity),
		    report.has_value() ? &*report : nullptr, shownPose.has_value() ? &*shownPose : nullptr);
		if (shown != nullptr && !creature_shown_face::StaysAfterPose(shown->face, animation->face))
		{
			registry.Remove<ShownCreatureFace>(entity);
			shown = nullptr;
		}
		if (shown != nullptr)
		{
			shown->worldFace = animation->face;
			if (!shownMatrices.empty())
			{
				auto drawn = creature_turn_pose::ToWorld(shownMatrices,
				                                         ecs::creature_placement::PlacementOf(registry, entity, *transform));
				shown->olderTurnPose =
				    shown->newerTurnPose.empty() ? animation->newerTurnPose : std::move(shown->newerTurnPose);
				shown->newerTurnPose = std::move(drawn);
			}
		}
		if (report.has_value())
		{
			auto& log = Locator::creatureAnimationLog::value();
			const auto turn = Locator::time::has_value() ? Locator::time::value().GetTurn() : 0u;
			const auto cause = CauseOf(registry, entity, *animation);
			if (report->missingBody.has_value())
			{
				log.Note(entity, {.problem = creature_animation_log::Problem::Missing,
				                  .layer = creature_animation_log::Layer::Body,
				                  .animation = *report->missingBody,
				                  .turn = turn,
				                  .by = "body",
				                  .detail = "the species has no such clip: the action ended at once and the body stood"});
			}
			log.Posed(entity, turn, report->clips, cause);
		}
	}
	if (animation->boneMatrices.empty())
	{
		return;
	}
	// The pose of the last turn becomes the older pose. The first pose is both poses. The body of the turn leans with the
	// slope of the land under it.
	auto pose = creature_turn_pose::ToWorld(animation->boneMatrices,
	                                        ecs::creature_placement::PlacementOf(registry, entity, *transform));
	animation->olderTurnPose = animation->newerTurnPose.empty() ? pose : std::move(animation->newerTurnPose);
	animation->newerTurnPose = std::move(pose);
	animation->lastTurnBones.resize(animation->newerTurnPose.size());
	std::ranges::transform(animation->newerTurnPose, animation->lastTurnBones.begin(),
	                       [](const glm::mat4& bone) { return glm::vec3(bone[3]); });
	animation->posedThisTurn = true;
}

void CreatureAnimationSystem::PoseTurn(entt::entity creature)
{
	auto& registry = Locator::entitiesRegistry::value();
	auto* animation = registry.TryGet<CreatureAnimation>(creature);
	if (animation == nullptr || animation->posedThisTurn)
	{
		return;
	}
	// The body takes its pose where its turn took it. The walk puts its clips on the body for the end of the turn. When
	// the body plays the animations of what it does with a thing, these animations make the pose and not the walk clips.
	if (Locator::creatureLocomotionSystem::has_value())
	{
		const auto* action = registry.TryGet<const CreatureObjectAction>(creature);
		const bool acting = action != nullptr && creature_object_actions::PosesBody(
		                                             action->phase == CreatureObjectAction::Phase::Playing, action->status);
		auto actionSlots = acting ? animation->slots : decltype(animation->slots) {};
		Locator::creatureLocomotionSystem::value().ShowTurnEnd(creature);
		if (acting)
		{
			animation->slots = std::move(actionSlots);
		}
	}
	constexpr auto k_TurnMs = std::chrono::duration<float, std::milli>(TimeSystemInterface::k_TurnDuration).count();
	BreatheForTurn(creature, k_TurnMs);
	PoseForTurn(creature, k_TurnMs);
}

void CreatureAnimationSystem::BreatheForTurn(entt::entity creature, float milliseconds)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto* body = registry.TryGet<const Creature>(creature);
	auto* animation = registry.TryGet<CreatureAnimation>(creature);
	if (body == nullptr || animation == nullptr)
	{
		return;
	}
	// Standing, the creature breathes in and out over its stand animation, from its first frame at its resting period.
	// Each game turn, before the body takes its pose, the period moves one step towards its target and the breath moves on
	// by the time of the turn. The frames do not change the breath.
	constexpr auto k_TurnSeconds = std::chrono::duration<float>(TimeSystemInterface::k_TurnDuration).count();
	const auto resting = creature_animation::BreathPeriod(ShownSize(*body));
	if (animation->breathPeriod <= 0.0f)
	{
		animation->breathPeriod = resting;
	}
	if (animation->breathTarget <= 0.0f)
	{
		animation->breathTarget = animation->breathPeriod;
	}
	animation->breathPeriod =
	    creature_animation::EaseBreathPeriod(animation->breathPeriod, animation->breathTarget, resting, k_TurnSeconds);
	// A frozen creature plays slower, down to not at all
	const auto seconds = milliseconds * animation->playbackScale / 1000.0f;
	animation->breathPhase = creature_animation::AdvanceBreath(animation->breathPhase, seconds, animation->breathPeriod);
}

void CreatureAnimationSystem::PoseTurns()
{
	auto& registry = Locator::entitiesRegistry::value();
	std::vector<entt::entity> creatures;
	registry.Each<const Creature, CreatureAnimation>(
	    [&creatures](entt::entity entity, const Creature& /*creature*/, CreatureAnimation& /*animation*/) {
		    creatures.push_back(entity);
	    });
	for (const auto entity : creatures)
	{
		PoseTurn(entity);
	}
	registry.Each<CreatureAnimation>([](CreatureAnimation& animation) { animation.posedThisTurn = false; });
}

void CreatureAnimationSystem::EndPoseTurn(entt::entity creature)
{
	auto& registry = Locator::entitiesRegistry::value();
	if (!registry.AllOf<Creature, CreatureAnimation>(creature))
	{
		return;
	}
	PoseTurn(creature);
	// The next turn of the creature starts with no pose
	registry.Get<CreatureAnimation>(creature).posedThisTurn = false;
}

void CreatureAnimationSystem::HoldTurnPose(entt::entity creature)
{
	auto& registry = Locator::entitiesRegistry::value();
	if (auto* animation = registry.TryGet<CreatureAnimation>(creature); animation != nullptr)
	{
		animation->olderTurnPose = animation->newerTurnPose;
	}
	if (auto* shown = registry.TryGet<ShownCreatureFace>(creature); shown != nullptr)
	{
		shown->olderTurnPose = shown->newerTurnPose;
	}
}

void CreatureAnimationSystem::PoseCopy(Registry& copies, entt::entity copy, float milliseconds, const Registry& bodies)
{
	const auto* self = copies.Valid(copy) ? copies.TryGet<const CreatureCopy>(copy) : nullptr;
	if (self == nullptr || !bodies.Valid(self->creature))
	{
		return;
	}
	const auto* creature = bodies.TryGet<const Creature>(self->creature);
	const auto* morph = bodies.TryGet<const CreatureMorph>(self->creature);
	auto* animation = copies.TryGet<CreatureAnimation>(copy);
	const auto* transform = copies.TryGet<const Transform>(copy);
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
		_keyframeTurns.Trim();
		PoseBody(BlendStores(), _keyframeTurns, *animation, *rig, morph->drawn, *transform, self->size, milliseconds, seconds,
		         milliseconds * creature_layers::BodyRate(self->size, animation->realTime),
		         HeadLookOfCopy(*animation, *transform, creature->species, self->size),
		         [this](uint32_t range) { return _random.GameRand(range); });
	}
	if (animation->boneMatrices.size() != base->GetBoneMatrices().size())
	{
		animation->boneMatrices = base->GetBoneMatrices();
	}

	// Its eyes are the creature's, looking where the copy's head looks
	auto* eyes = copies.TryGet<CreatureEyes>(copy);
	if (eyes == nullptr)
	{
		return;
	}
	if (const auto* own = bodies.TryGet<const CreatureEyes>(self->creature))
	{
		eyes->mode = own->mode;
		eyes->openness = own->openness;
	}
	eyes->lookAt = animation->lookAt;
	eyes->blink =
	    creature_eyes::AdvanceBlink(eyes->blink, static_cast<int32_t>(std::lround(milliseconds)),
	                                [this](uint32_t range) { return _random.LocalRand(static_cast<int32_t>(range)); });
	eyes->drawn = {};
	if (rig != nullptr && rig->eyes.has_value())
	{
		PlaceEyes(*eyes, *rig->eyes, morph->drawn, animation->boneMatrices, *transform, self->size,
		          static_cast<int32_t>(std::lround(milliseconds)),
		          [this](uint32_t range) { return _random.LocalRand(static_cast<int32_t>(range)); });
	}
}

namespace
{
/// A creature's animation blended as its body is drawn, if it has the animation and has been posed
const Animation* PosedAnimationOf(AnimationStores& stores, entt::entity creature, size_t index)
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
	return AnimationOf(stores, *animation, *rigs.Handle(rigId), morph->drawn, index);
}
} // namespace

std::optional<glm::vec3> CreatureAnimationSystem::BoneInAnimation(entt::entity creature, size_t animation, float timeMs,
                                                                  uint32_t bone, bool mirrored)
{
	const auto* played = PosedAnimationOf(BlendStores(), creature, animation);
	const auto* stand = PosedAnimationOf(BlendStores(), creature, creature_layers::animations::k_Stand);
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
	const auto* played = PosedAnimationOf(BlendStores(), creature, animation);
	const auto* stand = PosedAnimationOf(BlendStores(), creature, creature_layers::animations::k_Stand);
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
	const auto* played = PosedAnimationOf(BlendStores(), creature, animation);
	return played != nullptr ? std::optional(static_cast<float>(played->duration)) : std::nullopt;
}

std::optional<glm::vec3> CreatureAnimationSystem::AnimationTravel(entt::entity creature, size_t animation)
{
	const auto* played = PosedAnimationOf(BlendStores(), creature, animation);
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
	const auto* stand = PosedAnimationOf(BlendStores(), creature, creature_layers::animations::k_Stand);
	if (stand == nullptr)
	{
		return std::nullopt;
	}
	const auto& body = registry.Get<CreatureAnimation>(creature);
	// The lips are the point on the body that kisses, or else the head bone. Without the animation they stay at the
	// origin of the creature.
	const auto lipsIn = [&](size_t index) {
		const auto* played = PosedAnimationOf(BlendStores(), creature, index);
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

void CreatureAnimationSystem::Hurry(entt::entity creature)
{
	auto& registry = Locator::entitiesRegistry::value();
	if (!registry.Valid(creature))
	{
		return;
	}
	if (auto* animation = registry.TryGet<CreatureAnimation>(creature))
	{
		animation->body = creature_layers::Hurry(animation->body);
	}
	// An action on a thing hurries only while it plays its animations: the walk to the thing keeps its pace
	if (auto* action = registry.TryGet<CreatureObjectAction>(creature);
	    action != nullptr && action->phase == CreatureObjectAction::Phase::Playing &&
	    (action->status == creature_object_actions::Status::Running ||
	     action->status == creature_object_actions::Status::Contact) &&
	    creature_object_actions::Hurries(action->kind))
	{
		action->hurried = true;
	}
	if (auto* fighting = registry.TryGet<CreatureFighting>(creature);
	    fighting != nullptr && creature_fight::Hurries(fighting->fighter.state))
	{
		fighting->fighter.hurried = true;
	}
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

/// The body of a creature, or of the creature of a copy. `things` is the registry of the creature or the copy, and `land`
/// the registry of the creature of a copy.
std::optional<BodyOf> FindBody(const ecs::Registry& things, const ecs::Registry& land, entt::entity entity)
{
	if (const auto* creature = things.TryGet<const Creature>(entity); creature != nullptr)
	{
		const auto* morph = things.TryGet<const CreatureMorph>(entity);
		return morph != nullptr ? std::optional(BodyOf {creature->species, morph, ShownSize(*creature)}) : std::nullopt;
	}
	const auto* copy = things.TryGet<const CreatureCopy>(entity);
	if (copy == nullptr || !land.Valid(copy->creature))
	{
		return std::nullopt;
	}
	const auto* creature = land.TryGet<const Creature>(copy->creature);
	const auto* morph = land.TryGet<const CreatureMorph>(copy->creature);
	if (creature == nullptr || morph == nullptr)
	{
		return std::nullopt;
	}
	return BodyOf {creature->species, morph, copy->size};
}

std::optional<BodyOf> FindBody(const ecs::Registry& registry, entt::entity entity)
{
	return FindBody(registry, registry, entity);
}

const CreatureRig* RigOf(CreatureType species)
{
	const auto& rigs = Locator::resources::value().GetCreatureRigs();
	const auto id = creature::GetRigId(species);
	return rigs.Contains(id) ? &*rigs.Handle(id) : nullptr;
}
} // namespace

void CreatureAnimationSystem::AddPukeDrops(entt::entity creature)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto* animation = registry.TryGet<const CreatureAnimation>(creature);
	const auto* transform = registry.TryGet<const Transform>(creature);
	const auto body = FindBody(registry, creature);
	if (animation == nullptr || transform == nullptr || !body.has_value() ||
	    creature_layers::CurrentAnimation(animation->body) != creature_layers::animations::k_Puke)
	{
		return;
	}
	const auto* rig = RigOf(body->species);
	if (rig == nullptr)
	{
		return;
	}
	// The sick comes out of the body as it is posed at the end of its newest game turn
	const auto pose = creature::TurnPose(animation->newerTurnPose, animation->boneMatrices,
	                                     creature::PlacementMatrix(transform->position, transform->rotation, transform->scale));
	const std::span<const glm::mat4> bones = pose.bones;
	const auto& placement = pose.placement;
	// The sick comes out of the point at the lips, else out of the head bone
	glm::vec3 lips(0.0f);
	if (rig->pukePoint.has_value())
	{
		const auto& point = *rig->pukePoint;
		if (std::ranges::any_of(point.bones, [&bones](uint32_t bone) { return bone >= bones.size(); }))
		{
			return;
		}
		const auto triangle = creature::PosedTriangle(point.vertices, point.bones, body->morph->drawn, bones, placement,
		                                              creature::k_SkinPointOrder);
		lips = creature_eyes::PointOnTriangle(triangle[0], triangle[1], triangle[2], point.u, point.v).position;
	}
	else if (rig->actionPoints.has_value() && rig->actionPoints->head < bones.size())
	{
		lips = glm::vec3(creature::PosedBone(rig->actionPoints->head, bones, placement)[3]);
	}
	else
	{
		return;
	}
	// Each drop takes a shade of the land's light where the creature stands
	const auto light = FrameLandLightAt(transform->position);
	auto& random = Locator::gameRandom::value();
	auto& liquids = Locator::liquidParticleSystem::value();
	const auto size = creature_puke::DropSize(body->size);
	for (const auto& drop : creature_puke::DropsOf(body->size, light, [&random](int32_t n) { return random.LocalRand(n); }))
	{
		liquids.Add(lips, drop.velocity, size, drop.argb, liquid_particles::Kind::Liquid);
	}
}

void CreatureAnimationSystem::SetCreedPower(entt::entity creature, creature_hand_glow::Place place, float scale, float power)
{
	auto& registry = Locator::entitiesRegistry::value();
	if (!registry.Valid(creature))
	{
		return;
	}
	auto& glows = registry.AllOf<CreatureHandGlows>(creature) ? registry.Get<CreatureHandGlows>(creature)
	                                                          : registry.Assign<CreatureHandGlows>(creature);
	ApplyHum(creature, creature_hand_glow::SetCreedPower(glows.glows, place, scale, power));
}

void CreatureAnimationSystem::LoadCreedPowers(
    entt::entity creature, std::span<const creature_hand_glow::SavedPower, creature_hand_glow::k_PlaceCount> powers)
{
	auto& registry = Locator::entitiesRegistry::value();
	if (!registry.Valid(creature))
	{
		return;
	}
	auto& glows = registry.AllOf<CreatureHandGlows>(creature) ? registry.Get<CreatureHandGlows>(creature)
	                                                          : registry.Assign<CreatureHandGlows>(creature);
	const auto hum = creature_hand_glow::LoadCreedPowers(glows.glows, powers);
	glows.placed = {};
	glows.motion = {};
	ApplyHum(creature, hum);
}

void CreatureAnimationSystem::ApplyHum(entt::entity creature, creature_hand_glow::Hum hum)
{
	if (hum == creature_hand_glow::Hum::Unchanged || !Locator::soundTagSystem::has_value())
	{
		return;
	}
	// The creature keeps up the hum while its glows have power. When they lose it, the hum finishes its loop.
	constexpr auto k_Hum = static_cast<entt::id_type>(audio::SoundId::G_Creed_01);
	auto& tags = Locator::soundTagSystem::value();
	if (hum == creature_hand_glow::Hum::Stop)
	{
		tags.DetachObjectSound(creature, k_Hum);
	}
	else
	{
		// The hum loops. It adds to the other sounds of the creature, for example a script's sound.
		tags.AttachObjectSound(creature, k_Hum, true);
	}
}

void CreatureAnimationSystem::PlayCreedEvent(entt::entity creature, int32_t event)
{
	const auto glow = creature_hand_glow::CreedEventOf(event);
	auto& registry = Locator::entitiesRegistry::value();
	if (!glow.has_value() || !registry.Valid(creature))
	{
		return;
	}
	auto& glows = registry.AllOf<CreatureHandGlows>(creature) ? registry.Get<CreatureHandGlows>(creature)
	                                                          : registry.Assign<CreatureHandGlows>(creature);
	creature_hand_glow::Set(glows.glows, glow->place, creature_hand_glow::k_CreedEventScale, glow->power);
}

void CreatureAnimationSystem::PlaceHandGlows(Registry& things, entt::entity creature, int32_t frameMs, int32_t gameMs)
{
	using creature_hand_glow::Place;
	auto* hand = things.TryGet<CreatureHandGlows>(creature);
	const auto* animation = things.TryGet<const CreatureAnimation>(creature);
	const auto* transform = things.TryGet<const Transform>(creature);
	const auto body = FindBody(things, Locator::entitiesRegistry::value(), creature);
	if (hand == nullptr || animation == nullptr || transform == nullptr || !body.has_value())
	{
		return;
	}
	const auto* rig = RigOf(body->species);
	// The glows follow the body where the frame draws it
	const auto drawn = DrawnTransform(*transform, animation);
	const auto placement = creature::PlacementMatrix(drawn.position, drawn.rotation, drawn.scale);
	const std::span<const glm::mat4> bones = animation->boneMatrices;
	const auto posed = [&](uint32_t bone) { return creature::PosedBone(bone, bones, placement); };
	for (size_t i = 0; i < creature_hand_glow::k_PlaceCount; ++i)
	{
		auto& glow = hand->glows.at(i);
		auto& placed = hand->placed.at(i);
		auto& motion = hand->motion.at(i);
		placed.reset();
		if (!glow.has_value())
		{
			motion = {};
			continue;
		}
		const auto place = static_cast<Place>(i);
		glm::mat3 axes(1.0f);
		glm::vec3 point(0.0f);
		if (place == Place::Circling)
		{
			// It turns about the up axis above the body
			creature_hand_glow::Turn(motion, gameMs);
			axes = creature_hand_glow::CirclingAxes(motion.turn);
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
		creature_hand_glow::Slide(motion, frameMs);
		placed = CreatureHandGlows::Placed {
		    .model = creature_hand_glow::ModelMatrix(axes, point, transform->scale.x, glow->scale),
		    .zoom = zoom,
		    .slide = motion.slide,
		};
	}
}

std::optional<glm::vec3> CreatureAnimationSystem::MiddleOfGroinAndLips(const Registry& things, entt::entity creature)
{
	const auto* animation = things.TryGet<const CreatureAnimation>(creature);
	const auto* transform = things.TryGet<const Transform>(creature);
	const auto body = FindBody(things, Locator::entitiesRegistry::value(), creature);
	if (animation == nullptr || transform == nullptr || !body.has_value())
	{
		return std::nullopt;
	}
	const auto* rig = RigOf(body->species);
	// The points are read as the body is posed at the end of its newest game turn
	const auto pose = creature::TurnPose(animation->newerTurnPose, animation->boneMatrices,
	                                     creature::PlacementMatrix(transform->position, transform->rotation, transform->scale));
	const auto bones = pose.bones;
	const auto& placement = pose.placement;
	if (rig == nullptr || !rig->actionPoints.has_value() || rig->actionPoints->groin >= bones.size())
	{
		return std::nullopt;
	}
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

void CreatureAnimationSystem::AddAlignmentMotesOf(const Registry& copies, entt::entity copy)
{
	if (!Locator::liquidParticleSystem::has_value() || !Locator::gameRandom::has_value())
	{
		return;
	}
	const auto* animation = copies.TryGet<const CreatureAnimation>(copy);
	const auto* transform = copies.TryGet<const Transform>(copy);
	const auto body = FindBody(copies, Locator::entitiesRegistry::value(), copy);
	if (animation == nullptr || transform == nullptr || !body.has_value() || !copies.AllOf<CreatureCopy>(copy) ||
	    !creature_alignment_motes::MakesAny(body->morph->drawn.evilGood))
	{
		return;
	}
	auto& random = Locator::gameRandom::value();
	const creature_alignment_motes::LocalRandom localRandom = [&random](int32_t n) { return random.LocalRand(n); };
	const auto pose = creature::TurnPose(animation->newerTurnPose, animation->boneMatrices,
	                                     creature::PlacementMatrix(transform->position, transform->rotation, transform->scale));
	const creature_alignment_motes::BonePoint bonePoint = [&pose](uint32_t bone) {
		return glm::vec3(creature::PosedBone(bone, pose.bones, pose.placement)[3]);
	};
	auto& liquids = Locator::liquidParticleSystem::value();
	const auto bones = static_cast<uint32_t>(animation->boneMatrices.size());
	for (const auto& puff :
	     creature_alignment_motes::PuffsOf(body->morph->drawn.evilGood, body->size, bones, bonePoint, localRandom))
	{
		liquids.Add(puff.position, puff.velocity, puff.size, puff.argb, puff.kind);
	}
}
