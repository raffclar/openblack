/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#define LOCATOR_IMPLEMENTATIONS

#include "CameraBookmarkSystem.h"

#include <vector>

#include <glm/gtx/vec_swizzle.hpp>

#include "3D/LandIslandInterface.h"
#include "3D/PhysicsDrawMatrix.h"
#include "3D/TempleInteriorInterface.h"
#include "Camera/Camera.h"
#include "Camera/CreatureCameraModel.h"
#include "Camera/DanceCameraModel.h"
#include "Camera/DefaultWorldCameraModel.h"
#include "Camera/DoubleClickFlight.h"
#include "Camera/FightCameraModel.h"
#include "ECS/Archetypes/CameraBookmarkArchetype.h"
#include "ECS/BuildingSites.h"
#include "ECS/Components/Abode.h"
#include "ECS/Components/CameraBookmark.h"
#include "ECS/Components/Physics.h"
#include "ECS/Components/Town.h"
#include "ECS/Components/Transform.h"
#include "ECS/PlayerTowns.h"
#include "ECS/Reflection/BinaryArchive.h"
#include "ECS/Registry.h"
#include "ECS/Systems/CreatureModeSystemInterface.h"
#include "ECS/Systems/HandSystemInterface.h"
#include "ECS/Systems/PlayerSystemInterface.h"
#include "ECS/TempleConstruction.h"
#include "Locator.h"

using namespace openblack;
using namespace openblack::ecs::systems;
using namespace openblack::ecs::components;

namespace
{

/// The place of the player's hand on the land, or the camera's focus when the hand is not on the land
glm::vec3 HandPoint(const Camera& camera)
{
	// The keys of this computer's player measure from the hand that this computer draws
	if (Locator::handSystem::has_value())
	{
		if (const auto position = Locator::handSystem::value().GetDrawnHandPosition(); position.has_value())
		{
			return *position;
		}
	}
	return camera.GetFocus();
}

/// The place that the Tab key flies to in a town: its working storage pit, else its village centre, on the land
std::optional<glm::vec3> TownPlace(entt::entity town)
{
	const auto& registry = Locator::entitiesRegistry::value();
	std::optional<glm::vec3> place;
	if (const auto pit = ecs::construction::StoragePitOf(town); pit.has_value() && ecs::construction::IsBuilt(*pit))
	{
		place = registry.Get<const Transform>(*pit).position;
	}
	else
	{
		const auto id = registry.Get<const Town>(town).id;
		registry.Each<const Abode, const Transform>([&](entt::entity, const Abode& abode, const Transform& transform) {
			if (!place.has_value() && abode.type == AbodeNumber::TownCentre && abode.townId == id)
			{
				place = transform.position;
			}
		});
	}
	if (place.has_value() && Locator::terrainSystem::has_value())
	{
		place->y = Locator::terrainSystem::value().GetHeightAt(glm::xz(*place));
	}
	return place;
}

} // namespace

bool CameraBookmarkSystem::Initialize()
{
	_bookmarks = archetypes::CameraBookmarkArchetype::CreateAll();
	_back.reset();
	_townKey.reset();
	_lastTown.reset();
	_current = 0;
	_followed.reset();
	_pulseClockMs = 0;
	return true;
}

void CameraBookmarkSystem::FindAfterLoad()
{
	_bookmarks.fill(entt::null);
	Locator::entitiesRegistry::value().Each<const CameraBookmark>([this](entt::entity entity, const CameraBookmark& bookmark) {
		if (bookmark.number < _bookmarks.size())
		{
			_bookmarks.at(bookmark.number) = entity;
		}
	});
	// A saved world from before the seven slots can lack a slot: the slot gets a new bookmark, not set
	for (uint8_t number = 0; number < _bookmarks.size(); ++number)
	{
		if (_bookmarks.at(number) == entt::null)
		{
			_bookmarks.at(number) = archetypes::CameraBookmarkArchetype::Create(number);
		}
	}
	_back.reset();
	_lastTown.reset();
	_followed.reset();
}

void CameraBookmarkSystem::SaveState(reflection::BinaryWriter& out) const
{
	const auto backPoint = _back.has_value() ? std::optional(_back->point) : std::nullopt;
	const auto backOffset = _back.has_value() ? std::optional(_back->cameraOffset) : std::nullopt;
	reflection::Save(out, backPoint);
	reflection::Save(out, backOffset);
	reflection::Save(out, _current);
	reflection::Save(out, _followed);
	reflection::Save(out, _enabled);
	const auto townPoint = _townKey.has_value() ? std::optional(_townKey->point) : std::nullopt;
	const auto townOffset = _townKey.has_value() ? std::optional(_townKey->cameraOffset) : std::nullopt;
	reflection::Save(out, townPoint);
	reflection::Save(out, townOffset);
}

bool CameraBookmarkSystem::LoadState(reflection::BinaryReader& in)
{
	std::optional<glm::vec3> backPoint;
	std::optional<glm::vec3> backOffset;
	size_t current = 0;
	std::optional<size_t> followed;
	bool enabled = true;
	reflection::Load(in, backPoint);
	reflection::Load(in, backOffset);
	reflection::Load(in, current);
	reflection::Load(in, followed);
	reflection::Load(in, enabled);
	// A save made before the town key was kept has no town key
	std::optional<glm::vec3> townPoint;
	std::optional<glm::vec3> townOffset;
	if (in.Remaining() > 0)
	{
		reflection::Load(in, townPoint);
		reflection::Load(in, townOffset);
	}
	if (in.Failed())
	{
		return false;
	}
	_back = backPoint.has_value() && backOffset.has_value()
	            ? std::optional(View {.point = *backPoint, .cameraOffset = *backOffset})
	            : std::nullopt;
	_townKey = townPoint.has_value() && townOffset.has_value()
	               ? std::optional(View {.point = *townPoint, .cameraOffset = *townOffset})
	               : std::nullopt;
	_current = current;
	_followed = followed;
	_enabled = enabled;
	return true;
}

void CameraBookmarkSystem::Update(std::chrono::milliseconds frameGameTime, bool landDrawn)
{
	// The throb moves on only on a frame that draws the land
	if (landDrawn)
	{
		_pulseClockMs = camera_bookmarks::StepPulseClock(_pulseClockMs, static_cast<int32_t>(frameGameTime.count()));
	}
	const auto size = glm::vec3(GetMarkerHalfSize(), 1.0f);
	auto& registry = Locator::entitiesRegistry::value();
	registry.Each<const CameraBookmark, Transform>([&registry, size](const CameraBookmark& bookmark, Transform& transform) {
		// A marker on a thing goes where the frame draws the thing
		auto point = bookmark.point;
		if (bookmark.thing != entt::null && registry.Valid(bookmark.thing))
		{
			if (const auto* at = registry.TryGet<const Transform>(bookmark.thing); at != nullptr)
			{
				point = physics_draw::Position(at->position, registry.TryGet<const PhysicsDrawPose>(bookmark.thing));
			}
		}
		transform.position = camera_bookmarks::MarkerPlace(point);
		transform.scale = size;
	});
}

void CameraBookmarkSystem::ProcessTurn()
{
	auto& registry = Locator::entitiesRegistry::value();
	for (const auto entity : _bookmarks)
	{
		auto* bookmark = registry.TryGet<CameraBookmark>(entity);
		if (bookmark == nullptr || !bookmark->set || bookmark->thing == entt::null)
		{
			continue;
		}
		// A bookmark forgets a thing that has gone, and stays at its last place
		const auto* at = registry.Valid(bookmark->thing) ? registry.TryGet<const Transform>(bookmark->thing) : nullptr;
		if (at == nullptr)
		{
			bookmark->thing = entt::null;
			continue;
		}
		bookmark->point = at->position;
	}

	// The world camera flies to the bookmark that it follows, each turn. Another camera stops the follow. A move of the
	// camera by the player also stops it.
	if (!Locator::camera::has_value())
	{
		return;
	}
	const auto* world = dynamic_cast<const DefaultWorldCameraModel*>(&Locator::camera::value().GetModel());
	if (world == nullptr)
	{
		_followed.reset();
	}
	else if (_followed.has_value())
	{
		JumpTo(*_followed);
	}
	if (world != nullptr && world->MovedByPlayer())
	{
		_followed.reset();
	}
}

void CameraBookmarkSystem::SetBookmark(size_t slot, glm::vec3 point, std::optional<entt::entity> thing, glm::vec3 cameraOrigin)
{
	if (slot >= _bookmarks.size())
	{
		return;
	}
	_followed.reset();
	_current = slot;
	auto& registry = Locator::entitiesRegistry::value();
	const auto entity = _bookmarks.at(slot);
	auto& bookmark = registry.Get<CameraBookmark>(entity);
	bookmark.set = true;
	bookmark.thing = entt::null;
	bookmark.point = point;
	// A bookmark on a thing starts at the place of the thing
	if (thing.has_value() && registry.Valid(*thing))
	{
		if (const auto* at = registry.TryGet<const Transform>(*thing); at != nullptr)
		{
			bookmark.thing = *thing;
			bookmark.point = at->position;
		}
	}
	bookmark.cameraOffset = cameraOrigin - bookmark.point;
	registry.AssignOrReplace<Transform>(entity, camera_bookmarks::MarkerPlace(bookmark.point), glm::mat3(1.0f),
	                                    glm::vec3(GetMarkerHalfSize(), 1.0f));
}

void CameraBookmarkSystem::ClearBookmark(size_t slot)
{
	if (slot >= _bookmarks.size())
	{
		return;
	}
	auto& registry = Locator::entitiesRegistry::value();
	const auto entity = _bookmarks.at(slot);
	auto& bookmark = registry.Get<CameraBookmark>(entity);
	bookmark.set = false;
	bookmark.thing = entt::null;
	registry.Remove<Transform>(entity);
}

bool CameraBookmarkSystem::IsSet(size_t slot) const
{
	if (slot >= _bookmarks.size())
	{
		return false;
	}
	const auto* bookmark = Locator::entitiesRegistry::value().TryGet<const CameraBookmark>(_bookmarks.at(slot));
	return bookmark != nullptr && bookmark->set;
}

std::optional<CameraBookmarkSystem::View> CameraBookmarkSystem::ViewOf(size_t slot) const
{
	if (slot == camera_bookmarks::k_BackSlot)
	{
		return _back;
	}
	if (!IsSet(slot))
	{
		return std::nullopt;
	}
	const auto& bookmark = Locator::entitiesRegistry::value().Get<const CameraBookmark>(_bookmarks.at(slot));
	return View {.point = bookmark.point, .cameraOffset = bookmark.cameraOffset};
}

void CameraBookmarkSystem::JumpTo(size_t slot)
{
	const auto view = ViewOf(slot);
	const bool inTemple = Locator::temple::has_value() && Locator::temple::value().Active();
	if (!view.has_value() || inTemple || !Locator::camera::has_value())
	{
		return;
	}
	_current = slot;
	if (_followed != slot)
	{
		_followed.reset();
	}
	auto& camera = Locator::camera::value();
	// The jump flies the world camera. From the creature's camera the game first changes to the world camera, but not
	// from a camera that cannot be left: that of the hand on a creature or of a script. The fight view is a part of the
	// world camera.
	const auto isWorld = [&camera]() {
		const auto& model = camera.GetModel();
		return dynamic_cast<const DefaultWorldCameraModel*>(&model) != nullptr ||
		       dynamic_cast<const FightCameraModel*>(&model) != nullptr;
	};
	if (!isWorld())
	{
		if (!camera.GetModel().CanBeLeft())
		{
			return;
		}
		if (Locator::creatureModeSystem::has_value() && dynamic_cast<const CreatureCameraModel*>(&camera.GetModel()) != nullptr)
		{
			Locator::creatureModeSystem::value().Leave();
		}
		// From the camera of a dance, a new world camera starts from where the camera is, over the camera of the dance
		else if (dynamic_cast<const DanceCameraModel*>(&camera.GetModel()) != nullptr)
		{
			camera.SetModel(std::make_unique<DefaultWorldCameraModel>(camera.GetOrigin(), camera.GetFocus()));
		}
		if (!isWorld())
		{
			return;
		}
	}
	camera.GetModel().SetFlight(view->point + view->cameraOffset, view->point, camera_fly::k_BookmarkRise);
	// The go-back key keeps the view from before the jump: the place of the hand, and the position of the camera from it
	const auto handPoint = HandPoint(camera);
	_back = View {.point = handPoint, .cameraOffset = camera.GetOrigin() - handPoint};
}

void CameraBookmarkSystem::JumpBack()
{
	JumpTo(camera_bookmarks::k_BackSlot);
}

std::array<bool, camera_bookmarks::k_SteppedSlots> CameraBookmarkSystem::SteppedSet() const
{
	std::array<bool, camera_bookmarks::k_SteppedSlots> set {};
	for (size_t slot = 0; slot < set.size(); ++slot)
	{
		set.at(slot) = IsSet(slot);
	}
	return set;
}

void CameraBookmarkSystem::JumpToNext()
{
	if (const auto slot = camera_bookmarks::NextSet(_current, SteppedSet()); slot.has_value())
	{
		JumpTo(*slot);
	}
}

void CameraBookmarkSystem::JumpToPrevious()
{
	if (const auto slot = camera_bookmarks::PreviousSet(_current, SteppedSet()); slot.has_value())
	{
		JumpTo(*slot);
	}
}

void CameraBookmarkSystem::FlyToNextTown(std::optional<entt::entity> thingUnderHand)
{
	if (!Locator::playerSystem::has_value() || !Locator::camera::has_value())
	{
		return;
	}
	auto& registry = Locator::entitiesRegistry::value();
	const auto towns = player_towns::InOrder(registry, Locator::playerSystem::value().GetLocalPlayer());
	std::vector<camera_bookmarks::TownStop> stops;
	std::vector<std::optional<glm::vec3>> places;
	stops.reserve(towns.size());
	places.reserve(towns.size());
	for (const auto town : towns)
	{
		places.push_back(TownPlace(town));
		stops.push_back({.id = registry.Get<const Town>(town).id, .hasPlace = places.back().has_value()});
	}
	const auto next = camera_bookmarks::NextTown(stops, _lastTown);
	if (!next.has_value())
	{
		return;
	}
	const auto place = *places.at(*next);
	auto& camera = Locator::camera::value();
	// The town key keeps the place as a number key does: on the thing under the hand, if there is one
	_followed.reset();
	_current = camera_bookmarks::k_TownSlot;
	auto point = place;
	if (thingUnderHand.has_value() && registry.Valid(*thingUnderHand))
	{
		if (const auto* at = registry.TryGet<const Transform>(*thingUnderHand); at != nullptr)
		{
			point = at->position;
		}
	}
	_townKey = View {.point = point, .cameraOffset = camera.GetOrigin() - point};
	_lastTown = stops.at(*next).id;
	// Only the world camera flies
	const auto& model = camera.GetModel();
	if (dynamic_cast<const DefaultWorldCameraModel*>(&model) != nullptr ||
	    dynamic_cast<const FightCameraModel*>(&model) != nullptr)
	{
		camera.GetModel().SetFlight(camera_bookmarks::TownViewFrom(place), place, camera_fly::k_BookmarkRise);
	}
}

void CameraBookmarkSystem::ToggleFollow(size_t slot)
{
	if (!IsSet(slot))
	{
		return;
	}
	const auto& bookmark = Locator::entitiesRegistry::value().Get<const CameraBookmark>(_bookmarks.at(slot));
	switch (camera_bookmarks::ShiftKey(bookmark.thing != entt::null, _followed, slot))
	{
	case camera_bookmarks::FollowKey::Jump:
		JumpTo(slot);
		_followed.reset();
		break;
	case camera_bookmarks::FollowKey::Follow:
		_followed = slot;
		break;
	case camera_bookmarks::FollowKey::Stop:
		_followed.reset();
		break;
	}
}
