/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#define LOCATOR_IMPLEMENTATIONS

#include "VillagerDance.h"

#include <algorithm>
#include <bit>

#include <glm/gtx/euler_angles.hpp>
#include <glm/gtx/vec_swizzle.hpp>

#include "Common/GUtilsAngle.h"
#include "Common/GUtilsDistance.h"
#include "Common/GameRandom.h"
#include "ECS/Components/Dance.h"
#include "ECS/Components/LivingAction.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/Villager.h"
#include "ECS/Components/VillagerPose.h"
#include "ECS/Components/WallHug.h"
#include "ECS/DanceMoves.h"
#include "ECS/DanceRules.h"
#include "ECS/Dances.h"
#include "ECS/Registry.h"
#include "ECS/Systems/LivingActionSystemInterface.h"
#include "ECS/VillagerClips.h"
#include "ECS/WallHugRules.h"
#include "InfoConstants.h"
#include "Locator.h"
#include "VillagerAnimate.h"
#include "VillagerHome.h"

using namespace openblack;
using namespace openblack::ecs::components;
namespace villager_dance = openblack::ecs::villager_dance;
namespace moves = openblack::ecs::dance_moves;
using openblack::map_coords::MapCoords;

namespace
{
/// The state the interface puts a villager in
constexpr auto k_InterfaceState = static_cast<VillagerStates>(24);

const GVillagerStateTableInfo& StateInfo(VillagerStates state)
{
	return Locator::infoConstants::value().villagerStateTable.at(static_cast<size_t>(state));
}

/// The dance a villager dances in and its group there, with its place in the group; none when it is in no group
struct Placed
{
	Dance* dance {nullptr};
	std::size_t group {0};
	std::size_t slot {0};
};

std::optional<Placed> PlaceOf(entt::entity villager)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto dance = ecs::dances::DanceOf(registry, villager);
	if (dance == entt::null)
	{
		return std::nullopt;
	}
	const auto& dancer = registry.Get<const Dancer>(villager);
	auto& data = registry.Get<Dance>(dance);
	if (dancer.group >= data.groups.all.size())
	{
		return std::nullopt;
	}
	const auto& dancers = data.groups.all[dancer.group].dancers;
	const auto found = std::ranges::find(dancers, villager);
	if (found == dancers.end())
	{
		return std::nullopt;
	}
	return Placed {.dance = &data, .group = dancer.group, .slot = static_cast<std::size_t>(found - dancers.begin())};
}

/// Where the villager stands, as a map position: where its walk holds it, unless something else moved it since
MapCoords MapPositionOf(entt::entity villager)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto metres = glm::xz(registry.Get<const Transform>(villager).position);
	const auto* wallHug = registry.TryGet<const WallHug>(villager);
	const auto whole = wallHug != nullptr && wallHug->placedAt == metres ? wallHug->position : ecs::wall_hug::ToWhole(metres);
	return MapCoords {.x = whole.x, .z = whole.y};
}

/// The villager faces along a game angle at once, kept as given
void SetGameAngle(entt::entity villager, uint16_t angle)
{
	auto& registry = Locator::entitiesRegistry::value();
	auto* wallHug = registry.TryGet<WallHug>(villager);
	auto* transform = registry.TryGet<Transform>(villager);
	if (wallHug == nullptr || transform == nullptr)
	{
		return;
	}
	wallHug->gameAngle = angle;
	wallHug->yAngle = gutils::ConvertGameAngleTo3D(angle);
	transform->rotation = glm::eulerAngleY(-wallHug->yAngle - glm::radians(90.0f));
	registry.SetDirty();
}
} // namespace

uint32_t villager_dance::DanceSex(entt::entity villager)
{
	const auto* person = Locator::entitiesRegistry::value().TryGet<const Villager>(villager);
	if (person == nullptr || !Locator::infoConstants::has_value())
	{
		return ecs::components::DanceGroup::k_AnySex;
	}
	const auto& info =
	    Locator::infoConstants::value().villager.at(static_cast<size_t>(GVillagerInfo::Find(person->tribe, person->number)));
	return info.sex == SexType::Male ? ecs::components::DanceGroup::k_Men : ecs::components::DanceGroup::k_Women;
}

uint32_t villager_dance::InDance(LivingAction& action)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto villager = registry.ToEntity(action);
	const auto placed = PlaceOf(villager);
	if (!placed.has_value())
	{
		// A dancer in no group stands where it is
		return 1;
	}
	const auto& dance = *placed->dance;
	const auto& group = dance.groups.all[placed->group];
	const auto place = moves::SlotPosition(dance, placed->group, placed->slot, moves::Shapes());
	// Walks keep their goals in metres here: the place is taken as the walk would reach it, so that a dancer that has
	// walked there is there
	const auto whole = ecs::wall_hug::ToWhole(map_coords::ToMetres(place));
	const MapCoords slot {.x = whole.x, .z = whole.y, .altitude = place.altitude};
	const auto at = MapPositionOf(villager);
	const float distance = gutils::GetDistanceInMetres(at, slot);
	const auto* wallHug = registry.TryGet<const WallHug>(villager);
	const auto facing = moves::Facing({
	    .action = static_cast<moves::Action>(group.move.action),
	    .facing = wallHug != nullptr ? wallHug->gameAngle : uint16_t {0},
	    .towardsCentre = gutils::GetAngleFromXZ(at, moves::GroupCentre(dance, placed->group)),
	    .danceAngle = dance.angle,
	    .rotation = group.rotation,
	    .second = std::bit_cast<float>(group.move.second),
	    .distance = distance,
	});
	if (facing.has_value())
	{
		SetGameAngle(villager, *facing);
	}
	// TODO(opening): looking at the first dancer of another group (2), and the belief a worshipping dancer gives its
	// town's player (14)
	if (distance > 0.0f)
	{
		// It walks to its place, and dances again there; a place more than 4 m off is walked to round what is in the way
		// TODO(opening): the game's walk round things for places more than 4 m off, and the walk's first step taken this
		// turn rather than on the next
		auto& living = Locator::livingActionSystem::value();
		if (living.VillagerSetCurrentAndDestinationState(action, VillagerStates::MoveToPos, VillagerStates::InDance))
		{
			villager_home::SetupMobileMoveTo(action, map_coords::ToMetres(slot), VillagerStates::InDance);
		}
		living.VillagerSetState(action, LivingAction::Index::Top, VillagerStates::MoveToDancePos, true);
	}
	// TODO(opening): it remembers it was dancing, to go back to the script's state after reacting
	return 1;
}

uint32_t villager_dance::MoveToDancePos(LivingAction& action)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto villager = registry.ToEntity(action);
	const auto placed = PlaceOf(villager);
	if (!placed.has_value())
	{
		return 0;
	}
	// A group that moved this turn moves its dancers' places with it
	if (placed->dance->groups.all[placed->group].moved)
	{
		const auto slot = moves::SlotPosition(*placed->dance, placed->group, placed->slot, moves::Shapes());
		if (auto* wallHug = registry.TryGet<WallHug>(villager); wallHug != nullptr)
		{
			wallHug->goal = map_coords::ToMetres(slot);
		}
	}
	return villager_home::MoveToPos(action);
}

int32_t villager_dance::DanceClip(entt::entity villager, int32_t walkClip)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto placed = PlaceOf(villager);
	if (!placed.has_value())
	{
		return moves::DanceClip({}, {});
	}
	const auto& dance = *placed->dance;
	const auto& group = dance.groups.all[placed->group];
	std::optional<moves::Action> parentAction;
	if (group.parent.has_value() && *group.parent < dance.groups.all.size())
	{
		parentAction = static_cast<moves::Action>(dance.groups.all[*group.parent].move.action);
	}
	const auto* person = registry.TryGet<const Villager>(villager);
	return moves::DanceClip(
	    {
	        .inGroup = true,
	        .stopped = dance.state == Dance::State::Stopped,
	        .action = group.move.action,
	        .first = group.move.first,
	        .parentAction = parentAction,
	        .rate = dance.rate,
	        .female = person != nullptr && person->sex == Villager::Sex::FEMALE,
	        .walkClip = walkClip,
	    },
	    [](uint32_t limit) { return Locator::gameRandom::value().GameRand(limit); });
}

void villager_dance::PlayClipAgain(entt::entity villager)
{
	auto& registry = Locator::entitiesRegistry::value();
	if (!registry.Valid(villager) || !registry.AllOf<Villager, LivingAction>(villager))
	{
		return;
	}
	// Its part in the move may want another clip, which takes over in time with the dance
	villager_animate::SetAnim(villager, villager_animate::StateClip(villager), true);
	// The clip starts over with the move once it has played through
	const auto clip = villager_animate::CurrentClip(villager);
	if (!villager_clips::ClipMilliseconds(clip).has_value())
	{
		return;
	}
	auto& action = registry.Get<LivingAction>(villager);
	const auto move = villager_clips::OnDanceMove(action, clip);
	action.turnsSinceStateChange = static_cast<uint16_t>(move.turnsSinceStateChange);
	if (auto* pose = registry.TryGet<VillagerPose>(villager); move.restart && pose != nullptr)
	{
		pose->place = 0;
	}
}

bool villager_dance::ExitInDance(LivingAction& action, VillagerStates next)
{
	// Into another of the scripts' states it stays in the dance, and may go
	if (StateInfo(next).isScriptState != 0)
	{
		return false;
	}
	// TODO(opening): it remembers it was dancing, to go back to the script's state after reacting
	auto& registry = Locator::entitiesRegistry::value();
	const auto villager = registry.ToEntity(action);
	ecs::dances::RemoveDancer(registry, villager);
	// TODO(opening): its walk round things starts afresh
	// Having left the dance, it may go only into the states that come and go without changing what it was doing
	const bool letsGo =
	    StateInfo(next).isScriptInterruptableState != 0 || next == k_InterfaceState || next == VillagerStates::InDance;
	return !letsGo;
}
