/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The villager core's test hooks (docs/bw1-notes/villagers.md, section Test hooks): the trace and the
// OPENBLACK_TEST_VILLAGER_* environment variables. They are not part of the original.

#include <cstdio>
#include <cstdlib>

#include <algorithm>
#include <array>
#include <map>
#include <optional>
#include <string>
#include <vector>

#include <fmt/format.h>
#include <glm/vec3.hpp>
#include <spdlog/spdlog.h>

#include "Debug/DebugEnv.h"
#include "ECS/Abodes.h"
#include "ECS/Archetypes/VillagerArchetype.h"
#include "ECS/Components/Abode.h"
#include "ECS/Components/LivingAction.h"
#include "ECS/Components/Poisoned.h"
#include "ECS/Components/StoragePit.h"
#include "ECS/Components/Town.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/Villager.h"
#include "ECS/Components/WallHug.h"
#include "ECS/Life.h"
#include "ECS/ObjectCreationIndex.h"
#include "ECS/ObjectResources.h"
#include "ECS/PotResource.h"
#include "ECS/Registry.h"
#include "ECS/Systems/DebugHooksInterface.h"
#include "ECS/Systems/ScreenshotRequestSystemInterface.h"
#include "ECS/Town/BuildingSites.h"
#include "ECS/Town/TownEmergency.h"
#include "ECS/Town/TownQueries.h"
#include "ECS/Town/TownVillagers.h"
#include "ECS/Villager/VillagerCore.h"
#include "ECS/Villager/VillagerDecide.h"
#include "ECS/Villager/VillagerHome.h"
#include "ECS/Villager/VillagerResources.h"
#include "Game.h"
#include "InfoConstants.h"
#include "Locator.h"

namespace
{
/// What these hooks keep between calls, in the debug hooks' store (Locator::debugHooks)
struct VillagerDebugHooksState
{
	// OPENBLACK_TOWN_TRACE: each town's pots at the last trace. A map here, not a component on the town: trace-only
	// state that stays out of the game's components
	std::map<entt::entity, std::array<entt::entity, 2>> runDebugHooksLastPots {};
	// OPENBLACK_TEST_VILLAGER_SHOT: the screenshots still to take, read on the first call
	std::optional<std::vector<std::pair<uint32_t, std::string>>> runDebugHooksShots {};
	bool runDebugHooksHomesDumped {false}; // OPENBLACK_DUMP_VILLAGER_HOMES: the dump has been written
};

VillagerDebugHooksState& VillagerDebugHooksData()
{
	return openblack::Locator::debugHooks::value().Get<VillagerDebugHooksState>();
}
} // namespace

namespace openblack::ecs::villager
{
using namespace components;

namespace
{
/// OPENBLACK_VILLAGER_TRACE: unset -> off; "1" -> all (-1); else the creation index of one villager
std::optional<int64_t> TraceTarget()
{
	static const std::optional<int64_t> k_Target = []() -> std::optional<int64_t> {
		const char* value = std::getenv("OPENBLACK_VILLAGER_TRACE");
		if (value == nullptr || *value == '\0')
		{
			return std::nullopt;
		}
		const std::string text(value);
		if (text == "1")
		{
			return -1;
		}
		return std::atoll(value);
	}();
	return k_Target;
}

/// "<a>[,<n>]": the value and the creation index it applies to (none: all)
struct ValueFor
{
	std::string value;
	std::optional<int64_t> villager;
};

std::optional<ValueFor> ParseValueFor(const char* name)
{
	const char* raw = std::getenv(name);
	if (raw == nullptr || *raw == '\0')
	{
		return std::nullopt;
	}
	const std::string text(raw);
	const auto comma = text.find(',');
	ValueFor result {text.substr(0, comma), std::nullopt};
	if (comma != std::string::npos)
	{
		result.villager = std::atoll(text.substr(comma + 1).c_str());
	}
	return result;
}

bool Applies(const ValueFor& value, entt::entity villager)
{
	return !value.villager || object_index::Of(villager) == *value.villager;
}

std::vector<entt::entity> Villagers()
{
	std::vector<entt::entity> list;
	Locator::entitiesRegistry::value().Each<const Villager, const LivingAction>(
	    [&list](entt::entity entity, const Villager&, const LivingAction&) { list.push_back(entity); });
	return list;
}
} // namespace

bool TraceOn(entt::entity villager)
{
	const auto target = TraceTarget();
	if (!target)
	{
		return false;
	}
	return *target == -1 || object_index::Of(villager) == *target;
}

void Trace(entt::entity villager, const std::string& line)
{
	if (auto logger = spdlog::get("game"); logger != nullptr)
	{
		SPDLOG_LOGGER_INFO(logger, "Villager trace: {} (turn {}): {}", object_index::Of(villager), CurrentTurn(), line);
	}
}

void RunDebugHooks(uint32_t turn)
{
	auto& registry = Locator::entitiesRegistry::value();
	// OPENBLACK_TEST_VILLAGER_SHOT="<turn>,<path>[;<turn>,<path>...]": a screenshot at those game turns (one a turn)
	auto& hooks = VillagerDebugHooksData();
	if (!hooks.runDebugHooksShots.has_value())
	{
		hooks.runDebugHooksShots = [] {
			std::vector<std::pair<uint32_t, std::string>> list;
			const char* value = std::getenv("OPENBLACK_TEST_VILLAGER_SHOT");
			std::string text = value != nullptr ? value : "";
			size_t start = 0;
			while (start < text.size())
			{
				const auto end = std::min(text.find(';', start), text.size());
				const auto item = text.substr(start, end - start);
				if (const auto comma = item.find(','); comma != std::string::npos)
				{
					list.emplace_back(static_cast<uint32_t>(std::atoi(item.substr(0, comma).c_str())), item.substr(comma + 1));
				}
				start = end + 1;
			}
			return list;
		}();
	}
	auto& shots = *hooks.runDebugHooksShots;
	for (auto it = shots.begin(); it != shots.end(); ++it)
	{
		if (turn >= it->first && Locator::screenshotRequest::has_value())
		{
			Locator::screenshotRequest::value().Request(it->second);
			SPDLOG_LOGGER_INFO(spdlog::get("game"), "Villager test: screenshot at turn {} -> {}", turn, it->second);
			shots.erase(it);
			break;
		}
	}
	// OPENBLACK_DUMP_VILLAGER_HOMES ((openblack) a debug hook, off unless set): once, at the first turn, where the map
	// script put each villager (creation index, town id or -1, abode creation index or -1, vagrant), then the totals
	static const debug_env::Variable k_DumpVillagerHomes("OPENBLACK_DUMP_VILLAGER_HOMES");
	if (!hooks.runDebugHooksHomesDumped && k_DumpVillagerHomes.Get() != nullptr)
	{
		hooks.runDebugHooksHomesDumped = true;
		uint32_t total = 0;
		uint32_t housed = 0;
		uint32_t inTown = 0;
		uint32_t vagrants = 0;
		registry.Each<const Villager>([&](entt::entity entity, const Villager& v) {
			const auto* town = v.town != entt::null && registry.Valid(v.town) ? registry.TryGet<const Town>(v.town) : nullptr;
			const bool vagrant = town_villagers::IsVagrant(entity);
			++total;
			housed += v.abode != entt::null ? 1u : 0u;
			inTown += town != nullptr ? 1u : 0u;
			vagrants += vagrant ? 1u : 0u;
			SPDLOG_LOGGER_INFO(spdlog::get("game"), "Villager homes: {} town {} abode {}{}", object_index::Of(entity),
			                   town != nullptr ? static_cast<int64_t>(town->id) : int64_t {-1},
			                   v.abode != entt::null ? static_cast<int64_t>(object_index::Of(v.abode)) : int64_t {-1},
			                   vagrant ? " vagrant" : "");
		});
		SPDLOG_LOGGER_INFO(spdlog::get("game"), "Villager homes: {} villagers, {} with an abode, {} with a town, {} vagrants",
		                   total, housed, inTown, vagrants);
	}
	// OPENBLACK_TEST_VILLAGER_KILL="<reason 0-9>[,<n>[,<turn>]]": at turn 2 (or <turn>) VillagerDead(reason,
	// GetPlayerOf, its life, 1) of the villager with creation index n (empty or absent: all); the rest is the original's.
	// With OPENBLACK_TEST_CORPSE_TURNS=<n> the corpse's counter is n afterwards (test only)
	static const debug_env::Variable k_TestVillagerKill("OPENBLACK_TEST_VILLAGER_KILL");
	if (const char* kill = k_TestVillagerKill.Get(); kill != nullptr && *kill != '\0')
	{
		const std::string text(kill);
		std::vector<std::string> parts;
		size_t start = 0;
		while (start <= text.size())
		{
			const auto end = std::min(text.find(',', start), text.size());
			parts.push_back(text.substr(start, end - start));
			start = end + 1;
		}
		const auto reason = static_cast<DeathReason>(std::clamp(std::atoi(parts[0].c_str()), 0, 9));
		const std::optional<int64_t> who =
		    parts.size() > 1 && !parts[1].empty() ? std::optional<int64_t>(std::atoll(parts[1].c_str())) : std::nullopt;
		const auto at = parts.size() > 2 && !parts[2].empty() ? static_cast<uint32_t>(std::atoi(parts[2].c_str())) : 2u;
		if (turn == at)
		{
			for (const auto entity : Villagers())
			{
				if (registry.Valid(entity) && (!who || object_index::Of(entity) == *who))
				{
					VillagerDead(entity, reason, GetPlayerOf(entity), life::LifeOf(entity), 1);
					static const debug_env::Variable k_TestCorpseTurns("OPENBLACK_TEST_CORPSE_TURNS");
					if (const char* quick = k_TestCorpseTurns.Get(); quick != nullptr && IsDead(entity))
					{
						registry.Get<LivingAction>(entity).turnsUntilStateChange = static_cast<uint16_t>(std::atoi(quick));
					}
					Trace(entity, fmt::format("test: killed, reason {} -> top {} counter {}", static_cast<uint32_t>(reason),
					                          static_cast<uint32_t>(GetState(entity, Index::Top)),
					                          registry.Get<LivingAction>(entity).turnsUntilStateChange));
				}
			}
		}
	}
	if (turn == 2)
	{
		// OPENBLACK_TEST_VILLAGER_LIFE="<life>[,<n>]"
		if (const auto life = ParseValueFor("OPENBLACK_TEST_VILLAGER_LIFE"))
		{
			const float value = static_cast<float>(std::atof(life->value.c_str()));
			for (const auto entity : Villagers())
			{
				if (Applies(*life, entity))
				{
					life::SetLife(entity, value);
					Trace(entity, fmt::format("test: life set to {:.3f}", value));
				}
			}
		}
		// OPENBLACK_TEST_VILLAGER_FOOD="<food>[,<n>]": the belly
		if (const auto food = ParseValueFor("OPENBLACK_TEST_VILLAGER_FOOD"))
		{
			const float value = static_cast<float>(std::atof(food->value.c_str()));
			for (const auto entity : Villagers())
			{
				if (Applies(*food, entity))
				{
					registry.Get<Villager>(entity).food = value;
					Trace(entity, fmt::format("test: food set to {:.3f}", value));
				}
			}
		}
		// OPENBLACK_TEST_VILLAGER_NOTHING="<r>[,<n>]": the GameRand(9) of the next SetupNothingToDo
		if (const auto nothing = ParseValueFor("OPENBLACK_TEST_VILLAGER_NOTHING"))
		{
			const auto r = static_cast<uint32_t>(std::atoi(nothing->value.c_str()));
			for (const auto entity : Villagers())
			{
				if (Applies(*nothing, entity))
				{
					ForceNextNothingRoll(entity, r);
					Trace(entity, fmt::format("test: next SetupNothingToDo r={}", r));
				}
			}
		}
		// OPENBLACK_TEST_VILLAGER_AGE="<age>[,<n>]": the birth turn only, no meshes nor flags (to try 12 -> 13 and the
		// old age)
		if (const auto age = ParseValueFor("OPENBLACK_TEST_VILLAGER_AGE"))
		{
			const auto value = static_cast<uint32_t>(std::atoi(age->value.c_str()));
			for (const auto entity : Villagers())
			{
				if (Applies(*age, entity))
				{
					SetAgeBirthTurn(entity, value, turn);
					Trace(entity, fmt::format("test: age set to {}", value));
				}
			}
		}
		// OPENBLACK_TEST_HOMELESS=<n>: MakeHomeless of the villager n (129, then 36 without an abode)
		static const debug_env::Variable k_TestHomeless("OPENBLACK_TEST_HOMELESS");
		if (const char* homeless = k_TestHomeless.Get(); homeless != nullptr)
		{
			const auto n = std::atoll(homeless);
			for (const auto entity : Villagers())
			{
				if (object_index::Of(entity) == n)
				{
					const bool made = MakeHomeless(entity);
					Trace(entity, fmt::format("test: MakeHomeless = {}", made ? 1 : 0));
				}
			}
		}
		// OPENBLACK_TEST_VILLAGER_POISONED=<n>
		static const debug_env::Variable k_TestVillagerPoisoned("OPENBLACK_TEST_VILLAGER_POISONED");
		if (const char* poisoned = k_TestVillagerPoisoned.Get(); poisoned != nullptr)
		{
			const auto n = std::atoll(poisoned);
			for (const auto entity : Villagers())
			{
				if (object_index::Of(entity) == n && !registry.AllOf<Poisoned>(entity))
				{
					registry.Assign<Poisoned>(entity);
					Trace(entity, "test: poisoned");
				}
			}
		}
		// OPENBLACK_TEST_VILLAGER_CARRY="<food|wood>,<amount>[,<n>[,<tree 0-3>]]": villager::PickupResource for
		// villager n (or all), so the town's carried totals stay right; nothing else is forced
		static const debug_env::Variable k_TestVillagerCarry("OPENBLACK_TEST_VILLAGER_CARRY");
		if (const char* carry = k_TestVillagerCarry.Get(); carry != nullptr && *carry != '\0')
		{
			const std::string text(carry);
			std::vector<std::string> parts;
			size_t start = 0;
			while (start <= text.size())
			{
				const auto end = std::min(text.find(',', start), text.size());
				parts.push_back(text.substr(start, end - start));
				start = end + 1;
			}
			const auto type = !parts.empty() && parts[0] == "wood" ? ResourceType::Wood : ResourceType::Food;
			const auto amount = static_cast<int16_t>(parts.size() > 1 ? std::atoi(parts[1].c_str()) : 0);
			const std::optional<int64_t> who =
			    parts.size() > 2 && !parts[2].empty() ? std::optional<int64_t>(std::atoll(parts[2].c_str())) : std::nullopt;
			const auto tree = static_cast<uint8_t>(parts.size() > 3 ? std::atoi(parts[3].c_str()) : 0);
			for (const auto entity : Villagers())
			{
				if (!who || object_index::Of(entity) == *who)
				{
					PickupResource(entity, type, amount, tree);
					const auto& v = registry.Get<const Villager>(entity);
					Trace(entity,
					      fmt::format("test: carry {} {} tree {} (held {}/{})", type == ResourceType::Wood ? "wood" : "food",
					                  amount, tree, v.resourceHeld.at(0), v.resourceHeld.at(1)));
				}
			}
		}
		// OPENBLACK_TEST_VILLAGER_STATE="<state>[,<n>]": villager::SetTopState and its code
		if (const auto state = ParseValueFor("OPENBLACK_TEST_VILLAGER_STATE"))
		{
			const auto s = static_cast<VillagerStates>(std::atoi(state->value.c_str()));
			for (const auto entity : Villagers())
			{
				if (Applies(*state, entity) && registry.Valid(entity))
				{
					const auto code = SetTopState(entity, s);
					Trace(entity, fmt::format("test: SetTopState({}) = {:#x}, top {} final {}", static_cast<uint32_t>(s), code,
					                          static_cast<uint32_t>(GetState(entity, Index::Top)),
					                          static_cast<uint32_t>(GetState(entity, Index::Final))));
				}
			}
		}
		// OPENBLACK_TEST_BUILD_AT="<x>,<z>[,<desire>]": building_sites::ForceBuildingOfPlannedAtPos(MapCoords(x, 0, z),
		// desire x 5), what CHL BUILD_BUILDING does (desire 1 by default; the Land 1 temple: "1915.05,2508.89", (pending)
		// refused until abodes::BuildBy builds a CitadelHeart)
		static const debug_env::Variable k_TestBuildAt("OPENBLACK_TEST_BUILD_AT");
		if (const char* build = k_TestBuildAt.Get(); build != nullptr && *build != '\0')
		{
			float x = 0.0f;
			float z = 0.0f;
			float desire = 1.0f;
			if (std::sscanf(build, "%f,%f,%f", &x, &z, &desire) >= 2)
			{
				const map_coords::MapCoords pos {map_coords::ToFixed(x), map_coords::ToFixed(z), 0.0f};
				building_sites::ForceBuildingOfPlannedAtPos(pos, desire * 5.0f);
				SPDLOG_LOGGER_INFO(spdlog::get("game"), "Villager test: build at ({:.2f}, {:.2f}) desire {:.2f}", x, z,
				                   desire * 5.0f);
			}
		}
		// OPENBLACK_TEST_ABODE_LIFE="<index>,<life>[,<pit>]": the index-th abode that is not a storage pit
		// (the order of OPENBLACK_TEST_HIT_ABODE), or with pit = 1 the index-th storage pit, down to `life` through
		// abodes::ReduceLife(abode, life now - life, no player): the taps of the inhabitants
		// inside, the site, and for a pit or a town centre crossing 0.75 the town emergency, as a hit in the game
		static const debug_env::Variable k_TestAbodeLife("OPENBLACK_TEST_ABODE_LIFE");
		const char* abodeLife = k_TestAbodeLife.Get();
		if (abodeLife != nullptr && *abodeLife != '\0')
		{
			int index = 0;
			float value = 1.0f;
			int pit = 0;
			if (std::sscanf(abodeLife, "%d,%f,%d", &index, &value, &pit) >= 2)
			{
				std::optional<entt::entity> target;
				int seen = 0;
				registry.Each<const Abode, const Transform>([&](entt::entity e, const Abode&, const Transform&) {
					if (!target && registry.AllOf<StoragePit>(e) == (pit == 1) && seen++ == index)
					{
						target = e;
					}
				});
				if (target)
				{
					const float before = life::LifeOf(*target);
					const float after = abodes::ReduceLife(*target, before - value, std::nullopt);
					SPDLOG_LOGGER_INFO(spdlog::get("game"), "Villager test: abode {} life {:.4f} -> {:.4f} (site {})",
					                   static_cast<uint32_t>(*target), before, after,
					                   static_cast<uint32_t>(registry.Get<const Abode>(*target).buildingSite));
				}
			}
		}
		// OPENBLACK_TEST_TOWN_EMERGENCY="<town>": town_emergency::SetInStateOfEmergency on the town with that id
		// (CallAllVillagersToTownEmergency -> villager::CallToTownEmergency, housed villagers)
		static const debug_env::Variable k_TestTownEmergency("OPENBLACK_TEST_TOWN_EMERGENCY");
		const char* emergency = k_TestTownEmergency.Get();
		if (emergency != nullptr && *emergency != '\0')
		{
			const auto id = static_cast<uint32_t>(std::atoi(emergency));
			const auto& towns = registry.Context().towns;
			if (const auto it = towns.find(id); it != towns.end() && registry.Valid(it->second))
			{
				town_emergency::SetInStateOfEmergency(it->second);
				SPDLOG_LOGGER_INFO(spdlog::get("game"), "Villager test: town {} in a state of emergency (turn {})", id, turn);
			}
		}
		// OPENBLACK_TEST_VILLAGER_BORN_IN_WATER="x,z": a Celtic housewife of 25 there (VillagerArchetype::Create)
		static const debug_env::Variable k_TestVillagerBornInWater("OPENBLACK_TEST_VILLAGER_BORN_IN_WATER");
		if (const char* water = k_TestVillagerBornInWater.Get(); water != nullptr)
		{
			float x = 0.0f;
			float z = 0.0f;
			if (std::sscanf(water, "%f,%f", &x, &z) == 2)
			{
				const glm::vec3 position(x, 0.0f, z);
				const auto type = GVillagerInfo::Find(Tribe::CELTIC, VillagerNumber::Housewife);
				const auto entity = archetypes::VillagerArchetype::Create(position, position, type, 25, false);
				const auto* action = registry.TryGet<const LivingAction>(entity);
				Trace(entity,
				      fmt::format("test: born at ({:.1f}, {:.1f}): water {} -> state {}, counter {}", x, z,
				                  pot_resource::IsWater(position) ? 1 : 0, static_cast<uint32_t>(GetState(entity, Index::Top)),
				                  action != nullptr ? action->turnsUntilStateChange : 0));
			}
		}
	}
	// OPENBLACK_TOWN_TRACE: each town's temporary pots every 50 turns and when a slot changes, and
	// its storage pit's stock
	if (debug_env::TownTrace())
	{
		auto& s_LastPots = VillagerDebugHooksData().runDebugHooksLastPots;
		registry.Each<const Town>([&](entt::entity town, const Town& t) {
			auto& last = s_LastPots[town];
			const bool changed = last != t.temporaryPots;
			last = t.temporaryPots;
			if (!changed && turn % 50 != 0)
			{
				return;
			}
			const auto amount = [&registry](entt::entity pot, ResourceType type) -> int64_t {
				return pot != entt::null && registry.Valid(pot) ? static_cast<int64_t>(object_resources::GetResource(pot, type))
				                                                : int64_t {-1};
			};
			const auto food = t.temporaryPots.at(0);
			const auto wood = t.temporaryPots.at(1);
			SPDLOG_LOGGER_INFO(spdlog::get("game"), "Town trace: pots: town {} food {}/{} wood {}/{}", t.id,
			                   static_cast<uint32_t>(food), amount(food, ResourceType::Food), static_cast<uint32_t>(wood),
			                   amount(wood, ResourceType::Wood));
			// The building sites (head first) every 50 turns
			if (turn % 50 == 0)
			{
				const auto& sites = building_sites::SitesOf(town);
				std::string list;
				for (const auto site : sites)
				{
					const auto building = building_sites::GetBuilding(site);
					list += fmt::format(" [site {} building {} builders {}/{} pile {} pct {:.4f} repair {}]",
					                    static_cast<uint32_t>(site), static_cast<uint32_t>(building),
					                    building_sites::GetBuilderCount(site), building_sites::GetMaxBuilders(site),
					                    building_sites::GetResource(site, ResourceType::Wood),
					                    building != entt::null ? abodes::GetPercentBuilt(building) : 0.0f,
					                    building_sites::IsRepairSite(site) ? 1 : 0);
				}
				SPDLOG_LOGGER_INFO(spdlog::get("game"), "Town trace: sites: town {} n {}{}", t.id, sites.size(), list);
			}
			if (const auto pit = town_queries::GetStoragePit(town); pit != entt::null && turn % 50 == 0)
			{
				SPDLOG_LOGGER_INFO(spdlog::get("game"), "Town trace: pit: {} food {} wood {} pulse {}",
				                   static_cast<uint32_t>(pit), object_resources::GetResource(pit, ResourceType::Food),
				                   object_resources::GetResource(pit, ResourceType::Wood), t.buildPulse);
			}
		});
	}
	// the trace's summary every 100 turns
	if (TraceTarget() && turn % 100 == 0)
	{
		// Per town, who is inside, asleep (120), in a tent (238), and the homeless / vagrants lists
		registry.Each<const Town>([&](entt::entity town, const Town& t) {
			uint32_t inside = 0;
			uint32_t asleep = 0;
			uint32_t tents = 0;
			for (const auto entity : Villagers())
			{
				const auto& v = registry.Get<const Villager>(entity);
				if (v.town != town)
				{
					continue;
				}
				inside += (v.flags & Villager::k_FlagAtHome) != 0 ? 1 : 0;
				const auto top = GetState(entity, Index::Top);
				asleep += top == VillagerStates::SleepingAtHome ? 1 : 0;
				tents += top == VillagerStates::SleepInTent ? 1 : 0;
			}
			SPDLOG_LOGGER_INFO(spdlog::get("game"),
			                   "Villager trace: home: town {} inside {} asleep {} tents {} homeless {} vagrants {}", t.id,
			                   inside, asleep, tents, t.homelessVillagers.size(), town_villagers::Vagrants().size());
		});
		for (const auto entity : Villagers())
		{
			if (!TraceOn(entity))
			{
				continue;
			}
			const auto& v = registry.Get<const Villager>(entity);
			const auto& action = registry.Get<const LivingAction>(entity);
			// where it is and openblack's walk (the PathfindingSystem's move tag: L linear, O orbit, E exit circle, S
			// step through, F final step, A arrived, - none)
			const auto* transform = registry.TryGet<const components::Transform>(entity);
			const auto* wallHug = registry.TryGet<const components::WallHug>(entity);
			const char* walk = registry.AnyOf<components::MoveStateLinearTag>(entity)        ? "L"
			                   : registry.AnyOf<components::MoveStateOrbitTag>(entity)       ? "O"
			                   : registry.AnyOf<components::MoveStateExitCircleTag>(entity)  ? "E"
			                   : registry.AnyOf<components::MoveStateStepThroughTag>(entity) ? "S"
			                   : registry.AnyOf<components::MoveStateFinalStepTag>(entity)   ? "F"
			                   : registry.AnyOf<components::MoveStateArrivedTag>(entity)     ? "A"
			                                                                                 : "-";
			Trace(entity,
			      fmt::format("summary: life {:.6f} food {:.4f} top {} final {} previous {} tssc {} counter {} age {} "
			                  "at ({:.1f}, {:.1f}) goal ({:.1f}, {:.1f}) walk {}",
			                  v.life, v.food, action.states[0], action.states[1], action.states[2],
			                  action.turnsSinceStateChange, action.turnsUntilStateChange, GetAge(entity),
			                  transform != nullptr ? transform->position.x : 0.0f,
			                  transform != nullptr ? transform->position.z : 0.0f, wallHug != nullptr ? wallHug->goal.x : 0.0f,
			                  wallHug != nullptr ? wallHug->goal.y : 0.0f, walk));
		}
	}
}
} // namespace openblack::ecs::villager
