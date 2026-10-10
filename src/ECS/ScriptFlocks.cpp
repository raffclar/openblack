/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "ScriptFlocks.h"

#include <algorithm>
#include <optional>
#include <vector>

#include <glm/gtx/vec_swizzle.hpp>

#include "ECS/Components/ScriptFlock.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/WallHug.h"
#include "ECS/Registry.h"
#include "ECS/ScriptFlockRules.h"

using namespace openblack;
using namespace openblack::ecs;
using namespace openblack::ecs::components;
using map_coords::MapCoords;

namespace
{
/// The members' places in line, the first to the last
std::vector<uint8_t> Orders(const Registry& registry, const ScriptFlock& flock)
{
	std::vector<uint8_t> orders;
	orders.reserve(flock.members.size());
	std::ranges::transform(flock.members, std::back_inserter(orders), [&registry](entt::entity member) {
		const auto* membership = registry.TryGet<const ScriptFlockMember>(member);
		return membership != nullptr ? membership->order : uint8_t {0};
	});
	return orders;
}

/// Puts a living in a flock's line by its place, without anything else
void Insert(Registry& registry, ScriptFlock& flock, entt::entity living)
{
	const auto order = registry.Get<const ScriptFlockMember>(living).order;
	const auto at = script_flock_rules::InsertAt(Orders(registry, flock), order);
	flock.members.insert(flock.members.begin() + static_cast<std::ptrdiff_t>(at), living);
}

ScriptFlockMember& Membership(Registry& registry, entt::entity living)
{
	if (auto* membership = registry.TryGet<ScriptFlockMember>(living))
	{
		return *membership;
	}
	return registry.Assign<ScriptFlockMember>(living);
}
} // namespace

entt::entity script_flocks::Create(Registry& registry, const MapCoords& place)
{
	const auto flock = registry.Create();
	registry.Assign<ScriptFlock>(flock, ScriptFlock {.place = place});
	return flock;
}

void script_flocks::Destroy(Registry& registry, entt::entity flock)
{
	if (!IsFlock(registry, flock))
	{
		return;
	}
	for (const auto member : registry.Get<const ScriptFlock>(flock).members)
	{
		if (auto* membership = registry.Valid(member) ? registry.TryGet<ScriptFlockMember>(member) : nullptr)
		{
			membership->flock = entt::null;
		}
	}
	registry.Destroy(flock);
}

bool script_flocks::IsFlock(const Registry& registry, entt::entity thing)
{
	return thing != entt::null && registry.Valid(thing) && registry.AllOf<ScriptFlock>(thing);
}

bool script_flocks::IsMember(const Registry& registry, entt::entity flock, entt::entity living)
{
	if (!IsFlock(registry, flock))
	{
		return false;
	}
	const auto& members = registry.Get<const ScriptFlock>(flock).members;
	return std::ranges::find(members, living) != members.end();
}

entt::entity script_flocks::FlockOf(const Registry& registry, entt::entity living)
{
	const auto* membership = registry.Valid(living) ? registry.TryGet<const ScriptFlockMember>(living) : nullptr;
	return membership != nullptr && IsFlock(registry, membership->flock) ? membership->flock : entt::null;
}

std::size_t script_flocks::Size(const Registry& registry, entt::entity flock)
{
	return IsFlock(registry, flock) ? registry.Get<const ScriptFlock>(flock).members.size() : 0;
}

entt::entity script_flocks::Leader(const Registry& registry, entt::entity flock)
{
	if (!IsFlock(registry, flock))
	{
		return entt::null;
	}
	const auto& members = registry.Get<const ScriptFlock>(flock).members;
	return members.empty() ? entt::null : members.back();
}

bool script_flocks::AddLiving(Registry& registry, entt::entity flock, entt::entity living)
{
	// It leaves whatever flock it was in, this one too, and a flock left empty goes
	Remove(registry, living, true);
	if (!IsFlock(registry, flock) || IsMember(registry, flock, living))
	{
		return false;
	}
	Membership(registry, living).flock = flock;
	Insert(registry, registry.Get<ScriptFlock>(flock), living);
	return true;
}

void script_flocks::AddLeader(Registry& registry, entt::entity flock, entt::entity living)
{
	if (!IsFlock(registry, flock))
	{
		return;
	}
	const auto leader = Leader(registry, flock);
	if (leader == living)
	{
		return;
	}
	std::optional<uint8_t> lastOrder;
	if (leader != entt::null)
	{
		const auto* leaderMembership = registry.TryGet<const ScriptFlockMember>(leader);
		lastOrder = leaderMembership != nullptr ? leaderMembership->order : uint8_t {0};
	}
	Membership(registry, living).order = script_flock_rules::LeaderOrder(lastOrder);
	if (IsMember(registry, flock, living))
	{
		// A member already moves to its new place in line
		auto& data = registry.Get<ScriptFlock>(flock);
		std::erase(data.members, living);
		Insert(registry, data, living);
		return;
	}
	AddLiving(registry, flock, living);
}

void script_flocks::Remove(Registry& registry, entt::entity living, bool deleteWhenEmpty)
{
	auto* membership = registry.Valid(living) ? registry.TryGet<ScriptFlockMember>(living) : nullptr;
	if (membership == nullptr)
	{
		return;
	}
	const auto flock = membership->flock;
	membership->flock = entt::null;
	if (!IsFlock(registry, flock))
	{
		return;
	}
	auto& data = registry.Get<ScriptFlock>(flock);
	if (std::erase(data.members, living) == 0)
	{
		return;
	}
	if (data.members.empty() && deleteWhenEmpty)
	{
		Destroy(registry, flock);
	}
}

MapCoords script_flocks::PositionOf(const Registry& registry, entt::entity living)
{
	const auto* transform = registry.TryGet<const Transform>(living);
	return transform != nullptr ? map_coords::FromMetres(glm::xz(transform->position)) : MapCoords {};
}

MapCoords script_flocks::Position(const Registry& registry, entt::entity flock)
{
	const auto leader = Leader(registry, flock);
	if (leader != entt::null && registry.Valid(leader))
	{
		return PositionOf(registry, leader);
	}
	return IsFlock(registry, flock) ? registry.Get<const ScriptFlock>(flock).place : MapCoords {};
}

void script_flocks::MoveTo(Registry& registry, entt::entity flock, const MapCoords& place)
{
	if (!IsFlock(registry, flock))
	{
		return;
	}
	// The leader's goal changes, but it isn't sent there
	const auto leader = Leader(registry, flock);
	if (auto* wallHug = leader != entt::null && registry.Valid(leader) ? registry.TryGet<WallHug>(leader) : nullptr)
	{
		wallHug->goal = map_coords::ToMetres(place);
	}
	registry.Get<ScriptFlock>(flock).place = place;
}

entt::entity script_flocks::RandomMember(const Registry& registry, entt::entity flock, entt::entity exclude,
                                         const std::function<uint32_t(uint32_t)>& random)
{
	const auto size = Size(registry, flock);
	if (size == 0)
	{
		return entt::null;
	}
	const auto count = static_cast<uint32_t>(exclude != entt::null && size > 1 ? size - 1 : size);
	const auto drawn = random(count);
	const auto& members = registry.Get<const ScriptFlock>(flock).members;
	for (uint32_t index = 0; index < members.size(); ++index)
	{
		if (index >= drawn && members[index] != exclude)
		{
			return members[index];
		}
	}
	return entt::null;
}
