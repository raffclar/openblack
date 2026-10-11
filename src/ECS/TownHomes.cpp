/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "TownHomes.h"

#include <cmath>

#include <algorithm>
#include <numeric>
#include <vector>

#include "Common/GUtilsDistance.h"
#include "Common/RuntimeQsort.h"
#include "ECS/Components/Abode.h"
#include "ECS/Components/Town.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/Villager.h"
#include "ECS/Registry.h"

using namespace openblack;
using namespace openblack::ecs;
using namespace openblack::ecs::components;

namespace
{
/// Past this distance a building is liked no less for being further off
constexpr float k_FarthestLiked = 500.0f;
/// Kept off nothing in the shares of men and of people evening out a town's homes
constexpr float k_ShuffleTiny = 0.001f;
/// How much a building's want of people counts against its want of men
constexpr float k_ShufflePeopleWeight = 0.5f;
/// Each town evens out its homes this many turns after the one numbered before it
constexpr double k_ShuffleTownStagger = 20.0;

void AddFirst(std::vector<entt::entity>& list, entt::entity entity)
{
	if (std::ranges::find(list, entity) == list.end())
	{
		list.insert(list.begin(), entity);
	}
}

void LeaveAbode(Registry& registry, const town_homes::Homes& homes, entt::entity abode, entt::entity villager)
{
	if (homes.leaving)
	{
		homes.leaving(villager);
	}
	if (auto* building = registry.Valid(abode) ? registry.TryGet<Abode>(abode) : nullptr; building != nullptr)
	{
		std::erase(building->inhabitants, villager);
	}
}

Town* TownData(Registry& registry, entt::entity town)
{
	return town != entt::null && registry.Valid(town) ? registry.TryGet<Town>(town) : nullptr;
}

const Town* TownData(const Registry& registry, entt::entity town)
{
	return town != entt::null && registry.Valid(town) ? registry.TryGet<const Town>(town) : nullptr;
}

bool IsChildVillager(const Registry& registry, entt::entity villager)
{
	return registry.Get<const Villager>(villager).lifeStage == Villager::LifeStage::Child;
}

bool AtHome(const town_homes::Homes& homes, entt::entity villager)
{
	return homes.atHome && homes.atHome(villager);
}

/// How full a building is with its age's people, past full at 1; a building with no places for the age is full
float ShareFull(const Registry& registry, const town_homes::AbodeRoom& room, entt::entity abode, bool child)
{
	const auto household = town_homes::HouseholdOf(registry, abode);
	const uint32_t most = child ? room.maxChildren : room.maxAdults;
	if (most == 0)
	{
		return 1.0f;
	}
	return static_cast<float>(child ? household.children : household.adults) / static_cast<float>(most);
}

/// The first of a building's people of a sex out of doors, any age
entt::entity FirstOutOfDoors(const Registry& registry, const town_homes::Homes& homes, entt::entity abode, Villager::Sex sex)
{
	for (const auto person : registry.Get<const Abode>(abode).inhabitants)
	{
		const auto* villager = registry.Valid(person) ? registry.TryGet<const Villager>(person) : nullptr;
		if (villager != nullptr && villager->sex == sex && !AtHome(homes, person))
		{
			return person;
		}
	}
	return entt::null;
}

town_homes::Occupancy OccupancyOf(const Registry& registry, entt::entity abode, const town_homes::AbodeRoom& room,
                                  const Villager& newcomer, entt::entity villager)
{
	town_homes::Occupancy occupancy {.child = newcomer.lifeStage == Villager::LifeStage::Child,
	                                 .maxAdults = room.maxAdults,
	                                 .maxChildren = room.maxChildren};
	for (const auto resident : registry.Get<const Abode>(abode).inhabitants)
	{
		const auto* person = registry.Valid(resident) ? registry.TryGet<const Villager>(resident) : nullptr;
		if (person == nullptr)
		{
			continue;
		}
		++occupancy.people;
		if (person->lifeStage == Villager::LifeStage::Child)
		{
			++occupancy.children;
		}
		else
		{
			++occupancy.adults;
		}
		if (person->sex == newcomer.sex)
		{
			++occupancy.sameSex;
		}
	}
	const auto* from = registry.TryGet<const Transform>(abode);
	const auto* to = registry.TryGet<const Transform>(villager);
	if (from != nullptr && to != nullptr)
	{
		occupancy.distance = gutils::GetDistanceInMetres(from->position, to->position);
	}
	return occupancy;
}
} // namespace

using town_homes::Homes;
using town_homes::Occupancy;

float town_homes::ScoreForAddingVillager(const Occupancy& occupancy)
{
	const uint32_t most = occupancy.child ? occupancy.maxChildren : occupancy.maxAdults;
	if (most == 0)
	{
		return 0.0f;
	}
	const uint32_t living = occupancy.child ? occupancy.children : occupancy.adults;
	const float full = std::min(static_cast<float>(living) / static_cast<float>(most), 1.0f);
	float score = 1.0f - full;
	if (score > 0.0f)
	{
		// Liked better the fewer of its people share the newcomer's sex, and the nearer it is
		float sexes = 1.0f;
		if (occupancy.people != 0)
		{
			const float same = static_cast<float>(occupancy.sameSex) / static_cast<float>(occupancy.people);
			sexes = ((1.0f - same) + 1.0f) * 0.5f;
		}
		const float nearness = gutils::GetDistanceModifier(occupancy.distance, k_FarthestLiked);
		score = score * sexes * ((nearness + 1.0f) * 0.5f);
	}
	return score;
}

entt::entity town_homes::TownOfAbode(const Registry& registry, entt::entity abode)
{
	const auto* building = registry.Valid(abode) ? registry.TryGet<const Abode>(abode) : nullptr;
	if (building == nullptr)
	{
		return entt::null;
	}
	const auto& towns = registry.Context().towns;
	const auto found = towns.find(building->townId);
	if (found == towns.end() || !registry.Valid(found->second) || !registry.AllOf<Town>(found->second))
	{
		return entt::null;
	}
	return found->second;
}

entt::entity town_homes::FindAbodeWithSpace(const Registry& registry, const Homes& homes, entt::entity town,
                                            entt::entity villager, float leastScore)
{
	const auto* data = town != entt::null ? registry.TryGet<const Town>(town) : nullptr;
	const auto* newcomer = registry.TryGet<const Villager>(villager);
	if (data == nullptr || newcomer == nullptr)
	{
		return entt::null;
	}
	entt::entity best = entt::null;
	for (const auto abode : data->abodes)
	{
		if (!registry.Valid(abode) || !registry.AllOf<Abode>(abode))
		{
			continue;
		}
		const auto room = homes.room(abode);
		if (!room.has_value() || !room->functional)
		{
			continue;
		}
		const float score = ScoreForAddingVillager(OccupancyOf(registry, abode, *room, *newcomer, villager));
		if (score > leastScore)
		{
			leastScore = score;
			best = abode;
		}
	}
	return best;
}

bool town_homes::AddVillagerToTown(Registry& registry, const Homes& homes, entt::entity town, entt::entity villager)
{
	const auto* data = town != entt::null ? registry.TryGet<const Town>(town) : nullptr;
	if (data == nullptr || data->uninhabitable)
	{
		return false;
	}
	// TODO(villagers): the town's head counts take the villager in, and a town's first person sets up its worship site
	auto& person = registry.Get<Villager>(villager);
	person.town = town;
	const auto abode = person.abode;
	if (abode == entt::null || TownOfAbode(registry, abode) != town)
	{
		if (abode != entt::null)
		{
			LeaveAbode(registry, homes, abode, villager);
			person.abode = entt::null;
		}
		if (const auto home = FindAbodeWithSpace(registry, homes, town, villager, 0.0f); home != entt::null)
		{
			AddVillagerToAbode(registry, homes, home, villager);
			return true;
		}
		MakeHomeless(registry, homes, villager);
	}
	return true;
}

void town_homes::AddVillagerToAbode(Registry& registry, const Homes& homes, entt::entity abode, entt::entity villager)
{
	auto& person = registry.Get<Villager>(villager);
	const auto town = person.town;
	auto* homeless = town != entt::null ? registry.TryGet<Town>(town) : nullptr;
	if (homeless != nullptr && std::ranges::find(homeless->homelessVillagers, villager) != homeless->homelessVillagers.end())
	{
		std::erase(homeless->homelessVillagers, villager);
	}
	else if (person.abode != entt::null)
	{
		LeaveAbode(registry, homes, person.abode, villager);
	}
	auto& building = registry.Get<Abode>(abode);
	AddFirst(building.inhabitants, villager);
	person.abode = abode;
	if (const auto abodeTown = TownOfAbode(registry, abode); abodeTown != entt::null && abodeTown != town)
	{
		AddVillagerToTown(registry, homes, abodeTown, villager);
	}
	// The first adult of each sex to move in makes the building's couple
	if (person.lifeStage != Villager::LifeStage::Child)
	{
		auto& place = building.couple.at(static_cast<size_t>(person.sex));
		if (place == entt::null)
		{
			place = villager;
		}
	}
}

bool town_homes::MakeHomeless(Registry& registry, const Homes& homes, entt::entity villager)
{
	auto& person = registry.Get<Villager>(villager);
	if (person.abode != entt::null)
	{
		LeaveAbode(registry, homes, person.abode, villager);
		person.abode = entt::null;
	}
	auto* town = person.town != entt::null ? registry.TryGet<Town>(person.town) : nullptr;
	if (town == nullptr || std::ranges::find(town->homelessVillagers, villager) != town->homelessVillagers.end())
	{
		return false;
	}
	town->homelessVillagers.insert(town->homelessVillagers.begin(), villager);
	return true;
}

town_homes::Household town_homes::HouseholdOf(const Registry& registry, entt::entity abode)
{
	Household household;
	for (const auto person : registry.Get<const Abode>(abode).inhabitants)
	{
		const auto* villager = registry.Valid(person) ? registry.TryGet<const Villager>(person) : nullptr;
		if (villager == nullptr)
		{
			continue;
		}
		if (villager->lifeStage == Villager::LifeStage::Child)
		{
			++household.children;
		}
		else
		{
			++household.adults;
			if (villager->sex == Villager::Sex::MALE)
			{
				++household.adultMen;
			}
		}
	}
	return household;
}

entt::entity town_homes::SpouseOf(const Registry& registry, entt::entity abode, entt::entity villager)
{
	const auto* building = registry.Valid(abode) ? registry.TryGet<const Abode>(abode) : nullptr;
	const auto* person = registry.Valid(villager) ? registry.TryGet<const Villager>(villager) : nullptr;
	if (building == nullptr || person == nullptr)
	{
		return entt::null;
	}
	const auto own = static_cast<size_t>(person->sex);
	if (building->couple.at(own) != villager)
	{
		return entt::null;
	}
	const auto other = building->couple.at(1 - own);
	return other != entt::null && registry.Valid(other) ? other : entt::null;
}

bool town_homes::IsTooCrowded(uint32_t adults, uint32_t maxAdults, float tooCrowded)
{
	if (maxAdults == 0)
	{
		return true;
	}
	return static_cast<float>(adults) / static_cast<float>(maxAdults) >= tooCrowded;
}

void town_homes::RemoveVillagerFromTown(Registry& registry, const Homes& homes, entt::entity villager)
{
	// TODO(villager-life-cycle): its children lose their mother, and it leaves its town's worshippers
	auto& person = registry.Get<Villager>(villager);
	if (person.abode != entt::null)
	{
		LeaveAbode(registry, homes, person.abode, villager);
		person.abode = entt::null;
	}
	else if (auto* town = TownData(registry, person.town); town != nullptr)
	{
		std::erase(town->homelessVillagers, villager);
	}
	person.town = entt::null;
}

void town_homes::RemoveDeadVillager(Registry& registry, entt::entity villager)
{
	auto& person = registry.Get<Villager>(villager);
	if (auto* building =
	        person.abode != entt::null && registry.Valid(person.abode) ? registry.TryGet<Abode>(person.abode) : nullptr;
	    building != nullptr)
	{
		// One of the couple dying ends the couple
		if (building->couple.at(static_cast<size_t>(person.sex)) == villager)
		{
			building->couple = {entt::null, entt::null};
		}
		std::erase(building->inhabitants, villager);
	}
	person.abode = entt::null;
	if (auto* town = TownData(registry, person.town); town != nullptr)
	{
		std::erase(town->homelessVillagers, villager);
	}
	person.town = entt::null;
}

void town_homes::MoveInAnyway(Registry& registry, const Homes& homes, entt::entity abode, entt::entity villager)
{
	const auto oldTown = registry.Get<const Villager>(villager).town;
	const auto newTown = TownOfAbode(registry, abode);
	if (oldTown != newTown)
	{
		if (oldTown != entt::null)
		{
			RemoveVillagerFromTown(registry, homes, villager);
		}
		const auto room = homes.room(abode);
		if (newTown != entt::null && room.has_value() &&
		    ShareFull(registry, *room, abode, IsChildVillager(registry, villager)) >= 1.0f)
		{
			AddVillagerToTown(registry, homes, newTown, villager);
			return;
		}
	}
	AddVillagerToAbode(registry, homes, abode, villager);
}

bool town_homes::MoveInIfRoom(Registry& registry, const Homes& homes, entt::entity abode, entt::entity villager)
{
	const auto room = homes.room(abode);
	if (!room.has_value())
	{
		return false;
	}
	const auto household = HouseholdOf(registry, abode);
	const bool child = IsChildVillager(registry, villager);
	const auto left = child ? static_cast<int64_t>(room->maxChildren) - household.children
	                        : static_cast<int64_t>(room->maxAdults) - household.adults;
	if (left <= 0)
	{
		return false;
	}
	MoveInAnyway(registry, homes, abode, villager);
	return true;
}

bool town_homes::HasRoomForNewcomer(const Registry& registry, const Homes& homes, entt::entity town, uint32_t emptyTownAllows)
{
	const auto* data = town != entt::null && registry.Valid(town) ? registry.TryGet<const Town>(town) : nullptr;
	if (data == nullptr)
	{
		return false;
	}
	int64_t places = 0;
	int64_t room = 0;
	for (const auto abode : data->abodes)
	{
		if (!registry.Valid(abode) || !registry.AllOf<Abode>(abode))
		{
			continue;
		}
		const auto rooms = homes.room(abode);
		if (!rooms.has_value() || !rooms->built)
		{
			continue;
		}
		places += rooms->maxAdults;
		room += static_cast<int64_t>(rooms->maxAdults) - HouseholdOf(registry, abode).adults;
	}
	if (places == 0)
	{
		int64_t people = 0;
		registry.Each<const Villager>([&people, town](const Villager& villager) {
			if (villager.town == town)
			{
				++people;
			}
		});
		room = static_cast<int64_t>(emptyTownAllows) - people;
	}
	return room > 0;
}

bool town_homes::MoveIntoTown(Registry& registry, const Homes& homes, entt::entity town, entt::entity villager,
                              uint32_t emptyTownAllows)
{
	auto abode = FindAbodeWithSpace(registry, homes, town, villager, 0.0f);
	if (abode == entt::null)
	{
		if (!HasRoomForNewcomer(registry, homes, town, emptyTownAllows))
		{
			return false;
		}
		const auto& abodes = registry.Get<const Town>(town).abodes;
		const auto centre = std::ranges::find_if(abodes, [&registry](entt::entity building) {
			const auto* data = registry.Valid(building) ? registry.TryGet<const Abode>(building) : nullptr;
			return data != nullptr && data->type == AbodeNumber::TownCentre;
		});
		if (centre != abodes.end())
		{
			abode = *centre;
		}
		else if (!abodes.empty())
		{
			abode = abodes.front();
		}
		else
		{
			return false;
		}
	}
	MoveInAnyway(registry, homes, abode, villager);
	return true;
}

bool town_homes::MoveInByBuilding(Registry& registry, const Homes& homes, entt::entity abode, entt::entity villager,
                                  uint32_t emptyTownAllows)
{
	if (abode == entt::null || !registry.Valid(abode) || !registry.AllOf<Abode>(abode))
	{
		return false;
	}
	const auto room = homes.room(abode);
	if (room.has_value() && ShareFull(registry, *room, abode, IsChildVillager(registry, villager)) < 1.0f)
	{
		MoveInAnyway(registry, homes, abode, villager);
		return true;
	}
	const auto town = TownOfAbode(registry, abode);
	return town != entt::null && MoveIntoTown(registry, homes, town, villager, emptyTownAllows);
}

town_homes::NewHome town_homes::LookForBetterHome(Registry& registry, const Homes& homes, entt::entity villager)
{
	if (IsChildVillager(registry, villager))
	{
		return NewHome::Stays;
	}
	const auto& person = registry.Get<const Villager>(villager);
	const auto abode = person.abode != entt::null && registry.Valid(person.abode) && registry.AllOf<Abode>(person.abode)
	                       ? person.abode
	                       : entt::null;
	std::optional<AbodeRoom> room;
	if (abode != entt::null)
	{
		room = homes.room(abode);
		const uint32_t maxAdults = room.has_value() ? room->maxAdults : 0;
		const float tooCrowded = room.has_value() ? room->tooCrowded : 1.0f;
		if (!IsTooCrowded(HouseholdOf(registry, abode).adults, maxAdults, tooCrowded))
		{
			return NewHome::Stays;
		}
	}
	const auto town = person.town;
	const auto* data = TownData(registry, town);
	if (data == nullptr)
	{
		return NewHome::NoTown;
	}
	// It must find one it likes better than its own, counting itself among its own home's people
	float least = 0.0f;
	if (abode != entt::null && room.has_value())
	{
		least = ScoreForAddingVillager(OccupancyOf(registry, abode, *room, person, villager));
	}
	if (const auto better = FindAbodeWithSpace(registry, homes, town, villager, least);
	    better != entt::null && MoveInIfRoom(registry, homes, better, villager))
	{
		return NewHome::Moved;
	}
	if (std::ranges::find(data->homelessVillagers, villager) != data->homelessVillagers.end())
	{
		return NewHome::AlreadyHomeless;
	}
	MakeHomeless(registry, homes, villager);
	return NewHome::MadeHomeless;
}

town_homes::ShuffleWants town_homes::WantsOf(const TownPeople& town, const Household& household, uint32_t maxAdults)
{
	ShuffleWants wants;
	if (maxAdults == 0)
	{
		return wants;
	}
	const float full = static_cast<float>(household.adults) / static_cast<float>(maxAdults);
	const float townMen = (static_cast<float>(town.men) + k_ShuffleTiny) / (static_cast<float>(town.women) + k_ShuffleTiny);
	const float ownMen = (static_cast<float>(household.adultMen) + k_ShuffleTiny) /
	                     (static_cast<float>(household.adults - household.adultMen) + k_ShuffleTiny);
	const float townFull =
	    (static_cast<float>(town.adults) + k_ShuffleTiny) / (static_cast<float>(town.adultPlaces) + k_ShuffleTiny);
	wants.men = townMen - ownMen;
	wants.people = (townFull - full) * k_ShufflePeopleWeight;
	wants.roomForAdult = full < 1.0f;
	return wants;
}

std::vector<town_homes::ShuffleMove> town_homes::EvenOutMoves(std::span<const ShuffleWants> abodes)
{
	struct Entry
	{
		ShuffleWants wants;
		size_t index;
	};
	const auto size = [](const ShuffleWants& wants) { return std::fabs(wants.men * wants.men + wants.people * wants.people); };
	std::vector<Entry> sorted;
	sorted.reserve(abodes.size());
	for (size_t i = 0; i < abodes.size(); ++i)
	{
		sorted.push_back({.wants = abodes[i], .index = i});
	}
	// Most out of balance first; the sort never calls two equal
	RuntimeQsort(std::span<Entry>(sorted),
	             [&size](const Entry& a, const Entry& b) { return size(a.wants) > size(b.wants) ? -1 : 1; });

	std::vector<ShuffleMove> moves;
	for (size_t i = 0; i + 1 < sorted.size(); ++i)
	{
		const auto& current = sorted[i].wants;
		float bestSize = size(current);
		std::optional<size_t> best;
		for (size_t j = i + 1; j < sorted.size(); ++j)
		{
			const auto& other = sorted[j].wants;
			const float men = other.men + current.men;
			const float people = current.people + other.people;
			const float together = std::fabs(men * men + people * people);
			if (together < bestSize)
			{
				bestSize = together;
				best = j;
			}
		}
		if (!best.has_value())
		{
			continue;
		}
		const auto& other = sorted[*best].wants;
		const size_t currentIndex = sorted[i].index;
		const size_t otherIndex = sorted[*best].index;
		const bool currentWantsPeople = current.people > other.people;
		const bool currentWantsMen = current.men > other.men;
		// Only one of them wants people: one moves rather than two swapping, when the one wanting them has room
		const bool oneWantsPeople = other.people * current.people < 0.0f;
		const bool swap = !(oneWantsPeople && (currentWantsPeople ? current.roomForAdult : other.roomForAdult));
		if (swap)
		{
			moves.push_back({.kind = ShuffleMove::Kind::Swap,
			                 .to = currentWantsMen ? currentIndex : otherIndex,
			                 .from = currentWantsMen ? otherIndex : currentIndex});
		}
		else if (currentWantsPeople)
		{
			moves.push_back({.kind = ShuffleMove::Kind::Take, .to = currentIndex, .from = otherIndex, .man = currentWantsMen});
		}
		else
		{
			moves.push_back({.kind = ShuffleMove::Kind::Take, .to = otherIndex, .from = currentIndex, .man = !currentWantsMen});
		}
	}
	return moves;
}

bool town_homes::EvenOutDue(uint32_t townId, uint32_t turn, uint32_t every)
{
	if (every == 0)
	{
		return false;
	}
	const auto when = static_cast<uint32_t>(static_cast<double>(townId) * k_ShuffleTownStagger + static_cast<double>(turn));
	return when % every == 0;
}

bool town_homes::EvenOutHomes(Registry& registry, const Homes& homes, entt::entity town)
{
	const auto* data = TownData(registry, town);
	if (data == nullptr)
	{
		return false;
	}
	// The town's people, any age, by sex, and its adults
	TownPeople people;
	registry.Each<const Villager>([&people, town](const Villager& villager) {
		if (villager.town != town)
		{
			return;
		}
		++(villager.sex == Villager::Sex::MALE ? people.men : people.women);
		if (villager.lifeStage != Villager::LifeStage::Child)
		{
			++people.adults;
		}
	});
	// Its places for adults, in the homes built, and the working homes that can house anyone, in the town's order
	std::vector<entt::entity> buildings;
	std::vector<uint32_t> places;
	for (const auto abode : data->abodes)
	{
		if (!registry.Valid(abode) || !registry.AllOf<Abode>(abode))
		{
			continue;
		}
		const auto room = homes.room(abode);
		if (!room.has_value() || room->maxAdults + room->maxChildren == 0)
		{
			continue;
		}
		if (room->built)
		{
			people.adultPlaces += room->maxAdults;
		}
		if (room->functional)
		{
			buildings.push_back(abode);
			places.push_back(room->maxAdults);
		}
	}
	std::vector<ShuffleWants> wants;
	wants.reserve(buildings.size());
	for (size_t i = 0; i < buildings.size(); ++i)
	{
		wants.push_back(WantsOf(people, HouseholdOf(registry, buildings[i]), places[i]));
	}
	for (const auto& move : EvenOutMoves(wants))
	{
		const auto to = buildings.at(move.to);
		const auto from = buildings.at(move.from);
		if (move.kind == ShuffleMove::Kind::Swap)
		{
			const auto man = FirstOutOfDoors(registry, homes, from, Villager::Sex::MALE);
			const auto woman = man != entt::null ? FirstOutOfDoors(registry, homes, to, Villager::Sex::FEMALE) : entt::null;
			if (woman != entt::null)
			{
				MoveInAnyway(registry, homes, to, man);
				MoveInAnyway(registry, homes, from, woman);
				return true;
			}
			continue;
		}
		const auto mover = FirstOutOfDoors(registry, homes, from, move.man ? Villager::Sex::MALE : Villager::Sex::FEMALE);
		if (mover != entt::null)
		{
			MoveInAnyway(registry, homes, to, mover);
			return true;
		}
	}
	return false;
}
