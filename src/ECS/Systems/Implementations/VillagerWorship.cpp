/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "VillagerWorship.h"

#include <algorithm>
#include <vector>

#include <glm/geometric.hpp>
#include <glm/gtx/vec_swizzle.hpp>
#include <glm/vec2.hpp>
#include <spdlog/spdlog.h>

#include "ECS/Components/LivingAction.h"
#include "ECS/Components/ReactionRecords.h"
#include "ECS/Components/TownMagic.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/Villager.h"
#include "ECS/Components/WallHug.h"
#include "ECS/Components/WorshipSite.h"
#include "ECS/Effects/Reactions.h"
#include "ECS/Life.h"
#include "ECS/LivingTurn.h"
#include "ECS/Registry.h"
#include "ECS/Systems/Implementations/VillagerTeleport.h"
#include "ECS/Systems/LivingActionSystemInterface.h"
#include "ECS/ToBeDeleted.h"
#include "ECS/Town/BuildingSites.h"
#include "ECS/Villager/VillagerBuild.h"
#include "ECS/Villager/VillagerCore.h"
#include "ECS/Villager/VillagerDeath.h"
#include "InfoConstants.h"
#include "Locator.h"
#include "Magic/Core/Spell.h"
#include "Magic/Objects/MagicTeleport.h"
#include "Worship/TownMagic.h"
#include "Worship/WorshipPercentage.h"
#include "Worship/WorshipSite.h"
#include "Worship/WorshipTrace.h"

using namespace openblack;
using namespace openblack::ecs;
using namespace openblack::ecs::components;

namespace
{
/// The arrive point is reached within 10 m
constexpr float k_ArriveDistance = 10.0f;
/// The hide point is reached within 1 m
constexpr float k_HideDistance = 1.0f;

/// The state's row of info.dat villagerStateTable
const GVillagerStateTableInfo* TableOf(VillagerStates state)
{
	const auto& table = Locator::infoConstants::value().villagerStateTable;
	const auto i = static_cast<size_t>(state);
	return i < table.size() ? &table[i] : nullptr;
}

/// For an exit shared by `a` and `b`: the next state leaves by the same exit function, or it is not a final state (a
/// move on the way)
bool IsStateExitFunctionSameAs(VillagerStates next, VillagerStates a, VillagerStates b)
{
	const auto* info = TableOf(next);
	return next == a || next == b || info == nullptr || info->isFinalState == 0;
}

/// (not named Registry: with `using namespace openblack::ecs` in scope, outside this anonymous namespace `Registry()`
/// would be a functional cast that builds an empty ecs::Registry instead of calling the helper)
auto& Entities()
{
	return Locator::entitiesRegistry::value();
}

entt::entity TownOf(entt::entity villager)
{
	const auto* component = Entities().TryGet<const Villager>(villager);
	// (guard) the original reads this link with no availability test
	if (component == nullptr || component->town == entt::null || !ecs::IsAvailable(component->town))
	{
		return entt::null;
	}
	return component->town;
}

/// The villager's worship site: its town's
entt::entity SiteOf(entt::entity villager)
{
	const auto town = TownOf(villager);
	if (town == entt::null)
	{
		return entt::null;
	}
	const auto* magic = Entities().TryGet<const TownMagic>(town);
	// (guard) the original reads this link with no availability test
	if (magic == nullptr || magic->worshipSite == entt::null || !ecs::IsAvailable(magic->worshipSite))
	{
		return entt::null;
	}
	return magic->worshipSite;
}

WorshipVillager& StateOf(entt::entity villager)
{
	auto& registry = Entities();
	if (auto* state = registry.TryGet<WorshipVillager>(villager); state != nullptr)
	{
		return *state;
	}
	return registry.Assign<WorshipVillager>(villager);
}

VillagerStates StateNow(entt::entity villager)
{
	const auto* action = Entities().TryGet<const LivingAction>(villager);
	if (action == nullptr)
	{
		return VillagerStates::InvalidState;
	}
	return Locator::livingActionSystem::value().VillagerGetState(*action, LivingAction::Index::Top);
}

void SetState(entt::entity villager, VillagerStates state)
{
	auto* action = Entities().TryGet<LivingAction>(villager);
	if (action != nullptr)
	{
		Locator::livingActionSystem::value().VillagerSetState(*action, LivingAction::Index::Top, state, false);
	}
}

/// The walk (the DECIDE_WHAT_TO_DO pattern: the WallHug goal and a fresh linear move)
void WalkTo(entt::entity villager, const glm::vec3& goal)
{
	auto& registry = Entities();
	auto* wallHug = registry.TryGet<WallHug>(villager);
	if (wallHug == nullptr)
	{
		return;
	}
	wallHug->goal = glm::xz(goal);
	wallHug->step = glm::vec2(0.0f);
	registry.Remove<MoveStateLinearTag, MoveStateOrbitTag, MoveStateExitCircleTag, MoveStateStepThroughTag,
	                MoveStateFinalStepTag, MoveStateArrivedTag>(villager);
	registry.Remove<WallHugObjectReference>(villager);
	registry.Assign<MoveStateLinearTag>(villager);
	StateOf(villager).walking = true;
}

/// true while the pathfinding still moves the villager (VillagerMoveToPos's test), after this turn's step. (approximate)
/// openblack walks inside 59 / 60 / 213, where the original walks in MOVE_TO_POS first (to the dance place for 60,
/// with a hug for 213) or inside the dance: the move-to step here
bool Walking(entt::entity villager)
{
	auto& registry = Entities();
	if (!StateOf(villager).walking)
	{
		return false;
	}
	living_turn::MoveToStep(villager);
	const bool moving =
	    registry.AnyOf<MoveStateLinearTag, MoveStateOrbitTag, MoveStateExitCircleTag, MoveStateStepThroughTag>(villager);
	if (!moving)
	{
		registry.Remove<MoveStateFinalStepTag, MoveStateArrivedTag>(villager);
		StateOf(villager).walking = false;
	}
	return moving;
}

float FlatDistance(const glm::vec3& a, const glm::vec3& b)
{
	return glm::distance(glm::xz(a), glm::xz(b));
}

glm::vec3 PositionOf(entt::entity villager)
{
	return Entities().Get<const Transform>(villager).position;
}

/// The desire for life: 1 - ((life - min(life, threshold)) / (1 - threshold))^2. The info row is always 0 here and
/// below: damageThresholdToGoHome (0.3) and chantLifeRate (5e-6) are the same in every villager row
float DesireForLife(entt::entity villager)
{
	const float threshold = Locator::infoConstants::value().villager.at(0).damageThresholdToGoHome;
	const float life = life::LifeOf(villager);
	const float over = (life - std::min(threshold, life)) / (1.0f - threshold);
	return 1.0f - over * over;
}

/// Into the site's count and list of villagers, and the town's worshippers
void AddVillagerToWorshipSite(entt::entity villager, entt::entity siteEntity)
{
	auto& state = StateOf(villager);
	if (!state.atSite)
	{
		worship::percentage::AddWorshipper(TownOf(villager));
		state.atSite = true;
		state.requestedGoHome = false;
	}
	// into the site's list of villagers (at the head), counted only when new
	auto& site = Entities().Get<WorshipSite>(siteEntity);
	if (std::ranges::find(site.villagers, villager) == site.villagers.end())
	{
		site.villagers.insert(site.villagers.begin(), villager);
		++site.villagersAtSite;
	}
}

/// With a count, off the list (if it is there) and the count one down
void RemoveVillagerFromWorshipCount(WorshipSite& site, entt::entity villager)
{
	if (site.villagersAtSite == 0)
	{
		return;
	}
	std::erase(site.villagers, villager);
	--site.villagersAtSite;
}

} // namespace

bool villager_worship::IsAtWorshipSite(entt::entity villager)
{
	const auto* state = Entities().TryGet<const WorshipVillager>(villager);
	return state != nullptr && state->atSite;
}

/// Public for the town's villager removal
void villager_worship::RemoveVillagerFromWorshipSite(entt::entity villager)
{
	auto& state = StateOf(villager);
	const auto town = TownOf(villager);
	if (town != entt::null && state.atSite)
	{
		worship::percentage::RemoveWorshipper(town);
	}
	if (const auto siteEntity = SiteOf(villager); siteEntity != entt::null)
	{
		// only when the villager is in the site's list
		auto& site = Entities().Get<WorshipSite>(siteEntity);
		if (std::ranges::find(site.villagers, villager) != site.villagers.end())
		{
			RemoveVillagerFromWorshipCount(site, villager);
		}
		// out of the dance group
		worship::site::RemoveDancer(siteEntity, villager);
	}
	state.atSite = false;
	state.dancing = false;
	state.requestedGoHome = false;
}

namespace
{

/// Off the site's go-home queue
void RemoveGoHomeRequest(entt::entity siteEntity, entt::entity villager)
{
	auto& state = StateOf(villager);
	if (!state.requestedGoHome || siteEntity == entt::null)
	{
		return;
	}
	auto& queue = Entities().Get<WorshipSite>(siteEntity).goHomeRequests;
	const auto it = std::ranges::find(queue, villager);
	if (it == queue.end())
	{
		return;
	}
	state.requestedGoHome = false;
	queue.erase(it);
}

/// Into the go-home queue (sorted by the desire for life, the highest first), and another villager sent
/// (AdjustWorshipersWorshipping(1, 0, 1))
void RequestGoHome(entt::entity siteEntity, entt::entity villager)
{
	auto& state = StateOf(villager);
	auto& queue = Entities().Get<WorshipSite>(siteEntity).goHomeRequests;
	if (state.requestedGoHome || std::ranges::find(queue, villager) != queue.end())
	{
		return;
	}
	const float desire = DesireForLife(villager);
	const auto at = std::ranges::find_if(queue, [&](entt::entity other) { return DesireForLife(other) < desire; });
	queue.insert(at, villager);
	state.requestedGoHome = true;
	if (const auto town = TownOf(villager); town != entt::null)
	{
		worship::percentage::AdjustWorshipersWorshipping(town, 1, false, true);
	}
}

/// Below damageThresholdToGoHome (0.3) it asks to go home; above, it stops asking
void CheckRequestGoHome(entt::entity villager)
{
	const auto siteEntity = SiteOf(villager);
	if (siteEntity == entt::null)
	{
		return;
	}
	const float threshold = Locator::infoConstants::value().villager.at(0).damageThresholdToGoHome;
	if (life::LifeOf(villager) < threshold)
	{
		if (!StateOf(villager).requestedGoHome)
		{
			RequestGoHome(siteEntity, villager);
		}
	}
	else if (StateOf(villager).requestedGoHome)
	{
		RemoveGoHomeRequest(siteEntity, villager);
	}
}

/// The town's centre is functional and built. (inferred) both tests are skipped: a town centre that exists counts as
/// ready
bool TownCentreReady(entt::entity town)
{
	return worship::town::TownCentreOf(town) != entt::null;
}

/// 1 when the site is within the town info's maxDistanceThatVillagersWillGoToWorship (flat distance, <=). Farther,
/// the villager's player looks for a teleport stone that shortens the trip (teleport::FindRouteStone): 1 with that
/// stone in `routeStone`, 0 when it finds none. Without a player it is 1 with no stone.
bool CanIGetToTheWorshipSite(entt::entity villager, entt::entity siteEntity, entt::entity* routeStone = nullptr)
{
	const float maximum = Locator::infoConstants::value().town.maxDistanceThatVillagersWillGoToWorship;
	const auto at = PositionOf(villager);
	const auto site = Entities().Get<const Transform>(siteEntity).position;
	if (FlatDistance(at, site) <= maximum)
	{
		return true;
	}
	const auto player = villager_teleport::PlayerOf(villager);
	if (!player.has_value())
	{
		return true;
	}
	// the player's stone list, newest first
	const auto& stones = magic::teleport::StonesOf(*player);
	std::vector<glm::vec3> positions;
	positions.reserve(stones.size());
	for (const auto stone : stones)
	{
		positions.push_back(magic::teleport::MapPositionOf(stone));
	}
	const int index = magic::teleport::FindRouteStone(positions, magic::ToMap(at), magic::ToMap(site), maximum);
	if (index < 0)
	{
		return false;
	}
	if (routeStone != nullptr)
	{
		*routeStone = stones[static_cast<size_t>(index)];
	}
	return true;
}

/// The reaction record of that type (components::ReactionRecords) gets the game turn; without one a new {type, turn}
/// record is appended at the tail, the head (the oldest) dropped first when the list already holds 3. Unlike
/// reactions::Records it forgets nothing older than 1800 turns, and unlike StopReacting's RefreshRecord it adds
void SetReactionDoneWhen(entt::entity living, uint8_t type)
{
	auto& registry = Entities();
	auto& memory = registry.AllOf<ReactionRecords>(living) ? registry.Get<ReactionRecords>(living)
	                                                       : registry.Assign<ReactionRecords>(living);
	const auto now = effects::reactions::Turn();
	for (uint8_t i = 0; i < memory.count; ++i)
	{
		if (memory.records[i].type == type)
		{
			memory.records[i].turn = now;
			return;
		}
	}
	if (memory.count >= memory.records.size())
	{
		std::copy(memory.records.begin() + 1, memory.records.end(), memory.records.begin());
		--memory.count;
	}
	memory.records[memory.count++] = {type, now};
}

/// One more dancer on the way and the on-way flag, the walk to the arrive point (state 59), the town's count of
/// villagers on the way
bool GotoWorshipSiteForWorship(entt::entity villager)
{
	const auto siteEntity = SiteOf(villager);
	const auto town = TownOf(villager);
	if (siteEntity == entt::null)
	{
		return false;
	}
	auto& site = Entities().Get<WorshipSite>(siteEntity);
	++site.dancersOnWay;
	auto& state = StateOf(villager);
	state.onWay = true;
	// with a footpath the original walks on it to the arrive point (59); else it only sets 59
	const auto arrive = worship::site::GetSpecialPos(siteEntity, worship::site::Point::Arrive);
	SetState(villager, VillagerStates::ArrivesAtWorshipSiteForWorship);
	if (arrive)
	{
		WalkTo(villager, *arrive);
	}
	if (town != entt::null)
	{
		worship::percentage::AddVillagerOnWay(town, villager);
		StateOf(villager).onWayInTown = true;
	}
	if (worship::trace::Enabled())
	{
		SPDLOG_LOGGER_INFO(spdlog::get("ai"), "Worship trace: villager {} goes to worship site {}",
		                   static_cast<uint32_t>(villager), static_cast<uint32_t>(siteEntity));
	}
	return true;
}

/// Into the dance group, to its dance place (state 60)
bool StartWorshippingAtWorshipSite(entt::entity villager)
{
	const auto siteEntity = SiteOf(villager);
	if (siteEntity == entt::null)
	{
		return false;
	}
	worship::site::AddDancer(siteEntity, villager); // finds the dance group
	StateOf(villager).dancing = true;
	SetState(villager, VillagerStates::WorshippingAtWorshipSite);
	WalkTo(villager, worship::site::DancePosition(siteEntity, villager)); // to the dance place, then 60
	AddVillagerToWorshipSite(villager, siteEntity);
	return true;
}

/// To the site's hide point (state 213)
bool StartHidingAtWorshipSite(entt::entity villager)
{
	const auto siteEntity = SiteOf(villager);
	if (siteEntity == entt::null)
	{
		return false;
	}
	SetState(villager, VillagerStates::HidingAtWorshipSite);
	if (const auto hide = worship::site::GetSpecialPos(siteEntity, worship::site::Point::Hide); hide)
	{
		WalkTo(villager, *hide);
	}
	AddVillagerToWorshipSite(villager, siteEntity);
	return true;
}

/// The site gone or not the town's player's -> 163; fewer needed and first in the go-home queue (an empty queue counts
/// as first, read) -> 248. 1 when it left.
bool CheckVillagerGoBackToTownFromWorship(entt::entity villager)
{
	const auto town = TownOf(villager);
	if (town == entt::null)
	{
		return false;
	}
	const auto siteEntity = SiteOf(villager);
	if (siteEntity == entt::null || !TownCentreReady(town) ||
	    Entities().Get<const WorshipSite>(siteEntity).player != worship::town::OwnerOf(town))
	{
		SetState(villager, VillagerStates::DecideWhatToDo);
		return true;
	}
	if (worship::percentage::GetWorshipersNeeded(town, false, false, nullptr) >= 0)
	{
		return false;
	}
	const auto& queue = Entities().Get<const WorshipSite>(siteEntity).goHomeRequests;
	if (!queue.empty() && queue.front() != villager)
	{
		return false;
	}
	// SetTopState(248) == 1 -> 1. Its code, not the TOP: a pause first (239 with FINAL 248) also returns 1 and the
	// villager has left
	return ecs::villager::SetTopState(villager, VillagerStates::GoHomeFromWorship) == ecs::villager::k_Done;
}

/// life - chantDamage x chantLifeRate; at 0 VillagerDead (reason 4, worship). 0x21 when it died.
uint32_t ReduceVillagerLifeByChant(entt::entity villager)
{
	const auto siteEntity = SiteOf(villager);
	if (siteEntity == entt::null)
	{
		return 1;
	}
	const auto& site = Entities().Get<const WorshipSite>(siteEntity);
	const float rate = Locator::infoConstants::value().villager.at(0).chantLifeRate;
	life::ReduceLife(villager, site.chantDamage * rate);
	if (life::LifeOf(villager) > 0.0f)
	{
		return 1;
	}
	// VillagerDead(4 CHANT, town ? town's player : none, chantDamage x chantLifeRate, 1) (ECS/Villager/
	// VillagerDeath.h); 0x21. The worship-death count is the town's TownDeaths::byReason[4], and the site lets the
	// villager go through the worship state's exit (dying sets the top state 14)
	ecs::villager::VillagerDead(villager, DeathReason::Chant, ecs::villager::GetPlayerOf(villager), site.chantDamage * rate, 1);
	return 0x21;
}

/// A hungry villager eats from the site's food pot (GetFoodAtWorshipSite 241). openblack's villagers have no
/// belly yet: never.
bool CheckAllowedToRestAtWorshipSite(entt::entity /*villager*/)
{
	return false;
}

/// 0x23 when it stops (left, died, or went to eat), else 1
uint32_t ProcessInWorship(entt::entity villager)
{
	if (CheckVillagerGoBackToTownFromWorship(villager))
	{
		return 0x23;
	}
	CheckRequestGoHome(villager);
	if (ReduceVillagerLifeByChant(villager) == 0x21)
	{
		return 0x23;
	}
	return CheckAllowedToRestAtWorshipSite(villager) ? 0x23 : 1;
}

entt::entity EntityOf(LivingAction& action)
{
	return Entities().ToEntity(action);
}
} // namespace

bool villager_worship::CheckNeededForWorship(entt::entity villager)
{
	if (StateOf(villager).atSite)
	{
		if (!StartWorshippingAtWorshipSite(villager))
		{
			RemoveVillagerFromWorshipSite(villager);
			return false;
		}
		return true;
	}
	const auto town = TownOf(villager);
	if (town == entt::null || worship::percentage::GetWorshipPercentage(town) == 0.0f)
	{
		return false;
	}
	bool reachable = true;
	if (worship::percentage::GetWorshipersNeeded(town, true, true, &reachable) <= 0)
	{
		return false;
	}
	return CheckWorshipActivity(villager, reachable);
}

bool villager_worship::CheckWorshipActivity(entt::entity villager, bool requireReachable)
{
	const auto town = TownOf(villager);
	const auto siteEntity = SiteOf(villager);
	if (siteEntity == entt::null || town == entt::null || !TownCentreReady(town))
	{
		return false;
	}
	if (Entities().Get<const WorshipSite>(siteEntity).player != worship::town::OwnerOf(town))
	{
		return false;
	}
	entt::entity routeStone = entt::null;
	if (!CanIGetToTheWorshipSite(villager, siteEntity, &routeStone) && requireReachable)
	{
		return false;
	}
	// the worship site still to be built (its building site in the town's list) -> no GotoWorshipSiteForWorship. Its
	// site is the villager's (not null here: the citadel's site creation is not reached); without a building site in the
	// list 0; with one, SetTopState(163), then SetupBuildingObject == 1 -> 1
	bool building = false;
	if (const auto buildingSite = building_sites::GetBuildingSiteInList(town, siteEntity); buildingSite != entt::null)
	{
		ecs::villager::SetTopState(villager, VillagerStates::DecideWhatToDo);
		building = ecs::villager::SetupBuildingObject(villager, buildingSite) == 1;
	}
	if (!building && !GotoWorshipSiteForWorship(villager))
	{
		return false;
	}
	// when a teleport stone is what makes the site reachable, the walk goes through it. The REACT_TO_TELEPORT record
	// gets the turn (SetReactionDoneWhen: the record's turn, added when there is none, so the villager will not take
	// another teleport reaction for a while), the villager leaves the reactor list of the reaction it was following
	// (openblack keeps no reactor list in a Reaction, and SetupReactToTeleport replaces its slot anyway), and starting
	// the teleport reaction (which goes to SetupReactToTeleport) pushes GO_TOWARDS_TELEPORT_REACTION over the walk
	// that was just started; TELEPORT_REACTION jumps it and popping back puts it back on the way to the site
	if (routeStone != entt::null)
	{
		SetReactionDoneWhen(villager, static_cast<uint8_t>(openblack::Reaction::ReactToTeleport));
		villager_teleport::SetupReactToTeleport(villager, routeStone, magic::teleport::ReactionOf(routeStone));
		if (worship::trace::Enabled() || magic::teleport::TraceEnabled())
		{
			SPDLOG_LOGGER_INFO(
			    spdlog::get("ai"), "Worship trace: villager {} goes to worship site {} through teleport stone {}",
			    static_cast<uint32_t>(villager), static_cast<uint32_t>(siteEntity), static_cast<uint32_t>(routeStone));
		}
	}
	return true;
}

bool villager_worship::IsAvailableForWorshipSite(entt::entity villager, bool /*secondPass*/)
{
	// IsVillagerAvailable: not flagged, available for a state change, and the state's availability bit (the state
	// table's file field0xa8). The flag the first pass skips is not known: no openblack villager has it.
	const auto state = static_cast<size_t>(StateNow(villager));
	const auto& table = Locator::infoConstants::value().villagerStateTable;
	if (state >= table.size() || (table.at(state).field0xa8 & 1) == 0)
	{
		return false;
	}
	return !IsAtOrOnTheWayToWorshipSite(villager);
}

bool villager_worship::IsAtOrOnTheWayToWorshipSite(entt::entity villager)
{
	if (const auto* state = Entities().TryGet<const WorshipVillager>(villager); state != nullptr && state->atSite)
	{
		return true;
	}
	const auto now = StateNow(villager);
	return now == VillagerStates::ArrivesAtWorshipSiteForWorship || now == VillagerStates::ArrivesAtWorshipSiteWithSupplies;
}

void villager_worship::SendBackToTown(entt::entity villager)
{
	SetState(villager, VillagerStates::DecideWhatToDo);
}

uint32_t villager_worship::GotoWorshipSiteForWorshipState(LivingAction& action)
{
	// this is also the state function of row 58 (VillagerOriginalFns.h): a villager that comes back to the walk (popped
	// back after a reaction state, which resumes 59 as 58) runs GotoWorshipSiteForWorship again, so it sets 59 and walks
	// to the arrive point once more. That is how the villager that used the teleport stones on the way to the site
	// (CheckWorshipActivity) goes on from the stone it came out of
	return GotoWorshipSiteForWorship(EntityOf(action)) ? 1 : 0;
}

uint32_t villager_worship::ArrivesAtWorshipSiteForWorship(LivingAction& action)
{
	const auto villager = EntityOf(action);
	if (Walking(villager))
	{
		return 0; // the footpath walk (the state is the move's final state)
	}
	const auto siteEntity = SiteOf(villager);
	if (siteEntity == entt::null)
	{
		SetState(villager, VillagerStates::DecideWhatToDo);
		return 0;
	}
	const auto arrive = worship::site::GetSpecialPos(siteEntity, worship::site::Point::Arrive);
	if (!arrive || FlatDistance(PositionOf(villager), *arrive) < k_ArriveDistance)
	{
		const auto& site = Entities().Get<const WorshipSite>(siteEntity);
		if (static_cast<uint32_t>(worship::site::DancerCount(site)) < worship::site::InfoOf(site).maxDancersVisible)
		{
			StartWorshippingAtWorshipSite(villager);
		}
		else
		{
			StartHidingAtWorshipSite(villager);
		}
		return 0;
	}
	WalkTo(villager, *arrive);
	return 0;
}

uint32_t villager_worship::WorshippingAtWorshipSite(LivingAction& action)
{
	const auto villager = EntityOf(action);
	auto& state = StateOf(villager);
	if (state.onWay)
	{
		// the on-way flag clears and the dancers on the way go down on the first turn
		state.onWay = false;
		if (const auto siteEntity = SiteOf(villager); siteEntity != entt::null)
		{
			auto& site = Entities().Get<WorshipSite>(siteEntity);
			site.dancersOnWay = std::max(0, site.dancersOnWay - 1);
		}
	}
	if (Walking(villager))
	{
		return 0; // the walk to the dance place, then 60
	}
	if (ProcessInWorship(villager) == 1)
	{
		// the dance (60): the dance clip (VillagerAnimations maps 60 to it)
	}
	return 0;
}

uint32_t villager_worship::HidingAtWorshipSite(LivingAction& action)
{
	const auto villager = EntityOf(action);
	const auto siteEntity = SiteOf(villager);
	if (siteEntity == entt::null)
	{
		SetState(villager, VillagerStates::DecideWhatToDo);
		return 0;
	}
	if (Walking(villager))
	{
		return 0;
	}
	const auto hide = worship::site::GetSpecialPos(siteEntity, worship::site::Point::Hide);
	// (inferred) the + 1 m stands in for the arrival test: the WallHug stops within its arrive step of the point
	if (!hide || FlatDistance(PositionOf(villager), *hide) <= k_HideDistance + 1.0f)
	{
		ProcessInWorship(villager);
		return 0;
	}
	WalkTo(villager, *hide); // with a hug, then 213
	return 0;
}

bool villager_worship::ExitMoveToWorshipSite(LivingAction& action, VillagerStates next)
{
	// leaving for a state with another exit that is not a teleport reaction (states 201, 202, 251) -> off the town's
	// list of villagers on the way, the on-way flag cleared
	if (IsStateExitFunctionSameAs(next, VillagerStates::GotoWorshipSiteForWorship,
	                              VillagerStates::ArrivesAtWorshipSiteForWorship) ||
	    next == VillagerStates::GoTowardsTeleportReaction || next == VillagerStates::TeleportReaction ||
	    next == VillagerStates::GoTowardsTeleportReactionQuickly)
	{
		return false;
	}
	const auto villager = EntityOf(action);
	auto& state = StateOf(villager);
	// (approximate) the original reaches 60 / 213 through a move state (not a final one, so this exit does nothing then)
	// and WorshippingAtWorshipSite later clears the flag with the dancers on the way; openblack enters 60 / 213 at once,
	// so the flag is kept for them
	if (next != VillagerStates::WorshippingAtWorshipSite && next != VillagerStates::HidingAtWorshipSite)
	{
		state.onWay = false;
	}
	if (state.onWayInTown)
	{
		if (const auto town = TownOf(villager); town != entt::null)
		{
			worship::percentage::RemoveVillagerOnWay(town, villager);
		}
		state.onWayInTown = false;
	}
	return false;
}

bool villager_worship::ExitAtWorshipSite(LivingAction& action, VillagerStates next)
{
	// leaving for a state with another exit: off the go-home queue; for a state that is not a reactive one and not 241
	// (eating at the site) -> RemoveVillagerFromWorshipSite; else only off the site's count and out of the dance: it
	// stays counted in the town's worshippers
	if (IsStateExitFunctionSameAs(next, VillagerStates::WorshippingAtWorshipSite, VillagerStates::HidingAtWorshipSite))
	{
		return false;
	}
	const auto villager = EntityOf(action);
	const auto siteEntity = SiteOf(villager);
	RemoveGoHomeRequest(siteEntity, villager);
	const auto* info = TableOf(next);
	if ((info != nullptr && info->isReactionState != 0) || next == VillagerStates::GetFoodAtWorshipSite)
	{
		if (siteEntity != entt::null)
		{
			RemoveVillagerFromWorshipCount(Entities().Get<WorshipSite>(siteEntity), villager);
			worship::site::RemoveDancer(siteEntity, villager);
		}
		StateOf(villager).dancing = false;
		return false;
	}
	RemoveVillagerFromWorshipSite(villager);
	return false;
}
