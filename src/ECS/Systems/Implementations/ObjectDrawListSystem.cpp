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
#include "ECS/Components/HandGrab.h"
#include "ECS/Components/HighDetail.h"
#include "ECS/Components/MapCellResident.h"
#include "ECS/Components/Temple.h"
#include "ECS/Components/VillageTotem.h"
#include "ECS/Components/WalkPath.h"
#include "ECS/Map.h"
#include "ECS/Registry.h"
#include "EngineConfig.h"
#include "Locator.h"
#include "Visual/DetailLevel.h"
#include "Visual/LandDetail.h"

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
	return object_draw_list::DrawnOnItsOwn({
	    .temple = registry.AllOf<Temple>(entity),
	    .highDetail = registry.AllOf<HighDetail>(entity),
	    .walkingTrack = registry.AllOf<WalkPath>(entity),
	});
}
} // namespace

ObjectDrawListSystem::ObjectDrawListSystem()
{
	if (!Locator::entitiesRegistry::has_value())
	{
		return;
	}
	_registry = &Locator::entitiesRegistry::value();
	// When the hand releases the thing that it holds, make the list again on the next two frames
	_letGo = _registry->OnDestroy<InHand>().connect<&ObjectDrawListSystem::OnLetGo>(*this);
}

ObjectDrawListSystem::~ObjectDrawListSystem()
{
	// The registry may have gone first, taking its signals with it
	if (Locator::entitiesRegistry::has_value() && &Locator::entitiesRegistry::value() == _registry)
	{
		_letGo.release();
	}
}

void ObjectDrawListSystem::OnLetGo(entt::registry& /*registry*/, entt::entity /*entity*/)
{
	RequestRebuild(2);
}

void ObjectDrawListSystem::RequestRebuild(uint32_t frames)
{
	_requests.Ask(frames);
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
		// Its triangles as the game draws a near block: every cell, with the open sea left out
		std::array<graphics::land_detail::CellFlags, graphics::land_detail::k_CornerCount> flags;
		std::ranges::transform(lnd->cells, flags.begin(), [](const lnd::LNDCell& cell) {
			return graphics::land_detail::CellFlags {.split = cell.properties.split != 0,
			                                         .openSea = (cell.flags & k_OpenSeaFlag) != 0};
		});
		_blocks.push_back(
		    {.block = {.corner = block.GetMapPosition(), .highestAltitude = block.GetHighestAltitude()},
		     .at = block.GetBlockPosition(),
		     .landIndex = i,
		     .triangles = graphics::land_detail::BlockTriangles(graphics::land_detail::Mesh::Full, 0, true, flags)});
	}
}

void ObjectDrawListSystem::PrepareLand()
{
	LoadBlocks();
}

void ObjectDrawListSystem::FindParts()
{
	_parts.Clear();
	if (_registry == nullptr)
	{
		return;
	}
	// The draw of a totem draws its icon and its second plinth and icon. Only the test of the icon counts for the totem.
	_registry->Each<const VillageTotem>([this](entt::entity totem, const VillageTotem& parts) {
		for (const auto& [part, keeps] :
		     {std::pair {parts.icon, true}, std::pair {parts.ghost, false}, std::pair {parts.ghostIcon, false}})
		{
			if (_registry->Valid(part))
			{
				_parts.Add(part, {.owner = totem, .keepsOwner = keeps});
			}
		}
	});
}

void ObjectDrawListSystem::Update(uint32_t turn, bool landDrawn)
{
	FindParts();
	if (!Locator::camera::has_value() || !Locator::terrainSystem::has_value() || !Locator::entitiesMap::has_value() ||
	    _registry == nullptr)
	{
		// With nothing to see from, the list decides nothing
		_view.reset();
		_frame = {.rebuilt = true, .drawAll = true};
		return;
	}
	if (!landDrawn)
	{
		// Things made since the last frame go onto their blocks now, also on a frame that does not draw the land. The
		// blocks, their last showing, the clock's camera, the requests and the list stay as they are.
		static_cast<void>(Locator::entitiesMap::value().GetOffBlocks());
		_frame = object_draw_list::FrameWithoutLand();
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
	// A block in view that starts or stops to show triangles makes the list again. With the land mirrored in the sea,
	// a block's box reaches as far under the water as it stands over it.
	const bool reflected = !Locator::config::has_value() || Locator::config::value().detail.landReflection;
	bool changed = false;
	for (auto& block : _blocks)
	{
		block.block.reflected = reflected;
		block.inView = object_draw_list::BlockInView(_view->viewProjection, _view->nearPlane, block.block);
		if (block.inView)
		{
			const bool shows = Shows(block, _view->viewProjection, _view->nearPlane, _view->eye);
			changed = object_draw_list::ShowingChanged(shows, block.shown) || changed;
		}
	}
	// Things made since the last frame go onto their blocks now, as the game puts them there when they are made
	static_cast<void>(Locator::entitiesMap::value().GetOffBlocks());
	_frame = _clock.Next(turn, _view->eye, camera.GetFocus(), changed || _requests.Pending());
	if (_frame.drawAll)
	{
		_keptParts.Clear();
	}
	if (_frame.rebuilt)
	{
		_requests.Rebuilt();
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
			_keptByIndex.resize(index + 1, false);
		}
		_listedByIndex[index] = entity;
		// The full frame that follows decides it
		_keptByIndex[index] = false;
	}
}

bool ObjectDrawListSystem::Listed(entt::entity entity) const
{
	const auto index = static_cast<size_t>(entt::to_entity(entity));
	return index < _listedByIndex.size() && _listedByIndex[index] == entity;
}

bool ObjectDrawListSystem::Decides(const Registry& registry, entt::entity entity) const
{
	if (const auto part = _parts.Of(entity))
	{
		return Decides(registry, part->owner);
	}
	const auto* resident = registry.TryGet<const MapCellResident>(entity);
	return _view.has_value() && resident != nullptr && !resident->cells.empty() && !DrawnOnTheirOwn(registry, entity);
}

bool ObjectDrawListSystem::Drawn(const Registry& registry, entt::entity entity) const
{
	// A part is drawn when its owner is drawn
	if (const auto part = _parts.Of(entity))
	{
		return Drawn(registry, part->owner);
	}
	if (!Decides(registry, entity))
	{
		return true;
	}
	if (!_frame.landDrawn || !Listed(entity))
	{
		return false;
	}
	return _frame.drawAll || _keptByIndex[static_cast<size_t>(entt::to_entity(entity))];
}

bool ObjectDrawListSystem::NotesKept(entt::entity entity) const
{
	const auto part = _parts.Of(entity);
	return part.has_value() ? part->keepsOwner && Listed(part->owner) : Listed(entity);
}

void ObjectDrawListSystem::NoteFullFrame(entt::entity entity, bool staysDrawn)
{
	if (!_frame.drawAll)
	{
		return;
	}
	// A part that stays drawn keeps its owner drawn, whatever the test of the owner says
	if (const auto part = _parts.Of(entity))
	{
		if (part->keepsOwner && staysDrawn && Listed(part->owner))
		{
			_keptParts.Keep(part->owner);
			_keptByIndex[static_cast<size_t>(entt::to_entity(part->owner))] = true;
		}
		return;
	}
	if (Listed(entity))
	{
		_keptByIndex[static_cast<size_t>(entt::to_entity(entity))] = staysDrawn || _keptParts.Keeps(entity);
	}
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
	        .kept = static_cast<uint32_t>(std::ranges::count_if(
	            _list, [this](entt::entity entity) { return _keptByIndex[static_cast<size_t>(entt::to_entity(entity))]; })),
	        .full = _list.size() >= object_draw_list::k_MostObjects,
	        .rebuildsAsked = _requests.Left()};
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
	_requests.Reset();
	_parts.Clear();
	_keptParts.Clear();
}
