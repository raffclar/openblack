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

#include "3D/L3DAnim.h"
#include "3D/L3DMesh.h"
#include "Animals/AnimalAnimation.h"
#include "ECS/ClipSoundPlayer.h"
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
#include "ECS/Registry.h"
#include "ECS/VillagerDrawRules.h"
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
/// The game's speed units in one metre a second
constexpr float k_SpeedUnitsPerMetrePerSecond = 655.36f;
/// How far outside the camera's view a villager is still posed, in metres, should the camera move a little more before
/// the frame is drawn
constexpr float k_ViewMargin = 2.0f;

/// Where a villager is drawn this frame and the heading it is drawn with: between its last two turns' places while its
/// clip carries it along, turning after the way it faces. One in the physics or in the hand is drawn where it is.
void PlaceDrawing(ecs::Registry& registry, entt::entity entity, const Transform& transform, VillagerPose& pose, bool glides,
                  float turnFraction, uint32_t elapsed)
{
	if (registry.AnyOf<InPhysics, InHand>(entity))
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
	const auto* highDetail = registry.TryGet<const HighDetail>(entity);
	const auto headings =
	    ecs::villager_draw::StepHeadings(pose.easedHeading, pose.detailedHeading, *facing, elapsed, highDetail != nullptr,
	                                     highDetail != nullptr && highDetail->orders.turnAtOnce);
	pose.easedHeading = headings.eased;
	pose.detailedHeading = headings.detailed;
	pose.drawnHeading = headings.drawn;
}

/// The pose of a high-detail villager that just changed clip, with the old clip's pose held where it was mixed in
void MixInOldClip(VillagerPose& pose, bool turnAtOnce, std::span<const uint32_t> parents, std::vector<glm::mat4>& scratch)
{
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
	ecs::villager_draw::BlendPoses(pose.bones, scratch, blend.weight);
}
} // namespace

void LivingActionSystem::StartTurnPlaces()
{
	auto& registry = Locator::entitiesRegistry::value();
	registry.Each<const Villager, const Transform, VillagerPose>(
	    [](const Villager& /*unused*/, const Transform& transform, VillagerPose& pose) {
		    pose.turnStart = transform.position;
	    });
}

void LivingActionSystem::UpdatePoses(uint32_t turn, float turnFraction)
{
	if (!Locator::resources::has_value() || !Locator::infoConstants::has_value())
	{
		return;
	}
	auto& registry = Locator::entitiesRegistry::value();
	const auto now = animals::DrawTime(turn, std::clamp(turnFraction, 0.0f, 1.0f));
	// The milliseconds of the game's clock since the last frame, none when the clock went back
	const uint32_t elapsed = now >= _poseDrawTime ? now - _poseDrawTime : 0;
	_poseDrawTime = now;
	const auto& animations = Locator::resources::value().GetAnimations();
	auto& meshes = Locator::resources::value().GetMeshes();
	const auto& states = Locator::infoConstants::value().villagerStateTable;

	registry.Each<const Villager, const LivingAction, const Mesh, const Transform, VillagerPose>(
	    [&](entt::entity entity, const Villager& /*unused*/, const LivingAction& action, const Mesh& mesh,
	        const Transform& transform, VillagerPose& pose) {
		    // The clip is chosen by the villager's states as they change; here it only plays
		    const auto clipId = resources::HashIdentifier(static_cast<uint32_t>(pose.clip));
		    if (static_cast<int>(pose.clip) < 0 || !animations.Contains(clipId) || !meshes.Contains(mesh.id))
		    {
			    pose.bones.clear();
			    PlaceDrawing(registry, entity, transform, pose, false, turnFraction, elapsed);
			    return;
		    }
		    const auto clip = animations.Handle(clipId);
		    const animals::ClipTiming timing {.playTime = clip->GetPlayTime(),
		                                      .frameCount = clip->GetFrames().size(),
		                                      .looping = clip->IsLooping(),
		                                      .playedByTime = true,
		                                      .stride = clip->GetStride()};
		    // Villagers play their clips by the clock, except that in the moving states a clip that carries them along
		    // plays by the ground they cover
		    auto played = static_cast<int32_t>(elapsed);
		    const auto state = action.states.at(static_cast<size_t>(LivingAction::Index::Top));
		    if (const auto* wallHug = registry.TryGet<const WallHug>(entity);
		        wallHug != nullptr && states.at(state).movesWithGround != 0 && std::abs(timing.stride) >= k_StationaryStride)
		    {
			    const auto speed = static_cast<uint16_t>(std::lround(wallHug->speed * k_SpeedUnitsPerMetrePerSecond + 0.5f));
			    played = animals::MovingPlay(timing, speed, elapsed, transform.scale.x);
		    }
		    // The sounds on the clip's frames it passes play from the villager
		    ecs::clip_sound_player::Play(entity, pose.clip, *clip, pose.place, static_cast<uint32_t>(played),
		                                 transform.position);
		    pose.place = animals::AdvanceClip(timing, pose.place, played);
		    // While it walks with a clip that carries it along, it is drawn gliding from where it stood as the turn began
		    const bool glides = ecs::villager_draw::Glides(states.at(state).movesWithGround != 0, clip->IsPlayedByTime());
		    PlaceDrawing(registry, entity, transform, pose, glides, turnFraction, elapsed);
		    // One drawn in high detail changes from clip to clip over a moment
		    if (registry.AllOf<HighDetail>(entity))
		    {
			    ecs::villager_draw::StepClipBlend(pose.clipBlend, pose.clip, pose.place, elapsed);
		    }
	    });
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
	const auto view = graphics::view_frustum::FromViewProjection(viewProjection);

	registry.Each<const Villager, const Mesh, const Transform, VillagerPose>(
	    [&](entt::entity entity, const Villager& /*unused*/, const Mesh& mesh, const Transform& transform, VillagerPose& pose) {
		    const auto clipId = resources::HashIdentifier(static_cast<uint32_t>(pose.clip));
		    if (static_cast<int>(pose.clip) < 0 || !animations.Contains(clipId) || !meshes.Contains(mesh.id))
		    {
			    pose.bones.clear();
			    return;
		    }
		    const auto model = meshes.Handle(mesh.id);
		    // Out of view it keeps the bones it was last drawn with. The sea reflects only one flying in the physics, or
		    // held in the hand.
		    const auto box = model->GetBoundingBox();
		    const float scale = std::max({transform.scale.x, transform.scale.y, transform.scale.z});
		    const auto centre =
		        DrawnPosition(transform.position, &pose) + (transform.rotation * (box.Center() * transform.scale));
		    const float radius = (glm::length(box.Size()) * 0.5f * scale) + k_ViewMargin;
		    const bool seen = registry.AnyOf<InPhysics, InHand>(entity)
		                          ? graphics::view_frustum::SeesSphereOrReflection(view, centre, radius)
		                          : graphics::view_frustum::SeesSphere(view, centre, radius);
		    if (!seen)
		    {
			    return;
		    }
		    // The pose between the two keyframes around its place
		    const auto clip = animations.Handle(clipId);
		    const auto& frames = clip->GetFrames();
		    const auto& parents = model->GetBoneParents();
		    const auto span = animals::SpanAt(
		        {.playTime = clip->GetPlayTime(), .frameCount = frames.size(), .looping = clip->IsLooping()}, pose.place);
		    if (!model->IsBoned() || frames.empty() || frames[span.from].bones.size() != parents.size() ||
		        frames[span.to].bones.size() != parents.size())
		    {
			    pose.bones.clear();
			    return;
		    }
		    pose.bones.resize(parents.size());
		    animals::PoseBetween(frames[span.from].bones, frames[span.to].bones, span.t, parents, pose.bones);
		    // One drawn in high detail just changed clip shows the old clip's pose, held where it was, fading out
		    if (const auto* highDetail = registry.TryGet<const HighDetail>(entity); highDetail != nullptr)
		    {
			    MixInOldClip(pose, highDetail->orders.turnAtOnce, parents, _blendBones);
		    }
	    });
}
