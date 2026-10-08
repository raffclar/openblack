/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "VillagerDeath.h"

#include <cstdio>
#include <cstdlib>

#include <algorithm>
#include <array>

#include <fmt/format.h>
#include <glm/vec3.hpp>
#include <spdlog/spdlog.h>

#include "3D/AllMeshes.h"
#include "Audio/Game/AudioSystem.h"
#include "Audio/GameQueries.h"
#include "Audio/Services/Guidance.h"
#include "ECS/Components/Abode.h"
#include "ECS/Components/LivingAction.h"
#include "ECS/Components/Mesh.h"
#include "ECS/Components/SkeletalAnimation.h"
#include "ECS/Components/Town.h"
#include "ECS/Components/TownDeaths.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/Villager.h"
#include "ECS/DisappearSmoke.h"
#include "ECS/Effects/Alignment.h"
#include "ECS/Effects/Reactions.h"
#include "ECS/Events/Publish.h"
#include "ECS/Events/VillagerDeathEvents.h"
#include "ECS/Fire/FireEffect.h"
#include "ECS/Flocks.h"
#include "ECS/Life.h"
#include "ECS/ObjectCreationIndex.h"
#include "ECS/ObjectMetrics.h"
#include "ECS/Physics/PhysicsObjects.h"
#include "ECS/Registry.h"
#include "ECS/SeaCells.h"
#include "ECS/Systems/Implementations/VillagerReactions.h"
#include "ECS/Systems/VillagerStateSystemInterface.h"
#include "ECS/Systems/VillagerWorldQueriesInterface.h"
#include "ECS/ToBeDeleted.h"
#include "ECS/Town/AbodeQueries.h"
#include "ECS/Town/AbodeVillagers.h"
#include "ECS/Town/Graveyard.h"
#include "ECS/Town/TownQueries.h"
#include "ECS/Town/TownVillagers.h"
#include "ECS/Villager/VillagerAge.h"
#include "ECS/Villager/VillagerCore.h"
#include "ECS/Villager/VillagerDisciple.h"
#include "ECS/Villager/VillagerHome.h"
#include "ECS/Villager/VillagerMourning.h"
#include "ECS/Villager/VillagerResources.h"
#include "ECS/Villager/VillagerScript.h"
#include "ECS/Villager/VillagerSoul.h"
#include "InfoConstants.h"
#include "Locator.h"
#include "Resources/ResourcesInterface.h"

// A villager's death and its states (VillagerDeath.h; docs/bw1-notes/villagers.md, section Death).

namespace openblack::ecs::villager
{
using namespace components;

namespace
{
/// The local interface's player. (inferred) openblack has one local interface: PLAYER_ONE, as HandSystem.cpp's Dropper
/// and Alignment.cpp's ProcessForPlayer
constexpr PlayerNames k_LocalPlayer = PlayerNames::PLAYER_ONE;

/// The help table, one entry per death reason: A (killing people), B (losing villagers), C (death in the village)
constexpr std::array<uint8_t, 10> k_HelpA = {0, 0, 1, 0, 0, 1, 1, 0, 0, 0};
constexpr std::array<uint8_t, 10> k_HelpB = {0, 0, 1, 1, 0, 1, 0, 0, 0, 0};
constexpr std::array<uint8_t, 10> k_HelpC = {0, 1, 1, 1, 1, 0, 0, 1, 1, 0};

/// DEATH_REASON's names (Enums.h DeathReason)
constexpr std::array<const char*, static_cast<size_t>(DeathReason::_COUNT)> k_DeathReasonNames = {
    "NONE",      "STARVING",   "SPELL",  "ANIMAL", "CHANT", "PLAYER_INTERACTION", "PLAYER_INTERACTION_DROWN",
    "SACRIFICE", "EXHAUSTION", "OLD_AGE"};

/// The villager EndPhysics is ending, entt::null outside an EndingPhysicsScope (Locator::villagerStateSystem)
entt::entity& EndingPhysics()
{
	if (!Locator::villagerStateSystem::has_value())
	{
		std::fputs("ecs::villager: no villagerStateSystem in the locator (Locator::villagerStateSystem)\n", stderr);
		std::abort();
	}
	return Locator::villagerStateSystem::value().EndingPhysics();
}

Registry& Entities()
{
	return Locator::entitiesRegistry::value();
}

Villager* VillagerOf(entt::entity villager)
{
	return Entities().TryGet<Villager>(villager);
}

LivingAction* ActionOf(entt::entity villager)
{
	return Entities().TryGet<LivingAction>(villager);
}

const char* DeathName(DeathReason reason)
{
	return k_DeathReasonNames.at(std::min<size_t>(static_cast<size_t>(reason), k_DeathReasonNames.size() - 1));
}

size_t ReasonIndex(DeathReason reason)
{
	return std::min<size_t>(static_cast<size_t>(reason), k_HelpA.size() - 1);
}

/// The villager's town, entt::null without one
entt::entity TownEntityOf(entt::entity villager)
{
	const auto* v = VillagerOf(villager);
	// (guard) the original only reads the link: this stands for the missing town unlinking
	if (v == nullptr || v->town == entt::null || !ecs::IsAvailable(v->town) || !Entities().AllOf<Town>(v->town))
	{
		return entt::null;
	}
	return v->town;
}

/// The town's graveyard (graveyard::GetGraveyard: set when it becomes functional, cleared when it goes) and its
/// abode_queries::IsFunctional: nullopt none, else whether it is functional (120 turns with a functional one, else 600).
/// Read through the villagers' world queries
std::optional<bool> GraveyardOf(entt::entity town)
{
	if (town == entt::null)
	{
		return std::nullopt;
	}
	return Locator::villagerWorldQueries::value().Graveyard(town);
}

/// The smoke puff half the villager's height up
void CreateDisappearSmoke(entt::entity villager)
{
	const auto* transform = Entities().TryGet<const Transform>(villager);
	if (transform == nullptr)
	{
		return;
	}
	const float half = 0.5f * object::GetHeight(villager);
	DisappearSmoke::Create(transform->position + glm::vec3(0.0f, half, 0.0f), 1.0f);
}

/// The 3D object's mesh: the Mesh, or the one kept while it is not drawn (a -4 clip, VillagerAnimations' hiddenMesh)
entt::id_type CurrentMesh(entt::entity villager)
{
	auto& registry = Entities();
	if (const auto* mesh = registry.TryGet<const Mesh>(villager))
	{
		return mesh->id;
	}
	const auto* animation = registry.TryGet<const SkeletalAnimation>(villager);
	return animation != nullptr ? animation->hiddenMesh : 0;
}

/// Sets the 3D object's mesh, as VillagerHome's SetVillagerMeshes does it
void SetObjectMesh(entt::entity villager, entt::id_type id)
{
	auto& registry = Entities();
	if (auto* mesh = registry.TryGet<Mesh>(villager))
	{
		mesh->id = id;
		return;
	}
	if (auto* animation = registry.TryGet<SkeletalAnimation>(villager); animation != nullptr && animation->hiddenMesh != 0)
	{
		animation->hiddenMesh = id;
	}
}

/// The skeleton mesh: PersonSkeletonMale, for men, women and children alike
entt::id_type SkeletonMesh()
{
	return resources::HashIdentifier(MeshId::PersonSkeletonMale);
}

void EmitHelp(entt::entity villager, entt::entity town, const DeathHelp& help)
{
	events::Publish(events::VillagerDeathHelp {.villager = villager, .town = town, .help = help});
}

/// The game's handler of VillagerDeathHelp: the help sprites and sounds
void PlayDeathHelp(const events::VillagerDeathHelp& event)
{
	const auto villager = event.villager;
	const auto town = event.town;
	const auto& help = event.help;
	const auto* transform = Entities().TryGet<const Transform>(villager);
	const glm::vec3 position = transform != nullptr ? transform->position : glm::vec3(0.0f);
	if (help.killingPeople)
	{
		// Whether the villager is on screen (its bounding box). (approximate) its position through
		// GameQueries::pointOnScreen: the queries have no bounding-box test
		const auto& queries = audio::Queries();
		audio::guidance::RemarkKillingPeople(queries.pointOnScreen && queries.pointOnScreen(position));
	}
	if (help.deathInVillage)
	{
		audio::guidance::PlayVillageDeathRemark();
	}
	if (help.worshippersDying)
	{
		audio::guidance::WarnWorshippersDying();
	}
	if (help.losingVillagers)
	{
		audio::guidance::WarnLosingVillagers(position);
	}
	if (help.lowOnPeople && town != entt::null)
	{
		audio::guidance::WarnLowOnPeople(town_queries::HelpTownOf(town));
	}
}

/// The game's handler of VillagerDeadEffects: the smoke puff, then the soul
void MakeDeadEffects(const events::VillagerDeadEffects& event)
{
	const auto villager = event.villager;
	const auto& effects = event.effects;
	if (effects.smoke)
	{
		CreateDisappearSmoke(villager);
	}
	if (effects.soul)
	{
		const auto clip = villager_soul::Create(villager, effects.soulMesh, effects.heavenForced);
		if (TraceOn(villager))
		{
			Trace(villager,
			      fmt::format("dead: smoke, soul {} mesh {}, skeleton", clip, static_cast<uint32_t>(effects.soulMesh)));
		}
	}
}

/// The living part of the DEAD state (after the villager's own): the counter and the vanish
uint32_t LivingDead(entt::entity villager, LivingAction& action)
{
	// In a flock: out of it (ECS/Flocks.h)
	if (const auto flock = flocks::FlockOf(villager); flock != entt::null)
	{
		flocks::RemoveLiving(flock, villager, true);
		flocks::SetFlock(villager, entt::null);
	}
	const auto tick = living::DeadTick(action.turnsUntilStateChange, IsScriptControlled(villager), GetDeathReason(villager));
	action.turnsUntilStateChange = tick.counter;
	if (!tick.vanish)
	{
		if (TraceOn(villager) && tick.counter % 100 == 0)
		{
			Trace(villager, fmt::format("dead: {}", tick.counter));
		}
		return 1;
	}
	// The vanish: a smoke puff, then the villager is deleted; 5
	if (TraceOn(villager))
	{
		Trace(villager, "dead: vanish");
	}
	events::Publish(events::VillagerDeadEffects {.villager = villager, .effects = DeadEffects {.smoke = true}});
	Delete(villager);
	return 5;
}
} // namespace

// ---- the pure layer ----------------------------------------------------------------------------------------------

DeathHelp HelpFor(DeathReason reason, bool killerLocal, bool ownerLocal, bool killerIsOwner, bool hasTown, uint32_t adults,
                  uint32_t threshold)
{
	const auto r = ReasonIndex(reason);
	DeathHelp help;
	// The killer is the local player -> A; else the owner is -> C
	if (killerLocal)
	{
		help.killingPeople = k_HelpA.at(r) != 0;
	}
	else if (ownerLocal)
	{
		help.deathInVillage = k_HelpC.at(r) != 0;
	}
	if (!hasTown)
	{
		return help;
	}
	// CHANT -> the owner local: worshippers dying; else adults above the threshold (unsigned) -> B, the killer not the
	// owner and the owner local: losing villagers; else the owner local: low on people
	if (reason == DeathReason::Chant)
	{
		help.worshippersDying = ownerLocal;
	}
	else if (adults > threshold)
	{
		help.losingVillagers = k_HelpB.at(r) != 0 && !killerIsOwner && ownerLocal;
	}
	else
	{
		help.lowOnPeople = ownerLocal;
	}
	return help;
}

uint16_t DyingTime(bool functionalGraveyard, const GVillagerInfo& info)
{
	// Only the low 16 bits of the times are read
	return static_cast<uint16_t>(functionalGraveyard ? info.dyingTimeWithGraveyard : info.dyingTimeWithoutGraveyard);
}

int32_t DyingClip(bool water, uint8_t landType)
{
	// In the water: drowned; landType 2: P_DEAD2; else P_DYING
	if (water)
	{
		return 283;
	}
	return (landType & 3) == 2 ? 246 : 253;
}

int32_t DeadClip(bool water, uint8_t landType)
{
	// In the water: drowned; landType 2: P_DEAD2; else P_DEAD1
	if (water)
	{
		return 249;
	}
	return (landType & 3) == 2 ? 246 : 243;
}

// ---- VillagerDead and the states ---------------------------------------------------------------------------------

void VillagerDead(entt::entity villager, DeathReason reason, std::optional<PlayerNames> killer, float amount, int drop)
{
	auto& registry = Entities();
	if (!ecs::IsAvailable(villager) || VillagerOf(villager) == nullptr || ActionOf(villager) == nullptr)
	{
		return;
	}
	// In the physics: nothing; EndPhysics kills it at rest with reason 5 / 6
	if (IsInPhysics(villager))
	{
		if (TraceOn(villager))
		{
			Trace(villager, fmt::format("death: {} ignored (in the physics)", DeathName(reason)));
		}
		return;
	}
	// Already dead: nothing
	if (IsDead(villager))
	{
		return;
	}
	// A reason not in {0, 1, 3, 4, 8, 9} notifies the killer only in a multiplayer game (not ported: no multiplayer)
	const auto town = TownEntityOf(villager);
	// The owner is the town's player, else the neutral player; no killer -> the neutral player
	const auto ownerPlayer = GetPlayerOf(villager);
	const PlayerNames owner = ownerPlayer.value_or(PlayerNames::NEUTRAL);
	const PlayerNames by = killer.value_or(PlayerNames::NEUTRAL);
	const auto* t = town != entt::null ? registry.TryGet<const Town>(town) : nullptr;
	// the town's adults are read before SetDying: the dying villager still counts
	const uint32_t adults = t != nullptr ? t->stats.adults : 0;
	const uint32_t threshold =
	    Locator::infoConstants::has_value() ? Locator::infoConstants::value().town.populationUnderWhichHelpSpritesWarn : 0;
	const auto help =
	    HelpFor(reason, by == k_LocalPlayer, owner == k_LocalPlayer, by == owner, t != nullptr, adults, threshold);
	// The killer's / owner's help sprite (before the drops)
	EmitHelp(villager, town, DeathHelp {.killingPeople = help.killingPeople, .deathInVillage = help.deathInVillage});
	// drop != 0 -> CreateDroppedResource; then DropWood and DropFood always (the town's carried totals go back)
	if (drop != 0)
	{
		CreateDroppedResource(villager, std::nullopt, std::nullopt, std::nullopt);
	}
	DropWood(villager, 0);
	DropFood(villager, 0);
	// A disciple stops being one
	if (const auto* v = VillagerOf(villager); v != nullptr && (v->flags & Villager::k_FlagDisciple) != 0)
	{
		SetVillagerDisciple(villager, entt::null, VillagerDisciple::None, 0);
	}
	// The owner's alignment is updated: nothing without a player
	const bool child = IsChild(villager);
	if (ownerPlayer)
	{
		effects::alignment::UpdateForDeath(*ownerPlayer, reason, child, false);
	}
	if (t != nullptr)
	{
		// The town adds the amount per killer and reason. (not ported) no reader was found
		(void)amount;
		// The town's death statistics
		auto& deaths = registry.AllOf<TownDeaths>(town) ? registry.Get<TownDeaths>(town) : registry.Assign<TownDeaths>(town);
		deaths.lastDeathTurn = CurrentTurn();
		++deaths.count;
		++deaths.byPlayer.at(std::min<size_t>(static_cast<size_t>(by), deaths.byPlayer.size() - 1));
		++deaths.byReason.at(ReasonIndex(reason));
		++deaths.total5C.at(0);
		++deaths.total38;
		// The town's player's and the killer's game statistics count the death. TODO(statistics): openblack has no game
		// statistics
		// The town's graveyard gets the dead (only when there is one; AddDead tests IsFunctional and the 50 itself)
		if (const auto yard = graveyard::GetGraveyard(town); yard != entt::null)
		{
			graveyard::AddDead(yard);
		}
		// The town's pulse (TownProcess reads it)
		auto& changed = registry.Get<Town>(town);
		changed.buildPulsePrevious = 0;
		changed.buildPulse = 1;
		if (TraceOn(villager))
		{
			Trace(villager, fmt::format("death: town {} deaths[{}] = {} total {}", changed.id, static_cast<uint32_t>(reason),
			                            deaths.byReason.at(ReasonIndex(reason)), deaths.count));
		}
		// The town's help sprite
		EmitHelp(villager, town,
		         DeathHelp {.worshippersDying = help.worshippersDying,
		                    .losingVillagers = help.losingVillagers,
		                    .lowOnPeople = help.lowOnPeople});
	}
	if (TraceOn(villager))
	{
		Trace(villager, fmt::format("death: {} killer {} owner {} amount {:.4f} drop {} help {}{}{}{}{}", DeathName(reason),
		                            static_cast<int>(by), static_cast<int>(owner), amount, drop, help.killingPeople ? "k" : "",
		                            help.deathInVillage ? "v" : "", help.worshippersDying ? "w" : "",
		                            help.losingVillagers ? "l" : "", help.lowOnPeople ? "p" : ""));
	}
	else if (auto logger = spdlog::get("game"); logger != nullptr)
	{
		SPDLOG_LOGGER_INFO(logger, "Villager {} died ({})", object_index::Of(villager), DeathName(reason));
	}
	// Not dead yet -> SetDying; SACRIFICE: the killer's game statistics count it (TODO(statistics)) and the counter is 0
	if (const auto* v = VillagerOf(villager); v != nullptr && (v->status & Villager::k_StatusDead) == 0)
	{
		SetDying(villager);
		if (reason == DeathReason::Sacrifice)
		{
			if (auto* action = ActionOf(villager))
			{
				action->turnsUntilStateChange = 0;
			}
		}
	}
	// The reason is kept after SetDying
	if (auto* v = VillagerOf(villager))
	{
		v->deathReason = reason;
	}
}

uint32_t DestroyedByEffect(entt::entity villager, std::optional<PlayerNames> player, float amount)
{
	// A SPELL death with a drop; 1
	VillagerDead(villager, DeathReason::Spell, player, amount, 1);
	return 1;
}

uint32_t SetDying(entt::entity villager)
{
	auto* v = VillagerOf(villager);
	auto* action = ActionOf(villager);
	if (v == nullptr || action == nullptr)
	{
		return 1;
	}
	// The town first (DeleteDependants takes it out of the town)
	const auto town = TownEntityOf(villager);
	if ((v->status & Villager::k_StatusDead) == 0)
	{
		// Life 0
		life::SetLife(villager, 0.0f);
		// TOP 14 DYING: the exits of the state left run and the dying clip is chosen now, with the landType of the last
		// landing (landType 3 below comes after). (approximate) villager_reactions::SetTopState: it also ends openblack's
		// walk, as MOVE_TO_POS' exit would (not ported)
		villager_reactions::SetTopState(villager, VillagerStates::Dying);
		// Dead, out of its abode and town, then landType 3
		if (auto* now = VillagerOf(villager))
		{
			now->status = static_cast<uint16_t>(now->status | Villager::k_StatusDead);
		}
		DeleteDependants(villager);
		if (auto* now = VillagerOf(villager))
		{
			now->status = static_cast<uint16_t>(now->status | Villager::k_StatusLandTypeMask);
		}
	}
	// The corpse's counter: a functional graveyard in the town ? DyingTimeWithGraveyard : DyingTimeWithoutGraveyard;
	// also when it was dead already
	const bool graveyard = HasFunctionalGraveyard(town);
	if (auto* again = ActionOf(villager))
	{
		again->turnsUntilStateChange = DyingTime(graveyard, InfoOf(villager));
	}
	// Not counted out yet: out of the world population. openblack counts the entities
	// (magic::players::WorldPopulation skips the counted-out ones)
	if (auto* now = VillagerOf(villager); now != nullptr && (now->flags & Villager::k_FlagCountedOut) == 0)
	{
		now->flags = static_cast<uint16_t>(now->flags | Villager::k_FlagCountedOut);
	}
	if (TraceOn(villager))
	{
		const auto* again = ActionOf(villager);
		Trace(villager, fmt::format("setdying: counter {} (graveyard {})", again != nullptr ? again->turnsUntilStateChange : 0,
		                            graveyard ? 1 : 0));
	}
	return 1;
}

uint32_t SetDyingState(LivingAction& action)
{
	return SetDying(Entities().ToEntity(action));
}

uint32_t Dying(LivingAction& action)
{
	const auto villager = Entities().ToEntity(action);
	// SACRIFICE -> 15 DEAD at once (no dying clip); else 23 WAIT_FOR_ANIMATION until the dying clip has played, then 15
	if (GetDeathReason(villager) == DeathReason::Sacrifice)
	{
		SetTopState(villager, VillagerStates::Dead);
	}
	else
	{
		PlayAnimThenSetState(villager, VillagerStates::Dead);
	}
	// Not at home and (no town or no graveyard) -> a REACT_TO_DEATH reaction, no player. The town is already none here
	// (SetDying left it), so the graveyard test never stops it (literal)
	bool reaction = false;
	if (const auto* v = VillagerOf(villager); v != nullptr && (v->flags & Villager::k_FlagAtHome) == 0)
	{
		const auto town = TownEntityOf(villager);
		if (town == entt::null || !GraveyardOf(town))
		{
			effects::reactions::CreateReaction(villager, openblack::Reaction::ReactToDeath, PlayerNames::NEUTRAL, false);
			reaction = true;
		}
	}
	if (TraceOn(villager))
	{
		Trace(villager, fmt::format("dying: -> 15 (reaction {})", reaction ? "REACT_TO_DEATH" : "none"));
	}
	return 1;
}

uint32_t Dead(LivingAction& action)
{
	const auto villager = Entities().ToEntity(action);
	// A burning corpse stops burning
	if (auto* fire = fire::Find(villager); fire != nullptr)
	{
		fire::ToBeDeleted(*fire);
	}
	// The skeleton mesh
	const auto skeleton = SkeletonMesh();
	// Not controlled by a script
	if (!IsScriptControlled(villager))
	{
		// Not the skeleton yet: only the first DEAD turn
		if (CurrentMesh(villager) != skeleton)
		{
			DeadEffects effects {.smoke = true, .skeleton = true};
			// The smoke puff: made by the VillagerDeadEffects handler
			// Not in the water -> the soul; a drowned corpse has none
			const auto* transform = Entities().TryGet<const Transform>(villager);
			if (transform != nullptr && !sea_cells::IsWater(transform->position))
			{
				// Younger than grownUpAge (unsigned): the child mesh (ChildMeshHigh), heaven forced; else the standard
				// mesh (StdDetail), not forced
				const auto& info = InfoOf(villager);
				const bool young = GetAge(villager) < info.grownUpAge;
				effects.soul = true;
				effects.soulMesh = young ? info.childMeshHigh : info.stdDetail;
				effects.heavenForced = young;
			}
			else if (TraceOn(villager))
			{
				Trace(villager, "dead: smoke, water, skeleton");
			}
			// The smoke, then the soul, are made by the handler
			events::Publish(events::VillagerDeadEffects {.villager = villager, .effects = effects});
		}
		// The skeleton mesh, every turn; then a further call on the 3D object (not ported: (inferred) a reset of the 3D
		// object)
		SetObjectMesh(villager, skeleton);
	}
	// The living part: the counter and the vanish
	auto* again = ActionOf(villager);
	return again != nullptr ? LivingDead(villager, *again) : 1;
}

uint32_t CannotExitState(LivingAction& action, VillagerStates next)
{
	const auto villager = Entities().ToEntity(action);
	// A state with the same exit function, IN_HAND or FLYING -> 1; else 0
	if (IsStateExitFunctionSameAs(villager, next) || next == VillagerStates::InHand || next == VillagerStates::Flying)
	{
		return 1;
	}
	return 0;
}

// ---- queries -----------------------------------------------------------------------------------------------------

bool IsDead(entt::entity villager)
{
	const auto* v = VillagerOf(villager);
	if (v == nullptr)
	{
		return false;
	}
	// The dead status bit, or TOP 15 DEAD
	const auto top = GetState(villager, Index::Top);
	if ((v->status & Villager::k_StatusDead) != 0 || top == VillagerStates::Dead)
	{
		return true;
	}
	// Or not functional: functional means available and TOP not in 13..14 (SET_DYING, DYING)
	const auto t = static_cast<uint32_t>(top);
	const bool functional = villager::IsAvailable(villager) && !(t >= 13 && t <= 14);
	return !functional;
}

DeathReason GetDeathReason(entt::entity villager)
{
	const auto* v = VillagerOf(villager);
	return v != nullptr ? v->deathReason : DeathReason::None;
}

bool IsCountedOut(entt::entity villager)
{
	const auto* v = VillagerOf(villager);
	return v != nullptr && (v->flags & Villager::k_FlagCountedOut) != 0;
}

std::optional<PlayerNames> GetPlayerOf(entt::entity villager)
{
	// The town's owner, none without a town
	const auto town = TownEntityOf(villager);
	if (town == entt::null)
	{
		return std::nullopt;
	}
	return Entities().Get<const Town>(town).owner;
}

bool IsSkeleton(entt::entity villager)
{
	const auto* v = VillagerOf(villager);
	return v != nullptr && (v->status & Villager::k_StatusSkeleton) != 0;
}

void SetSkeleton(entt::entity villager, bool on)
{
	auto* v = VillagerOf(villager);
	if (v == nullptr)
	{
		return;
	}
	// The skeleton status bit
	v->status = static_cast<uint16_t>((v->status & ~Villager::k_StatusSkeleton) | (on ? Villager::k_StatusSkeleton : 0));
	const auto& info = InfoOf(villager);
	if (IsSkeleton(villager))
	{
		// The skeleton mesh
		SetObjectMesh(villager, SkeletonMesh());
	}
	else
	{
		// A child (younger than grownUpAge) its child meshes, else its detail meshes: openblack's one mesh of the LODs
		// (VillagerHome's SetVillagerMeshes)
		SetVillagerMeshes(villager, info, GetAge(villager) < info.grownUpAge, false);
	}
	// The scale for its age (draws a game random number)
	SetScaleForAge(villager, GetAge(villager));
}

// ---- deletion ----------------------------------------------------------------------------------------------------

void DeleteDependants(entt::entity villager)
{
	auto& registry = Entities();
	const auto* v = VillagerOf(villager);
	if (v == nullptr)
	{
		return;
	}
	// The abode and the town are read first
	auto abode = entt::entity(entt::null);
	// (guard) the original only reads the link: this stands for the unlinking when the abode is deleted
	if (v->abode != entt::null && ecs::IsAvailable(v->abode) && registry.AllOf<Abode>(v->abode))
	{
		abode = v->abode;
	}
	const auto town = TownEntityOf(villager);
	// TOP not 13 / 14 / 15 -> TOP 13 SET_DYING: the exits of the state left run (AT_HOME's exit leaves home).
	// (approximate) villager_reactions::SetTopState: the walk ends too (MOVE_TO_POS' exit is not ported)
	const auto top = GetState(villager, Index::Top);
	if (top != VillagerStates::SetDying && top != VillagerStates::Dying && top != VillagerStates::Dead &&
	    ActionOf(villager) != nullptr)
	{
		villager_reactions::SetTopState(villager, VillagerStates::SetDying);
	}
	// TOP not 29 MOVE_ON_PATH and on a footpath: out of its walkers. (not ported) openblack's villagers walk no
	// footpaths
	// A mother (a female villager) orphans her children
	if (InfoOf(villager).sex == SexType::Female)
	{
		villager_mourning::FindChildrenAndOrphanThem(villager);
	}
	// An abode -> out of it (which also leaves the town when the abode has one); else a town -> out of it; else out of
	// the vagrants
	if (abode != entt::null)
	{
		abode_villagers::RemoveDeletedVillagerFromAbode(abode, villager);
	}
	else if (town != entt::null)
	{
		town_villagers::RemoveVillager(town, villager);
	}
	else
	{
		town_villagers::RemoveFromVagrants(villager);
	}
}

void ToBeDeletedOverride(entt::entity villager)
{
	if (!Entities().Valid(villager) || VillagerOf(villager) == nullptr)
	{
		return;
	}
	// DeleteDependants, then the living part: a reaction -> villager_reactions::StopReacting (the fire's, teleport's,
	// shield's and the mourning's); out of the living list; the data path and script reminder (not ported); a dance
	// (pending); then the flock. The object's own part is ecs::ToBeDeleted's tail
	DeleteDependants(villager);
	if (villager_reactions::IsReacting(villager))
	{
		villager_reactions::StopReacting(villager);
	}
	// In a flock: out of it (ECS/Flocks.h)
	if (const auto flock = flocks::FlockOf(villager); flock != entt::null)
	{
		flocks::RemoveLiving(flock, villager, true);
		flocks::SetFlock(villager, entt::null);
	}
}

void Delete(entt::entity villager)
{
	// Through ecs::ToBeDeleted, whose villager branch is ToBeDeletedOverride. A villager not counted out leaves the world
	// population when destroyed; openblack counts the entities: the deleted one is no longer one
	ecs::ToBeDeleted(villager);
}

// ---- physics -----------------------------------------------------------------------------------------------------

EndingPhysicsScope::EndingPhysicsScope(entt::entity villager)
    : _previous(EndingPhysics())
{
	EndingPhysics() = villager;
}

EndingPhysicsScope::~EndingPhysicsScope()
{
	EndingPhysics() = _previous;
}

bool IsInPhysics(entt::entity villager)
{
	return villager != EndingPhysics() && physics::PhysicsObjects::IsFlying(villager);
}

// ---- events ------------------------------------------------------------------------------------------------------

void AddDeathEventHandlers(EventManager& manager)
{
	manager.AddHandler<events::VillagerDeathHelp>(PlayDeathHelp);
	manager.AddHandler<events::VillagerDeadEffects>(MakeDeadEffects);
}

// ---- graveyard ---------------------------------------------------------------------------------------------------

bool HasFunctionalGraveyard(entt::entity town)
{
	const auto graveyard = GraveyardOf(town);
	return graveyard.has_value() && *graveyard;
}
} // namespace openblack::ecs::villager

namespace openblack::ecs::living
{
DeadTickResult DeadTick(uint16_t counter, bool scriptControlled, DeathReason reason)
{
	DeadTickResult result {counter, false};
	// Not controlled by a script: the counter goes down; it was 0 -> vanish
	if (!scriptControlled)
	{
		result.counter = static_cast<uint16_t>(counter - 1);
		if (counter == 0)
		{
			result.vanish = true;
			return result;
		}
	}
	// SACRIFICE -> vanish
	if (reason == DeathReason::Sacrifice)
	{
		result.vanish = true;
	}
	return result;
}
} // namespace openblack::ecs::living
