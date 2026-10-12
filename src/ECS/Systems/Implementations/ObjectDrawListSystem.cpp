/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#define LOCATOR_IMPLEMENTATIONS

#include "ObjectDrawListSystem.h"

#include <algorithm>

#include <LNDFile.h>

#include "3D/LandBlock.h"
#include "3D/LandIslandInterface.h"
#include "Camera/Camera.h"
#include "ECS/Components/HighDetail.h"
#include "ECS/Components/MapCellResident.h"
#include "ECS/Components/Physics.h"
#include "ECS/Components/Temple.h"
#include "ECS/Map.h"
#include "ECS/Registry.h"
#include "Locator.h"

using namespace openblack::ecs::systems;
using namespace openblack::ecs::components;
namespace object_draw_list = openblack::object_draw_list;

namespace
{
/// Things drawn on their own, never by the list: the temple, and things the scripts draw in high detail
bool DrawnOnTheirOwn(const openblack::ecs::Registry& registry, entt::entity entity)
{
	return registry.AnyOf<Temple, HighDetail>(entity);
}
} // namespace

ObjectDrawListSystem::ObjectDrawListSystem()
{
	if (!Locator::entitiesRegistry::has_value())
	{
		return;
	}
	_registry = &Locator::entitiesRegistry::value();
	// A thing settling out of the physics makes the list again, so that it is drawn where it came to rest
	_physicsEnded = _registry->OnDestroy<InPhysics>().connect<&ObjectDrawListSystem::OnPhysicsEnded>(*this);
}

ObjectDrawListSystem::~ObjectDrawListSystem()
{
	// The registry may have gone first, taking its signals with it
	if (Locator::entitiesRegistry::has_value() && &Locator::entitiesRegistry::value() == _registry)
	{
		_physicsEnded.release();
	}
}

void ObjectDrawListSystem::OnPhysicsEnded(entt::registry& /*registry*/, entt::entity /*entity*/)
{
	RequestRebuild(1);
}

void ObjectDrawListSystem::RequestRebuild(uint32_t frames)
{
	_rebuildsAsked = std::max(_rebuildsAsked, frames);
}

void ObjectDrawListSystem::LoadBlocks()
{
	if (!_blocks.empty() || !Locator::terrainSystem::has_value())
	{
		return;
	}
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
		_blocks.push_back({.block = {.corner = block.GetMapPosition(), .highestAltitude = highest, .reflected = true},
		                   .at = block.GetBlockPosition()});
	}
}

void ObjectDrawListSystem::Update(uint32_t turn)
{
	if (!Locator::camera::has_value() || !Locator::terrainSystem::has_value() || !Locator::entitiesMap::has_value() ||
	    _registry == nullptr)
	{
		// With nothing to see from, the list decides nothing
		_view.reset();
		_frame = {.rebuilt = true, .drawAll = true};
		return;
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
	// Things made since the last frame go onto their blocks now, as the game puts them there when they are made
	static_cast<void>(Locator::entitiesMap::value().GetOffBlocks());
	_frame = _clock.Next(turn, _view->eye, camera.GetFocus(), changed || _rebuildsAsked > 0);
	if (_frame.rebuilt)
	{
		if (_rebuildsAsked > 0)
		{
			--_rebuildsAsked;
		}
		_madeAt = turn;
		MakeList();
	}
}

void ObjectDrawListSystem::MakeList()
{
	for (const auto entity : _list)
	{
		_listedByIndex.at(static_cast<size_t>(entt::to_entity(entity))) = entt::null;
	}
	std::vector<object_draw_list::NearBlock> inView;
	for (const auto& block : _blocks)
	{
		if (block.inView)
		{
			inView.push_back({.block = block.at, .distance = object_draw_list::BlockDistance(_view->eye, block.block)});
		}
	}
	const auto nearToFar = object_draw_list::NearToFar(inView);
	const auto& map = Locator::entitiesMap::value();
	const auto offBlocks = map.GetOffBlocks();
	_blocksWalked = 0;
	_farthestWalked = 0.0f;
	for (const auto& block : nearToFar)
	{
		if (block.distance >= object_draw_list::k_Range)
		{
			break;
		}
		++_blocksWalked;
		_farthestWalked = block.distance;
	}
	_list = object_draw_list::MakeList(
	    nearToFar, [&map](glm::ivec2 block) { return map.GetOnBlock(block); }, offBlocks,
	    [this](entt::entity entity) { return DrawnOnTheirOwn(*_registry, entity); });
	for (const auto entity : _list)
	{
		const auto index = static_cast<size_t>(entt::to_entity(entity));
		if (index >= _listedByIndex.size())
		{
			_listedByIndex.resize(index + 1, entt::null);
		}
		_listedByIndex[index] = entity;
	}
}

bool ObjectDrawListSystem::Listed(entt::entity entity) const
{
	const auto index = static_cast<size_t>(entt::to_entity(entity));
	return index < _listedByIndex.size() && _listedByIndex[index] == entity;
}

bool ObjectDrawListSystem::Drawn(const Registry& registry, entt::entity entity) const
{
	if (!_view.has_value())
	{
		return true;
	}
	const auto* resident = registry.TryGet<const MapCellResident>(entity);
	if (resident == nullptr || resident->cells.empty() || DrawnOnTheirOwn(registry, entity))
	{
		return true;
	}
	return Listed(entity);
}

ObjectDrawListSystemInterface::Summary ObjectDrawListSystem::GetSummary() const
{
	return {.active = _view.has_value(),
	        .frame = _frame,
	        .madeAt = _madeAt,
	        .blocksInView = static_cast<uint32_t>(std::ranges::count_if(_blocks, &LandBlock::inView)),
	        .blocksWalked = _blocksWalked,
	        .farthestWalked = _farthestWalked,
	        .things = static_cast<uint32_t>(_list.size()),
	        .full = _list.size() >= object_draw_list::k_MostObjects,
	        .rebuildsAsked = _rebuildsAsked};
}

void ObjectDrawListSystem::Reset()
{
	_clock.Reset();
	_blocks.clear();
	_view.reset();
	_frame = {.rebuilt = true, .drawAll = true};
	for (const auto entity : _list)
	{
		_listedByIndex.at(static_cast<size_t>(entt::to_entity(entity))) = entt::null;
	}
	_list.clear();
	_madeAt.reset();
	_rebuildsAsked = 0;
}
