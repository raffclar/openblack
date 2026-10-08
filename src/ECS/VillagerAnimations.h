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

#include <entt/entity/fwd.hpp>

#include "Enums.h"

namespace openblack::ecs
{

/// Which clip a villager plays, like the original (docs/bw1-notes/animation.md): the state's clip from info.dat or its
/// hard-coded animation function, the into / out-of clips of a state change, and the walk clip synced to the ground
/// covered.

/// The clip for the villager's current state (an ANM_ index; -4 = not drawn)
int32_t VillagerAnimId(entt::entity villager);

/// The out-of clip, called before the new state's entry (as the top state is set): -1 if `next` has no out-of clip
/// (the state table's field0xf0), else the current TOP's into / out-of function with (0, next); a clip sets the flags
/// 0x1800 (transitionFlags).
int32_t VillagerCallOutOfAnimation(entt::entity villager, VillagerStates next);

/// The tail of setting the top state, or the current and destination states: the state's speed
/// (SetVillagerStateSpeed, called with no test; its skips are its own), then the out-of clip if there was one, else
/// the state's clip and the into clip (the TOP's function with (1, entered); a clip sets 0x800 and clears 0x1000).
/// `entered` is the new top state, or the destination when both are set, not the current state; the out-of clip in
/// that case is also VillagerCallOutOfAnimation(villager, destination).
void VillagerApplyStateClips(entt::entity villager, VillagerStates entered, int32_t out);

/// Both halves at once, for the changes that bypass the exit and entry functions (LivingActionSystem::VillagerSetState
/// with skipTransition: the hand, the physics, the animals, LANDED). `previous` is the TOP
/// before the change; the new TOP is already set.
void OnVillagerStateChanged(entt::entity villager, VillagerStates previous, VillagerStates next);

/// While an into / out-of clip plays the state logic waits. Returns true if it must wait this turn; once the clip is
/// over it switches to the state's own clip.
bool VillagerWaitsForTransition(entt::entity villager, uint16_t turnsSinceStateChange);

/// SetAnim(VillagerAnimId(), n): the state's clip if it is another
/// one; `reset` (n != 0 and not dancing) starts it from the beginning
void VillagerSetStateClip(entt::entity villager, bool reset);

/// SetAnim(clip, n) with a clip the state names itself instead of VillagerAnimId's (the amazed villager's
/// 395 TALKING_AND_POINTING)
void VillagerSetClip(entt::entity villager, int32_t clip, bool reset);

/// The current clip has played once (turns in the state * 100 ms)
bool VillagerAnimationDone(entt::entity villager, uint16_t turnsSinceStateChange);

/// LANDED's clip function, by the landType (2 bits): 0 -> nothing carried ? 308 P_LANDED_FROM_FEET :
/// 309 P_LANDED_FROM_FEET_CARRY_OBJECT; 1 -> 306 P_LANDED; 2 and 3 -> 307 P_LANDED_FROM_BACK
[[nodiscard]] int32_t VillagerLandedClip(uint8_t landType, bool carriesNothing);

/// A new top state for a villager from outside its state logic (the hand, the physics): IN_HAND when picked up,
/// FLYING when thrown, LANDED when it comes to rest. The villager stops walking.
void SetVillagerState(entt::entity villager, VillagerStates state);

/// Per frame: gives new villagers their clip and feeds the walk sync (the speed of the moving states).
void UpdateVillagerAnimations();

} // namespace openblack::ecs
