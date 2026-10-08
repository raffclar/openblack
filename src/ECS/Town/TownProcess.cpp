/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "TownProcess.h"

#include <cstdlib>

#include <optional>
#include <string>

#include <spdlog/spdlog.h>

#include "ECS/Abodes.h"
#include "ECS/Components/Field.h"
#include "ECS/Components/Influence.h"
#include "ECS/Components/TotemStatue.h"
#include "ECS/Components/Town.h"
#include "ECS/Fields.h"
#include "ECS/Influence/Influence.h"
#include "ECS/MapCells.h"
#include "ECS/Registry.h"
#include "ECS/Scaffolds.h"
#include "ECS/Town/AbodeVillagers.h"
#include "ECS/Town/BuildingSites.h"
#include "ECS/Town/TownBelief.h"
#include "ECS/Town/TownDesire.h"
#include "ECS/Town/TownEmergency.h"
#include "ECS/Town/TownStats.h"
#include "ECS/Town/TownStores.h"
#include "ECS/Town/TownVillagers.h"
#include "ECS/Town/Workshops.h"
#include "ECS/Villager/VillagerCore.h"
#include "InfoConstants.h"
#include "Locator.h"
#include "Worship/Citadel.h"
#include "Worship/SpellDispenser.h"
#include "Worship/WorshipPercentage.h"

// The town's turn and the players' loop (TownProcess.h)

namespace openblack::ecs::town_process
{
using namespace components;

namespace
{
/// The town centre's turn, after the site's Process the caller ran: the abode's villager part; the totem's turn; then
/// the town's no-influence flag cleared unless the centre's site still has an adjustable head scaffold
void TownCentreProcess(entt::entity centre)
{
	auto& registry = Locator::entitiesRegistry::value();
	abode_villagers::ProcessAbode(centre);
	// The centre's totem
	std::vector<entt::entity> totems;
	registry.Each<const TotemStatue>([&](entt::entity plinth, const TotemStatue& statue) {
		if (statue.townCentre == centre)
		{
			totems.push_back(plinth);
		}
	});
	for (const auto plinth : totems)
	{
		worship::percentage::ProcessTotem(plinth);
	}
	// The town and its no-influence flag
	const auto town = abode_villagers::TownOf(centre);
	auto* townInfluence = town != entt::null ? registry.TryGet<TownInfluence>(town) : nullptr;
	if (townInfluence == nullptr || !townInfluence->noInfluence)
	{
		return;
	}
	// The site's head scaffold still valid for placing in the hand -> keep the flag
	if (const auto site = abodes::GetBuildingSite(centre); site != entt::null)
	{
		const auto& scaffolds = building_sites::ScaffoldsOf(site);
		if (!scaffolds.empty() && scaffolds.front() != entt::null && scaffolds::ValidForPlaceInHand(scaffolds.front()))
		{
			return;
		}
	}
	// Clear the flag and force the influence update
	townInfluence->noInfluence = false;
	influence::ForceNeedUpdateInfluence();
}

constexpr uint32_t k_WorshipEvery = 10;

/// OPENBLACK_TEST_TOWN_DESIRE="<d>,<boost>[,<town id>]": at turn 2, SetBoost(d, boost) with the re-sort of
/// SET_TOWN_DESIRE_BOOST (order 1), on every town or on the one with that id. Not part of the original
void RunTestBoost(uint32_t turn)
{
	if (turn != 2)
	{
		return;
	}
	const char* value = std::getenv("OPENBLACK_TEST_TOWN_DESIRE");
	if (value == nullptr || *value == '\0')
	{
		return;
	}
	const std::string text(value);
	const auto first = text.find(',');
	if (first == std::string::npos)
	{
		return;
	}
	const int desire = std::atoi(text.substr(0, first).c_str());
	const auto second = text.find(',', first + 1);
	const float boost = std::strtof(text.substr(first + 1, second - first - 1).c_str(), nullptr);
	std::optional<uint32_t> only;
	if (second != std::string::npos)
	{
		only = static_cast<uint32_t>(std::strtoul(text.substr(second + 1).c_str(), nullptr, 10));
	}
	if (desire < 0 || desire >= static_cast<int>(town_desire::k_Count))
	{
		return;
	}
	auto& registry = Locator::entitiesRegistry::value();
	registry.Each<const Town>([&](entt::entity entity, const Town& town) {
		if (!only.has_value() || town.id == *only)
		{
			town_desire::SetBoost(entity, static_cast<TownDesireInfo>(desire), boost, true);
			if (auto logger = spdlog::get("game"); logger != nullptr)
			{
				SPDLOG_LOGGER_INFO(logger, "OPENBLACK_TEST_TOWN_DESIRE: town {} desire {} boost {:.3f}", town.id, desire,
				                   boost);
			}
		}
	});
}
} // namespace

void ProcessTown(entt::entity town)
{
	auto& registry = Locator::entitiesRegistry::value();
	auto* t = registry.TryGet<Town>(town);
	if (t == nullptr)
	{
		return;
	}
	const uint32_t turn = villager::CurrentTurn();

	// 1 The plan request flag of the turn cleared
	t->requestedPlanThisTurn = false;
	// 2 The building sites' pruning
	building_sites::PruneSites(town);
	t = registry.TryGet<Town>(town);
	if (t == nullptr)
	{
		return;
	}
	// 0 (openblack) the town stats of the turn from the entities (the original keeps them incrementally), after the
	// pruning (nothing between step 1 and the desires reads them; the pruned sites are out of them)
	t->stats = town_stats::Compute(town);
	// 3 The base influence; 4 every processAbodeEvery turns (unsigned modulo) each structure (newest first) runs its
	//   turn and, unless the town has no influence, adds its influence; 5 with a player the influence is scaled.
	//   (approximate) the influence part is influence::ProcessTowns (InfluenceSources.cpp), which influence::ProcessTurn
	//   runs every turn (see ProcessPlayers): here only the turn part, in the same order. ProcessAbode for the classes
	//   that do not override it; the Workshop, TownCentre and SpellDispenser overrides run below, each with its abode
	//   part, and the Field's too (its second caller; the first is ProcessFieldsTurn, every turn)
	if (const auto every = Locator::infoConstants::value().town.processAbodeEvery; every != 0 && turn % every == 0)
	{
		for (const auto abode : town_stats::AbodesOf(town))
		{
			// A workshop: its own part first, the abode's turn (the site's Process included) at its end:
			// ecs::workshops::Process does all of it
			if (workshops::IsWorkshop(abode))
			{
				workshops::Process(abode);
				continue;
			}
			// A field: its second caller (the first: ProcessFieldsTurn, every turn)
			if (registry.AllOf<Field>(abode))
			{
				ProcessField(abode, turn);
				continue;
			}
			// The abode's turn (and the town centre's, which runs it first) starts with its building site's Process.
			// (openblack) here for every abode, before abode_villagers::ProcessAbode's part
			if (const auto site = abodes::GetBuildingSite(abode); site != entt::null)
			{
				building_sites::Process(site);
			}
			if (abode_villagers::RunsAbodeProcess(abode))
			{
				abode_villagers::ProcessAbode(abode);
			}
			else if (abodes::TypeOf(abode) == AbodeType::TownCentre)
			{
				TownCentreProcess(abode);
			}
			else if (worship::dispenser::IsDispenser(abode))
			{
				// A spell dispenser: the abode's turn first, then its own part
				abode_villagers::ProcessAbode(abode);
				worship::dispenser::Process(abode);
			}
		}
	}
	// 6 The town's desires
	town_desire::Process(town);
	// 7 The town's artifacts' turn. TODO(artifacts)
	// 8 Every 10 turns: n = GetWorshipersNeeded(1, 0, null); n > 0 -> AdjustWorshipersWorshipping(n, 1, 0)
	//   (Worship/WorshipPercentage)
	if (turn % k_WorshipEvery == 0)
	{
		const int needed = worship::percentage::GetWorshipersNeeded(town, true, false, nullptr);
		if (needed > 0)
		{
			worship::percentage::AdjustWorshipersWorshipping(town, needed, true, false);
		}
	}
	// 9 A list of objects with a call state (call state 5 -> taken off and deleted; unavailable -> taken off).
	//   (inferred) only the constructor and the load write it: empty in a new game, not ported
	// 10 The football pitch's turn when football is enabled: out of the plan
	// 11 The town's spell icons' turn. TODO(miracles): confirm whether the icons' step covers it
	// 12 Each available desire flag: its turn and one of its fields set from another; else cleared.
	//    TODO(flags): openblack does not make the 7 TownDesireFlags (CREATE_TOWN only skips their index)
	// 13 The player interaction (Protection / Mercy from the aggression slots).
	//    TODO(aggression): they stay 0
	// 14 ProcessTownRepairs: at most one new site (a repair site or a rebuild plan's)
	building_sites::ProcessTownRepairs(town);
	// 15 ProcessTownEmergency (the fire through ecs::fire::IsOnFire)
	town_emergency::ProcessTownEmergency(town);
	t = registry.TryGet<Town>(town);
	if (t == nullptr)
	{
		return;
	}
	// 16 The town's attitude to the creature. TODO(creature)
	// 17 The temporary pots: available, empty and with a functional storage pit -> ToBeDeleted, cleared; not available
	//    -> cleared
	town_stores::ProcessTemporaryPots(town);
	// 18 The missionaries' turn (the unavailable ones taken off). TODO(miracles)
	// 19 Unconditional: the belief (folded per player, the boredom, the desires' cost, the neutral pin, the conversion;
	//    ecs::town_belief). The conversion ends the old owner's walk (ProcessPlayers)
	town_belief::Fold(town);
	// 20 With a player: the player's alignment from the town's desires (and the alignment history). TODO(miracles): no
	//    function yet
	// 21 The build pulse: a previous pulse clears the current one; the previous takes the current
	if (t->buildPulsePrevious != 0)
	{
		t->buildPulse = 0;
	}
	t->buildPulsePrevious = t->buildPulse;
	// 22 The empty town countdown: running -> down by one; at 0 SetTownEmpty (TODO); else with people -> 0. The writer
	//    that sets 50 (RemoveVillager) is TODO too
	if (t->emptyCountdown != 0)
	{
		--t->emptyCountdown;
		if (t->emptyCountdown == 0)
		{
			// TODO: the town's SetTownEmpty (its belief part is town_belief::SetTownEmpty)
		}
		else if (t->stats.adults + t->stats.children != 0)
		{
			t->emptyCountdown = 0;
		}
	}
	// 23 Not neutral: an influence change above 0.01 flags the drawn influence for an update. (inferred) the influence
	//    drawing recomputes it: nothing to call
	// 24 (20 x the town id + turn) % shuffleVillagersEvery == 0 -> ShuffleVillagersAroundAbodes (one move a call)
	if (auto* again = registry.TryGet<Town>(town); again != nullptr && town_villagers::ShuffleDue(*again, turn))
	{
		town_villagers::ShuffleVillagersAroundAbodes(town);
	}
}

void ProcessPlayers()
{
	const uint32_t turn = villager::CurrentTurn();
	RunTestBoost(turn);
	// ProcessTown's influence steps 3-5 are not called here: openblack runs them for every town in the influence turn
	// hook (influence::ProcessTurn -> influence::ProcessTowns, InfluenceSources.cpp, inside magic::ProcessTurn).
	// (approximate) so the influence of the turn is computed after the desires instead of just before each town's; the
	// desires do not read it
	// Each player runs its citadel, then each of its towns. The player's alignment comes after its towns (For_Children
	// reads the last turn's): ecs::effects::alignment::ProcessPlayers in magic::ProcessTurn
	// The slots 0..7, the neutral one last. A town taken over in its fold (step 19, it goes to the tail of the new
	// owner's list) ends the old owner's walk: its later towns wait for the next turn; the taken town comes again under
	// the new owner when that slot is later (always for the neutral one); TownsOf keeps the list's order
	// (Town::ownerListStamp), so the taken town is the new owner's last
	auto& registry = Locator::entitiesRegistry::value();
	for (uint8_t p = 0; p < static_cast<uint8_t>(PlayerNames::_COUNT); ++p)
	{
		const auto player = static_cast<PlayerNames>(p);
		// The player's citadel before its towns
		worship::citadel::Process(worship::citadel::Of(player));
		for (const auto town : map_cells::TownsOf(player))
		{
			ProcessTown(town);
			if (const auto* t = registry.TryGet<Town>(town); t != nullptr && t->owner != player)
			{
				break;
			}
		}
	}
}
} // namespace openblack::ecs::town_process
