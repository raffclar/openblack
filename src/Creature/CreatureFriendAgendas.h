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

#include <vector>

#include "Creature/CreatureIdleMind.h"

/// The agendas of the actions that a creature does with or for another creature: waving at a friend, which the friend
/// does too, howling at a friend, and pining for and looking for a friend that it misses.
namespace openblack::creature_mind
{

/// Waving at a friend, as a joint action. The agenda has these steps:
/// 1. It asks the friend to play with it, with a playful face. If the friend refuses, the agenda stops.
/// 2. It makes the friend stand still in its mind.
/// 3. It tells the friend to turn to face it for a tenth of a second.
/// 4. It turns to face the friend for 1.4 seconds, with a smile.
/// 5. It tells the friend to wave.
/// 6. It waves. This is the main step.
/// 7. It lets the friend's mind go free again.
/// It waves from where it is: it does not walk to the friend.
[[nodiscard]] std::vector<Step> WaveAtFriend(uint32_t self, uint32_t friendly);
constexpr float k_FriendFacesWaverSeconds = 0.1f;
constexpr float k_FaceFriendToWaveSeconds = 1.4f;

/// Howling at a friend: it turns to face the friend for two seconds, then howls
[[nodiscard]] std::vector<Step> HowlAtFriend(uint32_t friendly);
constexpr float k_FaceToHowlSeconds = 2.0f;
constexpr size_t k_Howl = 217;

/// Pining for a friend: with a sad face it turns to face the friend for two seconds and points at it for two seconds.
/// Then it plays its sad action. This is the main step.
[[nodiscard]] std::vector<Step> PineForFriend(uint32_t friendly);
/// Looking for a friend: with a sad face it points at the friend for two seconds and plays its sad action, which is the
/// main step. Then it goes near the friend, to three times its own height, as it looks where it goes.
[[nodiscard]] std::vector<Step> LookForFriend(uint32_t friendly, float height);
constexpr float k_PineSeconds = 2.0f;
constexpr float k_LookForFriendHeights = 3.0f;

} // namespace openblack::creature_mind
