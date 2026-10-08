/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "VillagerEmergency.h"

#include <string>

#include <fmt/format.h>
#include <glm/gtc/constants.hpp>

#include "ECS/Components/LivingAction.h"
#include "ECS/Components/Town.h"
#include "ECS/Components/Villager.h"
#include "ECS/Registry.h"
#include "ECS/Town/TownQueries.h"
#include "ECS/Villager/VillagerCore.h"
#include "ECS/Villager/VillagerHome.h"
#include "ECS/Villager/VillagerOriginalFns.h"
#include "ECS/Villager/VillagerScript.h"
#include "ECS/Villager/VillagerTrace.h"
#include "Locator.h"

// The villager side of a damaged building and of the town emergency (VillagerEmergency.h)

namespace openblack::ecs::villager
{
using namespace components;
namespace tq = town_queries;

namespace
{
/// GotoCongregateInTownAfterEmergency's ring: 0.025 a villager, 20 m spread, 10 m base
constexpr float k_PerVillager = 0.025f;
constexpr float k_RingSpread = 20.0f;
constexpr float k_RingBase = 10.0f;
/// CongregateInTownAfterEmergency: GameRand(12) in the emergency, GameRand(5) after it
constexpr uint32_t k_GoHomeOneIn = 12;
constexpr uint32_t k_BackToWorkOneIn = 5;

Registry& Entities()
{
	return Locator::entitiesRegistry::value();
}

Villager* VillagerOf(entt::entity villager)
{
	return Entities().TryGet<Villager>(villager);
}

std::string Xz(glm::ivec2 pos)
{
	const auto m = tq::ToMetres(pos);
	return fmt::format("({:.1f}, {:.1f})", m.x, m.y);
}

void TraceIf(entt::entity villager, const std::string& line)
{
	if (TraceOn(villager))
	{
		Trace(villager, line);
	}
}
} // namespace

// ---- the hit on a home -------------------------------------------------------------------------------------------

void SetStateWhenTappedOnAbode(entt::entity villager)
{
	auto* v = VillagerOf(villager);
	if (v == nullptr)
	{
		return;
	}
	// IsAvailable and inside its home -> else 0
	if (!IsAvailable(villager) || (v->flags & Villager::k_FlagAtHome) == 0)
	{
		TraceIf(villager, "tap: not inside");
		return;
	}
	// FindPosOutsideAbode(none) (its own home; GameFloatRand(1.5), then
	// GameFloatRand(pi/4))
	const auto pos = FindPosOutsideAbode(villager, entt::null);
	villager::TraceFormatted(villager, "tap: inside -> out {} prev 163", Xz(pos));
	// SetupAfterTapOnAbode(pos, 163); 1
	SetupAfterTapOnAbode(villager, pos, VillagerStates::DecideWhatToDo);
}

void SetupAfterTapOnAbode(entt::entity villager, glm::ivec2 pos, VillagerStates previous)
{
	auto* action = Entities().TryGet<LivingAction>(villager);
	if (action == nullptr || VillagerOf(villager) == nullptr)
	{
		return;
	}
	// The raw PREVIOUS (not the villager's SetState, so
	// no town modifier and no passing-state filter)
	action->states.at(static_cast<size_t>(LivingAction::Index::Previous)) = static_cast<uint8_t>(previous);
	// SetupMoveToPos(pos, 197): the
	// walk's exit of the at-home row (ExitAtHome) takes it out of the home
	SetupMoveToPos(villager, tq::ToMetres(pos), VillagerStates::AfterTapOnAbode);
	// The after-tap flag (read by GetOwnDesiresTrigger / CheckSatisfySleep, cleared by CheckNeededForTownDesire)
	auto* v = VillagerOf(villager);
	if (v != nullptr)
	{
		v->flags = static_cast<uint16_t>(v->flags | Villager::k_FlagAfterTapOnAbode);
	}
}

uint32_t AfterTapOnAbode(LivingAction& action)
{
	const auto villager = Entities().ToEntity(action);
	// PlayAnimThenSetState(PREVIOUS, 1); 1
	const auto previous = GetState(villager, LivingAction::Index::Previous);
	villager::TraceFormatted(villager, "197: yawn -> {}", static_cast<uint32_t>(previous));
	PlayAnimThenSetState(villager, previous);
	return 1;
}

// ---- the town emergency ------------------------------------------------------------------------------------------

bool ReactsToTownEmergency(entt::entity villager)
{
	// The row of GetFinalState & 0xFF, its town emergency answer
	const auto final = static_cast<size_t>(static_cast<uint32_t>(GetFinalState(villager)) & 0xFF);
	if (final >= k_TownEmergencyReaction.size())
	{
		return false; // (openblack, guard) no state 255
	}
	switch (k_TownEmergencyReaction.at(final))
	{
	case TownEmergencyReaction::Always:
		return true; // always reacts
	case TownEmergencyReaction::PreviousState:
		// The PREVIOUS state, non-zero
		return GetState(villager, LivingAction::Index::Previous) != VillagerStates::InvalidState;
	case TownEmergencyReaction::None:
	default:
		return false; // an empty slot: not called
	}
}

void CallToTownEmergency(entt::entity villager)
{
	if (VillagerOf(villager) == nullptr)
	{
		return;
	}
	const bool reacts = ReactsToTownEmergency(villager);
	if (TraceOn(villager))
	{
		const auto final = static_cast<uint32_t>(GetFinalState(villager)) & 0xFF;
		const auto row =
		    final < k_TownEmergencyReaction.size() ? k_TownEmergencyReaction.at(final) : TownEmergencyReaction::None;
		const auto previous = static_cast<uint32_t>(GetState(villager, LivingAction::Index::Previous));
		std::string kind = "none";
		if (row == TownEmergencyReaction::Always)
		{
			kind = "always";
		}
		else if (row == TownEmergencyReaction::PreviousState)
		{
			kind = fmt::format("previous({})", previous);
		}
		Trace(villager, fmt::format("emergency: row {} {} -> {}", final, kind, reacts ? "242" : "stay"));
	}
	// != 0 -> SetTopState(242)
	if (reacts)
	{
		SetTopState(villager, VillagerStates::GotoCongregateInTownAfterEmergency);
	}
}

float CongregationDistance(uint32_t townPopulation)
{
	// The u32 count x 0.025f
	float k = static_cast<float>(townPopulation) * k_PerVillager;
	// !(k < 1) -> 1
	if (!(k < 1.0f))
	{
		k = 1.0f;
	}
	// k x 20 + 10, stored as a float
	return k * k_RingSpread + k_RingBase;
}

uint32_t GotoCongregateInTownAfterEmergency(LivingAction& action)
{
	const auto villager = Entities().ToEntity(action);
	// GetTown (villager::GetTown, VillagerHome.h) -> else 0 (stays in 242, literal)
	const auto town = GetTown(villager);
	if (town == entt::null)
	{
		TraceIf(villager, "242: no town");
		return 0;
	}
	// The town's GetCongregationPos
	auto pos = tq::GetCongregationPos(town);
	const auto congregation = pos;
	// Adults + children (u32 add)
	const auto& stats = Entities().Get<const Town>(town).stats;
	const uint32_t n = stats.adults + stats.children;
	const float d = CongregationDistance(n);
	// GameFloatRand(2 pi)
	const float a = GameFloatRand(glm::two_pi<float>());
	// pos += GetPosFromAngle(a, d)
	pos += tq::GetPosFromAngle(a, d);
	villager::TraceFormatted(villager, "242: congregation {} n {} d {:.2f} a {:.4f} -> {}", Xz(congregation), n, d, a, Xz(pos));
	// SetupMoveToWithHug(pos, 243); 1
	SetupMoveToWithHug(villager, tq::ToMetres(pos), VillagerStates::CongregateInTownAfterEmergency);
	return 1;
}

uint32_t CongregateInTownAfterEmergency(LivingAction& action)
{
	const auto villager = Entities().ToEntity(action);
	// GetTown && IsInStateOfEmergency
	const auto town = GetTown(villager);
	const bool emergency = town != entt::null && tq::IsInStateOfEmergency(Entities().Get<const Town>(town));
	VillagerStates next = VillagerStates::GotoCongregateInTownAfterEmergency;
	uint32_t r = 0;
	if (emergency)
	{
		// GameRand(12): != 0 -> 242, 0 -> 36 GO_HOME
		r = GameRand(k_GoHomeOneIn);
		if (r == 0)
		{
			next = VillagerStates::GoHome;
		}
	}
	else
	{
		// GameRand(5): != 0 -> 242, 0 -> 163
		r = GameRand(k_BackToWorkOneIn);
		if (r == 0)
		{
			next = VillagerStates::DecideWhatToDo;
		}
	}
	villager::TraceFormatted(villager, "243: emergency {} r {} -> {}", emergency ? 1 : 0, r, static_cast<uint32_t>(next));
	// PlayAnimThenSetState(next, 1): TOP 23 / FINAL next, the 243 clip kept; 1
	PlayAnimThenSetState(villager, next);
	return 1;
}
} // namespace openblack::ecs::villager
