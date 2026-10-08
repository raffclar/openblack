/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "Animations.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>

#include <algorithm>
#include <map>
#include <ranges>
#include <string>
#include <utility>

#include <fmt/format.h>
#include <glm/geometric.hpp>
#include <spdlog/spdlog.h>

#include "3D/L3DAnim.h"
#include "3D/L3DMesh.h"
#include "3D/SkeletalPose.h"
#include "Audio/Services/AnimationSounds.h"
#include "ECS/Components/CreatureBody.h"
#include "ECS/Components/DrawPosition.h"
#include "ECS/Components/Mesh.h"
#include "ECS/Components/SkeletalAnimation.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/Unavailable.h"
#include "ECS/Registry.h"
#include "ECS/Systems/DebugHooksInterface.h"
#include "Locator.h"
#include "Resources/ResourceManager.h"
#include "Resources/ResourcesInterface.h"

namespace openblack::ecs
{

using components::Mesh;
using components::SkeletalAnimation;

namespace
{
/// What the animation trace keeps between calls, in the debug hooks' store (Locator::debugHooks)
struct AnimationsDebugHooksState
{
	float traceClock {0.0f}; // OPENBLACK_ANIM_TRACE: game milliseconds since the last line
};

AnimationsDebugHooksState& AnimationsDebugHooksData()
{
	if (!Locator::debugHooks::has_value())
	{
		std::fputs("ecs::animations: no debug hooks in the locator (Locator::debugHooks)\n", stderr);
		std::abort();
	}
	return Locator::debugHooks::value().Get<AnimationsDebugHooksState>();
}
} // namespace

entt::id_type ClipId(uint32_t index)
{
	return resources::HashIdentifier(index);
}

void UpdateAnimations(float milliseconds)
{
	// OPENBLACK_ANIM_TRACE=1: every 2 s of game time, how many play each clip (and at what walk speed)
	auto& traceClock = AnimationsDebugHooksData().traceClock;
	traceClock += milliseconds;
	const bool trace = traceClock >= 2000.0f && std::getenv("OPENBLACK_ANIM_TRACE") != nullptr;
	if (trace)
	{
		traceClock = 0.0f;
		std::map<int32_t, std::pair<int, float>> clips;
		Locator::entitiesRegistry::value().Each<const SkeletalAnimation>([&clips](const SkeletalAnimation& animation) {
			auto& entry = clips[animation.clipIndex];
			++entry.first;
			entry.second = std::max(entry.second, animation.distanceSpeed);
		});
		std::string line;
		for (const auto& [clip, entry] : clips)
		{
			line += fmt::format(" {}x{} ({:.2f} m/s)", clip, entry.first, entry.second);
		}
		SPDLOG_LOGGER_INFO(spdlog::get("game"), "Anim trace:{}", line);
	}

	audio::AnimationSounds::Update();
	auto& registry = Locator::entitiesRegistry::value();
	auto& resources = Locator::resources::value();
	const auto& animations = resources.GetAnimations();
	const auto& meshes = resources.GetMeshes();
	registry.Each<SkeletalAnimation, const Mesh>(
	    [&](entt::entity entity, SkeletalAnimation& animation, const Mesh& mesh) {
		    // one lookup of each (an empty handle when the cache has none)
		    const auto clip = animation.hasClip ? animations.Handle(animation.clip) : decltype(animations.Handle(0)) {};
		    const auto model = clip ? meshes.Handle(mesh.id) : decltype(meshes.Handle(0)) {};
		    if (!clip || !model)
		    {
			    animation.pose.clear();
			    animation.locals.clear();
			    animation.drawnPose.clear();
			    return;
		    }
		    // the time advances by the frame's milliseconds; looping clips wrap, one-shot ones stop at the end
		    const auto duration = static_cast<float>(clip->GetDurationMs());
		    float advance = milliseconds * animation.speed;
		    // moving states: metres moved / scale / the clip's stride * its duration; clips with a stride under 0.05 always
		    // advance with time
		    if (animation.distanceSpeed > 0.0f && clip->GetCycleDistance() >= 0.05f)
		    {
			    const auto* transform = registry.TryGet<const components::Transform>(entity);
			    const float scale = transform != nullptr && transform->scale.x > 0.0f ? transform->scale.x : 1.0f;
			    const float metres = animation.distanceSpeed * milliseconds / 1000.0f / scale;
			    advance = metres / clip->GetCycleDistance() * duration;
		    }
		    const float before = animation.time;
		    animation.time += advance;
		    // the clip's sound events crossed this frame, both sides of a wrap
		    if (advance > 0.0f)
		    {
			    const auto from = static_cast<int32_t>(before);
			    const auto to = static_cast<int32_t>(animation.time);
			    if (clip->IsLooping() && duration > 0.0f && animation.time >= duration)
			    {
				    audio::AnimationSounds::Fire(entity, animation.clipIndex, from, static_cast<int32_t>(duration));
				    audio::AnimationSounds::Fire(entity, animation.clipIndex, 0,
				                                 static_cast<int32_t>(std::fmod(animation.time, duration)));
			    }
			    else
			    {
				    audio::AnimationSounds::Fire(entity, animation.clipIndex, from, to);
			    }
		    }
		    if (duration > 0.0f)
		    {
			    animation.time = clip->IsLooping() ? std::fmod(animation.time, duration) : std::min(animation.time, duration);
		    }
		    graphics::ComputePoseInto(*model, *clip, animation.time, animation.pose, animation.locals);
		    // a SuperVillager (ECS/SuperVillager.h): the clip drawn before a change is blended in for crossFadeMs, into its own
		    // bone buffer only (drawnPose); `pose` above stays the plain one
		    if (animation.crossFadeMs > 0)
		    {
			    animation.drawnPose.clear();
			    if (animation.crossFadeFrozen)
			    {
				    return; // no fade step this frame
			    }
			    auto& fade = animation.crossFade;
			    const bool fading =
			        StepCrossFade(fade, animation.clip, static_cast<int32_t>(milliseconds), animation.crossFadeMs);
			    // while there is fade left and no snap (DrawPosition::followSnap, the same flag as the yaw stage's).
			    // (openblack) a fade from a clip that is not loaded draws the plain pose
			    const auto* draw = registry.TryGet<const components::DrawPosition>(entity);
			    const bool snap = draw != nullptr && draw->followSnap;
			    if (fading && !snap && animations.Contains(fade.oldClip))
			    {
				    graphics::ComputeBlendedPose(*model, *clip, animation.time, *animations.Handle(fade.oldClip), fade.oldTime,
				                                 fade.weight, animation.drawnPose);
			    }
			    fade.lastTime = animation.time;
		    }
	    },
	    entt::exclude<components::Unavailable>);
}

const std::vector<glm::mat4>& DrawnPose(const SkeletalAnimation& animation)
{
	return animation.drawnPose.empty() ? animation.pose : animation.drawnPose;
}

bool StepCrossFade(components::SkeletalAnimation::CrossFade& fade, entt::id_type clip, int32_t milliseconds, int32_t fadeMs)
{
	if (!fade.hasLast)
	{
		// the first clip is only kept
		fade.hasLast = true;
		fade.lastClip = clip;
	}
	else if (fade.lastClip != clip)
	{
		// another clip: a fade from the clip and time drawn last
		fade.oldClip = fade.lastClip;
		fade.oldTime = fade.lastTime;
		fade.leftMs = fadeMs;
		fade.lastClip = clip;
		fade.weight = 1.0f;
	}
	else
	{
		// the same clip: the fade counts down
		fade.leftMs -= milliseconds;
		if (fade.leftMs < 0)
		{
			fade.leftMs = 0;
		}
		else
		{
			fade.weight = static_cast<float>(fade.leftMs) / static_cast<float>(fadeMs);
		}
	}
	return fade.leftMs != 0;
}

PoseMap PosesByInstance(const std::unordered_map<entt::entity, systems::RenderContext::EntityInstance>& entityInstances)
{
	PoseMap poses;
	Locator::entitiesRegistry::value().Each<const SkeletalAnimation>(
	    [&](entt::entity entity, const SkeletalAnimation& animation) {
		    if (animation.pose.empty())
		    {
			    return;
		    }
		    if (const auto instance = entityInstances.find(entity); instance != entityInstances.end())
		    {
			    poses.emplace(instance->second.index, &DrawnPose(animation));
		    }
	    });
	// a creature is posed by its own animations; looked up through the const registry, which makes no storage on a land
	// without a creature (the state hash counts the storages)
	std::as_const(Locator::entitiesRegistry::value())
	    .Each<const components::CreatureAnimation>([&](entt::entity entity, const components::CreatureAnimation& animation) {
		    if (animation.boneMatrices.empty())
		    {
			    return;
		    }
		    if (const auto instance = entityInstances.find(entity); instance != entityInstances.end())
		    {
			    poses.emplace(instance->second.index, &animation.boneMatrices);
		    }
	    });
	return poses;
}

PoseMap PosesByInstance(const systems::RenderContext& context)
{
	auto poses = PosesByInstance(context.entityInstances);
	for (const auto& [index, pose] : context.instancePoses)
	{
		poses.emplace(index, &pose);
	}
	return poses;
}

bool HasPose(const PoseMap& poses, uint32_t offset, uint32_t count)
{
	if (poses.empty())
	{
		return false;
	}
	return std::ranges::any_of(std::views::iota(0u, count),
	                           [&poses, offset](uint32_t i) { return poses.contains(offset + i); });
}

void UsePose(const PoseMap& poses, uint32_t instance, const graphics::L3DMesh& mesh, const glm::mat4*& matrices, uint8_t& count)
{
	const auto pose = poses.find(instance);
	if (pose == poses.end() || pose->second->size() != mesh.GetBoneMatrices().size())
	{
		return;
	}
	matrices = pose->second->data();
	count = static_cast<uint8_t>(pose->second->size());
}

} // namespace openblack::ecs
