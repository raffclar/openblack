/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#define LOCATOR_IMPLEMENTATIONS

#include <cmath>

#include <algorithm>
#include <optional>
#include <span>
#include <vector>

#include <glm/geometric.hpp>
#include <glm/gtx/vec_swizzle.hpp>

#include "3D/L3DAnim.h"
#include "3D/L3DMesh.h"
#include "3D/LandIslandInterface.h"
#include "Animals/AnimalAnimation.h"
#include "Animals/AnimalRules.h"
#include "ECS/ClipSoundPlayer.h"
#include "ECS/Components/CreatureObjectAction.h"
#include "ECS/Components/HandGrab.h"
#include "ECS/Components/HighDetail.h"
#include "ECS/Components/LivingAction.h"
#include "ECS/Components/LivingPhysics.h"
#include "ECS/Components/Mesh.h"
#include "ECS/Components/Physics.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/Villager.h"
#include "ECS/Components/VillagerPose.h"
#include "ECS/Components/WallHug.h"
#include "ECS/PoseThreads.h"
#include "ECS/Registry.h"
#include "ECS/Systems/TimeSystemInterface.h"
#include "ECS/VillagerAfterDeath.h"
#include "ECS/VillagerDrawRules.h"
#include "Graphics/MeshDetail.h"
#include "Graphics/ViewFrustum.h"
#include "InfoConstants.h"
#include "LivingActionSystem.h"
#include "Locator.h"
#include "Physics/LivingRules.h"
#include "Resources/ResourceManager.h"
#include "Resources/ResourcesInterface.h"

using namespace openblack;
using namespace openblack::ecs::components;
using namespace openblack::ecs::systems;

namespace
{
/// A clip whose stride is shorter than this stays where it is, and plays by the clock
constexpr float k_StationaryStride = 0.05f;
/// How far outside the camera's view a villager is still posed, in metres, should the camera move a little more before
/// the frame is drawn
constexpr float k_ViewMargin = 2.0f;

/// The stores of the components that the posing walks read. The walks make no component, so the stores stay the same for
/// all of a walk.
using PoseStores = ecs::ComponentLookups<InPhysics, InHand, HeldByCreature, HighDetail, WallHug>;

/// How far the ends of a villager's x and z axes lift to lean its model to the slope of the land, as the axes of an
/// animal do. Only a villager at most 0.2 above the land leans. Each axis points along the eased heading and is as long
/// as the size of the villager. Its lift is the height of the land at its end less the drawn height, between -0.3 and
/// 0.3.
std::optional<glm::vec2> SlopeLift(const LandIslandInterface* land, const Transform& transform, const glm::vec3& drawnAt,
                                   float easedHeading)
{
	if (land == nullptr)
	{
		return std::nullopt;
	}
	const float aboveLand = transform.position.y - land->GetHeightAt(glm::xz(transform.position));
	if (aboveLand > animals::k_HighestToFollowSlope)
	{
		return std::nullopt;
	}
	const auto turned = ecs::villager_draw::RotationOf(easedHeading);
	const float scale = transform.scale.x;
	const auto landAt = [&](const glm::vec3& axis) {
		return land->GetAltitude(animals::SlopeSampleUnit(drawnAt.x + (axis.x * scale)),
		                         animals::SlopeSampleUnit(drawnAt.z + (axis.z * scale)));
	};
	return glm::vec2(animals::SlopeLift(landAt(turned[0]), drawnAt.y), animals::SlopeLift(landAt(turned[2]), drawnAt.y));
}

/// Where a villager is drawn this frame and the heading it is drawn with: between its last two turns' places while its
/// clip carries it along, turning after the way it faces. One in the physics, in the god's hand or in the hand of a
/// creature is drawn where it is, turned as it is.
void PlaceDrawing(const PoseStores& stores, entt::entity entity, const Transform& transform, VillagerPose& pose, bool glides,
                  float turnFraction, uint32_t elapsed)
{
	pose.slopeLift.reset();
	if (stores.AnyOf<InPhysics, InHand, HeldByCreature>(entity))
	{
		pose.drawnAt.reset();
		pose.drawnHeading.reset();
		return;
	}
	pose.drawnAt = ecs::villager_draw::DrawnPosition(pose.turnStart.value_or(transform.position), transform.position,
	                                                 turnFraction, glides);
	const auto facing = ecs::villager_draw::HeadingOf(transform.rotation);
	if (!facing.has_value())
	{
		pose.drawnHeading.reset();
		return;
	}
	const auto* highDetail = stores.TryGet<HighDetail>(entity);
	const auto headings =
	    ecs::villager_draw::StepHeadings(pose.easedHeading, pose.detailedHeading, *facing, elapsed, highDetail != nullptr,
	                                     highDetail != nullptr && highDetail->orders.turnAtOnce);
	pose.easedHeading = headings.eased;
	pose.detailedHeading = headings.detailed;
	pose.drawnHeading = headings.drawn;
}

/// Poses a villager between the two keyframes of its clip around its place. A model with no bones, or bones that do not
/// agree with the clip, gets no pose, and the result is false.
bool PoseFromClip(VillagerPose& pose, const L3DAnim& clip, const graphics::L3DMesh& model)
{
	const auto& frames = clip.GetFrames();
	const auto& parents = model.GetBoneParents();
	const auto span =
	    animals::SpanAt({.playTime = clip.GetPlayTime(), .frameCount = frames.size(), .looping = clip.IsLooping()}, pose.place);
	if (!model.IsBoned() || frames.empty() || frames[span.from].bones.size() != parents.size() ||
	    frames[span.to].bones.size() != parents.size())
	{
		pose.bones.clear();
		return false;
	}
	pose.bones.resize(parents.size());
	animals::PoseBetween(frames[span.from].bones, frames[span.to].bones, span.t, parents, pose.bones);
	return true;
}

/// The pose of a high-detail villager that just changed clip, with the old clip's pose held where it was mixed in
void MixInOldClip(VillagerPose& pose, bool turnAtOnce, std::span<const uint32_t> parents, std::vector<glm::mat4>& scratch)
{
	pose.clipBones.clear();
	const auto& blend = pose.clipBlend.blend;
	if (!ecs::villager_draw::ShowsClipBlend(blend, turnAtOnce))
	{
		return;
	}
	const auto& animations = Locator::resources::value().GetAnimations();
	const auto fromId = resources::HashIdentifier(static_cast<uint32_t>(blend.from));
	if (!animations.Contains(fromId))
	{
		return;
	}
	const auto from = animations.Handle(fromId);
	const auto& frames = from->GetFrames();
	const auto span = animals::SpanAt(
	    {.playTime = from->GetPlayTime(), .frameCount = frames.size(), .looping = from->IsLooping()}, blend.fromPlace);
	if (frames.empty() || frames[span.from].bones.size() != parents.size() || frames[span.to].bones.size() != parents.size())
	{
		return;
	}
	scratch.resize(parents.size());
	animals::PoseBetween(frames[span.from].bones, frames[span.to].bones, span.t, parents, scratch);
	// Its shadow keeps to the clip it plays
	pose.clipBones.assign(pose.bones.begin(), pose.bones.end());
	ecs::villager_draw::BlendPoses(pose.bones, scratch, blend.weight);
}
} // namespace

void LivingActionSystem::StartTurnPlaces()
{
	auto& registry = Locator::entitiesRegistry::value();
	registry.Each<const Villager, const Transform, VillagerPose>(
	    [](const Villager& /*unused*/, const Transform& transform, VillagerPose& pose) {
		    // A villager that something put in a new place at once does not count as one that moved
		    pose.movedLastTurn = pose.turnStart.has_value() && glm::xz(*pose.turnStart) != glm::xz(transform.position);
		    pose.turnStart = transform.position;
	    });
}

void LivingActionSystem::UpdatePoses(std::chrono::milliseconds frameGameTime, float turnFraction)
{
	if (!Locator::resources::has_value() || !Locator::infoConstants::has_value())
	{
		return;
	}
	auto& registry = Locator::entitiesRegistry::value();
	const auto elapsed = static_cast<uint32_t>(frameGameTime.count());
	// A screen refresh of the game's, when a clip held at its end plays its sounds again; none while the game is paused
	const bool refresh = Locator::time::has_value() && !Locator::time::value().IsPaused() &&
	                     _soundPace.Step(static_cast<float>(Locator::time::value().GetFrameRealTime().count()));
	const auto& animations = Locator::resources::value().GetAnimations();
	const auto& meshes = Locator::resources::value().GetMeshes();
	const auto& states = Locator::infoConstants::value().villagerStateTable;
	// The souls of the dead rise by the same clock
	ecs::villager_after_death::MoveSouls(elapsed);
	const PoseStores stores(registry);

	// First the main thread finds the clip of each villager, because only the main thread can use the resources. The
	// clip is chosen by the villager's states as they change; here it only plays.
	_clipJobs.clear();
	_keptClips.Clear();
	_keptModels.Clear();
	registry.Each<const Villager, const LivingAction, const Mesh, const Transform, VillagerPose>(
	    [&](entt::entity entity, const Villager& /*unused*/, const LivingAction& action, const Mesh& mesh,
	        const Transform& transform, VillagerPose& pose) {
		    ClipJob job {.entity = entity, .action = &action, .transform = &transform, .pose = &pose};
		    const auto clipId = resources::HashIdentifier(static_cast<uint32_t>(pose.clip));
		    if (static_cast<int>(pose.clip) >= 0 && _keptClips.Contains(animations, clipId) &&
		        _keptModels.Contains(meshes, mesh.id))
		    {
			    job.clip = _keptClips.Find(animations, clipId);
		    }
		    _clipJobs.push_back(job);
	    });

	// Then the threads play the clips on. Each job writes only the pose of its own villager.
	const auto playOne = [&](size_t index, size_t /*lane*/) {
		auto& job = _clipJobs[index];
		auto& pose = *job.pose;
		const auto& transform = *job.transform;
		if (job.clip == nullptr)
		{
			pose.bones.clear();
			PlaceDrawing(stores, job.entity, transform, pose, false, turnFraction, elapsed);
			return;
		}
		const auto& clip = *job.clip;
		const animals::ClipTiming timing {.playTime = clip.GetPlayTime(),
		                                  .frameCount = clip.GetFrames().size(),
		                                  .looping = clip.IsLooping(),
		                                  .playedByTime = true,
		                                  .stride = clip.GetStride()};
		// Villagers play their clips by the clock, except that in the moving states a clip that carries them along
		// plays by the ground they cover
		auto played = static_cast<int32_t>(elapsed);
		const auto state = job.action->states.at(static_cast<size_t>(LivingAction::Index::Top));
		if (const auto* wallHug = stores.TryGet<WallHug>(job.entity);
		    wallHug != nullptr && states.at(state).movesWithGround != 0 && std::abs(timing.stride) >= k_StationaryStride)
		{
			// The whole speed state that the walk speed came from
			const auto speed = static_cast<uint16_t>(GetSpeedStateOf(wallHug->speed));
			// The part of a millisecond each frame leaves is carried on to the next, so that the legs keep pace with
			// the ground at any frame rate (the game dropped it every frame; deviations list)
			const auto carried = animals::MovingPlayCarried(timing, speed, elapsed, transform.scale.x, pose.groundCarry);
			played = carried.played;
			pose.groundCarry = carried.carry;
		}
		// The sounds on the frames that the clip passes play from the villager after all the jobs
		job.clipId = pose.clip;
		job.soundPlace = pose.place;
		job.soundPlayed = static_cast<uint32_t>(played);
		pose.place = animals::AdvanceClip(timing, pose.place, played);
		// While it walks with a clip that carries it along, it is drawn gliding from where it stood as the turn began
		const bool glides = ecs::villager_draw::Glides(states.at(state).movesWithGround != 0, clip.IsPlayedByTime());
		PlaceDrawing(stores, job.entity, transform, pose, glides, turnFraction, elapsed);
		// One drawn in high detail changes from clip to clip over a moment
		if (stores.AllOf<HighDetail>(job.entity))
		{
			ecs::villager_draw::StepClipBlend(pose.clipBlend, pose.clip, pose.place, elapsed);
		}
	};
	ecs::pose_threads::ForEach(_poseWorkers, _clipJobs.size(), ecs::pose_threads::k_VillagerClipsOnCaller, playOne);

	// Last the main thread plays the sounds on the frames that the clips passed, in the order of the walk. One world plays
	// the sounds of all the villagers in this frame.
	const auto clipSounds = ecs::clip_sound_player::MakeGameWorld();
	for (const auto& job : _clipJobs)
	{
		if (job.clip != nullptr)
		{
			ecs::clip_sound_player::Play(*clipSounds, job.entity, job.clipId, *job.clip, job.soundPlace, job.soundPlayed,
			                             job.transform->position, refresh);
		}
	}
	registry.SetDirty();
}

void LivingActionSystem::PoseVillagersInView(const glm::mat4& viewProjection)
{
	if (!Locator::resources::has_value())
	{
		return;
	}
	auto& registry = Locator::entitiesRegistry::value();
	const auto& animations = Locator::resources::value().GetAnimations();
	const auto& meshes = Locator::resources::value().GetMeshes();
	const auto* land = Locator::terrainSystem::has_value() ? &Locator::terrainSystem::value() : nullptr;
	const auto view = graphics::view_frustum::FromViewProjection(viewProjection);
	const PoseStores stores(registry);

	// First the main thread finds the model and the clip of each villager, because only the main thread can use the
	// resources. A clip that is not loaded yet loads only for a villager in view, after the threads.
	_viewJobs.clear();
	_keptClips.Clear();
	_keptModels.Clear();
	registry.Each<const Villager, const Mesh, const Transform, VillagerPose>(
	    [&](entt::entity entity, const Villager& /*unused*/, const Mesh& mesh, const Transform& transform, VillagerPose& pose) {
		    ViewJob job {.entity = entity, .transform = &transform, .pose = &pose};
		    if (_keptModels.Contains(meshes, mesh.id))
		    {
			    job.model = _keptModels.Find(meshes, mesh.id);
			    job.clipId = resources::HashIdentifier(static_cast<uint32_t>(pose.clip));
			    job.hasClip = static_cast<int>(pose.clip) >= 0 && _keptClips.Contains(animations, job.clipId);
			    job.clip = job.hasClip ? _keptClips.FindLoaded(animations, job.clipId) : nullptr;
		    }
		    _viewJobs.push_back(job);
	    });

	// Then the threads pose the villagers in view. Each job writes only the pose of its own villager.
	const auto poseOne = [&](size_t index, size_t /*lane*/) {
		auto& job = _viewJobs[index];
		auto& pose = *job.pose;
		const auto& transform = *job.transform;
		if (job.model == nullptr)
		{
			pose.bones.clear();
			return;
		}
		const auto& model = *job.model;
		// Out of view it keeps the bones it was last drawn with. The sea reflects only one flying in the physics, or held
		// in the hand.
		const auto box = model.GetBoundingBox();
		const float scale = std::max({transform.scale.x, transform.scale.y, transform.scale.z});
		const auto centre = DrawnPosition(transform.position, &pose) + (transform.rotation * (box.Center() * transform.scale));
		const float radius = (openblack::graphics::mesh_detail::HalfDiagonal(box.Size()) * scale) + k_ViewMargin;
		const bool seen = stores.AnyOf<InPhysics, InHand>(job.entity)
		                      ? graphics::view_frustum::SeesSphereOrReflection(view, centre, radius)
		                      : graphics::view_frustum::SeesSphere(view, centre, radius);
		// Only a villager in view leans to the slope of the land, as the game leans only the villagers it draws
		if (seen && pose.drawnAt.has_value() && pose.easedHeading.has_value())
		{
			pose.slopeLift = SlopeLift(land, transform, *pose.drawnAt, *pose.easedHeading);
		}
		if (!job.hasClip)
		{
			pose.bones.clear();
			return;
		}
		if (!seen)
		{
			return;
		}
		// A clip that is not loaded yet, and the old clip of one in high detail, wait for the main thread
		job.highDetail = stores.TryGet<HighDetail>(job.entity);
		job.afterJobs = job.clip == nullptr || job.highDetail != nullptr;
		if (job.clip != nullptr)
		{
			job.posed = PoseFromClip(pose, *job.clip, model);
		}
	};
	ecs::pose_threads::ForEach(_poseWorkers, _viewJobs.size(), ecs::pose_threads::k_VillagerPosesOnCaller, poseOne);

	// Last the main thread loads the clips that were not loaded, and mixes in the old clips of the villagers in high
	// detail that just changed clip, in the order of the walk
	for (auto& job : _viewJobs)
	{
		if (!job.afterJobs)
		{
			continue;
		}
		auto& pose = *job.pose;
		if (job.clip == nullptr)
		{
			job.clip = animations.Find(job.clipId);
			if (job.clip == nullptr)
			{
				continue;
			}
			job.posed = PoseFromClip(pose, *job.clip, *job.model);
		}
		// One drawn in high detail just changed clip shows the old clip's pose, held where it was, fading out
		if (job.highDetail != nullptr && job.posed)
		{
			MixInOldClip(pose, job.highDetail->orders.turnAtOnce, job.model->GetBoneParents(), _blendBones);
		}
	}
}
