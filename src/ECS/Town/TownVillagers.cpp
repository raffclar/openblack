/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "TownVillagers.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>

#include <algorithm>
#include <array>
#include <optional>
#include <utility>

#include <fmt/format.h>
#include <spdlog/spdlog.h>

#include "3D/MapCoords.h"
#include "Debug/DebugEnv.h"
#include "ECS/Components/Abode.h"
#include "ECS/Components/Town.h"
#include "ECS/Components/Villager.h"
#include "ECS/MapCells.h"
#include "ECS/Registry.h"
#include "ECS/Systems/Implementations/VillagerWorship.h"
#include "ECS/Systems/TownStateSystemInterface.h"
#include "ECS/Town/AbodeQueries.h"
#include "ECS/Town/AbodeVillagers.h"
#include "ECS/Town/TownStats.h"
#include "ECS/Villager/VillagerCore.h"
#include "ECS/Villager/VillagerHome.h"
#include "ECS/Villager/VillagerMourning.h"
#include "Game/GameStats.h"
#include "InfoConstants.h"
#include "Locator.h"
#include "Worship/TownMagic.h"
#include "Worship/WorshipPercentage.h"

// The town's villagers (TownVillagers.h)

namespace openblack::ecs::town_villagers
{
using namespace components;

namespace
{
const std::vector<entt::entity> k_NoVillagers;
/// The villagers without a town, the newest first (Locator::townStateSystem)
std::vector<entt::entity>& VagrantList()
{
	if (!Locator::townStateSystem::has_value())
	{
		std::fputs("ecs::town_villagers: no townStateSystem in the locator (Locator::townStateSystem)\n", stderr);
		std::abort();
	}
	return Locator::townStateSystem::value().Vagrants();
}
/// ShuffleDue: the town id x 20
constexpr float k_ShuffleIdFactor = 20.0f;

Registry& Entities()
{
	return Locator::entitiesRegistry::value();
}

Town* TownComponent(entt::entity town)
{
	auto& registry = Entities();
	return town != entt::null && registry.Valid(town) ? registry.TryGet<Town>(town) : nullptr;
}

/// Adding or removing a villager in the town's counts, those read at once (the rest is recomputed by
/// town_stats::Compute at the start of each town turn): adults / children and the sexes (children too)
void CountVillager(TownStats& stats, entt::entity villager, int32_t k)
{
	// (openblack, guard) not below 0: the counts are a cache that town_stats::Compute rebuilds each town turn, and a
	// removal before the first one would wrap
	const auto add = [k](uint32_t& count) {
		count = k < 0 && count == 0 ? 0u : static_cast<uint32_t>(static_cast<int32_t>(count) + k);
	};
	add(villager::IsChild(villager) ? stats.children : stats.adults);
	add(villager::InfoOf(villager).sex == SexType::Female ? stats.females : stats.males);
}

/// The qsort's short sort with ShuffleCompare (town_desire::ShortSort's rule)
void ShortSort(std::vector<ShuffleEntry>& a, int lo, int hi)
{
	while (hi > lo)
	{
		int max = lo;
		for (int p = lo + 1; p <= hi; ++p)
		{
			if (ShuffleCompare(a.at(static_cast<size_t>(p)), a.at(static_cast<size_t>(max))) > 0)
			{
				max = p;
			}
		}
		std::swap(a.at(static_cast<size_t>(max)), a.at(static_cast<size_t>(hi)));
		--hi;
	}
}

/// |m^2 + v^2|, in the original's order: m x m, v x v, their sum, its absolute value
float Magnitude(float m, float v)
{
	const float mm = m * m;
	const float vv = v * v;
	return std::fabs(mm + vv);
}
} // namespace

// ---- the homeless and the vagrants -------------------------------------------------------------------------------

const std::vector<entt::entity>& Homeless(entt::entity town)
{
	const auto* t = TownComponent(town);
	return t != nullptr ? t->homelessVillagers : k_NoVillagers;
}

bool IsVillagerInHomelessList(entt::entity town, entt::entity villager)
{
	const auto& list = Homeless(town);
	return std::find(list.begin(), list.end(), villager) != list.end();
}

void AddToHomelessList(entt::entity town, entt::entity villager)
{
	if (auto* t = TownComponent(town))
	{
		t->homelessVillagers.insert(t->homelessVillagers.begin(), villager); // the head
	}
}

bool RemoveFromHomelessList(entt::entity town, entt::entity villager)
{
	auto* t = TownComponent(town);
	if (t == nullptr)
	{
		return false;
	}
	auto& list = t->homelessVillagers;
	const auto it = std::find(list.begin(), list.end(), villager);
	if (it == list.end())
	{
		return false;
	}
	list.erase(it);
	return true;
}

const std::vector<entt::entity>& Vagrants()
{
	return VagrantList();
}

bool AddToVagrants(entt::entity villager)
{
	if (IsVagrant(villager))
	{
		return false;
	}
	auto& vagrants = VagrantList();
	vagrants.insert(vagrants.begin(), villager);
	return true;
}

bool IsVagrant(entt::entity villager)
{
	const auto& vagrants = VagrantList();
	return std::find(vagrants.begin(), vagrants.end(), villager) != vagrants.end();
}

bool RemoveFromVagrants(entt::entity villager)
{
	auto& vagrants = VagrantList();
	const auto it = std::find(vagrants.begin(), vagrants.end(), villager);
	if (it == vagrants.end())
	{
		return false;
	}
	vagrants.erase(it);
	return true;
}

void ClearVagrants()
{
	VagrantList().clear();
}

// ---- joining, moving, eating -------------------------------------------------------------------------------------

bool AddVillagerToTown(entt::entity town, entt::entity villager)
{
	auto& registry = Entities();
	auto* t = TownComponent(town);
	if (t == nullptr || !registry.AllOf<Villager>(villager))
	{
		return false;
	}
	// uninhabitable -> 0
	if (t->uninhabitable)
	{
		return false;
	}
	// the town's counts, SetTown(this)
	CountVillager(t->stats, villager, 1);
	villager::SetTown(villager, town);
	// an abode of this town -> on to the end; of another one -> out of it, SetAbode(0)
	const auto abode = registry.Get<const Villager>(villager).abode;
	bool done = false;
	if (abode != entt::null && registry.Valid(abode))
	{
		if (abode_villagers::TownOf(abode) == town)
		{
			done = true;
		}
		else
		{
			abode_villagers::RemoveAliveVillagerFromAbode(abode, villager);
			villager::SetAbode(villager, entt::null);
			// SetAbode(0) also clears the town: the original's town is null now (literal)
		}
	}
	if (!done)
	{
		// FindAbodeWithSpaceInTown(v, 0) -> AddVillagerToAbode, 1 (no worship site check)
		if (const auto found = FindAbodeWithSpaceInTown(town, villager, 0.0f); found != entt::null)
		{
			abode_villagers::AddVillagerToAbode(found, villager);
			if (villager::TraceOn(villager))
			{
				villager::Trace(villager, fmt::format("town: into abode {} (AddVillagerToTown)", static_cast<uint32_t>(found)));
			}
			return true;
		}
		villager::MakeHomelessNoStateChange(villager);
	}
	// adults + children == 1 -> CheckAddWorshipSite
	if (const auto* again = TownComponent(town); again != nullptr && again->stats.adults + again->stats.children == 1)
	{
		worship::town::CheckAddWorshipSite(town);
	}
	return true;
}

entt::entity FindAbodeWithSpaceInTown(entt::entity town, entt::entity villager, float minimum)
{
	// the town's structures, the newest first; IsFunctional and the score strictly above the best, the best starting
	// at `minimum`
	entt::entity best = entt::null;
	float bestScore = minimum;
	for (const auto abode : town_stats::AbodesOf(town))
	{
		if (!abode_queries::IsFunctional(abode))
		{
			continue;
		}
		const float score = abode_villagers::CalculateScoreForAddingVillagerToAbode(abode, villager);
		if (score > bestScore)
		{
			bestScore = score;
			best = abode;
		}
	}
	return best;
}

void ChildToAdult(entt::entity town, entt::entity villager)
{
	auto* t = TownComponent(town);
	if (t == nullptr)
	{
		return;
	}
	// the places (with an abode) are recomputed each town turn; children - 1, adults + 1
	// (openblack, guard) the children not below 0 (a cache rebuilt each town turn)
	t->stats.children = t->stats.children != 0 ? t->stats.children - 1 : 0;
	t->stats.adults = t->stats.adults + 1;
	(void)villager;
}

void UseFood(entt::entity town, uint32_t amount)
{
	auto* t = TownComponent(town);
	if (t == nullptr)
	{
		return;
	}
	// added in double precision, then rounded to a float
	t->foodUsed = static_cast<float>(static_cast<double>(amount) + static_cast<double>(t->foodUsed));
	// the owner's food eaten statistic += n
	game_stats::FoodEaten(static_cast<size_t>(t->owner), amount);
}

void RemoveVillager(entt::entity town, entt::entity villager)
{
	auto& registry = Entities();
	auto* t = TownComponent(town);
	if (t == nullptr || !registry.AllOf<Villager>(villager))
	{
		return;
	}
	// the children whose mother it is go to 131 MORN_DEATH and lose her
	villager_mourning::FindChildrenAndOrphanThem(villager);
	// out of the town's counts
	CountVillager(t->stats, villager, -1);
	// an abode -> RemoveAliveVillagerFromAbode, SetAbode(0); else out of the homeless list
	const auto abode = registry.Get<const Villager>(villager).abode;
	if (abode != entt::null && registry.Valid(abode))
	{
		abode_villagers::RemoveAliveVillagerFromAbode(abode, villager);
		villager::SetAbode(villager, entt::null);
	}
	else
	{
		RemoveFromHomelessList(town, villager);
	}
	// out of the town's on-the-way-to-worship list
	worship::percentage::RemoveVillagerOnWay(town, villager);
	// at the worship site -> RemoveVillagerFromWorshipSite (it needs the town still set); it sets no state
	if (villager_worship::IsAtWorshipSite(villager))
	{
		villager_worship::RemoveVillagerFromWorshipSite(villager);
	}
	villager::SetTown(villager, entt::null);
	// adults + children == 0 -> the empty countdown = 50 (TownProcess counts it down; the town turning empty at 0 is
	// TODO(towns))
	if (auto* now = TownComponent(town); now != nullptr && now->stats.adults + now->stats.children == 0)
	{
		now->emptyCountdown = 50;
	}
	// mother = none
	if (auto* v = registry.TryGet<Villager>(villager))
	{
		v->mother = entt::null;
	}
}

// ---- the shuffle -------------------------------------------------------------------------------------------------

bool ShuffleDue(const Town& town, uint32_t turn)
{
	return ShuffleDueEvery(town.id, turn, Locator::infoConstants::value().town.shuffleVillagersEvery);
}

bool ShuffleDueEvery(uint32_t townId, uint32_t turn, uint32_t every)
{
	if (every == 0)
	{
		return false; // (openblack, guard) the original divides by zero
	}
	// the id as a float x 20, plus the turn as a signed integer, truncated
	const float scaled = static_cast<float>(static_cast<double>(townId)) * k_ShuffleIdFactor;
	const float sum = scaled + static_cast<float>(static_cast<int32_t>(turn));
	const auto value = static_cast<uint32_t>(map_coords::FtoL(sum));
	// unsigned remainder by shuffleVillagersEvery
	return value % every == 0;
}

int ShuffleCompare(const ShuffleEntry& a, const ShuffleEntry& b)
{
	// |b|^2 < |a|^2 -> -1, else 1
	return Magnitude(b.male, b.villager) < Magnitude(a.male, a.villager) ? -1 : 1;
}

void SortShuffle(std::vector<ShuffleEntry>& entries)
{
	// the original's qsort (the same steps as town_desire::MsvcQsort: CUTOFF 8, the middle as the pivot)
	if (entries.size() < 2)
	{
		return;
	}
	constexpr int k_Cutoff = 8;
	std::array<int, 30> loStack {};
	std::array<int, 30> hiStack {};
	int stack = 0;
	int lo = 0;
	int hi = static_cast<int>(entries.size()) - 1;
	const auto at = [&entries](int i) -> ShuffleEntry& { return entries.at(static_cast<size_t>(i)); };
	for (;;)
	{
		const int size = hi - lo + 1;
		if (size <= k_Cutoff)
		{
			ShortSort(entries, lo, hi);
		}
		else
		{
			const int mid = lo + size / 2;
			std::swap(at(mid), at(lo));
			int loGuy = lo;
			int hiGuy = hi + 1;
			for (;;)
			{
				do
				{
					++loGuy;
				} while (loGuy <= hi && ShuffleCompare(at(loGuy), at(lo)) <= 0);
				do
				{
					--hiGuy;
				} while (hiGuy > lo && ShuffleCompare(at(hiGuy), at(lo)) >= 0);
				if (hiGuy < loGuy)
				{
					break;
				}
				std::swap(at(loGuy), at(hiGuy));
			}
			std::swap(at(lo), at(hiGuy));
			if (hiGuy - lo > hi - loGuy)
			{
				if (lo + 1 < hiGuy)
				{
					loStack.at(static_cast<size_t>(stack)) = lo;
					hiStack.at(static_cast<size_t>(stack)) = hiGuy - 1;
					++stack;
				}
				if (loGuy < hi)
				{
					lo = loGuy;
					continue;
				}
			}
			else
			{
				if (loGuy < hi)
				{
					loStack.at(static_cast<size_t>(stack)) = loGuy;
					hiStack.at(static_cast<size_t>(stack)) = hi;
					++stack;
				}
				if (lo + 1 < hiGuy)
				{
					hi = hiGuy - 1;
					continue;
				}
			}
		}
		--stack;
		if (stack < 0)
		{
			return;
		}
		lo = loStack.at(static_cast<size_t>(stack));
		hi = hiStack.at(static_cast<size_t>(stack));
	}
}

bool PlanShuffle(const std::vector<ShuffleEntry>& sorted, size_t i, float (*percentAdults)(entt::entity), ShufflePlan& plan)
{
	if (i + 1 >= sorted.size())
	{
		return false;
	}
	const auto& a = sorted.at(i);
	// best = |a.m^2 + a.v^2|
	float best = Magnitude(a.male, a.villager);
	// the c after a: |(c.m + a.m)^2 + (c.v + a.v)^2| strictly below the best
	std::optional<size_t> found;
	for (size_t j = i + 1; j < sorted.size(); ++j)
	{
		const auto& c = sorted.at(j);
		const float m = c.male + a.male;
		const float v = c.villager + a.villager;
		const float d = Magnitude(m, v);
		if (d < best)
		{
			best = d;
			found = j;
		}
	}
	if (!found)
	{
		return false;
	}
	const auto& b = sorted.at(*found);
	// moreVillager = a.v > b.v
	const bool moreVillager = a.villager > b.villager;
	// a.v x b.v < 0 (opposite signs): the winner (moreVillager ? a : b) not below 100% full of adults keeps the swap;
	// below it, no swap
	bool swap = true;
	if (a.villager * b.villager < 0.0f)
	{
		const auto winner = moreVillager ? a.abode : b.abode;
		if (percentAdults != nullptr && percentAdults(winner) < 1.0f)
		{
			swap = false;
		}
	}
	// moreMale = a.m > b.m
	const bool moreMale = a.male > b.male;
	plan.a = i;
	plan.b = *found;
	plan.swap = swap;
	if (swap)
	{
		// moreMale ? a.SwapMaleForFemaleFrom(b) : b.SwapMaleForFemaleFrom(a)
		plan.firstIsA = moreMale;
		plan.male = false;
	}
	else
	{
		// moreVillager ? a.TakeVillagerFrom(b, moreMale) : b.TakeVillagerFrom(a, !moreMale)
		plan.firstIsA = moreVillager;
		plan.male = moreVillager ? moreMale : !moreMale;
	}
	return true;
}

void ShuffleVillagersAroundAbodes(entt::entity town)
{
	// the town's structures (newest first) that are functional with MaxVillagers + MaxChildren != 0
	std::vector<ShuffleEntry> entries;
	for (const auto abode : town_stats::AbodesOf(town))
	{
		if (!abode_queries::IsFunctional(abode) ||
		    abode_villagers::MaxVillagers(abode) + abode_villagers::MaxChildren(abode) == 0)
		{
			continue;
		}
		// {abode, CalculateDesireToGainMale, 0.5 x CalculateDesireToGainVillager}
		const float male = abode_villagers::CalculateDesireToGainMale(abode);
		const float villager = abode_villagers::CalculateDesireToGainVillager(abode) * 0.5f;
		entries.push_back({abode, male, villager});
	}
	// none -> nothing
	if (entries.empty())
	{
		return;
	}
	SortShuffle(entries);
	// i = 0 .. n - 2; one move a call (the first that moves someone ends it)
	for (size_t i = 0; i + 1 < entries.size(); ++i)
	{
		ShufflePlan plan;
		if (!PlanShuffle(entries, i, &abode_villagers::GetPercentAbodeFullWithAdults, plan))
		{
			continue;
		}
		const auto a = entries.at(plan.a).abode;
		const auto b = entries.at(plan.b).abode;
		const auto first = plan.firstIsA ? a : b;
		const auto second = plan.firstIsA ? b : a;
		const uint32_t moved = plan.swap ? abode_villagers::SwapMaleForFemaleFrom(first, second)
		                                 : abode_villagers::TakeVillagerFrom(first, second, plan.male);
		if (debug_env::TownTrace())
		{
			if (auto logger = spdlog::get("game"); logger != nullptr)
			{
				SPDLOG_LOGGER_INFO(logger, "Town trace: shuffle: {} -> {} ({}{}) = {}", static_cast<uint32_t>(second),
				                   static_cast<uint32_t>(first), plan.swap ? "swap" : "take",
				                   plan.swap ? "" : (plan.male ? " male" : " female"), moved);
			}
		}
		if (moved != 0)
		{
			break;
		}
	}
}

// ---- towns near a point ------------------------------------------------------------------------------------------

entt::entity GetNearestTown(glm::ivec2 pos, float radius)
{
	return map_cells::GetNearestTown(map_coords::MapCoords {pos.x, pos.y, 0.0f}, radius);
}
} // namespace openblack::ecs::town_villagers
