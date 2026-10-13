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
#include <array>

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
#include "Graphics/LandDetail.h"
#include "Locator.h"

using namespace openblack::ecs::systems;
using namespace openblack::ecs::components;
namespace object_draw_list = openblack::object_draw_list;

namespace
{
/// Bit 1 of a cell's flags marks open sea, where the land is not drawn
constexpr uint8_t k_OpenSeaFlag = 0x02;

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
	const auto& blocks = Locator::terrainSystem::value().GetBlocks();
	for (size_t i = 0; i < blocks.size(); ++i)
	{
		const auto& block = blocks[i];
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
		// Its triangles as the game draws a near block: every cell, with the open sea left out
		std::array<graphics::land_detail::CellFlags, graphics::land_detail::k_CornerCount> flags;
		std::ranges::transform(lnd->cells, flags.begin(), [](const lnd::LNDCell& cell) {
			return graphics::land_detail::CellFlags {.split = cell.properties.split != 0,
			                                         .openSea = (cell.flags & k_OpenSeaFlag) != 0};
		});
		_blocks.push_back(
		    {.block = {.corner = block.GetMapPosition(), .highestAltitude = highest, .reflected = true},
		     .at = block.GetBlockPosition(),
		     .landIndex = i,
		     .triangles = graphics::land_detail::BlockTriangles(graphics::land_detail::Mesh::Full, 0, true, flags)});
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
	// A block in view that starts or stops to show triangles makes the list again
	bool changed = false;
	for (auto& block : _blocks)
	{
		block.inView = object_draw_list::BlockInView(_view->viewProjection, _view->nearPlane, block.block);
		if (block.inView)
		{
			const bool shows = Shows(block, _view->viewProjection, _view->nearPlane, _view->eye);
			changed = object_draw_list::ShowingChanged(shows, block.shown) || changed;
		}
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

bool ObjectDrawListSystem::Shows(const LandBlock& block, const glm::mat4& viewProjection, float nearPlane, glm::vec3 eye)
{
	const auto& blocks = Locator::terrainSystem::value().GetBlocks();
	const auto* cells = block.landIndex < blocks.size() ? blocks[block.landIndex].GetCells() : nullptr;
	if (cells == nullptr)
	{
		return false;
	}
	// The corners stand at the heights the cells have now
	const auto cornerAt = [&block, cells](uint16_t corner) {
		const auto x = static_cast<float>(corner / graphics::land_detail::k_CornersPerSide);
		const auto z = static_cast<float>(corner % graphics::land_detail::k_CornersPerSide);
		return glm::vec3(block.block.corner.x + x * LandIslandInterface::k_CellSize,
		                 static_cast<float>(cells[corner].altitude) * object_draw_list::k_AltitudeUnit,
		                 block.block.corner.y + z * LandIslandInterface::k_CellSize);
	};
	return object_draw_list::BlockShowsWith(viewProjection, nearPlane, eye, graphics::land_detail::k_CornerCount, cornerAt,
	                                        block.triangles);
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
