/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "TownDesire.h"

#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstdlib>

#include <algorithm>
#include <optional>
#include <string>
#include <unordered_map>
#include <utility>

#include <fmt/format.h>
#include <spdlog/spdlog.h>

#include "3D/DayNightClock.h"
#include "3D/SkyType.h"
#include "Audio/Services/Guidance.h"
#include "ECS/Components/Abode.h"
#include "ECS/Components/TownMagic.h"
#include "ECS/Components/Villager.h"
#include "ECS/Effects/Alignment.h"
#include "ECS/Events/Publish.h"
#include "ECS/Events/TownDesireEvents.h"
#include "ECS/Life.h"
#include "ECS/ObjectResources.h"
#include "ECS/Registry.h"
#include "ECS/StoragePitStore.h"
#include "ECS/Systems/DayNightClockSystemInterface.h"
#include "ECS/Systems/DebugHooksInterface.h"
#include "ECS/Systems/TownStateSystemInterface.h"
#include "ECS/Town/AbodeQueries.h"
#include "ECS/Town/BuildingSites.h"
#include "ECS/Town/TownQueries.h"
#include "ECS/Town/TownStats.h"
#include "ECS/Villager/VillagerCore.h"
#include "ECS/Villager/VillagerDecide.h"
#include "ECS/Villager/VillagerSatisfy.h"
#include "Game.h"
#include "Game/GameStats.h"
#include "Locator.h"
#include "Magic/Core/Players.h"

// The town's desires (TownDesire.h)

namespace openblack::ecs::town_desire
{
using namespace components;

namespace
{
// The original's constants (all floats unless "double"). The original's game logic runs at single precision, so the
// chains are float steps; a double constant is added in double then rounded once to float
constexpr float k_Zero = 0.0f;
constexpr float k_One = 1.0f;
constexpr float k_Half = 0.5f;
constexpr float k_Milli = 0.001f;
constexpr float k_Tenth = 0.1f;
constexpr float k_Tiny = 0.0001f;
constexpr float k_TinyAbodes = 1e-5f;
constexpr double k_TinyPop = 1e-5; // a double
constexpr float k_WarnAt = 0.95f;
constexpr float k_Two = 2.0f;
constexpr float k_FoodWarnOffset = 0.9f;
constexpr float k_Three = 3.0f;
constexpr float k_OneAndHalf = 1.5f;
constexpr float k_HomelessShare = 0.2f;
constexpr float k_MinusOne = -1.0f;
constexpr float k_UnhappyOffset = 0.6f;
constexpr float k_ZeroTrigger = 0.001f;         // a zero trigger becomes this
constexpr uint32_t k_PlaytimeAfterTurn = 0xFA0; // turn 4000
constexpr uint32_t k_AverageEvery = 50;

Registry& Entities()
{
	return Locator::entitiesRegistry::value();
}

/// The clamp of most desire functions: less than 0 or NaN -> 0; up to 1 -> itself; else 1
float Clamp01(float v)
{
	if (v < 0.0f || std::isnan(v))
	{
		return k_Zero;
	}
	return v <= 1.0f ? v : k_One;
}

/// 1 < x (or NaN) -> 1, else x
float MinOne(float x)
{
	return (1.0f < x || std::isnan(x)) ? 1.0f : x;
}

/// The original's float to int: truncation towards 0 (the low 32 bits are kept)
/// Kept apart from TruncateToInt because it truncates through a 64-bit integer and returns an unsigned value
uint32_t Ftol(float v)
{
	return static_cast<uint32_t>(static_cast<int64_t>(v));
}

float Raw(const TownDesire& d, size_t i)
{
	// raw + boost + boostA, added in this order
	return d.raw.at(i) + d.boost.at(i) + d.boostA.at(i);
}

float Desire(const TownDesire& d, size_t i)
{
	// desire + boost + boostA
	return d.desire.at(i) + d.boost.at(i) + d.boostA.at(i);
}

void Warn(const DesireContext& c, Warning warning, float value)
{
	if (c.warn != nullptr && *c.warn)
	{
		(*c.warn)(warning, value);
	}
}

/// (int) carried food + the storage pit's food, or the temporary pot's
uint32_t FoodAvailable(const DesireInputs& in)
{
	uint32_t food = Ftol(in.stats.foodCarried);
	if (in.storageFood.has_value())
	{
		return food + *in.storageFood;
	}
	if (in.potFood.has_value())
	{
		food += *in.potFood;
	}
	return food;
}

/// foodWantedMultiplier x the food for dinner (float precision)
float DesiredFood(const DesireContext& c)
{
	return c.town.foodWantedMultiplier * c.in.stats.foodForDinner;
}

/// (wood at the sites + carried, truncated toward zero) + the storage pit's wood, or the pot's
uint32_t WoodAvailable(const DesireInputs& in)
{
	uint32_t wood = Ftol(in.stats.woodAtSites + in.stats.woodCarried);
	if (in.storageWood.has_value())
	{
		return wood + *in.storageWood;
	}
	if (in.potWood.has_value())
	{
		wood += *in.potWood;
	}
	return wood;
}

/// The minimum (minimumWoodForDesire) / maximum (maximumWoodForDesire) wood: k = abodes /
/// numOfBuildingsForDesiredWood; k <= 1 uses 1 (so k = max(k, 1)); float precision: Wood stores it, the
/// modification does not, which gives the same value
float WoodScale(const DesireContext& c, float factor)
{
	const float k = static_cast<float>(c.in.abodeCount) / c.town.numOfBuildingsForDesiredWood;
	const float scale = (k <= 1.0f || std::isnan(k)) ? 1.0f : k;
	return scale * factor;
}

float WoodMinimum(const DesireContext& c)
{
	return WoodScale(c, c.town.minimumWoodForDesire);
}

float WoodMaximum(const DesireContext& c)
{
	return WoodScale(c, c.town.maximumWoodForDesire);
}

// ---- Amount / Desired (read only by the debug trace) -------------------------------------------------------------

uint32_t AmountAbodes(const DesireContext& c)
{
	return c.in.stats.abodesWithPlaces;
}

uint32_t DesiredAbodes(const DesireContext& c)
{
	// per abode = abodes ? (adults + children) / abodes : 0; then (that + abodes + 0.001) / (places + 0.001),
	// truncated toward zero
	// (signed)
	const auto& s = c.in.stats;
	const float perAbode =
	    s.abodesWithPlaces != 0 ? static_cast<float>(s.adults + s.children) / static_cast<int32_t>(s.abodesWithPlaces) : 0.0f;
	const float num = perAbode + static_cast<int32_t>(s.abodesWithPlaces) + k_Milli;
	const float den = static_cast<float>(static_cast<int32_t>(s.totalPlaces)) + k_Milli;
	return Ftol(num / den);
}

uint32_t AmountCivic(const DesireContext& c)
{
	return c.in.stats.civicBuildings;
}

uint32_t DesiredCivic(const DesireContext& c)
{
	return c.in.stats.civicPlans;
}

uint32_t AmountSupplyWorship([[maybe_unused]] const DesireContext& c)
{
	// storage pit && worship site ? the pit's food : 0. TODO: the worship site's part; 0
	return 0;
}

uint32_t DesiredSupplyWorship([[maybe_unused]] const DesireContext& c)
{
	// From the worship site. TODO: 0
	return 0;
}

// ---- the table -------------------------------------------------------------------------------------------------

/// The desire function table, in the order of TownDesireInfo
constexpr std::array<DesireFunctions, k_Count> k_DesireTable = {{
    {"Food", DesireForFood, nullptr, nullptr, villager::CheckSatisfyFoodDesire, ModificationFood, false, true},
    {"Wood", DesireForWood, nullptr, nullptr, villager::CheckSatisfyWoodDesire, ModificationWood, false, true},
    {"Playtime", DesireForPlaytime, nullptr, nullptr, villager::CheckSatisfyPlaytimeDesire, ModificationGeneral, true, false},
    {"Protection", DesireForProtection, nullptr, nullptr, nullptr, ModificationGeneral, true, false},
    {"Mercy", DesireForMercy, nullptr, nullptr, nullptr, ModificationGeneral, true, false},
    {"Abodes", DesireForAbodes, AmountAbodes, DesiredAbodes, villager::CheckSatisfyAbodesDesire, ModificationGeneral, false,
     false},
    {"Civic_Buildings", DesireForCivicBuildings, AmountCivic, DesiredCivic, villager::CheckSatisfyCivicBuildings,
     ModificationGeneral, false, false},
    {"Supply_Worship", DesireForSupplyWorship, AmountSupplyWorship, DesiredSupplyWorship, villager::CheckSatisfySupplyWorship,
     ModificationGeneral, false, false},
    {"For_Children", DesireForChildren, nullptr, nullptr, nullptr, ModificationGeneral, false, true},
    {"To_Build", DesireToBuild, nullptr, nullptr, villager::CheckSatisfyToBuild, ModificationToBuild, false, true},
    {"For_Rain", DesireForRain, nullptr, nullptr, nullptr, ModificationGeneral, false, false},
    {"For_Sun", DesireForSun, nullptr, nullptr, nullptr, ModificationGeneral, false, false},
    {"Repair_Town", DesireToRepair, nullptr, nullptr, villager::CheckSatisfyToRepair, ModificationGeneral, false, true},
    // Suppy_Workshop (sic)
    {"Suppy_Workshop", DesireToSupplyWorkshop, nullptr, nullptr, villager::CheckSatisfySupplyWorkshop, ModificationGeneral,
     false, false},
    {"For_Wonder", DesireToBuildWonder, nullptr, nullptr, nullptr, ModificationGeneral, false, false},
    {"Relaxation", DesireForRelaxation, nullptr, nullptr, villager::CheckSatisfyRelaxation, ModificationGeneral, true, false},
    {"Sleep", DesireForSleep, nullptr, nullptr, villager::CheckSatisfySleep, ModificationGeneral, true, false},
}};

/// The sort's comparator: a < b (or NaN) -> 1; equal -> 0; else -1 (descending)
int Compare(const DesireSort& a, const DesireSort& b)
{
	if (a.value < b.value || std::isnan(a.value) || std::isnan(b.value))
	{
		return 1;
	}
	if (a.value == b.value)
	{
		return 0;
	}
	return -1;
}

/// The quicksort's selection sort for small ranges: while hi > lo, the max of [lo, hi] (comp(p, max) > 0 takes p, so
/// the last of equal maxima stays the first found) goes to hi
void ShortSort(std::array<DesireSort, k_Count>& a, int lo, int hi)
{
	while (hi > lo)
	{
		int max = lo;
		for (int p = lo + 1; p <= hi; ++p)
		{
			if (Compare(a.at(static_cast<size_t>(p)), a.at(static_cast<size_t>(max))) > 0)
			{
				max = p;
			}
		}
		std::swap(a.at(static_cast<size_t>(max)), a.at(static_cast<size_t>(hi)));
		--hi;
	}
}

const DesireSort& Entry(const std::array<DesireSort, k_Count>& sorted, size_t k)
{
	return sorted.at(k);
}

Town* TownOf(entt::entity town)
{
	auto& registry = Entities();
	if (town == entt::null || !registry.Valid(town))
	{
		return nullptr;
	}
	return registry.TryGet<Town>(town);
}

const std::array<GTownDesireInfo, 17>& DesireInfo()
{
	return Locator::infoConstants::value().townDesire;
}

/// A town's warnings, sent as events::TownDesireWarning
WarningSink SinkFor(entt::entity town)
{
	return [town](Warning warning, float value) {
		events::Publish(events::TownDesireWarning {.town = town, .warning = warning, .value = value});
	};
}

/// The game's handler of TownDesireWarning: the guidance call with the town's help data
void PlayWarning(const events::TownDesireWarning& event)
{
	const auto help = town_queries::HelpTownOf(event.town);
	switch (event.warning)
	{
	case Warning::VillagersUnhappy:
		audio::guidance::WarnVillagersUnhappy(help);
		break;
	case Warning::LowOnFood:
		audio::guidance::WarnLowOnFood(help, event.value);
		break;
	case Warning::LowOnWood:
		audio::guidance::WarnLowOnWood(help, event.value);
		break;
	}
}

/// The context of a town entity (its inputs are gathered by the caller)
DesireContext ContextFor(const Town& town, const DesireInputs& in, const WarningSink* warn)
{
	const auto& info = Locator::infoConstants::value();
	// The fixed row GVillagerInfo[10] (see DesireContext)
	const auto& farmer = info.villager.at(10);
	return DesireContext {town.desire, in, info.town, info.townDesire, farmer.maxFoodCarried, farmer.maxWoodCarried, warn};
}

/// OPENBLACK_TOWN_TRACE="1[,<every>][,raw]": the 17 desires of each town every <every> turns (50) and whenever the
/// first of order 1 changes; with ",raw" order 2 too
struct TownTrace
{
	bool on {false};
	uint32_t every {50};
	bool raw {false};
};

const TownTrace& TraceConfig()
{
	static const TownTrace k_Config = [] {
		TownTrace config;
		const char* value = std::getenv("OPENBLACK_TOWN_TRACE");
		if (value == nullptr || *value == '\0' || std::string(value) == "0")
		{
			return config;
		}
		config.on = true;
		std::string text(value);
		size_t start = text.find(',');
		while (start != std::string::npos)
		{
			const size_t next = text.find(',', start + 1);
			const auto part = text.substr(start + 1, next == std::string::npos ? std::string::npos : next - start - 1);
			if (part == "raw")
			{
				config.raw = true;
			}
			else if (!part.empty())
			{
				config.every = std::max(1u, static_cast<uint32_t>(std::strtoul(part.c_str(), nullptr, 10)));
			}
			start = next;
		}
		return config;
	}();
	return k_Config;
}

/// What the town trace keeps between calls, in the debug hooks' store (Locator::debugHooks)
struct TownDesireDebugHooksState
{
	std::unordered_map<uint32_t, uint32_t> lastFirst; // OPENBLACK_TOWN_TRACE: each town's last first desire
};

TownDesireDebugHooksState& TownDesireDebugHooksData()
{
	if (!Locator::debugHooks::has_value())
	{
		std::fputs("ecs::town_desire: no debug hooks in the locator (Locator::debugHooks)\n", stderr);
		std::abort();
	}
	return Locator::debugHooks::value().Get<TownDesireDebugHooksState>();
}

void TraceTown(const Town& town, const TownDesire& desire, uint32_t turn, std::optional<float> average)
{
	const auto& config = TraceConfig();
	if (!config.on)
	{
		return;
	}
	auto& lastFirst = TownDesireDebugHooksData().lastFirst;
	const uint32_t first = desire.sorted.at(0).index;
	const auto last = lastFirst.find(town.id);
	const bool changed = last == lastFirst.end() || last->second != first;
	lastFirst[town.id] = first;
	if (!changed && turn % config.every != 0 && !average.has_value())
	{
		return;
	}
	auto logger = spdlog::get("game");
	if (logger == nullptr)
	{
		return;
	}
	std::string line = fmt::format("town {} turn {} pop {}:", town.id, turn, town.stats.adults + town.stats.children);
	for (const auto& e : desire.sorted)
	{
		const auto d = static_cast<size_t>(e.index) < k_Count ? e.index : 0u;
		line += fmt::format(" [{} {} {:.3f}", e.index, k_DesireTable.at(d).name, e.value);
		if (d == static_cast<size_t>(TownDesireInfo::ForSleep) || desire.raw.at(d) != desire.desire.at(d))
		{
			line += fmt::format(" raw {:.3f}", Raw(desire, d));
		}
		line += "]";
	}
	if (average.has_value())
	{
		line += fmt::format(" avg {:.3f}", *average);
	}
	SPDLOG_LOGGER_INFO(logger, "{}", line);
	if (config.raw)
	{
		std::string raw = fmt::format("town {} turn {} raw:", town.id, turn);
		for (const auto& e : desire.sortedRaw)
		{
			const auto d = static_cast<size_t>(e.index) < k_Count ? e.index : 0u;
			raw += fmt::format(" [{} {} {:.3f}]", e.index, k_DesireTable.at(d).name, e.value);
		}
		SPDLOG_LOGGER_INFO(logger, "{}", raw);
	}
}
} // namespace

const std::array<DesireFunctions, k_Count>& Table()
{
	return k_DesireTable;
}

// ---- the desire functions ----------------------------------------------------------------------------------------

float DesireForFood(const DesireContext& c)
{
	// a = the food available (unsigned) + 1e-4
	const auto a = static_cast<float>(FoodAvailable(c.in)) + k_Tiny;
	// v = 1 - a / (the desired food + 1e-4); at float precision the comparison below reads the stored float
	const float v = 1.0f - a / (DesiredFood(c) + k_Tiny);
	// v >= 0.95, a player, the local one: WarnLowOnFood(min(v, 2) - 0.9)
	if (v >= k_WarnAt && c.in.owner.has_value() && c.in.isLocalPlayer)
	{
		const float m = v < k_Two ? v : k_Two;
		Warn(c, Warning::LowOnFood, m - k_FoodWarnOffset);
	}
	return Clamp01(v);
}

float DesireForWood(const DesireContext& c)
{
	const auto& d = c.desire;
	// R12 + R9 + R6 + R5, the raws then the boosts then the boosts A, in this order; < 3 or 3
	const float sum = d.raw.at(12) + d.raw.at(9) + d.raw.at(6) + d.raw.at(5) + d.boost.at(12) + d.boost.at(9) + d.boost.at(6) +
	                  d.boostA.at(12) + d.boost.at(5) + d.boostA.at(9) + d.boostA.at(6) + d.boostA.at(5);
	// NaN keeps the sum
	const float s = (sum < k_Three || std::isnan(sum)) ? sum : k_Three;
	// a = (craftsmen + 0.001) / (adults + 0.001) + S
	const auto craftsmen = c.in.stats.disciples.at(static_cast<size_t>(VillagerDisciple::Craftsman));
	const float a = (static_cast<float>(craftsmen) + k_Milli) / (static_cast<float>(c.in.stats.adults) + k_Milli) + s;
	// w = the wood available, B = the minimum wood, C = the maximum wood
	const auto w = static_cast<float>(WoodAvailable(c.in));
	const auto b = WoodMinimum(c);
	const auto cMax = WoodMaximum(c);
	// v = (1 - min(w / C, 1)) x (1 - min(w / B, 1) + a) (NaN kept)
	float x = w / b;
	x = (x < 1.0f || std::isnan(x)) ? x : 1.0f;
	const float t = 1.0f - x + a;
	float y = w / cMax;
	y = (y < 1.0f || std::isnan(y)) ? y : 1.0f;
	const auto v = (1.0f - y) * t;
	// v >= 0.95, a player, the local one: WarnLowOnWood(min(v, 2) - 1)
	if (v >= k_WarnAt && c.in.owner.has_value() && c.in.isLocalPlayer)
	{
		const float m = v < k_Two ? v : k_Two;
		Warn(c, Warning::LowOnWood, m - k_One);
	}
	return Clamp01(v);
}

float DesireForPlaytime(const DesireContext& c)
{
	// D0, D1, D5, D6, D9 all strictly below their desireTriggersVillagerAction, else 0
	for (const size_t d : {0u, 1u, 5u, 6u, 9u})
	{
		const auto value = Desire(c.desire, d);
		if (!(value < c.info.at(d).desireTriggersVillagerAction))
		{
			return k_Zero;
		}
	}
	// the game turn > 4000, else 0
	return c.in.turn > k_PlaytimeAfterTurn ? k_Tenth : k_Zero;
}

float DesireForProtection(const DesireContext& c)
{
	return c.in.protection;
}

float DesireForMercy(const DesireContext& c)
{
	return c.in.mercy;
}

float DesireForAbodes(const DesireContext& c)
{
	const auto& s = c.in.stats;
	// a = adults / (adult places + 1e-5), < 1.5 or 1.5
	const float a = static_cast<int32_t>(s.adults) / (static_cast<float>(static_cast<int32_t>(s.adultPlaces)) + k_TinyAbodes);
	const auto aStored = (a < k_OneAndHalf || std::isnan(a)) ? a : k_OneAndHalf;
	// c = children / (child places + 1e-5), min(c, 1.5); c <= a keeps a, else m = min(c, 1.5)
	const float cRatio =
	    static_cast<int32_t>(s.children) / (static_cast<float>(static_cast<int32_t>(s.childPlaces)) + k_TinyAbodes);
	const float cMin = (cRatio < k_OneAndHalf || std::isnan(cRatio)) ? cRatio : k_OneAndHalf;
	const float m = (cMin <= aStored || std::isnan(cMin)) ? aStored : cMin;
	// m^4 x (1 - R9)
	const float m4 = m * m * m * m;
	const auto p = m4 * (1.0f - Raw(c.desire, 9));
	// (1 - D6) x that, clamped to [0, 1]
	return Clamp01((1.0f - Desire(c.desire, 6)) * p);
}

float DesireForCivicBuildings(const DesireContext& c)
{
	const auto& s = c.in.stats;
	const uint32_t pop = s.adults + s.children;
	float sum = k_Zero;
	for (size_t i = 0; i < 16; ++i)
	{
		// the town has none of that abode number
		if (s.abodesByNumber.at(i) != 0)
		{
			continue;
		}
		// the tribe's PopulationWhenNeeded for it > -1
		const auto& needed = c.in.populationWhenNeeded.at(i);
		if (!needed.has_value() || *needed <= -1)
		{
			continue;
		}
		const int32_t p = *needed;
		// p == 0, or homeless / pop < 0.2 (also NaN, so pop 0 with no homeless passes)
		if (p != 0)
		{
			const float share = static_cast<float>(c.in.homeless) / static_cast<int32_t>(pop);
			if (!(share < k_HomelessShare || std::isnan(share)))
			{
				continue;
			}
		}
		// p <= pop (signed)
		if (p > static_cast<int32_t>(pop))
		{
			continue;
		}
		// s += 0.5 (pop - p + 0.001) / (p + 0.001) + 0.5
		const float add = (static_cast<float>(static_cast<int32_t>(pop) - p) + k_Milli) / (static_cast<float>(p) + k_Milli);
		sum = add * k_Half + sum + k_Half;
	}
	// min(s, 1) (NaN kept)
	return (sum < k_One || std::isnan(sum)) ? sum : k_One;
}

float DesireForSupplyWorship([[maybe_unused]] const DesireContext& c)
{
	return k_Zero;
}

float DesireForChildren(const DesireContext& c)
{
	const auto& s = c.in.stats;
	// A = R0 >= 1 ? 0 : (R0 > 0 ? 1 - R0 : 1)
	float a = k_One;
	const float r0 = Raw(c.desire, 0);
	if (!(r0 < 1.0f || std::isnan(r0)))
	{
		a = k_Zero; // 1 - 1
	}
	else if (!(r0 <= 0.0f || std::isnan(r0)))
	{
		a = 1.0f - r0;
	}
	// B = children < child places (unsigned) ? 1 : 0
	const float b = s.children < s.childPlaces ? k_One : k_Zero;
	// C = adults < adult places ? 1 : 0.5
	const float cAdults = s.adults < s.adultPlaces ? k_One : k_Half;
	// P = D3 > 0 ? D3 : 0, M = D4 > 0 ? D4 : 0; Q = (1 - M)(1 - P)
	const float d3 = Desire(c.desire, 3);
	const float pProtection = (d3 <= 0.0f || std::isnan(d3)) ? k_Zero : d3;
	const float d4 = Desire(c.desire, 4);
	const float mMercy = (d4 <= 0.0f || std::isnan(d4)) ? 0.0f : d4;
	const auto q = (1.0f - pProtection) * (1.0f - mMercy);
	// al = a player ? 0.5 x alignment^3 : 0
	float al = k_Zero;
	if (c.in.owner.has_value())
	{
		const float alignment = c.in.alignment;
		al = alignment * alignment * alignment * k_Half;
	}
	// X = a player ? its tribal power 4 : 1; v = X (1 + al) Q C B A
	const float x = c.in.owner.has_value() ? c.in.tribalPower4 : 1.0f;
	auto v = x * (1.0f + al) * q * cAdults * b * a;
	// no functional creche -> v x 0.5
	if (!c.in.crecheFunctional)
	{
		v = v * k_Half;
	}
	return Clamp01(v);
}

float DesireToBuild(const DesireContext& c)
{
	// the sum of the sites' desire for villagers
	float sum = k_Zero;
	for (const float site : c.in.siteDesires)
	{
		sum = site + sum;
	}
	// 1 < s -> 1
	return (k_One < sum || std::isnan(sum)) ? k_One : sum;
}

float DesireForRain([[maybe_unused]] const DesireContext& c)
{
	return k_Zero;
}

float DesireForSun([[maybe_unused]] const DesireContext& c)
{
	return k_Zero;
}

float AbodeDesireToBeRepaired(const RepairInput& abode, const GTownInfo& town)
{
	// how repaired it is > thresholdToStartRepairing -> 0
	if (abode.life > town.thresholdToStartRepairing)
	{
		return k_Zero;
	}
	// a home (AbodeType & 2) with nobody -> 0
	if (abode.livingQuarters && abode.inhabitants == 0)
	{
		return k_Zero;
	}
	// fully repaired (life >= 1) -> 0
	if (!(abode.life < k_One || std::isnan(abode.life)))
	{
		return k_Zero;
	}
	// min(((1 - life) x 0.5 + 0.5) x desireToBeRepaired, 1)
	const float v = ((1.0f - abode.life) * k_Half + k_Half) * abode.desireToBeRepaired;
	return (v < 1.0f || std::isnan(v)) ? v : k_One;
}

float DesireToRepair(const DesireContext& c)
{
	// the abodes' desire to be repaired
	float sum = k_Zero;
	for (const auto& abode : c.in.abodes)
	{
		sum = AbodeDesireToBeRepaired(abode, c.town) + sum;
	}
	// the plans' desire to be repaired (wasBuilt ? the info's : 0): a plan made from a built abode
	// (plans::CreateFromBuilding: a destroyed built abode leaves a rebuild plan); the values come from
	// building_sites::DesireInputsOf -> plans::GetDesireToBeRepaired. The abodes above are summed with no site
	// filter: a rock-damaged house raises Repair_Town although CheckSatisfyToRepair cannot take its ordinary site
	// (literal)
	for (const float plan : c.in.planRepairDesires)
	{
		sum = plan + sum;
	}
	// 1 < s -> 1
	return (k_One < sum || std::isnan(sum)) ? k_One : sum;
}

float DesireToSupplyWorkshop([[maybe_unused]] const DesireContext& c)
{
	return k_Zero;
}

float DesireToBuildWonder(const DesireContext& c)
{
	// b = the town's belief in its player. TODO: not ported (0)
	const float belief = c.in.belief;
	// b x (1 - min(0.5 (D0 + (D1 + D5)), 0.5)) (NaN kept)
	const auto d5 = Desire(c.desire, 5);
	const auto d15 = Desire(c.desire, 1) + d5;
	float h = (Desire(c.desire, 0) + d15) * k_Half;
	h = (h < k_Half || std::isnan(h)) ? h : k_Half;
	return (1.0f - h) * belief;
}

float DesireForRelaxation(const DesireContext& c)
{
	// sky = the sky type of the visual time
	const float visual = c.in.visualHour;
	const float sky = sky_type::At(visual);
	// the evening ramp with 0.5 x relaxationMod for both widths
	const auto w = c.town.relaxationMod * k_Half;
	const float ramp = sky_type::EveningRamp(visual, w, w);
	// x = 1 - sky; x > 0 else 0; R = ramp x x
	float x = 1.0f - sky;
	x = (x <= 0.0f || std::isnan(x)) ? 0.0f : x;
	const float r = ramp * x;
	// R < 0.1 (or NaN) -> 0.1; R > 1 -> 1
	if (r < k_Tenth || std::isnan(r))
	{
		return k_Tenth;
	}
	return r <= 1.0f ? r : k_One;
}

float DesireForSleep(const DesireContext& c)
{
	// sky = the sky type of the visual time
	const float visual = c.in.visualHour;
	const float sky = sky_type::At(visual);
	// s = the evening ramp (1, 0) + sky - bedTimeMod
	float s = sky_type::EveningRamp(visual, 1.0f, 0.0f) + sky - c.town.bedTimeMod;
	// s > 0 else 0; s^2 (not clamped: 6.25 at night)
	s = (s <= 0.0f || std::isnan(s)) ? 0.0f : s;
	return s * s;
}

// ---- GetDesire, the modifications ---------------------------------------------------------------------------------

float GetDesire(const TownDesire& desire, size_t d)
{
	return Desire(desire, d);
}

float GetRawDesire(const TownDesire& desire, size_t d)
{
	return Raw(desire, d);
}

float GetDesireVillagerModification(const DesireContext& c, size_t d)
{
	// the entry's modification, with d
	const auto fn = k_DesireTable.at(d).modification;
	return fn != nullptr ? fn(c, d) : k_One;
}

float ModificationGeneral(const DesireContext& c, size_t d)
{
	// 1 - min(doingNow[d] / (adults + children + 1e-5 (double)), 1)
	const auto pop = static_cast<float>(static_cast<double>(c.in.stats.adults + c.in.stats.children) + k_TinyPop);
	return 1.0f - MinOne(c.desire.doingNow.at(d) / pop);
}

float ModificationFood(const DesireContext& c, size_t d)
{
	// n = the farmer's max food (150) x doingNow[d] + 1e-4
	auto n = static_cast<float>(c.farmerMaxFood) * c.desire.doingNow.at(d) + k_Tiny;
	// + the storage pit's food, 0 without one (the pot is not read)
	const float store = c.in.storageFood.has_value() ? static_cast<float>(*c.in.storageFood) : 0.0f;
	n = store + n;
	// 1 - min(n / (the desired food + 1e-4), 1)
	return 1.0f - MinOne(n / (DesiredFood(c) + k_Tiny));
}

float ModificationWood(const DesireContext& c, size_t d)
{
	// the farmer's max wood (250) x doingNow[d] + 1e-4
	auto n = static_cast<float>(c.farmerMaxWood) * c.desire.doingNow.at(d) + k_Tiny;
	// + the storage pit's wood
	const float store = c.in.storageWood.has_value() ? static_cast<float>(*c.in.storageWood) : 0.0f;
	n = store + n;
	// 1 - min(n / (the maximum wood + 1e-4), 1)
	return 1.0f - MinOne(n / (WoodMaximum(c) + k_Tiny));
}

float ModificationToBuild(const DesireContext& c, [[maybe_unused]] size_t d)
{
	// a = 1e-4 + the sum of the sites' builders, b = 1e-4 + the sum of their places
	auto a = k_Tiny;
	auto b = k_Tiny;
	for (size_t i = 0; i < c.in.siteBuilders.size(); ++i)
	{
		a = static_cast<float>(c.in.siteBuilders.at(i)) + a;
		const int32_t places = i < c.in.sitePlaces.size() ? c.in.sitePlaces.at(i) : 0;
		b = static_cast<float>(places) + b;
	}
	// 1 - min(a / b, 1) (no sites: 1e-4 / 1e-4, so 0)
	return 1.0f - MinOne(a / b);
}

float GetTemporaryDesireVillagerModification(const TownDesire& desire, uint32_t population, size_t k)
{
	// pop = adults + children + 1e-5 (double)
	const auto pop = static_cast<float>(static_cast<double>(population) + k_TinyPop);
	// x = doingNow[k] - doingNowAtStart[k]; kept when >= 0, else 0
	float x = desire.doingNow.at(k) - desire.doingNowAtStart.at(k);
	x = (x < 0.0f) ? 0.0f : x;
	// 1 - min(x / pop, 1)
	return 1.0f - MinOne(x / pop);
}

// ---- Process -----------------------------------------------------------------------------------------------------

float CallDesireFunction(TownDesire& desire, const DesireContext& c, size_t d)
{
	const auto& entry = k_DesireTable.at(d);
	// no function -> 0 (none of the 17)
	if (entry.function == nullptr)
	{
		return k_Zero;
	}
	const float f = entry.function(c);
	// x the desire info's TribeMultiplier of the town's tribe -> raw[d]
	const auto tribe = static_cast<size_t>(static_cast<int32_t>(c.in.tribe));
	const auto& multipliers = c.info.at(d).tribeMultiplier;
	// (openblack, guard) a tribe out of the 9 (Tribe::NONE) takes 1: the original always has a GTribeInfo here
	const float multiplier = tribe < multipliers.size() ? multipliers.at(tribe) : k_One;
	const auto raw = multiplier * f;
	desire.raw.at(d) = raw;
	// raw x GetDesireVillagerModification(d), < -1 (or NaN) -> -1, > 1 -> 1
	const float v = GetDesireVillagerModification(c, d) * raw;
	if (v < k_MinusOne || std::isnan(v))
	{
		return k_MinusOne;
	}
	return v <= 1.0f ? v : k_One;
}

void ProcessDesire(TownDesire& desire, const DesireContext& c, size_t d)
{
	// doingNow[d] < 0 (or NaN) -> 0
	auto& doing = desire.doingNow.at(d);
	if (doing < k_Zero || std::isnan(doing))
	{
		doing = k_Zero;
	}
	// the copies at the start of the turn
	desire.doingNowAtStart.at(d) = desire.doingNow.at(d);
	desire.doingNowCountAtStart.at(d) = desire.doingNowCount.at(d);
	const auto& entry = k_DesireTable.at(d);
	// Amount / Desired (unsigned)
	if (entry.amount != nullptr)
	{
		desire.amount.at(d) = static_cast<float>(entry.amount(c));
	}
	if (entry.desired != nullptr)
	{
		desire.desired.at(d) = static_cast<float>(entry.desired(c));
	}
	// desire[d] = CallDesireFunction(d)
	desire.desire.at(d) = CallDesireFunction(desire, c, d);
	// The original also adds the desire to a network debug checksum: not ported
}

void MsvcQsort(std::array<DesireSort, k_Count>& entries)
{
	// The original's quicksort, on element indices (its byte-offset test `higuy - 1 - lo >= hi - loguy` is
	// (higuy - lo) > (hi - loguy) in elements)
	constexpr int k_Cutoff = 8; // ranges this small use ShortSort
	std::array<int, 30> loStack {};
	std::array<int, 30> hiStack {};
	int stack = 0;
	int lo = 0;
	int hi = static_cast<int>(k_Count) - 1;
	for (;;)
	{
		const int size = hi - lo + 1;
		if (size <= k_Cutoff)
		{
			ShortSort(entries, lo, hi);
		}
		else
		{
			// the middle (lo + size / 2) swapped to lo as the pivot
			const int mid = lo + size / 2;
			std::swap(entries.at(static_cast<size_t>(mid)), entries.at(static_cast<size_t>(lo)));
			int loGuy = lo;
			int hiGuy = hi + 1;
			for (;;)
			{
				// do loguy++ while loguy <= hi && comp(loguy, lo) <= 0
				do
				{
					++loGuy;
				} while (loGuy <= hi &&
				         Compare(entries.at(static_cast<size_t>(loGuy)), entries.at(static_cast<size_t>(lo))) <= 0);
				// do higuy-- while higuy > lo && comp(higuy, lo) >= 0
				do
				{
					--hiGuy;
				} while (hiGuy > lo &&
				         Compare(entries.at(static_cast<size_t>(hiGuy)), entries.at(static_cast<size_t>(lo))) >= 0);
				// higuy < loguy -> break; else swap(loguy, higuy)
				if (hiGuy < loGuy)
				{
					break;
				}
				std::swap(entries.at(static_cast<size_t>(loGuy)), entries.at(static_cast<size_t>(hiGuy)));
			}
			// swap(lo, higuy)
			std::swap(entries.at(static_cast<size_t>(lo)), entries.at(static_cast<size_t>(hiGuy)));
			if (hiGuy - lo > hi - loGuy)
			{
				// lo + 1 < higuy -> push (lo, higuy - 1)
				if (lo + 1 < hiGuy)
				{
					loStack.at(static_cast<size_t>(stack)) = lo;
					hiStack.at(static_cast<size_t>(stack)) = hiGuy - 1;
					++stack;
				}
				// loguy < hi -> lo = loguy, again
				if (loGuy < hi)
				{
					lo = loGuy;
					continue;
				}
			}
			else
			{
				// loguy < hi -> push (loguy, hi)
				if (loGuy < hi)
				{
					loStack.at(static_cast<size_t>(stack)) = loGuy;
					hiStack.at(static_cast<size_t>(stack)) = hi;
					++stack;
				}
				// lo + 1 < higuy -> hi = higuy - 1, again
				if (lo + 1 < hiGuy)
				{
					hi = hiGuy - 1;
					continue;
				}
			}
		}
		// pop, or done
		--stack;
		if (stack < 0)
		{
			return;
		}
		lo = loStack.at(static_cast<size_t>(stack));
		hi = hiStack.at(static_cast<size_t>(stack));
	}
}

void SortDesires(TownDesire& desire)
{
	// {boost + boostA, desire + boost + boostA, d} for d in order, then the sort
	for (size_t d = 0; d < k_Count; ++d)
	{
		auto& e = desire.sorted.at(d);
		e.boosts = desire.boost.at(d) + desire.boostA.at(d);
		e.value = GetDesire(desire, d);
		e.index = static_cast<uint32_t>(d);
	}
	MsvcQsort(desire.sorted);
}

void SortRawDesires(TownDesire& desire)
{
	// {boostA (a bit copy), raw + boost + boostA, d}, then the sort
	for (size_t d = 0; d < k_Count; ++d)
	{
		auto& e = desire.sortedRaw.at(d);
		e.boosts = desire.boostA.at(d);
		e.value = GetRawDesire(desire, d);
		e.index = static_cast<uint32_t>(d);
	}
	MsvcQsort(desire.sortedRaw);
}

std::optional<float> Process(TownDesire& desire, const DesireContext& c)
{
	// population = adults + children - on the way - worshipping (unsigned arithmetic)
	const uint32_t people = c.in.stats.children - static_cast<uint32_t>(c.in.onWayToWorship) -
	                        static_cast<uint32_t>(c.in.worshipping) + c.in.stats.adults;
	desire.population = static_cast<float>(people);
	// ProcessDesire(d) for d = 0..16
	for (size_t d = 0; d < k_Count; ++d)
	{
		ProcessDesire(desire, c, d);
	}
	SortDesires(desire);
	SortRawDesires(desire);
	// The original's debug trace is openblack's OPENBLACK_TOWN_TRACE
	// turn % 50 == 0, the town and its player
	if (c.in.turn % k_AverageEvery != 0 || !c.in.owner.has_value())
	{
		return std::nullopt;
	}
	// max(R5, R6) and max(R4, R3) (the first if greater)
	const float r5 = Raw(desire, 5);
	const auto r6 = Raw(desire, 6);
	const float max56 = r5 > r6 ? r5 : r6;
	const float r4 = Raw(desire, 4);
	const auto r3 = Raw(desire, 3);
	const float max34 = r4 > r3 ? r4 : r3;
	// (2 R0 + raw1 + boost1 + boostA1 + max34 + max56) / divisorForAverageDesires
	float sum = Raw(desire, 0);
	sum = sum + sum;
	sum = sum + desire.raw.at(1) + desire.boost.at(1) + desire.boostA.at(1);
	sum = sum + max34 + max56;
	const float average = sum / c.town.divisorForAverageDesires;
	// the player's game stats: one more sample, += 1 - avg (a float)
	game_stats::TownDesire(static_cast<size_t>(*c.in.owner), 1.0f - average);
	// avg > thresholdForAverageDesiresHelpSprites, the local player: WarnVillagersUnhappy(town, avg - 0.6)
	const auto stored = average;
	if (!(average <= c.town.thresholdForAverageDesiresHelpSprites || std::isnan(average)) && c.in.isLocalPlayer)
	{
		Warn(c, Warning::VillagersUnhappy, stored - k_UnhappyOffset);
	}
	return stored;
}

uint32_t CheckVillagerNeeded(const TownDesire& desire, const std::array<GTownDesireInfo, 17>& info, uint32_t population,
                             bool child, float trigger, const std::function<uint32_t(size_t d)>& checkSatisfy,
                             const std::function<void(const ShareOutStep&)>& trace)
{
	// trigger == 0 (or NaN) -> 0.001
	if (trigger == k_Zero || std::isnan(trigger))
	{
		trigger = k_ZeroTrigger;
	}
	// (child: the caller's IsChild)
	for (size_t k = 0; k < k_Count; ++k)
	{
		const auto& e = Entry(desire.sorted, k);
		// (openblack) an index out of the table cannot happen (Process writes 0..16); skip it
		if (static_cast<size_t>(e.index) >= k_Count)
		{
			continue;
		}
		const size_t d = e.index;
		const auto& entry = k_DesireTable.at(d);
		// t = trigger + desireTriggersVillagerAction; < 1 or 1, as a float
		const float sum = trigger + info.at(d).desireTriggersVillagerAction;
		const float t = (sum < 1.0f || std::isnan(sum)) ? sum : k_One;
		// a child and the entry not for children -> next (no cut)
		if (child && !entry.children)
		{
			if (trace)
			{
				trace({k, e.index, e.value, 0.0f, t, "skip(child)"});
			}
			continue;
		}
		// no CheckSatisfy -> next (no cut)
		if (entry.checkSatisfy == nullptr)
		{
			if (trace)
			{
				trace({k, e.index, e.value, 0.0f, t, "skip(nocs)"});
			}
			continue;
		}
		// GetTemporaryDesireVillagerModification(k) x value <= t -> return 0. Literal oddity: the modification is
		// asked with the loop index k, not with the desire's index d, so position k of the order is corrected with
		// the counters of desire number k
		const float temporary = GetTemporaryDesireVillagerModification(desire, population, k);
		const float m = temporary * e.value;
		if (m <= t || std::isnan(m))
		{
			if (trace)
			{
				trace({k, e.index, e.value, temporary, t, "cut"});
			}
			return 0;
		}
		// CheckSatisfy on the villager == 1 -> 1
		const uint32_t satisfied = checkSatisfy(d);
		if (trace)
		{
			trace({k, e.index, e.value, temporary, t, satisfied == 1 ? "cs=1" : "cs=0"});
		}
		if (satisfied == 1)
		{
			return 1;
		}
	}
	return 0;
}

float GetDesireSignificanceToVillager(const TownDesire& desire, const GTownDesireInfo& info, size_t d)
{
	// D(d) - desireTriggersVillagerAction; > 0 else 0
	const auto value = Desire(desire, d);
	const float v = value - info.desireTriggersVillagerAction;
	return (v <= 0.0f || std::isnan(v)) ? k_Zero : v;
}

std::optional<int> GetMostDesired(const TownDesire& desire)
{
	// best = 0, none; desire[d] > best takes it
	float best = k_Zero;
	std::optional<int> index;
	for (size_t d = 0; d < k_Count; ++d)
	{
		if (best < desire.desire.at(d))
		{
			best = desire.desire.at(d);
			index = static_cast<int>(d);
		}
	}
	return index;
}

std::optional<int> GetMostSignificantRawDesire(const TownDesire& desire, float minimum)
{
	// sortedRaw[0].value < m (or NaN) -> none, else its index
	const auto& first = desire.sortedRaw.at(0);
	if (first.value < minimum || std::isnan(first.value) || std::isnan(minimum))
	{
		return std::nullopt;
	}
	return static_cast<int>(first.index);
}

std::optional<int> FindDesire(std::string_view name)
{
	// case-insensitive against the 17 names; none
	for (size_t d = 0; d < k_Count; ++d)
	{
		const std::string_view entry(k_DesireTable.at(d).name);
		if (entry.size() == name.size() && std::equal(entry.begin(), entry.end(), name.begin(), [](char a, char b) {
			    return std::tolower(static_cast<unsigned char>(a)) == std::tolower(static_cast<unsigned char>(b));
		    }))
		{
			return static_cast<int>(d);
		}
	}
	return std::nullopt;
}

// ---- the entity layer --------------------------------------------------------------------------------------------

namespace
{
constexpr std::array<DesireSort, k_Count> k_EmptyOrder {};

size_t Index(TownDesireInfo d)
{
	return static_cast<size_t>(static_cast<int>(d));
}

bool ValidDesire(TownDesireInfo d)
{
	return static_cast<int>(d) >= 0 && Index(d) < k_Count;
}
} // namespace

float GetDesire(entt::entity town, TownDesireInfo d)
{
	const auto* t = TownOf(town);
	return t != nullptr && ValidDesire(d) ? GetDesire(t->desire, Index(d)) : k_Zero;
}

float GetRawDesire(entt::entity town, TownDesireInfo d)
{
	const auto* t = TownOf(town);
	return t != nullptr && ValidDesire(d) ? GetRawDesire(t->desire, Index(d)) : k_Zero;
}

float TownNeedsSum(const TownDesire& desire)
{
	// The raw desires 13, 12, 9, 7, 6, 5, 4, 3, 1, 0 (no boosts), added in that order, each step rounded to float
	const auto& raw = desire.raw;
	float sum = raw.at(13);
	for (const size_t d : {12u, 9u, 7u, 6u, 5u, 4u, 3u, 1u, 0u})
	{
		sum += raw.at(d);
	}
	// x 0.2 (a float); < 0 -> 0, > 1 -> 1
	sum *= 0.2f;
	return sum < 0.0f ? 0.0f : sum > 1.0f ? 1.0f : sum;
}

float TownNeedsSum(entt::entity town)
{
	const auto* t = TownOf(town);
	return t != nullptr ? TownNeedsSum(t->desire) : 0.0f;
}

const std::array<DesireSort, k_Count>& GetSortedDesires(entt::entity town)
{
	const auto* t = TownOf(town);
	return t != nullptr ? t->desire.sorted : k_EmptyOrder;
}

const std::array<DesireSort, k_Count>& GetSortedRawDesires(entt::entity town)
{
	const auto* t = TownOf(town);
	return t != nullptr ? t->desire.sortedRaw : k_EmptyOrder;
}

float GetField(entt::entity town, TownDesireInfo d, Field field)
{
	const auto* t = TownOf(town);
	if (t == nullptr || !ValidDesire(d))
	{
		return k_Zero;
	}
	const auto i = Index(d);
	switch (field)
	{
	case Field::BoostA:
		return t->desire.boostA.at(i);
	case Field::Boost:
		return t->desire.boost.at(i);
	case Field::Desire:
		return t->desire.desire.at(i);
	case Field::Raw:
		return t->desire.raw.at(i);
	}
	return k_Zero;
}

float GetDesireSignificanceToVillager(entt::entity town, TownDesireInfo d)
{
	const auto* t = TownOf(town);
	if (t == nullptr || !ValidDesire(d))
	{
		return k_Zero;
	}
	return GetDesireSignificanceToVillager(t->desire, DesireInfo().at(Index(d)), Index(d));
}

std::optional<int> GetMostDesired(entt::entity town)
{
	const auto* t = TownOf(town);
	return t != nullptr ? GetMostDesired(t->desire) : std::nullopt;
}

std::optional<int> GetMostSignificantRawDesire(entt::entity town, float minimum)
{
	const auto* t = TownOf(town);
	return t != nullptr ? GetMostSignificantRawDesire(t->desire, minimum) : std::nullopt;
}

DesireInputs GatherInputs(entt::entity town)
{
	DesireInputs in;
	auto& registry = Entities();
	const auto* t = TownOf(town);
	if (t == nullptr)
	{
		return in;
	}
	in.stats = t->stats;
	if (const auto* magic = registry.TryGet<const TownMagic>(town); magic != nullptr)
	{
		in.worshipping = magic->worshipping; // read only
		in.onWayToWorship = magic->onWayToWorship;
	}
	in.homeless = static_cast<uint32_t>(t->homelessVillagers.size());
	const auto* tribe = registry.TryGet<const Tribe>(town);
	in.tribe = tribe != nullptr ? *tribe : Tribe::CELTIC; // (openblack, guard) every scripted town has its Tribe
	// the abode list and its count (the fields are abodes too)
	const auto abodes = town_stats::AbodesOf(town);
	in.abodeCount = static_cast<uint32_t>(abodes.size());
	const auto livingQuarters = static_cast<uint32_t>(AbodeType::LivingQuarters);
	for (const auto abode : abodes)
	{
		RepairInput repair;
		repair.life = life::LifeOf(abode);
		const auto* info = town_stats::AbodeInfoOf(abode, in.tribe);
		repair.livingQuarters = info != nullptr && (static_cast<uint32_t>(info->abodeType) & livingQuarters) != 0;
		repair.inhabitants = static_cast<uint32_t>(registry.Get<Abode>(abode).inhabitants.size());
		repair.desireToBeRepaired = info != nullptr ? info->desireToBeRepaired : 0.0f;
		in.abodes.push_back(repair);
	}
	// the storage pit and its resources (StoragePitStore)
	if (const auto pit = town_queries::GetStoragePit(town); pit != entt::null)
	{
		in.storageFood = StoragePitStore::GetResource(pit, ResourceType::Food);
		in.storageWood = StoragePitStore::GetResource(pit, ResourceType::Wood);
	}
	// the temporary pots (ecs::town_stores): the food and wood available test the slot only (no availability check),
	// then read the resource (object_resources). (approximate) a slot whose entity is gone counts as empty (until
	// the town's process clears it, a recycled entity could be read)
	if (const auto pot = t->temporaryPots.at(0); pot != entt::null && registry.Valid(pot))
	{
		in.potFood = object_resources::GetResource(pot, ResourceType::Food);
	}
	if (const auto pot = t->temporaryPots.at(1); pot != entt::null && registry.Valid(pot))
	{
		in.potWood = object_resources::GetResource(pot, ResourceType::Wood);
	}
	// the building sites (head first) and the plans (oldest first): DesireInputsOf, in the list orders the float
	// sums keep
	auto sites = building_sites::DesireInputsOf(town);
	in.siteDesires = std::move(sites.siteDesires);
	in.siteBuilders = std::move(sites.siteBuilders);
	in.sitePlaces = std::move(sites.sitePlaces);
	in.planRepairDesires = std::move(sites.planRepairDesires);
	for (size_t i = 0; i < in.populationWhenNeeded.size(); ++i)
	{
		if (const auto* info = town_stats::FindAbodeInfo(in.tribe, static_cast<AbodeNumber>(i)); info != nullptr)
		{
			in.populationWhenNeeded.at(i) = info->populationWhenNeeded;
		}
	}
	// the town's player: always one in openblack (NEUTRAL when the script gave none)
	in.owner = t->owner;
	// (inferred) the local player is PLAYER_ONE
	in.isLocalPlayer = t->owner == PlayerNames::PLAYER_ONE;
	in.alignment = effects::alignment::Get(t->owner);
	in.tribalPower4 = magic::players::MagicOf(t->owner).tribalPower.at(4);
	const auto creche = town_queries::GetCreche(town);
	in.crecheFunctional = creche != entt::null && abode_queries::IsFunctional(creche);
	// TODO: the town's belief in its player is not ported (For_Wonder 0)
	in.belief = 0.0f;
	in.protection = t->protectionDesire;
	in.mercy = t->mercyDesire;
	// the visual time: the game's day / night clock (the single source)
	if (!Locator::dayNightClock::has_value())
	{
		std::fputs("ecs::town_desire: no day/night clock in the locator (Locator::dayNightClock)\n", stderr);
		std::abort();
	}
	in.visualHour = Locator::dayNightClock::value().Clock().GetVisualTime();
	in.turn = villager::CurrentTurn();
	return in;
}

void Process(entt::entity town)
{
	auto* t = TownOf(town);
	if (t == nullptr)
	{
		return;
	}
	const auto in = GatherInputs(town);
	const auto sink = SinkFor(town);
	const auto context = ContextFor(*t, in, &sink);
	const auto average = Process(t->desire, context);
	TraceTown(*t, t->desire, in.turn, average);
}

float CalculateDesireForFood(entt::entity town)
{
	const auto* t = TownOf(town);
	if (t == nullptr)
	{
		return k_Zero;
	}
	const auto in = GatherInputs(town);
	const auto sink = SinkFor(town);
	return DesireForFood(ContextFor(*t, in, &sink));
}

float FoodDesireValue(entt::entity town)
{
	const auto* t = TownOf(town);
	if (t == nullptr)
	{
		return k_Zero;
	}
	const auto in = GatherInputs(town);
	return DesireForFood(ContextFor(*t, in, nullptr));
}

float CallDesireFunctionNow(entt::entity town, TownDesireInfo d)
{
	auto* t = TownOf(town);
	if (t == nullptr || !ValidDesire(d))
	{
		return k_Zero;
	}
	const auto in = GatherInputs(town);
	const auto sink = SinkFor(town);
	return CallDesireFunction(t->desire, ContextFor(*t, in, &sink), Index(d));
}

uint32_t CheckVillagerNeededForTownDesire(entt::entity town, entt::entity villager, float trigger)
{
	const auto* t = TownOf(town);
	if (t == nullptr)
	{
		return 0;
	}
	const auto check = [villager](size_t d) -> uint32_t {
		const auto fn = k_DesireTable.at(d).checkSatisfy;
		return fn != nullptr ? fn(villager) : 0;
	};
	std::function<void(const ShareOutStep&)> trace;
	if (villager::TraceOn(villager))
	{
		trace = [villager](const ShareOutStep& s) {
			villager::Trace(villager, fmt::format("civic: t={:.4f} k={} d={} v={:.4f} tmp={:.4f} -> {}", s.threshold, s.k, s.d,
			                                      s.value, s.temporary, s.result));
		};
	}
	// pop of GetTemporaryDesireVillagerModification: adults + children
	const uint32_t population = t->stats.adults + t->stats.children;
	return CheckVillagerNeeded(t->desire, DesireInfo(), population, villager::IsChild(villager), trigger, check, trace);
}

void SetBoost(entt::entity town, TownDesireInfo d, float boost, bool resort)
{
	auto* t = TownOf(town);
	if (t == nullptr || !ValidDesire(d))
	{
		return;
	}
	t->desire.boost.at(Index(d)) = boost;
	if (resort)
	{
		SortDesires(t->desire); // order 1 only
	}
}

namespace
{
/// What a missing town's alignment turns are (Locator::townStateSystem)
std::array<int32_t, k_Count>& NoTownAlignmentTurns()
{
	if (!Locator::townStateSystem::has_value())
	{
		std::fputs("ecs::town_desire: no townStateSystem in the locator (Locator::townStateSystem)\n", stderr);
		std::abort();
	}
	return Locator::townStateSystem::value().NoTownAlignmentTurns();
}
} // namespace

std::array<int32_t, k_Count>& AlignmentTurns(entt::entity town)
{
	auto* t = TownOf(town);
	return t != nullptr ? t->desire.alignmentTurns : NoTownAlignmentTurns();
}

// ---- scripts -----------------------------------------------------------------------------------------------------

bool ScriptSetTownDesireBoost(entt::entity thing, int32_t desire, float boost, std::vector<std::string>* errors)
{
	auto* t = TownOf(thing);
	// no thing, or not a town -> "Thing not valid!"
	if (t == nullptr && errors != nullptr)
	{
		errors->emplace_back("Thing not valid!");
	}
	// desire >= 17 (signed), boost < -1 (or NaN), boost > 1 -> "Invalid Params"
	const bool rangeOk = desire < static_cast<int32_t>(k_Count) && !(boost < k_MinusOne || std::isnan(boost)) && boost <= k_One;
	if (!rangeOk && errors != nullptr)
	{
		errors->emplace_back("Invalid Params");
	}
	// both right -> boost[desire] = boost and SortDesires (order 1 only).
	// (approximate) desire < 0, which the original writes out of the array, is not written
	if (!rangeOk || t == nullptr || desire < 0)
	{
		return false;
	}
	t->desire.boost.at(static_cast<size_t>(desire)) = boost;
	SortDesires(t->desire);
	return true;
}

float ScriptGetDesire(int32_t d, const std::function<entt::entity()>& popObject, std::vector<std::string>* errors)
{
	// d out of [0, 17) -> "Invalid desire" and PUSH 0 without the second POP
	if (d < 0 || d >= static_cast<int32_t>(k_Count))
	{
		if (errors != nullptr)
		{
			errors->emplace_back("Invalid desire");
		}
		return k_Zero;
	}
	// POP the object; none -> "Object no longer valid"
	const auto thing = popObject();
	auto& registry = Entities();
	if (thing == entt::null || !registry.Valid(thing))
	{
		if (errors != nullptr)
		{
			errors->emplace_back("Object no longer valid");
		}
		return k_Zero;
	}
	// not a town -> PUSH 0 (no message)
	const auto* t = registry.TryGet<const Town>(thing);
	if (t == nullptr)
	{
		return k_Zero;
	}
	// PUSH GetRawDesire(d) (a float)
	return GetRawDesire(t->desire, static_cast<size_t>(d));
}

void MapTownDesireBoost(entt::entity town, std::string_view name, float value)
{
	// the town (found by the caller) and FindDesire(name); both -> boost[d] = value, no re-sort, no range check
	auto* t = TownOf(town);
	const auto d = FindDesire(name);
	if (t == nullptr || !d.has_value())
	{
		return;
	}
	t->desire.boost.at(static_cast<size_t>(*d)) = value;
}

void AddDesireEventHandlers(EventManager& manager)
{
	manager.AddHandler<events::TownDesireWarning>(PlayWarning);
}
} // namespace openblack::ecs::town_desire
