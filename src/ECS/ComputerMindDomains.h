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

#include "ECS/ComputerMind.h"

/// The parts of the mind of a computer god. Each part has its own file of rules, so that each kind of node has one
/// owner.
namespace openblack::computer_mind
{

/// The part of the mind that owns the rules of a kind of node
enum class MindDomain : uint8_t
{
	/// The root, the top nodes, the needs and the hand's own actions
	Core,
	/// The food and the wood of the towns
	Towns,
	/// The worshippers, the building sites and the people of the towns
	Worship,
	/// The choice of a miracle, its charge and its cast
	Miracles,
	/// The care of the god's creature and its emergencies
	CreatureCare,
	/// The play with the god's creature and its lessons
	CreaturePlay,
	/// The towns that the god takes from other players
	Conquest,
	/// The attacks on other players, their towns and their creatures
	War,
	/// The defence of the god's towns and its replies to other players
	Defence,
	/// What the god thinks of each player, and what it says and gives to them
	Attitudes,

	_Count
};

namespace detail
{

/// The owner of each kind of node, in the order of the kinds
constexpr std::array<MindDomain, static_cast<size_t>(NodeKind::_Count)> k_Domains {{
    MindDomain::Core,         // Root
    MindDomain::Core,         // TopPlayerInteract
    MindDomain::CreatureCare, // TopCreatureInteract
    MindDomain::Core,         // TopTownInteract
    MindDomain::Attitudes,    // TopDefeatPlayer
    MindDomain::Core,         // TopReactToPlayer
    MindDomain::Core,         // TopWorldInteract
    MindDomain::Attitudes,    // NeedGiveToPlayer
    MindDomain::Attitudes,    // NeedTalktoPlayer
    MindDomain::Conquest,     // NeedExpandInfluence
    MindDomain::Core,         // NeedLookaftercreature
    MindDomain::Core,         // NeedTeachCreature
    MindDomain::Core,         // NeedPlayWithCreature
    MindDomain::Core,         // NeedReactToCreatureEmergency
    MindDomain::Core,         // NeedAttendTownDesires
    MindDomain::Core,         // NeedExpandTown
    MindDomain::Core,         // NeedAttendToWorshippersWorshippers
    MindDomain::Core,         // NeedIncreaseTownPopulation
    MindDomain::Attitudes,    // NeedDefeatPlayer
    MindDomain::Attitudes,    // NeedPlayerWithMoreSpells
    MindDomain::Attitudes,    // NeedPlayerWithBetterCreature
    MindDomain::Attitudes,    // NeedPlayerWithMoreWorshippers
    MindDomain::Core,         // NeedReactToPlayer
    MindDomain::Defence,      // NeedCheckTownsForBeliefAttack
    MindDomain::Miracles,     // NeedCheckTownForDamage
    MindDomain::Defence,      // NeedCheckTownForProtection
    MindDomain::War,          // ActionThrowThingAtPlayer
    MindDomain::Attitudes,    // ActionSendNiceTextToPlayer
    MindDomain::Attitudes,    // ActionSendNastyTextToPlayer
    MindDomain::Attitudes,    // ActionTauntPlayer
    MindDomain::Attitudes,    // ActionScarePlayer
    MindDomain::Attitudes,    // ActionGiveCreatureThingToPlayer
    MindDomain::Attitudes,    // ActionGiveToPlayer
    MindDomain::CreatureCare, // ActionGiveCreatureFood
    MindDomain::CreatureCare, // ActionCastFoodForMyCreature
    MindDomain::CreatureCare, // ActionMakeCreatureDrink
    MindDomain::CreatureCare, // ActionRestoreCreatureHealth
    MindDomain::CreatureCare, // ActionMakeCreatureSleep
    MindDomain::CreatureCare, // ActionSendCreatureHome
    MindDomain::CreatureCare, // ActionMoveCreaturetoWarmerArea
    MindDomain::CreatureCare, // ActionMoveCreaturetoCoollerArea
    MindDomain::CreaturePlay, // ActionTeachCreatureNormalAction
    MindDomain::CreaturePlay, // ActionTeachCreatureAggressiveMagic
    MindDomain::CreaturePlay, // ActionTeachCreatureCompassionateMagic
    MindDomain::CreaturePlay, // ActionTeachCreatureImpressiveMagic
    MindDomain::CreaturePlay, // ActionThrowFoodatCreature
    MindDomain::CreaturePlay, // ActionSlapCreature
    MindDomain::CreaturePlay, // ActionStrokeCreature
    MindDomain::CreaturePlay, // ActionAttachLeashtoMovingObject
    MindDomain::CreaturePlay, // ActionCastSpellOnMyCreatureForALaugh
    MindDomain::CreatureCare, // ActionReacttoCreatureStarvingEmergency
    MindDomain::CreatureCare, // ActionReacttoCreatureDamagedEmergency
    MindDomain::CreatureCare, // ActionReacttoCreatureIllEmergency
    MindDomain::Towns,        // ActionTownWithHighestFoodDesire
    MindDomain::Towns,        // ActionTownWithHighestWoodDesire
    MindDomain::Worship,      // ActionGiveBuildingSiteWorkers
    MindDomain::Worship,      // ActionGiveBuildingSiteWood
    MindDomain::Worship,      // ActionGiveMyTownPopulation
    MindDomain::Worship,      // ActionGiveWorshippersFood
    MindDomain::Worship,      // ActionGiveWorshippersRest
    MindDomain::Worship,      // ActionWorshipSiteThatNeedsVillagers
    MindDomain::Conquest,     // ActionTakeOverTown
    MindDomain::War,          // ActionDestroyTown
    MindDomain::Miracles,     // ActionChargeUpSpell
    MindDomain::War,          // ActionAttackCreature
    MindDomain::Defence,      // ActionReactToAggressiveCreature
    MindDomain::Defence,      // ActionReactToPlayerCastingSpells
    MindDomain::Defence,      // ActionReactToPlayerThrowingStuff
    MindDomain::Defence,      // ActionDefendTown
    MindDomain::Miracles,     // ActionFindBurningAbode
    MindDomain::Defence,      // ActionFindDamagedAbode
    MindDomain::Miracles,     // ActionFindUnprotectedAbode
    MindDomain::Worship,      // ActionCombineScaffolds
    MindDomain::Worship,      // ActionFindPlaceToUseScaffoldForBuilding
    MindDomain::Worship,      // ActionFillWorkshop
    MindDomain::Conquest,     // ActionTakeOverTownByImpressing
    MindDomain::Conquest,     // ActionTakeOverTownByFulfillingFoodDesire
    MindDomain::Conquest,     // ActionTakeOverTownByFulfillingWoodDesire
    MindDomain::Defence,      // ActionDefendTownAgainstBeliefAttack
    MindDomain::Worship,      // ActionMakeDiscipleBreader
    MindDomain::Conquest,     // ObjectGetCreaturetoHelp
    MindDomain::Worship,      // ObjectGetCreaturetoHelpBuild
    MindDomain::Towns,        // ObjectShallCastFoodSpell
    MindDomain::Towns,        // ObjectGetFoodFromField
    MindDomain::Towns,        // ObjectGetFoodFromFishFarm
    MindDomain::Towns,        // ObjectGetFoodFromOtherTownForStoragePit
    MindDomain::Towns,        // ObjectGetFoodFromOtherTownForObject
    MindDomain::Towns,        // ObjectShallCastWoodSpell
    MindDomain::Towns,        // ObjectGetWoodFromForest
    MindDomain::Towns,        // ObjectGetWoodFromOtherTown
    MindDomain::Worship,      // ObjectCastFoodSpellOnWorshipSite
    MindDomain::Worship,      // ObjectShallCastWoodSpellAgain
    MindDomain::Towns,        // ObjectCastFoodSpellOn
    MindDomain::Towns,        // ObjectCastWoodSpellOn
    MindDomain::Worship,      // ObjectFindFreeVillagerBuilderInTown
    MindDomain::Worship,      // ObjectFindFreeVillagerInArea
    MindDomain::Conquest,     // ObjectUseCreatureToImpressTown
    MindDomain::Conquest,     // ObjectThrowThingsToImpress
    MindDomain::Conquest,     // ObjectCastSpellToImpress
    MindDomain::Conquest,     // ObjectCastSpellToFulfilTownNeeds
    MindDomain::War,          // ObjectDestroytownWithSpell
    MindDomain::War,          // ObjectDestroytownWithCreature
    MindDomain::War,          // ObjectDestroytownWithObject
    MindDomain::War,          // ObjectAttackCreatureWithSpells
    MindDomain::War,          // ObjectAttackCreatureWithCreature
    MindDomain::War,          // ObjectThrowrockatCreature
    MindDomain::Defence,      // ObjectDefendTownWithCreature
    MindDomain::Defence,      // ObjectDefendtownWithSpells
    MindDomain::CreaturePlay, // ObjectObjectToLeashTo
    MindDomain::CreatureCare, // ObjectCastFoodForMyCreature
    MindDomain::CreatureCare, // ObjectRestoreMyCreatureHealth
    MindDomain::CreaturePlay, // ObjectTeachCreatureAggressiveMagic
    MindDomain::CreaturePlay, // ObjectTeachCreatureCompassionateMagic
    MindDomain::CreaturePlay, // ObjectTeachCreatureImpressiveMagic
    MindDomain::CreaturePlay, // ObjectCastSpellOnMyCreatureForALaugh
    MindDomain::CreatureCare, // ObjectRestoreMyCreatureHealthAgain
    MindDomain::Defence,      // ObjectUseCreatureAsDiverseionAgainstPlayer
    MindDomain::Miracles,     // ObjectCastShieldToProtect
    MindDomain::Miracles,     // ObjectCastPhysicalShieldToProtect
    MindDomain::Miracles,     // ObjectFindShieldSpell
    MindDomain::Miracles,     // ObjectCastWaterSpell
    MindDomain::Worship,      // ObjectFindScaffoldToCombineWith
    MindDomain::Worship,      // ObjectFindLocationToUseScaffold
    MindDomain::Core,         // ObjectDIRECTPickupAndDropObject
    MindDomain::Core,         // ObjectRaiseTotemInTown
    MindDomain::Worship,      // ObjectTakeVillagerFromTownToWorshipSite
    MindDomain::Worship,      // ObjectTakeVillagerFromWorshipSiteToTown
    MindDomain::Defence,      // ObjectCastSpellToDefendAgainstBelief
    MindDomain::Worship,      // ObjectCreateDiscipleToFulfillTownDesire
    MindDomain::Worship,      // ObjectCastHealSpellAtWorshipSite
}};

} // namespace detail

/// The part of the mind that owns the rules of the kind of node
[[nodiscard]] constexpr MindDomain DomainOf(NodeKind kind)
{
	return detail::k_Domains.at(static_cast<size_t>(kind));
}

} // namespace openblack::computer_mind
