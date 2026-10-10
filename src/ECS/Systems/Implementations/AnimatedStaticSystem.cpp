/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#define LOCATOR_IMPLEMENTATIONS

#include "AnimatedStaticSystem.h"

#include <cmath>

#include <algorithm>
#include <array>
#include <limits>
#include <vector>

#include <LNDFile.h>
#include <glm/geometric.hpp>

#include "3D/L3DAnim.h"
#include "3D/L3DMesh.h"
#include "3D/LandBlock.h"
#include "3D/LandIslandInterface.h"
#include "Animals/AnimalAnimation.h"
#include "Camera/Camera.h"
#include "ECS/Components/AnimatedStatic.h"
#include "ECS/Components/Mesh.h"
#include "ECS/Components/Mobile.h"
#include "ECS/Components/Transform.h"
#include "ECS/Registry.h"
#include "InfoConstants.h"
#include "Locator.h"
#include "Resources/ResourceManager.h"
#include "Resources/ResourcesInterface.h"

using namespace openblack;
using namespace openblack::ecs::components;
using namespace openblack::ecs::systems;

namespace
{
/// Half the height of a model, as its box over every part in its resting pose gives it
std::optional<float> HalfHeightOf(MeshId mesh)
{
	if (!Locator::resources::has_value())
	{
		return std::nullopt;
	}
	const auto& meshes = Locator::resources::value().GetMeshes();
	const auto id = resources::HashIdentifier(mesh);
	if (!meshes.Contains(id))
	{
		return std::nullopt;
	}
	return meshes.Handle(id)->GetBoundingBox().Size().y * 0.5f;
}

/// The model's bones posed at a place in a clip, each bone placed by its parent; none when the clip doesn't fit it
void Pose(const L3DAnim& clip, const animals::ClipTiming& timing, const graphics::L3DMesh& model, uint32_t place,
          std::vector<glm::mat4>& bones)
{
	const auto& frames = clip.GetFrames();
	const auto& parents = model.GetBoneParents();
	const auto span = animals::SpanAt(timing, place);
	if (!model.IsBoned() || frames.empty() || span.from >= frames.size() || span.to >= frames.size() ||
	    frames[span.from].bones.size() != parents.size() || frames[span.to].bones.size() != parents.size())
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

bool AnimatedStaticSystem::SetOpenState(entt::entity object, int32_t openState)
{
	auto& registry = Locator::entitiesRegistry::value();
	auto* still = registry.Valid(object) ? registry.TryGet<AnimatedStatic>(object) : nullptr;
	if (still == nullptr)
	{
		return false;
	}
	still->openState = openState;
	return true;
}

std::optional<uint32_t> AnimatedStaticSystem::GateStoneValue(entt::entity object) const
{
	const auto& registry = Locator::entitiesRegistry::value();
	const auto* still = registry.Valid(object) ? registry.TryGet<const AnimatedStatic>(object) : nullptr;
	if (still == nullptr)
	{
		return std::nullopt;
	}
	return animated_static::GateStoneValue(still->gateStones);
}

bool AnimatedStaticSystem::LayGateStone(entt::entity plinth, entt::entity stone)
{
	auto& registry = Locator::entitiesRegistry::value();
	if (!registry.Valid(plinth) || !registry.Valid(stone) || !Locator::infoConstants::has_value())
	{
		return false;
	}
	auto* still = registry.TryGet<AnimatedStatic>(plinth);
	const auto* mobile = registry.TryGet<const MobileStatic>(stone);
	if (still == nullptr || mobile == nullptr || still->type != AnimatedStaticInfo::GateStonePlinth)
	{
		return false;
	}
	const auto& infos = Locator::infoConstants::value().mobileStatic;
	const auto index = static_cast<size_t>(mobile->type);
	if (index >= infos.size() || !animated_static::IsGateStoneKind(infos[index].mobileType))
	{
		return false;
	}
	// A full plinth still takes the stone, which is then lost
	[[maybe_unused]] const bool laid = animated_static::AddGateStone(still->gateStones, infos[index].meshId);
	return true;
}

std::optional<std::vector<animated_static::RouteCircle>> AnimatedStaticSystem::RouteCircles(entt::entity gate) const
{
	const auto& registry = Locator::entitiesRegistry::value();
	if (!registry.Valid(gate) || !Locator::resources::has_value())
	{
		return std::nullopt;
	}
	const auto* still = registry.TryGet<const AnimatedStatic>(gate);
	const auto* pose = registry.TryGet<const AnimatedStaticPose>(gate);
	const auto* transform = registry.TryGet<const Transform>(gate);
	const auto* mesh = registry.TryGet<const Mesh>(gate);
	if (still == nullptr || pose == nullptr || transform == nullptr || mesh == nullptr ||
	    still->type != AnimatedStaticInfo::NorseGate)
	{
		return std::nullopt;
	}
	const auto& meshes = Locator::resources::value().GetMeshes();
	const float halfWidth = meshes.Contains(mesh->id) ? meshes.Handle(mesh->id)->GetBoundingBox().Size().x * 0.5f : 0.0f;
	// Its own across direction on the land, sized as it is
	const auto across = transform->rotation * glm::vec3(transform->scale.x, 0.0f, 0.0f);
	const bool openAndStill = still->openState == animated_static::k_Open &&
	                          !animated_static::IsMoving(still->openState, pose->place, pose->restingPlace);
	return animated_static::GateRouteCircles(glm::vec2(transform->position.x, transform->position.z),
	                                         glm::vec2(across.x, across.z), halfWidth, transform->scale.x, openAndStill);
}

void AnimatedStaticSystem::Update(uint32_t turn, float turnFraction)
{
	if (!Locator::resources::has_value() || !Locator::infoConstants::has_value())
	{
		return;
	}
	auto& registry = Locator::entitiesRegistry::value();
	const auto now = animals::DrawTime(turn, std::clamp(turnFraction, 0.0f, 1.0f));
	// The milliseconds of the game's clock since the last frame, none when the clock went back
	const uint32_t elapsed = _drawTime.has_value() && now >= *_drawTime ? now - *_drawTime : 0;
	_drawTime = now;

	const auto& animations = Locator::resources::value().GetAnimations();
	auto& meshes = Locator::resources::value().GetMeshes();
	const auto& infos = Locator::infoConstants::value().animatedStatic;

	// The stones of a plinth that is gone go with it
	std::vector<entt::entity> orphans;
	registry.Each<const PlinthStone>([&registry, &orphans](entt::entity entity, const PlinthStone& stone) {
		if (!registry.Valid(stone.plinth) || !registry.AllOf<AnimatedStaticPose>(stone.plinth))
		{
			orphans.push_back(entity);
		}
	});
	for (const auto orphan : orphans)
	{
		registry.Destroy(orphan);
	}

	const auto frame = DrawFrame(turn);

	struct StoneUpdate
	{
		entt::entity plinth;
		std::vector<animated_static::StoneDraw> draws;
	};
	std::vector<StoneUpdate> plinths;
	bool changed = !orphans.empty();

	registry.Each<const AnimatedStatic, const Mesh, const Transform, AnimatedStaticPose>(
	    [&](entt::entity entity, const AnimatedStatic& still, const Mesh& mesh, const Transform& transform,
	        AnimatedStaticPose& pose) {
		    const auto type = static_cast<size_t>(still.type);
		    if (type >= infos.size())
		    {
			    return;
		    }
		    // It plays on only on the frames it is drawn, as the land's draw list draws it
		    if (frame.rebuilt)
		    {
			    pose.inDrawList = InDrawList(transform.position);
		    }
		    const bool drawn = object_draw_list::Drawn(frame, pose.inDrawList, pose.onScreen);
		    if (frame.drawAll && pose.inDrawList)
		    {
			    pose.onScreen = OnScreen(transform, mesh);
		    }
		    const auto clipId = resources::HashIdentifier(static_cast<uint32_t>(infos[type].defaultAnim));
		    if (static_cast<int>(infos[type].defaultAnim) < 0 || !animations.Contains(clipId))
		    {
			    pose.bones.clear();
			    return;
		    }
		    const auto clip = animations.Handle(clipId);
		    const auto rest = animated_static::OpenRestingPlace(clip->GetPlayTime(), clip->GetFrames().size());
		    const auto place = drawn ? animated_static::StepClip(still.openState, pose.place, elapsed, rest) : pose.place;
		    const bool moved = place != pose.place || pose.bones.empty();
		    pose.place = place;
		    pose.restingPlace = rest;
		    if (moved && meshes.Contains(mesh.id))
		    {
			    const animals::ClipTiming timing {.playTime = clip->GetPlayTime(),
			                                      .frameCount = clip->GetFrames().size(),
			                                      .looping = clip->IsLooping(),
			                                      .playedByTime = true,
			                                      .stride = clip->GetStride()};
			    Pose(*clip, timing, *meshes.Handle(mesh.id), pose.place, pose.bones);
			    changed = true;
		    }

		    if (still.type != AnimatedStaticInfo::GateStonePlinth)
		    {
			    return;
		    }
		    animated_static::PlinthLook look {.openState = still.openState,
		                                      .moving = animated_static::IsMoving(still.openState, pose.place, rest),
		                                      .place = pose.place,
		                                      .playTime = clip->GetPlayTime(),
		                                      .plinthHalfHeight = HalfHeightOf(infos[type].meshId).value_or(0.0f)};
		    for (size_t slot = 0; slot < animated_static::k_GateStoneSlots; ++slot)
		    {
			    if (still.gateStones.at(slot).has_value())
			    {
				    look.stoneHalfHeights.at(slot) = HalfHeightOf(*still.gateStones.at(slot)).value_or(0.0f);
			    }
		    }
		    plinths.push_back({.plinth = entity, .draws = animated_static::PlinthStoneDraws(look)});
	    });

	// Each plinth's stones are drawn as models of their own, made and unmade as their number changes, placed on the plinth
	// as it stands and turned and sized as it is
	for (const auto& [plinth, draws] : plinths)
	{
		auto& stones = registry.Get<AnimatedStaticPose>(plinth).stones;
		while (stones.size() > draws.size())
		{
			registry.Destroy(stones.back());
			stones.pop_back();
			changed = true;
		}
		while (stones.size() < draws.size())
		{
			const auto stone = registry.Create();
			registry.Assign<PlinthStone>(stone, plinth, false);
			stones.push_back(stone);
		}
		const auto still = registry.Get<const AnimatedStatic>(plinth);
		const auto transform = registry.Get<const Transform>(plinth);
		for (size_t i = 0; i < draws.size(); ++i)
		{
			const auto& draw = draws[i];
			const auto meshId = resources::HashIdentifier(*still.gateStones.at(draw.slot));
			const auto* mesh = registry.TryGet<const Mesh>(stones[i]);
			if (mesh == nullptr || mesh->id != meshId)
			{
				registry.AssignOrReplace<Mesh>(stones[i], meshId, static_cast<int8_t>(0), static_cast<int8_t>(1));
			}
			const Transform placed {.position = transform.position + glm::vec3(0.0f, draw.lift, 0.0f),
			                        .rotation = transform.rotation,
			                        .scale = transform.scale};
			if (auto* current = registry.TryGet<Transform>(stones[i]); current == nullptr)
			{
				registry.Assign<Transform>(stones[i], placed);
			}
			else if (current->position != placed.position || current->rotation != placed.rotation ||
			         current->scale != placed.scale)
			{
				*current = placed;
				changed = true;
			}
			registry.Get<PlinthStone>(stones[i]).pickable = draw.pickable;
		}
	}
	if (changed)
	{
		registry.SetDirty();
	}
}

void AnimatedStaticSystem::Reset()
{
	_drawTime.reset();
	_drawClock.Reset();
	_blocks.clear();
	_blockGrid.fill(-1);
	_view.reset();
}

void AnimatedStaticSystem::LoadBlocks()
{
	if (!_blocks.empty() || !Locator::terrainSystem::has_value())
	{
		return;
	}
	_blockGrid.fill(-1);
	for (const auto& block : Locator::terrainSystem::value().GetBlocks())
	{
		const auto& lnd = block.GetLndBlock();
		if (lnd == nullptr)
		{
			continue;
		}
		// The highest of its cells, as the land had them when it was loaded
		uint8_t highest = 0;
		for (const auto& cell : lnd->cells)
		{
			highest = std::max(highest, cell.altitude);
		}
		const auto at = block.GetBlockPosition();
		if (at.x >= 0 && at.x < k_GridBlocks && at.y >= 0 && at.y < k_GridBlocks)
		{
			_blockGrid.at(static_cast<size_t>((at.x * k_GridBlocks) + at.y)) = static_cast<int32_t>(_blocks.size());
		}
		_blocks.push_back({.block = {.corner = block.GetMapPosition(), .highestAltitude = highest, .reflected = true}});
	}
}

object_draw_list::Frame AnimatedStaticSystem::DrawFrame(uint32_t turn)
{
	if (!Locator::camera::has_value() || !Locator::terrainSystem::has_value())
	{
		// With nothing to see from, everything is drawn
		_view.reset();
		return {.rebuilt = true, .drawAll = true};
	}
	LoadBlocks();
	const auto& camera = Locator::camera::value();
	const auto& projection = camera.GetProjectionMatrix();
	_view = object_draw_list::View {.viewProjection = camera.GetViewProjectionMatrix(),
	                                .nearPlane = camera.GetNearClip(),
	                                .focalHeight = projection[1][1],
	                                .aspect = projection[0][0] != 0.0f ? projection[1][1] / projection[0][0] : 1.0f,
	                                .eye = camera.GetOrigin()};
	// A change in the blocks in view makes the list again
	bool changed = false;
	for (auto& block : _blocks)
	{
		const bool inView = object_draw_list::BlockInView(_view->viewProjection, _view->nearPlane, block.block);
		changed = changed || inView != block.inView;
		block.inView = inView;
	}
	const auto frame = _drawClock.Next(turn, _view->eye, camera.GetFocus(), changed);
	if (frame.rebuilt)
	{
		_anyListed = false;
		for (auto& block : _blocks)
		{
			block.listed = block.inView && object_draw_list::BlockDistance(_view->eye, block.block) < object_draw_list::k_Range;
			_anyListed = _anyListed || block.listed;
		}
	}
	return frame;
}

bool AnimatedStaticSystem::InDrawList(glm::vec3 position) const
{
	if (!_view.has_value())
	{
		return true;
	}
	// The block of the land cell it stands on; off the land's blocks it is listed with the first block listed
	const auto cellX = static_cast<int32_t>(std::floor(position.x / 10.0f));
	const auto cellZ = static_cast<int32_t>(std::floor(position.z / 10.0f));
	const auto blockX = cellX >> 4;
	const auto blockZ = cellZ >> 4;
	if (cellX < 0 || cellZ < 0 || blockX >= k_GridBlocks || blockZ >= k_GridBlocks)
	{
		return _anyListed;
	}
	const auto index = _blockGrid.at(static_cast<size_t>((blockX * k_GridBlocks) + blockZ));
	return index < 0 ? _anyListed : _blocks.at(static_cast<size_t>(index)).listed;
}

bool AnimatedStaticSystem::OnScreen(const Transform& transform, const Mesh& mesh) const
{
	if (!_view.has_value() || !Locator::resources::has_value())
	{
		return true;
	}
	const auto& meshes = Locator::resources::value().GetMeshes();
	if (!meshes.Contains(mesh.id))
	{
		return true;
	}
	const auto box = meshes.Handle(mesh.id)->GetBoundingBox();
	const auto centre = transform.position + (transform.rotation * (transform.scale * box.Center()));
	const float radius = std::abs(transform.scale.x) * glm::length(box.Size() * 0.5f);
	return object_draw_list::SphereOnScreen(*_view, transform.position, centre, radius);
}
