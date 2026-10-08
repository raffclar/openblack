/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <cstddef>
#include <cstdint>

#include <array>
#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include <entt/entity/entity.hpp>

#include "ECS/Components/Town.h"
#include "Enums.h"
#include "InfoConstants.h"

namespace openblack
{
class EventManager;
} // namespace openblack

// The town's desires: the 17 desire functions of the table, Process, the two sorted orders (an unstable quicksort, as
// in the original), the share-out CheckVillagerNeededForTownDesire and the readers other systems use (audio, miracles).
//
// Two layers: the pure one (a DesireContext of plain values; what test/test_town_desire.cpp checks) and the entity
// one (GatherInputs from the ECS, then the pure functions). The original's game logic runs at single precision, so
// every add / multiply / divide rounds to float, as here (ints are exact as floats below 2^24).

namespace openblack::ecs::town_desire
{
constexpr size_t k_Count = 17;
using components::DesireSort;
using components::TownStats;

// ---- the pure layer ----------------------------------------------------------------------------------------------

/// AbodeDesireToBeRepaired's inputs of one abode of the town
struct RepairInput
{
	float life {1.0f};               ///< How repaired it is (its life)
	bool livingQuarters {false};     ///< The abode info's AbodeType & 2
	uint32_t inhabitants {0};        ///< The villagers living in it
	float desireToBeRepaired {0.0f}; ///< The abode info's desire to be repaired
};

/// What the desire functions read of the town, gathered once per Process (GatherInputs)
struct DesireInputs
{
	TownStats stats;            ///< (openblack: recomputed per turn, ecs::town_stats)
	int32_t worshipping {0};    ///< TownMagic::worshipping
	int32_t onWayToWorship {0}; ///< TownMagic::onWayToWorship
	uint32_t homeless {0};
	uint32_t abodeCount {0};
	/// The storage pit's food / wood, nullopt without one
	std::optional<uint32_t> storageFood;
	std::optional<uint32_t> storageWood;
	/// the temporary pots (Town::temporaryPots, ecs::town_stores): their food / wood, nullopt without one
	std::optional<uint32_t> potFood;
	std::optional<uint32_t> potWood;
	/// the building sites (head first: the newest first): their desire for villagers, builders and places
	/// (building_sites::DesireInputsOf)
	std::vector<float> siteDesires;
	std::vector<uint32_t> siteBuilders;
	std::vector<int32_t> sitePlaces;
	/// the abode list (newest first) for Repair_Town
	std::vector<RepairInput> abodes;
	/// the plans (oldest first) for Repair_Town: their desire to be repaired (the info's when set, else 0;
	/// building_sites::DesireInputsOf)
	std::vector<float> planRepairDesires;
	/// The tribe's GAbodeInfo PopulationWhenNeeded for the 16 abode numbers; nullopt: no record
	/// (the original would read a null record; openblack skips it)
	std::array<std::optional<int32_t>, 16> populationWhenNeeded {};
	/// The town's player, whose game stats are read; none without one (openblack's towns always have one, NEUTRAL)
	std::optional<PlayerNames> owner {PlayerNames::NEUTRAL};
	bool isLocalPlayer {false};    ///< The owner is the local player (inferred: owner == PLAYER_ONE)
	float alignment {0.0f};        ///< The player's alignment (ecs::effects::alignment::Get)
	float tribalPower4 {1.0f};     ///< The player's tribal power 4, 1 without a player (PlayerMagic::tribalPower)
	bool crecheFunctional {false}; ///< The town has a functional creche
	float belief {0.0f};           ///< The town's belief in its player. TODO: not ported, 0
	float protection {0.0f};       ///< From the player's interaction with the town. TODO(agresiones): 0
	float mercy {0.0f};            ///< Likewise
	float visualHour {12.0f};      ///< The visual time of day (DayNightClock::GetVisualTime)
	uint32_t turn {0};             ///< The game turn
	Tribe tribe {Tribe::CELTIC};   ///< (inferred: the original's tribe type is openblack's Tribe for TribeMultiplier)
};

/// The guidance calls of the desires (audio::guidance): which one and its value
enum class Warning
{
	VillagersUnhappy, ///< From Process: WarnVillagersUnhappy (value avg - 0.6, not used)
	LowOnFood,        ///< From DesireForFood: WarnLowOnFood(min(v, 2) - 0.9)
	LowOnWood,        ///< From DesireForWood: WarnLowOnWood(min(v, 2) - 1)
};
using WarningSink = std::function<void(Warning, float)>;

/// Everything a desire function reads: the town's TownDesire (the values of this turn for the desires already done,
/// of the last turn for the rest), the inputs and the info
struct DesireContext
{
	const components::TownDesire& desire;
	const DesireInputs& in;
	const GTownInfo& town;
	const std::array<GTownDesireInfo, 17>& info;
	/// GVillagerInfo[10] ("African Farmer Male") maxFoodCarried / maxWoodCarried: a fixed row in the original (literal
	/// oddity), 150 / 250 with info.dat
	uint32_t farmerMaxFood {150};
	uint32_t farmerMaxWood {250};
	const WarningSink* warn {nullptr}; ///< null: no guidance (FoodDesireValue, the tests that do not look)
};

using DesireFn = float (*)(const DesireContext&);
using AmountFn = uint32_t (*)(const DesireContext&);
using ModificationFn = float (*)(const DesireContext&, size_t d);
using CheckSatisfyFn = uint32_t (*)(entt::entity villager);

/// One entry of the desire function table
struct DesireFunctions
{
	const char* name;
	DesireFn function;           ///< The desire's value
	AmountFn amount;             ///< Debug trace only
	AmountFn desired;            ///< Debug trace only
	CheckSatisfyFn checkSatisfy; ///< Whether the villager would serve it
	ModificationFn modification; ///< Called with d
	bool children;               ///< A child may serve it (CheckVillagerNeededForTownDesire)
	bool flag64;                 ///< Its only reader is never called: unused
};
/// The table (TownDesire.cpp, the order of TownDesireInfo)
[[nodiscard]] const std::array<DesireFunctions, k_Count>& Table();

// The desire functions. R(d) = GetRawDesire, D(d) = GetDesire.
/// With the LowOnFood warning
[[nodiscard]] float DesireForFood(const DesireContext& c);
/// With the LowOnWood warning
[[nodiscard]] float DesireForWood(const DesireContext& c);
[[nodiscard]] float DesireForPlaytime(const DesireContext& c);
/// The town's protection
[[nodiscard]] float DesireForProtection(const DesireContext& c);
/// The town's mercy
[[nodiscard]] float DesireForMercy(const DesireContext& c);
[[nodiscard]] float DesireForAbodes(const DesireContext& c);
[[nodiscard]] float DesireForCivicBuildings(const DesireContext& c);
/// Always 0
[[nodiscard]] float DesireForSupplyWorship(const DesireContext& c);
[[nodiscard]] float DesireForChildren(const DesireContext& c);
[[nodiscard]] float DesireToBuild(const DesireContext& c);
/// Always 0
[[nodiscard]] float DesireForRain(const DesireContext& c);
[[nodiscard]] float DesireForSun(const DesireContext& c);
[[nodiscard]] float DesireToRepair(const DesireContext& c);
/// Always 0
[[nodiscard]] float DesireToSupplyWorkshop(const DesireContext& c);
[[nodiscard]] float DesireToBuildWonder(const DesireContext& c);
[[nodiscard]] float DesireForRelaxation(const DesireContext& c);
[[nodiscard]] float DesireForSleep(const DesireContext& c);

/// An abode's desire to be repaired
[[nodiscard]] float AbodeDesireToBeRepaired(const RepairInput& abode, const GTownInfo& town);

/// desire + boost + boostA
[[nodiscard]] float GetDesire(const components::TownDesire& desire, size_t d);
/// raw + boost + boostA
[[nodiscard]] float GetRawDesire(const components::TownDesire& desire, size_t d);

/// The entry's modification with d
[[nodiscard]] float GetDesireVillagerModification(const DesireContext& c, size_t d);
/// the general one: 1 - min(doingNow[d] / (adults + children + 1e-5), 1)
[[nodiscard]] float ModificationGeneral(const DesireContext& c, size_t d);
/// Food: 1 - min((150 doingNow[d] + 1e-4 + store food) / (desired food + 1e-4), 1)
[[nodiscard]] float ModificationFood(const DesireContext& c, size_t d);
/// Wood: 1 - min((250 doingNow[d] + 1e-4 + store wood) / (the maximum wood + 1e-4), 1)
[[nodiscard]] float ModificationWood(const DesireContext& c, size_t d);
/// To_Build: 1 - min((1e-4 + sum of the sites' builders) / (1e-4 + sum of their places), 1)
[[nodiscard]] float ModificationToBuild(const DesireContext& c, size_t d);
/// 1 - min(max(doingNow[k] - doingNowAtStart[k], 0) / (pop + 1e-5), 1), pop = adults + children
[[nodiscard]] float GetTemporaryDesireVillagerModification(const components::TownDesire& desire, uint32_t population, size_t k);

/// raw = f x TribeMultiplier[tribe]; returns clamp(raw x mod, -1, 1)
float CallDesireFunction(components::TownDesire& desire, const DesireContext& c, size_t d);
/// doingNow[d] >= 0, the copies doingNowAtStart / doingNowCountAtStart, Amount / Desired, desire = CallDesireFunction
void ProcessDesire(components::TownDesire& desire, const DesireContext& c, size_t d);
/// The original's quicksort (8 or fewer -> a selection sort, else the middle as pivot; not stable) with its comparator
/// (value: a < b -> 1, == -> 0, else -1: descending, NaN -> 1)
void MsvcQsort(std::array<DesireSort, k_Count>& entries);
/// order 1 (sorted): {boost + boostA, GetDesire, d} for d = 0..16, then MsvcQsort
void SortDesires(components::TownDesire& desire);
/// order 2 (sortedRaw): {boostA, GetRawDesire, d}, then MsvcQsort
void SortRawDesires(components::TownDesire& desire);
/// The population, the 17 desires in order 0..16, both orders, the average every 50 turns and its
/// WarnVillagersUnhappy. Returns the average when it was computed (the trace)
std::optional<float> Process(components::TownDesire& desire, const DesireContext& c);

/// One step of the share-out's loop, for the trace (OPENBLACK_VILLAGER_TRACE)
struct ShareOutStep
{
	size_t k;
	uint32_t d;
	float value;
	float temporary;
	float threshold;
	const char* result; ///< "skip(child)", "skip(nocs)", "cut", "cs=0", "cs=1"
};
/// CheckVillagerNeededForTownDesire on order 1 (the pure part): `checkSatisfy(d)` stands for the entry's CheckSatisfy
/// on the villager (only called for an entry that has one). 0 or 1
uint32_t CheckVillagerNeeded(const components::TownDesire& desire, const std::array<GTownDesireInfo, 17>& info,
                             uint32_t population, bool child, float trigger,
                             const std::function<uint32_t(size_t d)>& checkSatisfy,
                             const std::function<void(const ShareOutStep&)>& trace = {});

/// max(D(d) - info[d].desireTriggersVillagerAction, 0)
[[nodiscard]] float GetDesireSignificanceToVillager(const components::TownDesire& desire, const GTownDesireInfo& info,
                                                    size_t d);
/// The strict argmax of desire above 0 (no boosts); none when all are <= 0
[[nodiscard]] std::optional<int> GetMostDesired(const components::TownDesire& desire);
/// sortedRaw[0].value >= m ? sortedRaw[0].index : none
[[nodiscard]] std::optional<int> GetMostSignificantRawDesire(const components::TownDesire& desire, float minimum);
/// The name's index (case-insensitive, against the 17 names of the table); none when no name matches
[[nodiscard]] std::optional<int> FindDesire(std::string_view name);

// ---- the entity layer (the API for audio, miracles and the villagers) ---------------------------------------------

/// GetDesire / GetRawDesire of a town entity (0 without one)
[[nodiscard]] float GetDesire(entt::entity town, TownDesireInfo d);
[[nodiscard]] float GetRawDesire(entt::entity town, TownDesireInfo d);
/// For the villagers' state speed: S = 0.2 x (raw 13 + 12 + 9 + 7 + 6 + 5 + 4 + 3 + 1 + 0) (no boosts) clamped to
/// [0, 1], at float precision. 0 without a town
[[nodiscard]] float TownNeedsSum(const components::TownDesire& desire);
[[nodiscard]] float TownNeedsSum(entt::entity town);
/// Order 1 (sorted) and order 2 (sortedRaw, the one the guidance's town desire sounds read). An empty (all 0) array
/// without a town
[[nodiscard]] const std::array<DesireSort, k_Count>& GetSortedDesires(entt::entity town);
[[nodiscard]] const std::array<DesireSort, k_Count>& GetSortedRawDesires(entt::entity town);
/// The four arrays of TownDesire (the resource drop sound reads Raw + Boost + BoostA of 0, 1 and 10)
enum class Field
{
	BoostA, ///< Nobody writes it in a new game (inferred: only Load)
	Boost,  ///< SET_TOWN_DESIRE_BOOST, TOWN_DESIRE_BOOST
	Desire, ///< With the modification, in [-1, 1]
	Raw,    ///< Function x TribeMultiplier, not clamped
};
[[nodiscard]] float GetField(entt::entity town, TownDesireInfo d, Field field);
/// GetDesireSignificanceToVillager of a town entity
[[nodiscard]] float GetDesireSignificanceToVillager(entt::entity town, TownDesireInfo d);
[[nodiscard]] std::optional<int> GetMostDesired(entt::entity town);
[[nodiscard]] std::optional<int> GetMostSignificantRawDesire(entt::entity town, float minimum);
/// DesireForFood now, literal: with WarnLowOnFood when v >= 0.95 for the local player (a villager arriving at
/// food calls it too)
float CalculateDesireForFood(entt::entity town);
/// openblack: the same value without the warning (for readers)
[[nodiscard]] float FoodDesireValue(entt::entity town);
/// CallDesireFunction now, on a town entity (an abode removing or adding resources calls it before the change):
/// raw[d] is rewritten and the function's warning may play (WarnLowOnFood / WarnLowOnWood); returns
/// clamp(raw x modification, -1, 1). 0 without a town
float CallDesireFunctionNow(entt::entity town, TownDesireInfo d);
/// CheckVillagerNeeded for the town, the villager and its trigger: 0 or 1
uint32_t CheckVillagerNeededForTownDesire(entt::entity town, entt::entity villager, float trigger);
/// The scripts' boost (boost[d] = boost); `resort`: SortDesires (order 1 only, as SET_TOWN_DESIRE_BOOST)
void SetBoost(entt::entity town, TownDesireInfo d, float boost, bool resort);
/// Turns above desireAffectsAlignmentAfter: written only by the alignment by desires
[[nodiscard]] std::array<int32_t, k_Count>& AlignmentTurns(entt::entity town);

/// Process for a town entity (from the town's process; ecs::town_process)
void Process(entt::entity town);
/// The inputs of the pure layer from the ECS (the town's stats must be this turn's: town_process computes them)
[[nodiscard]] DesireInputs GatherInputs(entt::entity town);

// ---- scripts -----------------------------------------------------------------------------------------------------

/// The script command after its three POPs (boost, desire, thing): "Thing not valid!" without a town, "Invalid
/// Params" unless desire < 17 (signed) and -1 <= boost <= 1; both right: boost[desire] = boost and SortDesires (order 1
/// only). (approximate) a negative desire, which the original writes out of the array, is not written. Returns
/// whether it wrote; `errors` gets the messages
bool ScriptSetTownDesireBoost(entt::entity thing, int32_t desire, float boost, std::vector<std::string>* errors);
/// The script's GetDesire after its first POP (d): d out of [0, 17) -> "Invalid desire" and 0 without the second POP
/// (literal: the object stays on the stack); else `popObject()`, no thing -> "Object no longer valid" and 0, not a
/// town -> 0 (no message), else GetRawDesire(d)
float ScriptGetDesire(int32_t d, const std::function<entt::entity()>& popObject, std::vector<std::string>* errors);
/// The map command TOWN_DESIRE_BOOST: town and FindDesire(name) found -> boost[d] = value, no re-sort and no range
/// check
void MapTownDesireBoost(entt::entity town, std::string_view name, float value);

// ---- events ------------------------------------------------------------------------------------------------------

/// The game's handler of events::TownDesireWarning, the town's guidance call. Added once when the game starts
void AddDesireEventHandlers(EventManager& manager);
} // namespace openblack::ecs::town_desire
