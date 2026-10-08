/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "AbodeVillagers.h"

#include <algorithm>
#include <optional>

#include "Common/GUtilsDistance.h"
#include "ECS/Abodes.h"
#include "ECS/Components/Abode.h"
#include "ECS/Components/Town.h"
#include "ECS/Components/Villager.h"
#include "ECS/Registry.h"
#include "ECS/Town/AbodeQueries.h"
#include "ECS/Town/TownDesire.h"
#include "ECS/Town/TownQueries.h"
#include "ECS/Town/TownStats.h"
#include "ECS/Town/TownVillagers.h"
#include "ECS/Villager/VillagerCore.h"
#include "ECS/Villager/VillagerHome.h"
#include "InfoConstants.h"
#include "Locator.h"

// The abode's villagers (AbodeVillagers.h)

namespace openblack::ecs::abode_villagers
{
using namespace components;

namespace
{
const std::vector<entt::entity> k_NoVillagers;

/// The ratios' small offset and the score's distance scale
constexpr float k_Thousandth = 0.001f;
constexpr float k_ScoreDistance = 500.0f;
/// The empty timer grows by 0.001 each processed turn; the decay happens at 1
constexpr float k_EmptyTimerStep = 0.001f;
/// field0xB9 counts up to 200
constexpr uint8_t k_CounterB9Max = 200;

Registry& Entities()
{
	return Locator::entitiesRegistry::value();
}

Abode* AbodeComponent(entt::entity abode)
{
	auto& registry = Entities();
	return abode != entt::null && registry.Valid(abode) ? registry.TryGet<Abode>(abode) : nullptr;
}

/// The villager info's sex (no child test)
bool IsMaleVillager(entt::entity villager)
{
	return villager::InfoOf(villager).sex == SexType::Male;
}

bool IsFemaleVillager(entt::entity villager)
{
	return villager::InfoOf(villager).sex == SexType::Female;
}

bool IsInside(entt::entity villager)
{
	const auto* v = Entities().TryGet<const Villager>(villager);
	return v != nullptr && (v->flags & Villager::k_FlagAtHome) != 0;
}

/// The list's removal: the villager is unlinked if it is there
void Unlink(Abode& abode, entt::entity villager)
{
	auto& list = abode.inhabitants;
	if (const auto it = std::find(list.begin(), list.end(), villager); it != list.end())
	{
		list.erase(it);
	}
}

/// The counts of RemoveAlive / RemoveDeleted (never below 0)
void DropCounts(Abode& abode, entt::entity villager)
{
	if (villager::IsChild(villager))
	{
		if (abode.childCount != 0)
		{
			--abode.childCount;
		}
		return;
	}
	if (abode.adultCount != 0)
	{
		--abode.adultCount;
	}
	if (abode.adultMaleCount != 0)
	{
		abode.adultMaleCount = static_cast<uint8_t>(abode.adultMaleCount - (IsMaleVillager(villager) ? 1 : 0));
	}
}
} // namespace

// ---- reading -----------------------------------------------------------------------------------------------------

const std::vector<entt::entity>& VillagersOf(entt::entity abode)
{
	const auto* a = AbodeComponent(abode);
	return a != nullptr ? a->inhabitants : k_NoVillagers;
}

uint8_t PresentAtHome(entt::entity abode)
{
	const auto* a = AbodeComponent(abode);
	return a != nullptr ? a->presentAtHome : 0;
}

entt::entity TownOf(entt::entity abode)
{
	const auto* a = AbodeComponent(abode);
	if (a == nullptr)
	{
		return entt::null;
	}
	const auto town = town_queries::TownByKey(a->townId);
	return town != entt::null && Entities().Valid(town) ? town : entt::null;
}

const GAbodeInfo* InfoOf(entt::entity abode)
{
	const auto town = TownOf(abode);
	const auto* tribe = town != entt::null ? Entities().TryGet<const Tribe>(town) : nullptr;
	// (openblack, guard) as GatherInputs: every scripted town has its Tribe
	return town_stats::AbodeInfoOf(abode, tribe != nullptr ? *tribe : Tribe::CELTIC);
}

uint32_t MaxVillagers(entt::entity abode)
{
	const auto* info = InfoOf(abode);
	return info != nullptr ? info->maxVillagersInAbode : 0;
}

uint32_t MaxChildren(entt::entity abode)
{
	const auto* info = InfoOf(abode);
	return info != nullptr ? info->maxChildrenInAbode : 0;
}

int32_t GetRoomLeftForAdults(entt::entity abode)
{
	const auto* a = AbodeComponent(abode);
	return static_cast<int32_t>(MaxVillagers(abode)) - (a != nullptr ? a->adultCount : 0);
}

int32_t GetRoomLeftForChildren(entt::entity abode)
{
	const auto* a = AbodeComponent(abode);
	return static_cast<int32_t>(MaxChildren(abode)) - (a != nullptr ? a->childCount : 0);
}

bool IsTooCrowded(entt::entity abode)
{
	// MaxVillagers 0 -> 1; adults / MaxVillagers not below percentTooCrowded -> 1
	const auto* info = InfoOf(abode);
	const auto* a = AbodeComponent(abode);
	if (info == nullptr || a == nullptr || info->maxVillagersInAbode == 0)
	{
		return true;
	}
	const float f = static_cast<float>(a->adultCount) / static_cast<float>(info->maxVillagersInAbode);
	return !(f < info->percentTooCrowded);
}

float GetPercentAbodeFullWithAdults(entt::entity abode)
{
	// MaxVillagers 0 -> 1; else adults / MaxVillagers
	const auto max = MaxVillagers(abode);
	const auto* a = AbodeComponent(abode);
	if (max == 0 || a == nullptr)
	{
		return 1.0f;
	}
	return static_cast<float>(a->adultCount) / static_cast<float>(max);
}

float GetPercentAbodeFullWithChildren(entt::entity abode)
{
	// MaxChildren 0 -> 1; else children / MaxChildren as an integer division
	const auto max = MaxChildren(abode);
	const auto* a = AbodeComponent(abode);
	if (max == 0 || a == nullptr)
	{
		return 1.0f;
	}
	return static_cast<float>(static_cast<uint32_t>(a->childCount) / max);
}

float ScoreForAdding(uint32_t count, uint32_t max, float sameSex, uint32_t listSize, float distance)
{
	// max 0 -> 0
	if (max == 0)
	{
		return 0.0f;
	}
	// f = count / max; not below 1 -> 1
	float f = static_cast<float>(count) / static_cast<float>(max);
	if (!(f < 1.0f))
	{
		f = 1.0f;
	}
	// room = 1 - f; room <= 0 -> room
	const float room = 1.0f - f;
	if (!(room > 0.0f))
	{
		return room;
	}
	// sex = 1; with a list: ((1 - sameSex / count) + 1) x 0.5
	float sex = 1.0f;
	if (listSize != 0)
	{
		const float share = sameSex / static_cast<float>(listSize);
		const float rest = 1.0f - share;
		const float plusOne = rest + 1.0f;
		sex = plusOne * 0.5f;
	}
	// (GetDistanceModifier(d, 500) + 1) x 0.5 x sex x room
	const float modifier = gutils::GetDistanceModifier(distance, k_ScoreDistance);
	const float plusOne = modifier + 1.0f;
	const float half = plusOne * 0.5f;
	const float withSex = half * sex;
	return withSex * room;
}

float CalculateScoreForAddingVillagerToAbode(entt::entity abode, entt::entity villager)
{
	const auto* a = AbodeComponent(abode);
	if (a == nullptr)
	{
		return 0.0f;
	}
	// A child counts against the children's places, else the adults'
	const bool child = villager::IsChild(villager);
	const uint32_t max = child ? MaxChildren(abode) : MaxVillagers(abode);
	const uint32_t count = child ? a->childCount : a->adultCount;
	// The villagers of the list (the children too, and the villager itself if it is there) whose sex is the
	// villager's, a float sum
	const auto sex = villager::InfoOf(villager).sex;
	float same = 0.0f;
	for (const auto other : a->inhabitants)
	{
		if (villager::InfoOf(other).sex == sex)
		{
			same = same + 1.0f;
		}
	}
	const float distance = town_queries::GetDistanceInMetres(town_queries::PosOf(abode), town_queries::PosOf(villager));
	return ScoreForAdding(count, max, same, static_cast<uint32_t>(a->inhabitants.size()), distance);
}

float DesireToGainMale(uint32_t townMales, uint32_t townFemales, uint8_t adults, uint8_t adultMales)
{
	// The town's men + 0.001 over its women + 0.001
	const float men = static_cast<float>(townMales) + k_Thousandth;
	const float women = static_cast<float>(townFemales) + k_Thousandth;
	const float townRatio = men / women;
	// Minus the house's men + 0.001 over its women (a signed int sub) + 0.001
	const float houseMen = static_cast<float>(adultMales) + k_Thousandth;
	const float houseWomen = static_cast<float>(static_cast<int32_t>(adults) - static_cast<int32_t>(adultMales)) + k_Thousandth;
	const float houseRatio = houseMen / houseWomen;
	return townRatio - houseRatio;
}

float CalculateDesireToGainMale(entt::entity abode)
{
	// MaxVillagers 0 or no town -> 0
	const auto* a = AbodeComponent(abode);
	const auto town = TownOf(abode);
	if (a == nullptr || MaxVillagers(abode) == 0 || town == entt::null)
	{
		return 0.0f;
	}
	const auto& stats = Entities().Get<const Town>(town).stats;
	// The men / women of the town
	return DesireToGainMale(stats.males, stats.females, a->adultCount, a->adultMaleCount);
}

float DesireToGainVillager(uint32_t townAdults, uint32_t townAdultPlaces, float percentAdults)
{
	// (the town's adults + 0.001) / (its adult places + 0.001), stored as a float
	const float adults = static_cast<float>(townAdults) + k_Thousandth;
	const float places = static_cast<float>(static_cast<int32_t>(townAdultPlaces)) + k_Thousandth;
	const float ratio = adults / places;
	// Minus GetPercentAbodeFullWithAdults
	return ratio - percentAdults;
}

float CalculateDesireToGainVillager(entt::entity abode)
{
	// MaxVillagers 0 or no town -> 0
	const auto town = TownOf(abode);
	if (MaxVillagers(abode) == 0 || town == entt::null)
	{
		return 0.0f;
	}
	const auto& stats = Entities().Get<const Town>(town).stats;
	return DesireToGainVillager(stats.adults, stats.adultPlaces, GetPercentAbodeFullWithAdults(abode));
}

// ---- the list ----------------------------------------------------------------------------------------------------

void AddVillagerToAbode(entt::entity abode, entt::entity villager)
{
	auto& registry = Entities();
	auto* v = registry.TryGet<Villager>(villager);
	if (AbodeComponent(abode) == nullptr || v == nullptr)
	{
		return;
	}
	// The villager's town before the move
	const auto oldTown = v->town != entt::null && registry.Valid(v->town) ? v->town : entt::null;
	if (oldTown != entt::null && town_villagers::IsVillagerInHomelessList(oldTown, villager))
	{
		// Out of the town's homeless list
		town_villagers::RemoveFromHomelessList(oldTown, villager);
	}
	else if (const auto old = v->abode; old != entt::null && registry.Valid(old))
	{
		// Out of its old abode
		RemoveAliveVillagerFromAbode(old, villager);
	}
	else
	{
		// Out of the global vagrants if it is there
		town_villagers::RemoveFromVagrants(villager);
	}
	// At the head of the list (RemoveAlive may have touched the component: read it again)
	auto& a = registry.Get<Abode>(abode);
	a.inhabitants.insert(a.inhabitants.begin(), villager);
	// SetAbode (the villager's town = the abode's, or none)
	villager::SetAbode(villager, abode);
	// The abode's town: AddVillagerToTown when it is not the old one, then the town's places left are updated.
	// (approximate) the town stats are recomputed each town turn: only the counts the original adds at once
	// (AddVillagerToTown's) are kept here
	if (const auto town = TownOf(abode); town != entt::null)
	{
		if (town != oldTown)
		{
			town_villagers::AddVillagerToTown(town, villager);
		}
	}
	auto& again = registry.Get<Abode>(abode);
	// A child ++ChildCount; an adult: MaleFemale[sex] when empty, ++AdultCount, AdultMaleCount += IsMaleVillager
	if (villager::IsChild(villager))
	{
		again.childCount = static_cast<uint8_t>(again.childCount + 1);
		return;
	}
	const auto sex = static_cast<size_t>(villager::InfoOf(villager).sex == SexType::Female ? 1 : 0);
	if (again.maleFemale.at(sex) == entt::null)
	{
		again.maleFemale.at(sex) = villager;
	}
	again.adultCount = static_cast<uint8_t>(again.adultCount + 1);
	again.adultMaleCount = static_cast<uint8_t>(again.adultMaleCount + (IsMaleVillager(villager) ? 1 : 0));
}

void RemoveAliveVillagerFromAbode(entt::entity abode, entt::entity villager)
{
	if (AbodeComponent(abode) == nullptr)
	{
		return;
	}
	// Inside -> SetTopState(163): the at-home exit does the LeaveHome
	if (IsInside(villager))
	{
		villager::SetTopState(villager, VillagerStates::DecideWhatToDo);
	}
	auto* a = AbodeComponent(abode);
	if (a == nullptr)
	{
		return;
	}
	// The counts, then the list
	DropCounts(*a, villager);
	Unlink(*a, villager);
	villager::SetAbode(villager, entt::null);
	// With a town, the original updates the town's places left here (recomputed each town turn instead)
}

void RemoveDeletedVillagerFromAbode(entt::entity abode, entt::entity villager)
{
	auto* a = AbodeComponent(abode);
	if (a == nullptr)
	{
		return;
	}
	// MaleFemale[sex] == the villager -> both entries of the pair cleared
	const auto sex = static_cast<size_t>(villager::InfoOf(villager).sex == SexType::Female ? 1 : 0);
	if (a->maleFemale.at(sex) == villager)
	{
		a->maleFemale.at(sex == 0 ? 1 : 0) = entt::null;
		a->maleFemale.at(sex) = entt::null;
	}
	// The counts, the list, SetAbode(none)
	DropCounts(*a, villager);
	Unlink(*a, villager);
	villager::SetAbode(villager, entt::null);
	// With a town, RemoveVillager (the town's places left are recomputed each town turn)
	if (const auto town = TownOf(abode); town != entt::null)
	{
		town_villagers::RemoveVillager(town, villager);
	}
}

void RemoveAllVillagersFromAbode(entt::entity abode)
{
	// From the head, on a copy: HomeDeleted takes each villager out of the list
	const auto list = VillagersOf(abode);
	for (const auto villager : list)
	{
		if (Entities().Valid(villager))
		{
			villager::HomeDeleted(villager);
		}
	}
}

void ArriveHome(entt::entity abode)
{
	if (auto* a = AbodeComponent(abode))
	{
		a->presentAtHome = static_cast<uint8_t>(a->presentAtHome + 1);
	}
}

void LeaveHome(entt::entity abode)
{
	if (auto* a = AbodeComponent(abode))
	{
		a->presentAtHome = static_cast<uint8_t>(a->presentAtHome - 1);
	}
}

void ChildToAdult(entt::entity abode, entt::entity villager)
{
	auto* a = AbodeComponent(abode);
	if (a == nullptr)
	{
		return;
	}
	// --ChildCount when not 0, ++AdultCount, AdultMaleCount += IsMaleVillager
	if (a->childCount != 0)
	{
		--a->childCount;
	}
	a->adultCount = static_cast<uint8_t>(a->adultCount + 1);
	a->adultMaleCount = static_cast<uint8_t>(a->adultMaleCount + (IsMaleVillager(villager) ? 1 : 0));
	// With a town, the town's ChildToAdult too
	if (const auto town = TownOf(abode); town != entt::null)
	{
		town_villagers::ChildToAdult(town, villager);
	}
}

uint32_t SwapMaleForFemaleFrom(entt::entity x, entt::entity y)
{
	// The first man of y's list that is not inside
	entt::entity man = entt::null;
	for (const auto v : VillagersOf(y))
	{
		if (IsMaleVillager(v) && !IsInside(v))
		{
			man = v;
			break;
		}
	}
	if (man == entt::null)
	{
		return 0;
	}
	// The first woman of x's list that is not inside
	entt::entity woman = entt::null;
	for (const auto v : VillagersOf(x))
	{
		if (IsFemaleVillager(v) && !IsInside(v))
		{
			woman = v;
			break;
		}
	}
	if (woman == entt::null)
	{
		return 0;
	}
	// ForceMoveVillagerToAbode(man -> x), then (woman -> y)
	villager::ForceMoveVillagerToAbode(man, x);
	villager::ForceMoveVillagerToAbode(woman, y);
	return 1;
}

uint32_t TakeVillagerFrom(entt::entity x, entt::entity y, bool male)
{
	// y's list from the head: the first of the asked sex that is not inside
	for (const auto v : VillagersOf(y))
	{
		const bool sex = male ? IsMaleVillager(v) : IsFemaleVillager(v);
		if (sex && !IsInside(v))
		{
			villager::ForceMoveVillagerToAbode(v, x);
			return 1;
		}
	}
	return 0;
}

// ---- the turn ----------------------------------------------------------------------------------------------------

bool RunsAbodeProcess(entt::entity abode)
{
	const auto* info = InfoOf(abode);
	if (info == nullptr)
	{
		return true;
	}
	switch (info->abodeType)
	{
	case AbodeType::Field:          // ecs::Fields
	case AbodeType::TownCentre:     // its own process
	case AbodeType::Workshop:       // ecs::workshops::Process (ProcessAbode at its end)
	case AbodeType::SpellDispenser: // its own process
	case AbodeType::FootballPitch:  // (inferred) its own class, not read
		return false;
	default:
		return true;
	}
}

void ProcessAbode(entt::entity abode)
{
	auto* a = AbodeComponent(abode);
	if (a == nullptr)
	{
		return;
	}
	// An abode's building site is processed by TownProcess step 3 (building_sites::Process), just before this.
	// Empty of adults and of children, built, not in a script ((inferred) no openblack abode is) and, with a town, one
	// that is not uninhabitable
	const auto town = TownOf(abode);
	const bool uninhabitable = town != entt::null && Entities().Get<const Town>(town).uninhabitable;
	if (GetPercentAbodeFullWithAdults(abode) == 0.0f && GetPercentAbodeFullWithChildren(abode) == 0.0f &&
	    abode_queries::IsBuilt(abode) && !uninhabitable)
	{
		// The empty timer += 0.001; at 1: ReduceLife(emptyAbodeLifeReducer, no player) and the timer back to 0
		a->emptyTimer = a->emptyTimer + k_EmptyTimerStep;
		if (!(a->emptyTimer < 1.0f))
		{
			const auto* info = InfoOf(abode);
			const float reducer = info != nullptr ? info->emptyAbodeLifeReducer : 0.0f;
			// abodes::ReduceLife: the life, the taps of the inhabitants (none here: the abode is empty),
			// StopBeingFunctional, the building site. The original also sets a flag only during the call (for a
			// statistic, not ported): not kept
			abodes::ReduceLife(abode, reducer, std::nullopt);
			a = AbodeComponent(abode);
			if (a == nullptr)
			{
				return;
			}
			a->emptyTimer = 0.0f;
		}
	}
	// field0xB9 below 200 -> ++ (no reader found)
	if (a->field0xB9 < k_CounterB9Max)
	{
		++a->field0xB9;
	}
}
} // namespace openblack::ecs::abode_villagers
