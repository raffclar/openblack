/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <cstdint>

#include <array>
#include <vector>

#include <entt/entity/entity.hpp>

#include "Enums.h"

namespace openblack::ecs::components
{

struct Abode
{
	AbodeNumber type;
	/// The town, by its id (Town::id, a key of RegistryContext::towns)
	uint32_t townId;
	/// (openblack) no town: removing the structure from its town (when the abode is deleted, abodes::OnToBeDeleted)
	/// leaves the abode without a town; no town has this id
	static constexpr uint32_t k_NoTown = 0xFFFFFFFFu;
	// If a village does not have a ABODE_STORAGE_PIT then other abodes are used
	// by the villagers
	uint32_t foodAmount;
	uint32_t woodAmount;
	/// The villagers of the abode, the head first (a villager moving in is inserted at the head). Its order decides who
	/// moves in the shuffle (swapping a male for a female, taking a villager from it) and who is the pair at bed time.
	/// Changed only by ecs::abode_villagers
	std::vector<entt::entity> inhabitants;
	/// MaleFemaleVillagers[sex]: the first adult of each sex that moved in; removing a deleted villager clears both,
	/// removing a living one none (literal)
	std::array<entt::entity, 2> maleFemale {entt::null, entt::null};
	/// The empty abode's clock (+0.001 a processed turn, ReduceLife at 1)
	float emptyTimer {0.0f};
	/// AdultCount / AdultMaleCount / ChildCount (a villager moving in or out, a child growing up)
	uint8_t adultCount {0};
	uint8_t adultMaleCount {0};
	/// PresentAtHome: villagers inside now; only arriving home (++) and leaving home (--) change it. Lights the chimney
	/// smoke and the night windows
	uint8_t presentAtHome {0};
	uint8_t childCount {0};
	/// Counts up to 200 in each process of the abode; read when it stops being functional (>= 200 with a player: the
	/// player's statistic, not ported)
	uint8_t field0xB9 {0};

	// ---- the construction state ---------
	/// Under construction
	static constexpr uint32_t k_UnderConstruction = 0x2;
	/// Not repaired / a repair asked for (planned abodes, the town repairs, MakeFunctional = !IsRepaired; cleared when
	/// repaired)
	static constexpr uint32_t k_NotRepaired = 0x4;
	/// Built (made without underConstruction, or once built)
	static constexpr uint32_t k_Built = 0x8;
	/// (openblack) built by default: every abode made outside the plans (CREATE_ABODE, CREATE_TOWN_CENTRE, the fields,
	/// the spell dispenser) is whole; AbodeArchetype::Create(underConstruction) gives a plan's one bit 1
	uint32_t buildFlags {k_Built};
	/// PercentBuilt: 0 under construction, else the creation's percent ((inferred) 1 for the whole ones); building moves
	/// it, Built sets 1
	float percentBuilt {1.0f};
	/// The building site (set when the site is made; cleared when it is deleted): an entity with
	/// components::BuildingSite. The building is drawn as under construction while it is set
	entt::entity buildingSite {entt::null};
	/// The town statistics have counted it (when it is made functional, once). (openblack) true by default for the whole
	/// abodes (made functional when built); false for a plan's until it is made functional. town_stats::Compute counts
	/// only the abodes that have it
	bool addedToTownStats {true};
	/// ShouldNotBeAddedToPlanned: the building is not planned again when it goes. Set by the scaffolds (building a
	/// planned building, put in the hand, destroying things in the way); read when the abode is moved to the planned
	/// abodes (no rebuild plan when set)
	bool shouldNotBeAddedToPlanned {false};
	/// The abode info it was made with (AbodeArchetype::Create); None for the abodes made elsewhere (their record is
	/// looked up by number and mesh, town_stats::AbodeInfoOf)
	AbodeInfo info {AbodeInfo::None};
};

/// abodes::RedrawConstruction's state while the abode has a building site and no DestructionMesh: the percent built for
/// drawing its components::DrawMesh / NotDrawn was made for
struct AbodeConstructionDraw
{
	float percent {-1.0f};
};

} // namespace openblack::ecs::components
