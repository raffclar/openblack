/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#define LOCATOR_IMPLEMENTATIONS

#include "WhaleSystem.h"

#include <limits>
#include <optional>
#include <vector>

#include <glm/gtc/matrix_transform.hpp>
#include <glm/vec2.hpp>
#include <glm/vec4.hpp>

#include "3D/AllMeshes.h"
#include "3D/L3DAnim.h"
#include "3D/L3DMesh.h"
#include "3D/LandIslandInterface.h"
#include "Animals/AnimalAnimation.h"
#include "Animals/AnimalRules.h"
#include "ECS/Components/Animal.h"
#include "ECS/Components/Mesh.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/Whale.h"
#include "ECS/Registry.h"
#include "ECS/Systems/WaterRingSystemInterface.h"
#include "ECS/WhaleRules.h"
#include "Locator.h"
#include "Resources/ResourceManager.h"
#include "Resources/ResourcesInterface.h"

using namespace openblack;
using namespace openblack::ecs::components;
using namespace openblack::ecs::systems;

namespace
{
/// Whales swim with the boned shark's swimming clip, round and round
constexpr auto k_SwimClip = AnimId::SharkBonedSwim;

/// The model's bones posed `place` milliseconds into the clip, each placed by its parent; none when the clip doesn't fit
/// the model
void Pose(const graphics::L3DMesh& model, const L3DAnim& clip, const animals::ClipTiming& timing, uint32_t place,
          std::vector<glm::mat4>& bones)
{
	const auto& frames = clip.GetFrames();
	const auto& parents = model.GetBoneParents();
	const auto span = animals::SpanAt(timing, place);
	if (!model.IsBoned() || frames.empty() || frames[span.from].bones.size() != parents.size() ||
	    frames[span.to].bones.size() != parents.size())
	{
		bones.clear();
		return;
	}
	bones.resize(parents.size());
	const auto& from = frames[span.from].bones;
	const auto& to = frames[span.to].bones;
	for (size_t i = 0; i < bones.size(); ++i)
	{
		bones[i] = from[i] + ((to[i] - from[i]) * span.t);
		if (parents[i] != std::numeric_limits<uint32_t>::max())
		{
			bones[i] = bones[parents[i]] * bones[i];
		}
	}
}
} // namespace

void WhaleSystem::ProcessTurn()
{
	Locator::entitiesRegistry::value().Each<Whale>([](Whale& whale) { whale.turnStart = whale.position; });
}

void WhaleSystem::Update(std::chrono::duration<float, std::milli> gameTime, float turnFraction)
{
	if (!Locator::terrainSystem::has_value() || !Locator::resources::has_value())
	{
		return;
	}
	auto& registry = Locator::entitiesRegistry::value();
	const auto& land = Locator::terrainSystem::value();
	auto& resources = Locator::resources::value();
	const auto& animations = resources.GetAnimations();
	auto& meshes = resources.GetMeshes();
	// The frame's whole milliseconds of game time
	const auto frameMilliseconds = static_cast<int32_t>(gameTime.count());
	const auto clipId = resources::HashIdentifier(static_cast<uint32_t>(k_SwimClip));
	registry.Each<Whale, Transform, const Mesh, AnimalPose>([&](Whale& whale, Transform& transform, const Mesh& mesh,
	                                                            AnimalPose& pose) {
		// It swims on through its clip
		std::optional<animals::ClipTiming> timing;
		if (animations.Contains(clipId))
		{
			const auto clip = animations.Handle(clipId);
			timing = animals::ClipTiming {
			    .playTime = clip->GetPlayTime(), .frameCount = clip->GetFrames().size(), .looping = clip->IsLooping()};
			whale.clipPlace = animals::AdvanceClip(*timing, whale.clipPlace, frameMilliseconds);
		}

		// Drawn between the start of its turn and where it is, facing the way it moved
		whale.heading = ecs::whale_rules::Heading(whale.turnStart, whale.position, whale.heading);
		const auto startHeight = land.GetHeightAt(glm::vec2(whale.turnStart.x, whale.turnStart.z));
		const auto endHeight = land.GetHeightAt(glm::vec2(whale.position.x, whale.position.z));
		transform.position = ecs::whale_rules::Drawn(whale.turnStart, startHeight, whale.position, endHeight, turnFraction);
		transform.rotation = animals::Orientation(whale.heading, 0.0f);

		if (!timing.has_value() || !meshes.Contains(mesh.id))
		{
			return;
		}
		const auto model = meshes.Handle(mesh.id);
		Pose(*model, *animations.Handle(clipId), *timing, whale.clipPlace, pose.bones);

		// The wake comes from the point fixed to its bones, where it is drawn this frame
		const auto& bonePoint = model->GetFirstBonePoint();
		if (!bonePoint.has_value() || bonePoint->bone >= pose.bones.size())
		{
			return;
		}
		const auto placed =
		    glm::scale(glm::translate(glm::mat4(1.0f), transform.position) * glm::mat4(transform.rotation), transform.scale);
		const auto world = placed * pose.bones[bonePoint->bone] * glm::vec4(bonePoint->point, 1.0f);
		if (const auto ring = ecs::whale_rules::WakeRing(_wakeTimer, glm::vec3(world), whale.heading, frameMilliseconds);
		    ring.has_value() && Locator::waterRingSystem::has_value())
		{
			// A ring that doesn't fit among the many already on the water isn't made
			static_cast<void>(Locator::waterRingSystem::value().Add(*ring));
		}
	});
	registry.SetDirty();
}
