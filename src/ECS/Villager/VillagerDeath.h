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

#include <functional>
#include <optional>

#include <entt/entity/entity.hpp>

#include "ECS/Components/LivingAction.h"
#include "Enums.h"

namespace openblack
{
class EventManager;
struct GVillagerInfo;
enum class MeshId : uint32_t;
} // namespace openblack

// A villager's death (docs/bw1-notes/villagers.md, section Death): VillagerDead (the owner's alignment, the town's death
// counters, the help sprites, SetDying), the states 13 SET_DYING, 14 DYING and 15 DEAD (the dying clip, the smoke, the
// soul, the skeleton, the corpse's 600 / 120 turns), the deletion (ToBeDeletedOverride, DeleteDependants) and the guard
// of EndPhysics' dead branches. The mourning is VillagerMourning.h, the soul VillagerSoul.h.

namespace openblack::ecs::villager
{
// ---- the pure layer ----------------------------------------------------------------------------------------------

/// The help-sprite calls of VillagerDead, from a table of three flags per reason (A, B, C)
struct DeathHelp
{
	bool killingPeople {false};    ///< RemarkKillingPeople (A, the killer is the local player)
	bool deathInVillage {false};   ///< PlayVillageDeathRemark (C, else the owner is the local player)
	bool worshippersDying {false}; ///< WarnWorshippersDying (reason 4, the owner local)
	bool losingVillagers {false};  ///< WarnLosingVillagers (B, adults above the threshold)
	bool lowOnPeople {false};      ///< WarnLowOnPeople (adults at or below the threshold)
};
/// `adults` is the town's adult count read before SetDying (the dying one still counts), `threshold` the town info's
/// populationUnderWhichHelpSpritesWarn
[[nodiscard]] DeathHelp HelpFor(DeathReason reason, bool killerLocal, bool ownerLocal, bool killerIsOwner, bool hasTown,
                                uint32_t adults, uint32_t threshold);
/// SetDying's corpse time: a functional graveyard in the town -> DyingTimeWithGraveyard (120), else
/// DyingTimeWithoutGraveyard (600)
[[nodiscard]] uint16_t DyingTime(bool functionalGraveyard, const GVillagerInfo& info);
/// The dying clip: in the water 283 P_INTO_DEAD_DROWNED, landType 2 -> 246 P_DEAD2, else 253 P_DYING
[[nodiscard]] int32_t DyingClip(bool water, uint8_t landType);
/// The dead clip: in the water 249 P_DEAD_DROWNED, landType 2 -> 246 P_DEAD2, else 243 P_DEAD1
[[nodiscard]] int32_t DeadClip(bool water, uint8_t landType);

// ---- VillagerDead and the states ---------------------------------------------------------------------------------

/// A villager dies (reason, killer, amount, drop): nothing while it is in the physics or once dead;
/// the help sprites, the drops (`drop` only decides CreateDroppedResource; DropWood / DropFood always), the owner's
/// alignment, the town's counters and pulse, SetDying and the reason. `killer` none = the neutral player
void VillagerDead(entt::entity villager, DeathReason reason, std::optional<PlayerNames> killer, float amount, int drop);
/// An effect took the last of the life: VillagerDead(2 SPELL, player, amount, 1); 1
uint32_t DestroyedByEffect(entt::entity villager, std::optional<PlayerNames> player, float amount);
/// The villager starts dying (also the function of row 13 SET_DYING): life 0, TOP 14 DYING, out of its abode and town
/// (DeleteDependants), landType 3, the corpse's counter, out of the world population. 1
uint32_t SetDying(entt::entity villager);
/// Row 13 SET_DYING: SetDying
uint32_t SetDyingState(components::LivingAction& action);
/// Row 14 DYING: the dying clip, then 15 DEAD
uint32_t Dying(components::LivingAction& action);
/// Row 15 DEAD: the villager's part, then the living part (DeadTick). 1, or 5 once the corpse went (the entity is gone
/// then)
uint32_t Dead(components::LivingAction& action);
/// Row 15's exit: 1 only for IN_HAND 24, FLYING 10 or a state with the same exit function (IsStateExitFunctionSameAs)
uint32_t CannotExitState(components::LivingAction& action, VillagerStates next);

// ---- queries -----------------------------------------------------------------------------------------------------

/// The dead status bit, or TOP 15 DEAD, or not functional (functional: IsAvailable and TOP not in 13..14)
[[nodiscard]] bool IsDead(entt::entity villager);
/// The reason it died
[[nodiscard]] DeathReason GetDeathReason(entt::entity villager);
/// Out of the world population (SetDying)
[[nodiscard]] bool IsCountedOut(entt::entity villager);
/// The town's owner, none without a town
[[nodiscard]] std::optional<PlayerNames> GetPlayerOf(entt::entity villager);
/// The skeleton status bit
[[nodiscard]] bool IsSkeleton(entt::entity villager);
/// SET_SKELETON (also a creation argument): the status bit, the mesh (the skeleton, else the villager's own meshes) and
/// SetScaleForAge. (pending) no caller sets it yet: CHL SET_SKELETON is Intro's, and the creation argument
void SetSkeleton(entt::entity villager, bool on);

// ---- deletion ----------------------------------------------------------------------------------------------------

/// SET_DYING through the real exits unless already 13 / 14 / 15, the footpath (not ported), a mother's orphans, out of
/// its abode (abode_villagers::RemoveDeletedVillagerFromAbode), else its town (town_villagers::RemoveVillager), else the
/// vagrants
void DeleteDependants(entt::entity villager);
/// The villager's own part of the deletion (ecs::ToBeDeleted's villager branch calls it when the villager is marked):
/// DeleteDependants, then StopReacting of the reaction it follows, then out of its flock
void ToBeDeletedOverride(entt::entity villager);
/// Deletes the villager through ecs::ToBeDeleted (ToBeDeletedOverride, then the object's tail). No death: no reason, no
/// counters, no corpse
void Delete(entt::entity villager);

// ---- physics -----------------------------------------------------------------------------------------------------

/// The original leaves the physics before EndPhysics reaches its dead branches; the physics calls openblack's EndPhysics
/// handler while the body is still listed, so the handler marks the villager out of the physics for that call
/// (VillagerDead's first test)
class EndingPhysicsScope
{
public:
	explicit EndingPhysicsScope(entt::entity villager);
	~EndingPhysicsScope();
	EndingPhysicsScope(const EndingPhysicsScope&) = delete;
	EndingPhysicsScope& operator=(const EndingPhysicsScope&) = delete;

private:
	entt::entity _previous;
};
/// VillagerDead's first test: in the physics. (approximate) a flying body
/// (PhysicsObjects::IsFlying: openblack's resting proxies are not counted, as the animals' dying test in ECS/AnimalAI)
[[nodiscard]] bool IsInPhysics(entt::entity villager);

// ---- events ------------------------------------------------------------------------------------------------------

/// What a dead villager makes, sent as events::VillagerDeadEffects: the first dead turn's smoke, soul and skeleton, and
/// the vanish's smoke only
struct DeadEffects
{
	bool smoke {false};
	bool soul {false};
	MeshId soulMesh {};
	bool heavenForced {false};
	bool skeleton {false};
};
/// The game's handlers of the death events: events::VillagerDeathHelp plays the help sprites and sounds,
/// events::VillagerDeadEffects makes the smoke puff and the soul. Added once when the game starts
void AddDeathEventHandlers(EventManager& manager);

// ---- graveyard ---------------------------------------------------------------------------------------------------

/// Whether the town has a functional graveyard (the villagers' world queries; SetDying and the REACT_TO_DEATH priority)
[[nodiscard]] bool HasFunctionalGraveyard(entt::entity town);
} // namespace openblack::ecs::villager

namespace openblack::ecs::living
{
struct DeadTickResult
{
	uint16_t counter {0};
	bool vanish {false};
};
/// The DEAD state's counter, shared by the villagers and the animals: not controlled by a script -> c = counter,
/// counter = c - 1, c == 0 -> vanish (tested before the decrement); then reason 7 SACRIFICE -> vanish
[[nodiscard]] DeadTickResult DeadTick(uint16_t counter, bool scriptControlled, DeathReason reason);
} // namespace openblack::ecs::living
