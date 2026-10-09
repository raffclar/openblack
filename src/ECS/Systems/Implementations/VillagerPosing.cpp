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
#include <limits>
#include <optional>

#include "3D/L3DAnim.h"
#include "3D/L3DMesh.h"
#include "Animals/AnimalAnimation.h"
#include "ECS/ClipSoundPlayer.h"
#include "ECS/Components/LivingAction.h"
#include "ECS/Components/LivingPhysics.h"
#include "ECS/Components/Mesh.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/Villager.h"
#include "ECS/Components/VillagerPose.h"
#include "ECS/Components/WallHug.h"
#include "ECS/Registry.h"
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
} // namespace

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

		    // The pose between the two keyframes around its place, each bone then placed by its parent
		    const auto model = meshes.Handle(mesh.id);
		    const auto& frames = clip->GetFrames();
		    const auto& parents = model->GetBoneParents();
		    const auto span = animals::SpanAt(timing, pose.place);
		    if (!model->IsBoned() || frames.empty() || frames[span.from].bones.size() != parents.size() ||
		        frames[span.to].bones.size() != parents.size())
		    {
			    pose.bones.clear();
			    return;
		    }
		    pose.bones.resize(parents.size());
		    const auto& from = frames[span.from].bones;
		    const auto& to = frames[span.to].bones;
		    for (size_t i = 0; i < pose.bones.size(); ++i)
		    {
			    pose.bones[i] = from[i] + ((to[i] - from[i]) * span.t);
			    if (parents[i] != std::numeric_limits<uint32_t>::max())
			    {
				    pose.bones[i] = pose.bones[parents[i]] * pose.bones[i];
			    }
		    }
	    });
	registry.SetDirty();
}
