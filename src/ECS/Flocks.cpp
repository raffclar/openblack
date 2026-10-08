/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "Flocks.h"

#include <algorithm>
#include <vector>

#include <entt/entity/entity.hpp>

#include "Common/GUtilsDistance.h"
#include "Common/GameRandom.h"
#include "ECS/Components/Animal.h"
#include "ECS/Components/AnimalBrain.h"
#include "ECS/Components/Flock.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/Villager.h"
#include "ECS/Components/WallHug.h"
#include "ECS/Registry.h"
#include "ECS/ToBeDeleted.h"
#include "InfoConstants.h"
#include "Locator.h"

namespace openblack::ecs::flocks
{
using components::Animal;
using components::AnimalBrain;
using components::Flock;
using components::Transform;
using components::Villager;
using components::WallHug;

namespace
{
Registry& Entities()
{
	return Locator::entitiesRegistry::value();
}

Flock* FlockData(entt::entity flock)
{
	auto& registry = Entities();
	return flock != entt::null && registry.Valid(flock) ? registry.TryGet<Flock>(flock) : nullptr;
}

/// The living's flock link written: a villager's or an animal's
void SetLink(entt::entity living, entt::entity flock)
{
	auto& registry = Entities();
	if (auto* villager = registry.TryGet<Villager>(living); villager != nullptr)
	{
		villager->flock = flock;
	}
	else if (auto* animal = registry.TryGet<Animal>(living); animal != nullptr)
	{
		animal->flock = flock;
	}
}

/// The living's flock order written
void SetOrder(entt::entity living, uint8_t order)
{
	auto& registry = Entities();
	if (auto* villager = registry.TryGet<Villager>(living); villager != nullptr)
	{
		villager->flockOrder = order;
	}
	else if (auto* animal = registry.TryGet<Animal>(living); animal != nullptr)
	{
		animal->flockOrder = order;
	}
}

/// The insertion of AddMember and AddLeader: before the first node from the head (members.back()) with an order >=
/// the new one's, else at the tail (members.front())
void Insert(Flock& flock, entt::entity living)
{
	const uint8_t order = OrderOf(living);
	auto& members = flock.members;
	for (size_t i = members.size(); i-- > 0;)
	{
		if (OrderOf(members[i]) >= order)
		{
			// before it in the original's list = after it here
			members.insert(members.begin() + static_cast<std::ptrdiff_t>(i + 1), living);
			return;
		}
	}
	members.insert(members.begin(), living);
}

glm::vec3 PositionOf(entt::entity living)
{
	const auto* transform = Entities().TryGet<const Transform>(living);
	return transform != nullptr ? transform->position : glm::vec3(0.0f);
}
} // namespace

entt::entity Create(const glm::vec3& position, std::optional<uint32_t> info, std::optional<PlayerNames> player, int32_t id)
{
	auto& registry = Entities();
	const auto entity = registry.Create();
	auto& flock = registry.Assign<Flock>(entity);
	// the info and the player
	flock.info = info;
	flock.player = player;
	// the id; SetDomainCentrePos (no member yet: only the centre); the saved centre = pos; domainRadius 80,
	// flockDistance 30
	flock.id = id;
	flock.domainCentre = position;
	flock.savedDomainCentre = position;
	flock.domainRadius = 0x50;
	flock.flockDistance = 0x1E;
	return entity;
}

entt::entity Create(const glm::vec3& position, int32_t id)
{
	return Create(position, std::nullopt, std::nullopt, id);
}

entt::entity CreateFor(entt::entity living)
{
	// the living's Pos, no id; then AddMember
	const auto flock = Create(PositionOf(living), -1);
	AddMember(flock, living);
	return flock;
}

entt::entity FlockOf(entt::entity living)
{
	auto& registry = Entities();
	if (living == entt::null || !registry.Valid(living))
	{
		return entt::null;
	}
	if (const auto* villager = registry.TryGet<const Villager>(living); villager != nullptr)
	{
		return villager->flock;
	}
	if (const auto* animal = registry.TryGet<const Animal>(living); animal != nullptr)
	{
		return animal->flock;
	}
	return entt::null;
}

uint8_t OrderOf(entt::entity living)
{
	auto& registry = Entities();
	if (living == entt::null || !registry.Valid(living))
	{
		return 0;
	}
	if (const auto* villager = registry.TryGet<const Villager>(living); villager != nullptr)
	{
		return villager->flockOrder;
	}
	if (const auto* animal = registry.TryGet<const Animal>(living); animal != nullptr)
	{
		return animal->flockOrder;
	}
	return 0;
}

void SetFlock(entt::entity living, entt::entity flock)
{
	// an old flock -> RemoveLiving(living, true); then the link = flock
	if (const auto old = FlockOf(living); old != entt::null)
	{
		RemoveLiving(old, living, true);
	}
	SetLink(living, flock);
}

bool AddMember(entt::entity flock, entt::entity living)
{
	// a field of the old flock cleared when it points at this living ((not ported) that field is not kept)
	// out of its flock (even this one) with deleteWhenEmpty
	if (const auto old = FlockOf(living); old != entt::null)
	{
		RemoveLiving(old, living, true);
	}
	auto* data = FlockData(flock);
	if (data == nullptr)
	{
		// (approximate, openblack) the removal above deleted this flock: its only member came back to a flock about to
		// go, so it logs a false failure and is left without a flock (the original keeps the flock object alive)
		return false;
	}
	// already in the list -> false
	if (std::find(data->members.begin(), data->members.end(), living) != data->members.end())
	{
		return false;
	}
	Insert(*data, living);
	// SetFlock (its old flock is gone already: only the link to this one)
	SetFlock(living, flock);
	return true;
}

void AddLeader(entt::entity flock, entt::entity living)
{
	auto* data = FlockData(flock);
	if (data == nullptr)
	{
		return;
	}
	// the tail's order + 1, 5 without a tail (a byte: 255 + 1 is 0)
	const entt::entity tail = data->members.empty() ? entt::null : data->members.front();
	const auto order = static_cast<uint8_t>(tail == entt::null ? 5u : OrderOf(tail) + 1u);
	// the tail already
	if (tail == living)
	{
		return;
	}
	SetOrder(living, order);
	auto& members = data->members;
	if (const auto found = std::find(members.begin(), members.end(), living); found != members.end())
	{
		// unlinked (--count) and inserted again by its order (++count), no SetFlock
		members.erase(found);
		Insert(*data, living);
		return;
	}
	AddMember(flock, living);
}

bool RemoveLiving(entt::entity flock, entt::entity living, bool deleteWhenEmpty)
{
	auto* data = FlockData(flock);
	if (data == nullptr)
	{
		SetLink(living, entt::null);
		return false;
	}
	auto& members = data->members;
	const auto found = std::find(members.begin(), members.end(), living);
	if (found == members.end())
	{
		// not a member: the link cleared all the same, false
		SetLink(living, entt::null);
		return false;
	}
	// unlinked, --count; the link cleared
	members.erase(found);
	SetLink(living, entt::null);
	// no member left and deleteWhenEmpty -> deleted
	if (members.empty() && deleteWhenEmpty)
	{
		ToBeDeleted(flock);
	}
	return true;
}

entt::entity SeparateIntoNewFlock(entt::entity flock, entt::entity living, bool deleteWhenEmpty)
{
	// (openblack) read before the removal, which may delete this flock at once (the original reads the deleted object,
	// still allocated until ProcessDeadList)
	const auto* data = FlockData(flock);
	const uint16_t radius = data != nullptr ? data->domainRadius : static_cast<uint16_t>(0x50);
	const uint16_t distance = data != nullptr ? data->flockDistance : static_cast<uint16_t>(0x1E);
	RemoveLiving(flock, living, deleteWhenEmpty);
	const auto own = CreateFor(living);
	if (auto* fresh = FlockData(own); fresh != nullptr)
	{
		fresh->domainRadius = radius;
		fresh->flockDistance = distance;
	}
	return own;
}

void Merge(entt::entity keeper, entt::entity other)
{
	auto& registry = Entities();
	// The other's cursor from its head; while it has a member. As in the original, merging a flock into itself (an
	// attach of a flock to itself) never empties `other` and hangs the game: kept literal
	for (auto* from = FlockData(other); from != nullptr && !from->members.empty(); from = FlockData(other))
	{
		auto* to = FlockData(keeper);
		if (to == nullptr)
		{
			return;
		}
		const auto head = from->members.back();
		// keeper maxMembers += other maxMembers, at every member
		to->maxMembers += from->maxMembers;
		// the keeper's cursor as an Animal: maxMembers clamped to its info's maxFlockSize.
		// (approximate) the cursor is not kept: AddMember moves it to the node it has just inserted, which is
		// members.back() only when that member's order is 0; and at the first step it can be null (no clamp)
		if (!to->members.empty())
		{
			if (const auto* animal = registry.TryGet<const Animal>(to->members.back()); animal != nullptr)
			{
				const auto& infos = Locator::infoConstants::value().animal;
				const auto type = static_cast<size_t>(animal->type);
				if (type < infos.size() && to->maxMembers > infos[type].maxFlockSize)
				{
					to->maxMembers = infos[type].maxFlockSize;
				}
			}
		}
		// AddMember(keeper, head), which takes it out of the other (deleted when emptied)
		if (!AddMember(keeper, head))
		{
			// (openblack, guard) a member that cannot move would loop for ever; the original never meets it
			RemoveLiving(other, head, true);
		}
	}
}

entt::entity Leader(entt::entity flock)
{
	const auto* data = FlockData(flock);
	if (data == nullptr)
	{
		return entt::null;
	}
	for (const auto member : data->members)
	{
		if (IsAvailable(member))
		{
			return member;
		}
	}
	return entt::null;
}

bool IsLeader(entt::entity living)
{
	// no flock -> false; a tail -> tail == this; no tail -> this == null
	const auto flock = FlockOf(living);
	return flock != entt::null && Leader(flock) == living;
}

uint32_t Size(entt::entity flock)
{
	const auto* data = FlockData(flock);
	return data != nullptr ? static_cast<uint32_t>(data->members.size()) : 0;
}

std::vector<entt::entity> MembersFromHead(entt::entity flock)
{
	const auto* data = FlockData(flock);
	if (data == nullptr)
	{
		return {};
	}
	return {data->members.rbegin(), data->members.rend()};
}

entt::entity RandomMember(entt::entity flock, entt::entity exclude)
{
	const auto members = MembersFromHead(flock);
	const auto count = static_cast<uint32_t>(members.size());
	if (count == 0)
	{
		return entt::null;
	}
	// GameRand(count - 1) with an exclude and more than one member, else GameRand(count)
	const uint32_t r = game_random::GameRand(exclude != entt::null && count > 1 ? count - 1 : count);
	// from the head, index >= r and not the excluded one
	for (uint32_t i = 0; i < count; ++i)
	{
		if (i >= r && members[i] != exclude)
		{
			return members[i];
		}
	}
	return entt::null;
}

entt::entity FindLiving(entt::entity flock, const std::function<bool(entt::entity)>& match)
{
	// the cursor from the head, the first the callback accepts
	for (const auto member : MembersFromHead(flock))
	{
		if (match(member))
		{
			return member;
		}
	}
	return entt::null;
}

glm::vec3 GetFlockPos(entt::entity flock)
{
	const auto* data = FlockData(flock);
	if (data == nullptr)
	{
		return glm::vec3(0.0f);
	}
	// the tail's Pos; without one, the flock's domain centre
	if (const auto tail = Leader(flock); tail != entt::null)
	{
		return PositionOf(tail);
	}
	return data->domainCentre;
}

void SetDomainCentrePos(entt::entity flock, const glm::vec3& position)
{
	auto* data = FlockData(flock);
	if (data == nullptr)
	{
		return;
	}
	auto& registry = Entities();
	// the tail's destination
	if (const auto tail = Leader(flock); tail != entt::null)
	{
		if (auto* wallHug = registry.TryGet<WallHug>(tail); wallHug != nullptr && registry.AllOf<Villager>(tail))
		{
			wallHug->goal = glm::vec2(position.x, position.z);
		}
		else if (auto* brain = registry.TryGet<AnimalBrain>(tail); brain != nullptr)
		{
			brain->goal = glm::vec2(position.x, position.z);
		}
	}
	data->domainCentre = position;
}

bool PosWithinDomain(entt::entity living, glm::vec2 position, float factor)
{
	const auto* data = FlockData(FlockOf(living));
	if (data == nullptr)
	{
		return false;
	}
	// GetDistanceInMetres from the domain centre; radius x factor < distance -> false
	const float distance = gutils::GetDistanceInMetres(glm::vec2(data->domainCentre.x, data->domainCentre.z), position);
	const float limit = static_cast<float>(data->domainRadius) * factor;
	return !(limit < distance);
}

} // namespace openblack::ecs::flocks
