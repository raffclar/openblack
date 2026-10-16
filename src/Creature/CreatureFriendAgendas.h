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

#include <utility>
#include <vector>

#include <glm/vec2.hpp>

#include "Creature/CreatureIdleMind.h"

/// The agendas of the actions that a creature does with or for another creature: waving at a friend, which the friend
/// does too, howling at a friend, pining for and looking for a friend that it misses, and the joint actions that two
/// friends do side by side: being happy, having a poo, sitting, sleeping, drinking, and helping or attacking a town.
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

/// Two creatures in a joint action: the creature that asks and its friend, by their entities' numbers, where each one
/// stands on the ground and how tall each one is
struct Friends
{
	uint32_t self {0};
	uint32_t friendly {0};
	glm::vec2 selfPoint {0.0f};
	glm::vec2 friendPoint {0.0f};
	float height {0.0f};
	float friendHeight {0.0f};
};

/// The places of two friends side by side: from the point halfway between them, the creature stands its own height
/// along the first axis of the map, and the friend stands its own height the other way. The first of the pair is the
/// creature's place.
[[nodiscard]] std::pair<glm::vec2, glm::vec2> SideBySide(const Friends& friends);
/// The places of two friends about a clear place: the creature stands a quarter of the radius along both axes of the
/// map from the middle, and the friend stands the same distance the other way. The first of the pair is the creature's
/// place.
[[nodiscard]] std::pair<glm::vec2, glm::vec2> AboutPlace(glm::vec2 middle, float radius);
/// The radius of the clear place that two friends sit in, and the radius of the clear place that they sleep in, from
/// their heights
[[nodiscard]] float SitTogetherRadius(float height, float friendHeight);
[[nodiscard]] float SleepTogetherRadius(float height, float friendHeight);
constexpr float k_SitTogetherHeights = 1.7f;
constexpr float k_SleepTogetherHeights = 2.0f;

/// Being happy with a friend. The agenda has these steps:
/// 1. It asks the friend to be its friend, as it looks at it. If the friend refuses, the agenda stops.
/// 2. It tells the friend to go to its place (see SideBySide), to within the friend's height.
/// 3. It goes to its own place, to within its own height, as it looks where it goes.
/// 4. It waits until the friend does not move, as it looks at it.
/// 5. It tells the friend to turn to face it, for a tenth of a second.
/// 6. It turns to face the friend for a tenth of a second, and flutters its eyelids as it looks at it.
/// 7. It tells the friend to play the happy action.
/// 8. It plays the happy action. No step is the main step, so the last step is.
[[nodiscard]] std::vector<Step> BeHappyWithFriend(const Friends& friends);
constexpr float k_FaceFriendSeconds = 0.1f;

/// Having a poo with a friend: the first four steps of BeHappyWithFriend, then:
/// 5. It tells the friend to squat for a poo for four seconds, with a smile.
/// 6. It squats for a poo for four seconds, with a smile. This is the main step.
/// 7. It tells the friend to do its poo, with a smile.
/// 8. It does its poo, with a smile.
/// 9. It turns to face the friend for four seconds and up to four seconds more, with a smile, as it looks at it.
/// 10. It plays the happy action.
[[nodiscard]] std::vector<Step> PooWithFriend(const Friends& friends, const Random& random);
constexpr float k_PooTogetherSeconds = 4.0f;
constexpr float k_FaceAfterPooSeconds = 4.0f;
constexpr float k_FaceAfterPooExtraSeconds = 4.0f;

/// Sitting with a friend about a clear place (see AboutPlace): the first three steps of BeHappyWithFriend to these
/// places, then:
/// 4. It waits until the friend does not move, and flutters its eyelids as it looks at it.
/// 5. It tells the friend to turn to face it, for a tenth of a second.
/// 6. It turns to face the friend for a tenth of a second, as it looks at it.
/// 7. It tells the friend to sit for ten seconds and up to five seconds more.
/// 8. It sits for ten seconds and up to five seconds more, as it looks at the friend. This is the main step.
/// 9. It turns to face the friend for a tenth of a second, and flutters its eyelids as it looks at it.
/// 10. It plays the happy action.
[[nodiscard]] std::vector<Step> SitWithFriend(const Friends& friends, glm::vec2 middle, float radius, const Random& random);
constexpr float k_SitTogetherSeconds = 10.0f;
constexpr float k_SitTogetherExtraSeconds = 5.0f;

/// Sleeping with a friend about a clear place (see AboutPlace). Half the time it first yawns with sleepy eyes. Then the
/// first four steps of BeHappyWithFriend to these places, and:
/// 5. It tells the friend to turn to face it, for a tenth of a second.
/// 6. It turns to face the friend for a tenth of a second, and flutters its eyelids as it looks at it.
/// 7. It tells the friend to sleep for seventeen seconds.
/// 8. It sleeps for seventeen seconds. This is the main step. This sleep does not rest it.
/// 9. It plays the confused action with sleepy eyes and a grimace.
[[nodiscard]] std::vector<Step> SleepWithFriend(const Friends& friends, glm::vec2 middle, float radius, const Random& random);
constexpr float k_SleepTogetherSeconds = 17.0f;

/// Drinking with a friend at the coast nearest the creature, facing the sea nearest that coast. The friend's place is
/// its own height along the first axis of the map from the coast. The agenda has these steps:
/// 1. It asks the friend to be its friend, as it looks at it. If the friend refuses, the agenda stops.
/// 2. It tells the friend to go to its place, to within the friend's height.
/// 3. It goes to the coast, to within its own height, as it looks where it goes.
/// 4. It waits until the friend does not move, as it looks at it.
/// 5. It tells the friend to turn to face the sea.
/// 6. It turns to face the sea.
/// 7. It tells the friend to play the drink action.
/// 8. It plays the drink action. This is the main step.
/// 9. It tells the friend to drink.
/// 10. It drinks.
/// 11. It turns to face the friend for four seconds and up to four seconds more, with a smile, and flutters its eyelids
///     as it looks at it.
[[nodiscard]] std::vector<Step> DrinkWithFriend(const Friends& friends, glm::vec2 coast, glm::vec2 sea, const Random& random);
constexpr float k_FaceAfterDrinkSeconds = 4.0f;
constexpr float k_FaceAfterDrinkExtraSeconds = 4.0f;

/// The town of a joint action with a town: the town and its storage pit, by their entities' numbers, where the town is
/// and the radius of the pit on the ground
struct FriendsTown
{
	uint32_t town {0};
	uint32_t pit {0};
	glm::vec2 townPoint {0.0f};
	float pitRadius {0.0f};
};
/// Helping a town with a friend (desire Compassion), or attacking it with a friend (desire Anger). The agenda has these
/// steps:
/// 1. It asks the friend to be its friend, with a playful face, as it looks at it. If the friend refuses, the agenda
///    stops.
/// 2. It turns to face the friend for a tenth of a second, and flutters its eyelids as it looks at it.
/// 3. It plays the happy action.
/// 4. It points at the town for two seconds, with a playful face, as it looks at the town. This is the main step.
/// 5. It tells the friend to go near the storage pit, to twice the pit's radius and the creature's own height.
/// 6. It goes near the pit to the same distance, as it looks where it goes.
/// 7. It tells the friend to turn to face it, for a tenth of a second.
/// 8. It turns to face the friend for a tenth of a second.
/// 9. It tells the friend to have the desire about the town.
/// 10. It has the desire about the town.
/// The first of the two creatures to take up the desire stops the joint action, and so the agenda of the other one.
[[nodiscard]] std::vector<Step> TownWithFriend(const Friends& friends, const FriendsTown& town,
                                               creature_desires::Desire desire);
constexpr float k_PointAtTownSeconds = 2.0f;
constexpr float k_NearPitTimes = 2.0f;

} // namespace openblack::creature_mind
