/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "VillagerDisciple.h"

#include <array>
#include <string>

#include <fmt/format.h>
#include <glm/gtc/constants.hpp>

#include "ECS/Components/LivingAction.h"
#include "ECS/Components/Town.h"
#include "ECS/Components/Villager.h"
#include "ECS/Registry.h"
#include "ECS/Systems/VillagerDiscipleJobsInterface.h"
#include "ECS/ToBeDeleted.h"
#include "ECS/Town/TownQueries.h"
#include "ECS/Villager/VillagerBuild.h"
#include "ECS/Villager/VillagerCore.h"
#include "ECS/Villager/VillagerFarmer.h"
#include "ECS/Villager/VillagerFisherman.h"
#include "ECS/Villager/VillagerSatisfy.h"
#include "ECS/Villager/VillagerTrace.h"
#include "ECS/VillagerAnimations.h"
#include "InfoConstants.h"
#include "Locator.h"

// The disciples (VillagerDisciple.h)

namespace openblack::ecs::villager
{
using namespace components;
namespace tq = town_queries;

namespace
{
/// The disciple table, as the original has it (13 records): {start state, reaction 0x18, marker, held at its job,
/// fetches wood, town desire, moves into town}
constexpr std::array<DiscipleInfo, k_DiscipleInfoCount> k_DiscipleInfos = {{
    {0, 0, 0, 0, 0, -1, 0},    // 0 NONE
    {0, 1, 1, 1, 0, 0, 1},     // 1 FARMER
    {0x31, 1, 1, 1, 1, 1, 0},  // 2 FORESTER: 49 FORESTER_ARRIVES_AT_FOREST
    {0, 1, 1, 1, 0, 0, 0},     // 3 FISHERMAN
    {0, 1, 1, 1, 1, 9, 1},     // 4 BUILDER
    {0xFE, 0, 1, 1, 0, 8, 1},  // 5 BREEDER: 254 BREEDER_JUST_LANDED
    {0, 1, 1, 1, 0, -1, 0},    // 6 PROTECTION
    {0xE2, 0, 1, 0, 0, -1, 0}, // 7 MISSIONARY: 226 MISSIONARY_DISCIPLE
    {0, 1, 1, 1, 1, -1, 1},    // 8 CRAFTSMAN
    {0xA3, 0, 1, 1, 0, -1, 0}, // 9 TRADER: 163 DECIDE_WHAT_TO_DO
    {0xA3, 0, 1, 0, 0, -1, 0}, // 10 CHANGE_HOUSE: 163
    {0x3A, 0, 0, 0, 0, -1, 0}, // 11 WORSHIP: 58 GOTO_WORSHIP_SITE_FOR_WORSHIP
    {0xF4, 0, 0, 0, 0, -1, 0}, // 12 FROM_VORTEX: 244 SCRIPT_IN_CROWD
}};

Registry& Entities()
{
	return Locator::entitiesRegistry::value();
}

Villager* VillagerOf(entt::entity villager)
{
	return Entities().TryGet<Villager>(villager);
}

/// The villager's town entity (entt::null without one)
entt::entity TownEntityOf(entt::entity villager)
{
	const auto* v = VillagerOf(villager);
	// (guard) the original reads the town link unchecked: this stands for the missing town unlinking
	if (v == nullptr || v->town == entt::null || !ecs::IsAvailable(v->town) || !Entities().AllOf<Town>(v->town))
	{
		return entt::null;
	}
	return v->town;
}

/// The town centre (Town::centre); entt::null without one
entt::entity CentreOf(entt::entity town)
{
	const auto& t = Entities().Get<const Town>(town);
	// (guard) the original tests the pointer only: an id openblack kept after the centre went is none
	return t.centre != entt::null && Entities().Valid(t.centre) ? t.centre : entt::null;
}

void TraceIf(entt::entity villager, const std::string& line)
{
	if (TraceOn(villager))
	{
		Trace(villager, line);
	}
}

/// The job call of a case: the disciple jobs service runs the type's check
uint32_t Job(entt::entity villager, uint8_t disciple)
{
	return Locator::villagerDiscipleJobs::value().DiscipleJob(villager, disciple);
}

/// DiscipleDecideWhatToDo's fallback
uint32_t Fallback(entt::entity villager)
{
	// Already in 221 DISCIPLE_NOTHING_TO_DO -> 0
	if (GetFinalState(villager) == VillagerStates::DiscipleNothingToDo)
	{
		return 0;
	}
	// Only a disciple held at its job
	const auto* v = VillagerOf(villager);
	if (v == nullptr || (v->flags & Villager::k_FlagDisciple) == 0 || !DiscipleHeldAtJob(v->discipleType))
	{
		return 0;
	}
	// SetDiscipleNothingToDo found a point -> 1
	return SetDiscipleNothingToDo(villager) != 0 ? 1 : 0;
}
} // namespace

/// (pending) the breeder (the flocks, the shepherd states): 0
uint32_t SetupBreederDisciple([[maybe_unused]] entt::entity villager)
{
	return 0;
}

/// (pending) the trader and its route: 0
uint32_t CheckTrader([[maybe_unused]] entt::entity villager)
{
	return 0;
}

// ---- the disciple table ------------------------------------------------------------------------------------------

const DiscipleInfo& GetDiscipleInfo(uint8_t disciple)
{
	return k_DiscipleInfos.at(disciple < k_DiscipleInfos.size() ? disciple : 0);
}

VillagerStates DiscipleStartState(uint8_t disciple)
{
	return static_cast<VillagerStates>(static_cast<uint8_t>(GetDiscipleInfo(disciple).startState));
}

bool DiscipleCreatesJobReaction(uint8_t disciple)
{
	return GetDiscipleInfo(disciple).createsJobReaction != 0;
}

uint32_t DiscipleMarker(uint8_t disciple)
{
	return GetDiscipleInfo(disciple).marker;
}

bool DiscipleHeldAtJob(uint8_t disciple)
{
	return GetDiscipleInfo(disciple).heldAtJob == 1;
}

bool DiscipleFetchesWood(uint8_t disciple)
{
	return GetDiscipleInfo(disciple).fetchesWood != 0;
}

int32_t DiscipleTownDesire(uint8_t disciple)
{
	return GetDiscipleInfo(disciple).townDesire;
}

bool DiscipleMovesIntoTown(uint8_t disciple)
{
	return GetDiscipleInfo(disciple).movesIntoTown != 0;
}

// ---- the disciple ------------------------------------------------------------------------------------------------

uint32_t SetVillagerDisciple(entt::entity villager, [[maybe_unused]] entt::entity thing, VillagerDisciple disciple,
                             [[maybe_unused]] int32_t h)
{
	// Outside 0..12 -> 0, nothing done
	const auto d = static_cast<int32_t>(disciple);
	if (d < 0 || d >= static_cast<int32_t>(k_DiscipleInfoCount))
	{
		return 0;
	}
	auto* v = VillagerOf(villager);
	if (v == nullptr)
	{
		return 0; // (guard) the original's villager is always one
	}
	const auto type = static_cast<uint8_t>(d);
	// With a town and a new disciple type the town's disciple counts move from the old type to the new (0 skipped).
	// (approximate) a no-op: openblack recomputes TownStats::disciples at each Town::Process (TownStats.cpp, by the
	// disciple flag and type), and no reader of the counts between two Processes is known
	villager::TraceFormatted(villager, "disciple: {} -> {}", static_cast<uint32_t>(v->discipleType),
	                         static_cast<uint32_t>(type));
	if (type != 0)
	{
		// A disciple, no longer a follower
		v->flags = static_cast<uint16_t>((v->flags & 0xFBFF) | Villager::k_FlagDisciple);
		// The 3D object is given the type's marker. (pending) what that shows (a disciple marker, inferred): nothing is
		// drawn
		v->discipleType = type;
		return 1;
	}
	// Neither a disciple nor a follower; the marker cleared (pending, as above)
	v->flags = static_cast<uint16_t>(v->flags & 0xF9FF);
	v->discipleType = 0;
	return 1;
}

uint32_t DiscipleDecideWhatToDo(entt::entity villager)
{
	const auto* v = VillagerOf(villager);
	if (v == nullptr)
	{
		return 0;
	}
	// One case per disciple type 1..12; any other -> the fallback
	const uint8_t disciple = v->discipleType;
	switch (disciple)
	{
	case 1:
	{
		// FARMER: the town's best field (the score is not read); no town or no field -> the fallback
		const auto town = TownEntityOf(villager);
		if (town == entt::null)
		{
			break;
		}
		float score = 0.0f;
		const auto field = FindBestField(town, villager, score);
		if (field == entt::null)
		{
			break;
		}
		// A field to harvest and more food carried than minFoodToShowGraphic (signed 16 bits) -> 31
		// GOTO_STORAGE_PIT_FOR_DROP_OFF; 1
		const auto food = VillagerOf(villager)->resourceHeld.at(0);
		if (FieldActivityOf(field) == 2 && food > static_cast<int16_t>(InfoOf(villager).minFoodToShowGraphic))
		{
			villager::TraceFormatted(villager, "disciple 1: harvest field, food {} -> 31", food);
			SetTopState(villager, VillagerStates::GotoStoragePitForDropOff);
			return 1;
		}
		// Becomes the field's farmer -> 1
		if (VillagerBecomesFarmer(villager, field) == 1)
		{
			TraceIf(villager, "disciple 1: farmer");
			return 1;
		}
		break;
	}
	case 2:
		// FORESTER: the wood desire check == 1 -> 1
		if (Job(villager, disciple) == 1)
		{
			return 1;
		}
		break;
	case 3:
		// FISHERMAN: looking for water == 1 -> 1
		if (Job(villager, disciple) == 1)
		{
			return 1;
		}
		break;
	case 4:
		// BUILDER: needed for a building == 1 -> 1
		if (Job(villager, disciple) == 1)
		{
			return 1;
		}
		break;
	case 5:
		// BREEDER: SetupBreederDisciple == 1 -> 1. (pending) not ported (0): the fallback, as the original takes it for
		// any other result
		if (Job(villager, disciple) == 1)
		{
			return 1;
		}
		break;
	case 6:
		// PROTECTION: the fallback itself
		break;
	case 7:
		// MISSIONARY: 0 at once, not even the fallback
		return 0;
	case 8:
		// CRAFTSMAN: the workshop supply check != 0 -> 1. (pending, vws) VillagerSatisfy's is still neutral (0): the
		// fallback
		if (Job(villager, disciple) != 0)
		{
			return 1;
		}
		break;
	case 9:
		// TRADER: CheckTrader == 1 -> 1. (pending) not ported (0): the fallback, as the original takes it for any other
		// result
		if (Job(villager, disciple) == 1)
		{
			return 1;
		}
		break;
	case 10:
		// CHANGE_HOUSE: no longer a disciple, whatever follows
		SetVillagerDisciple(villager, villager, VillagerDisciple::None, 0);
		// Moving into the closest abode -> 234 GO_HOME_AND_CHANGE and a call on the new player; 1. (pending) neither
		// the move nor the abode search is ported (CheckMoveHouse: VillagerInteract.h): taken as failed, the fallback,
		// which then answers 0 (the disciple flag was just cleared)
		TraceIf(villager, "disciple 10: CheckMoveHouse 0x757180 (pending)");
		break;
	case 11:
		// WORSHIP: 58 GOTO_WORSHIP_SITE_FOR_WORSHIP; 1
		SetTopState(villager, VillagerStates::GotoWorshipSiteForWorship);
		return 1;
	case 12:
		// FROM_VORTEX: the table's start state 244 SCRIPT_IN_CROWD, then no longer a disciple; 1
		SetTopState(villager, DiscipleStartState(disciple));
		SetVillagerDisciple(villager, entt::null, VillagerDisciple::None, 0);
		return 1;
	default:
		// 0 and 13..255: the fallback
		break;
	}
	return Fallback(villager);
}

// ---- 221 DISCIPLE_NOTHING_TO_DO ----------------------------------------------------------------------------------

uint32_t SetDiscipleNothingToDo(entt::entity villager)
{
	// No prayer point -> 0
	glm::ivec2 p(0);
	if (FindDisciplePrayerPos(villager, p) == 0)
	{
		return 0;
	}
	// Reset the countdown, then walk to the point into 221 (the walk's result unused); 1
	if (auto* action = Entities().TryGet<LivingAction>(villager))
	{
		action->turnsUntilStateChange = 0;
	}
	const auto m = tq::ToMetres(p);
	villager::TraceFormatted(villager, "disciple: nothing to do -> 221 ({:.1f}, {:.1f})", m.x, m.y);
	SetupMoveToWithHug(villager, m, VillagerStates::DiscipleNothingToDo);
	return 1;
}

uint32_t FindDisciplePrayerPos(entt::entity villager, glm::ivec2& out)
{
	// No town -> 0, out untouched
	const auto town = TownEntityOf(villager);
	if (town == entt::null)
	{
		return 0;
	}
	// No town centre -> out = the town's position; 1
	const auto centre = CentreOf(town);
	if (centre == entt::null)
	{
		out = tq::PosOf(town);
		return 1;
	}
	// The angle from the centre towards me
	const auto at = tq::PosOf(centre);
	const float toMe = tq::Get3DAngleFromXZ(at, tq::PosOf(villager));
	// Within +-45 degrees of my side. The original runs the FPU at float precision, so float arithmetic matches it
	// exactly
	const float angle = GameFloatRand(glm::half_pi<float>()) - glm::quarter_pi<float>() + toMe;
	// Up to 4 beyond the centre's 2D radius
	const float spread = GameFloatRand(4.0f);
	const float distance = tq::Get2DRadius(centre) + spread;
	// out = the centre + that offset; 1
	out = at + tq::GetPosFromAngle(angle, distance);
	return 1;
}

uint32_t DiscipleNothingToDo(LivingAction& action)
{
	const auto villager = Entities().ToEntity(action);
	// The town's build pulse (a building or resource pulse) -> a new random countdown. (pending) the original reads
	// the town without a null test; (guard) no town: no pulse
	const auto town = TownEntityOf(villager);
	if (town != entt::null && Entities().Get<const Town>(town).buildPulse != 0)
	{
		action.turnsUntilStateChange = static_cast<uint16_t>(GameRand(10));
	}
	// Count down; still > 0 (signed 16 bits) -> 1
	action.turnsUntilStateChange = static_cast<uint16_t>(static_cast<int16_t>(action.turnsUntilStateChange) - 1);
	if (static_cast<int16_t>(action.turnsUntilStateChange) > 0)
	{
		return 1;
	}
	// The clip not done -> wait one more turn; 1
	if (!ecs::VillagerAnimationDone(villager, action.turnsSinceStateChange))
	{
		action.turnsUntilStateChange = 1;
		return 1;
	}
	// Nothing found to do -> wait 300 turns; 1
	if (DiscipleDecideWhatToDo(villager) == 0)
	{
		action.turnsUntilStateChange = 300;
	}
	return 1;
}

uint32_t EnterDiscipleNothingToDo(LivingAction& action, [[maybe_unused]] VillagerStates final,
                                  [[maybe_unused]] VillagerStates next)
{
	const auto villager = Entities().ToEntity(action);
	// A town with a centre -> look at the centre; 1
	if (const auto town = TownEntityOf(villager); town != entt::null)
	{
		if (const auto centre = CentreOf(town); centre != entt::null)
		{
			LookAtPos(villager, tq::PosOf(centre), 2);
		}
	}
	return 1;
}
} // namespace openblack::ecs::villager
