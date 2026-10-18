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
#include <glm/vec3.hpp>

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

/// Kissing a friend. The two stand the kissing distance from the point halfway between them (see
/// creature_kiss::KissPlaces). The agenda has these steps:
/// 1. It asks the friend to be its friend, as it looks at it. If the friend refuses, the agenda stops.
/// 2. It tells the friend to go to its place, to within a tenth of a metre.
/// 3. It goes to its own place, to within a tenth of a metre, with a face of compassion, as it looks at the place.
/// 4. It tells the friend to turn to face it, for a tenth of a second.
/// 5. It turns to face the friend for a tenth of a second, with a face of compassion, and flutters its eyelids.
/// 6. It tells the friend to kiss it.
/// 7. It kisses the friend. This is the main step.
/// 8. Half the time, it then turns to face the friend for ten seconds with a smile, fluttering its eyelids, and plays
///    the happy action.
/// middleHeight is the height of the point halfway between the two. The creature looks at its place at that height.
[[nodiscard]] std::vector<Step> KissFriend(const Friends& friends, float kissingDistance, float middleHeight,
                                           const Random& random);
constexpr float k_KissPlaceWithin = 0.1f;
constexpr float k_FaceAfterKissSeconds = 10.0f;

/// Talking to a friend, and telling a friend a joke. The agenda has these steps:
/// 1. It goes near the friend, to three times its own height, with a smile, and flutters its eyelids as it looks at it.
/// 2. It turns to face the friend for four seconds and up to four seconds more, with a smile, fluttering its eyelids.
///    This is the main step.
/// 3. It asks the friend to be its friend, as it looks at it. If the friend refuses, the agenda stops.
/// 4. It tells the friend to turn to face it for four seconds.
/// 5. It talks, and flutters its eyelids as it looks at the friend.
/// 6. To talk, it tells the friend to talk too. To tell a joke, it tells the friend to laugh.
[[nodiscard]] std::vector<Step> TalkToFriend(uint32_t self, uint32_t friendly, float height, const Random& random);
[[nodiscard]] std::vector<Step> TellFriendAJoke(uint32_t self, uint32_t friendly, float height, const Random& random);
constexpr float k_TalkNearHeights = 3.0f;
constexpr float k_FaceToTalkSeconds = 4.0f;
constexpr float k_FaceToTalkExtraSeconds = 4.0f;
constexpr float k_FriendFacesTalkerSeconds = 4.0f;
constexpr size_t k_Laugh = 218;

/// Dancing with a friend. The agenda has these steps:
/// 1. It goes near the friend, to two times its own height, as it looks at it.
/// 2. It turns to face the friend for a tenth of a second, and flutters its eyelids as it looks at it.
/// 3. It plays the happy action.
/// 4. It asks the friend to be its friend, as it looks at it. If the friend refuses, the agenda stops.
/// 5. It makes the friend stand still in its mind.
/// 6. It tells the friend to turn to face it, for a tenth of a second.
/// 7. It waits until the friend does not move, with a playful face, as it looks at it.
/// 8. It tells the friend to dance alone for twenty seconds.
/// 9. It dances alone for twenty seconds, with a playful face. This is the main step.
/// 10. It lets the mind of the friend go free again.
[[nodiscard]] std::vector<Step> DanceWithFriend(uint32_t self, uint32_t friendly, float height);
constexpr float k_DanceNearHeights = 2.0f;
constexpr float k_DanceTogetherSeconds = 20.0f;

/// The first steps of arguing with, moping about with and showing off to a friend:
/// 1. It asks the friend to be its friend, as it looks at it. If the friend refuses, the agenda stops.
/// 2. It tells the friend to go near it, to three times the height of the creature.
/// 3. It goes near the friend, to three times its own height, with the face of the action, as it looks at the friend.
/// 4. It tells the friend to turn to face it for four seconds and up to four seconds more.
/// Arguing with a friend then has these steps:
/// 5. It turns to face the friend for a second with a smile, and flutters its eyelids as it looks at it.
/// 6. It tells the friend to turn to face it for a second.
/// 7. It turns to face the friend for four seconds and up to four seconds more, angry, as it looks at it.
/// 8. It points at the friend for two seconds, angry.
/// 9. It tells the friend to play the angry action. Then it plays the angry action, angry.
/// 10. It tells the friend to wait for five seconds. Then it waits for five seconds.
/// 11. It tells the friend to wait for two seconds. Then it waits for two seconds with a smile. This is the main step.
/// 12. It turns to face the friend for a tenth of a second, fluttering its eyelids, and plays the happy action.
[[nodiscard]] std::vector<Step> ArgueWithFriend(uint32_t self, uint32_t friendly, float height, const Random& random);
/// Moping about with a friend, after the first four steps with a sad face:
/// 5. It turns to face the friend for four seconds and up to four seconds more, sad, fluttering its eyelids.
/// 6. It tells the friend to play the sad action.
/// 7. It plays the sad action, sad. This is the main step.
/// 8. Half the time, it turns to face the friend for a tenth of a second, fluttering its eyelids, and plays the happy
///    action.
[[nodiscard]] std::vector<Step> MopeAboutWithFriend(uint32_t self, uint32_t friendly, float height, const Random& random);
/// Showing off to a friend, after the first four steps with a smile:
/// 5. It turns to face the friend for four seconds and up to four seconds more, with a smile, fluttering its eyelids.
/// 6. It points at its home for three seconds and grimaces, as it looks at its home. This is the main step.
/// 7. It tells the friend to turn to face its home.
/// 8. It points at the friend for two seconds, sad.
/// 9. It tells the friend to play the confused action.
/// 10. It turns to face the friend for a tenth of a second, fluttering its eyelids, and plays the happy action.
[[nodiscard]] std::vector<Step> ShowOffToFriend(uint32_t self, uint32_t friendly, float height, glm::vec3 home,
                                                const Random& random);
constexpr float k_TogetherNearHeights = 3.0f;
constexpr float k_FaceTogetherSeconds = 4.0f;
constexpr float k_FaceTogetherExtraSeconds = 4.0f;
constexpr float k_FaceToArgueSeconds = 1.0f;
constexpr float k_PointAtFriendSeconds = 2.0f;
constexpr float k_ArgueWaitSeconds = 5.0f;
constexpr float k_AfterArgueSeconds = 2.0f;
constexpr float k_PointAtHomeSeconds = 3.0f;

/// Getting the attention of a friend that does something with another creature:
/// 1. It turns to face the friend for a tenth of a second, and flutters its eyelids as it looks at it.
/// 2. Half the time, it points at the friend for 0.4 seconds, sad.
/// 3. Half the time, it waves.
/// 4. A third of the time, it plays the action that asks the player to look at it.
/// 5. It turns to face the friend for a tenth of a second, fluttering its eyelids.
/// 6. It plays the sad action. This is the main step.
[[nodiscard]] std::vector<Step> GetAttentionFromFriend(uint32_t friendly, const Random& random);
constexpr float k_PointForAttentionSeconds = 0.4f;
constexpr size_t k_LookAtMe = 69;

/// Telling another creature to go away. Half the time it fights the creature. Else the agenda has these steps:
/// 1. It asks the creature to fight, with a playful face, as it looks at it. If the creature refuses, the agenda stops.
/// 2. It makes the creature stand still in its mind.
/// 3. It tells the creature to turn to face it, for a tenth of a second.
/// 4. It turns to face the creature for 1.4 seconds with a smile, as it looks at it.
/// 5. It plays the angry action, amazed.
/// 6. It points for three seconds at the place beyond the creature, and grimaces, as it looks at the place. The place is
///    as far from the creature as the creature is from it. This is the main step.
/// 7. It tells the creature to play the sad action. Then it plays the angry action, amazed.
/// 8. It tells the creature to go to that place, to within two times the height of the creature.
/// 9. It waits until the creature does not move, as it looks at it.
/// 10. It tells the creature to play the sad action. Then it plays the angry action, amazed.
/// 11. It lets the mind of the creature go free again.
/// The places are where the bodies of the two are.
[[nodiscard]] std::vector<Step> TellCreatureToSodOff(uint32_t self, uint32_t other, glm::vec3 selfPlace, glm::vec3 otherPlace,
                                                     float otherHeight, const Random& random);
constexpr float k_FaceToSodOffSeconds = 1.4f;
constexpr float k_PointAwaySeconds = 3.0f;
constexpr float k_GoAwayWithinHeights = 2.0f;

/// Examining another creature with a friend. The agenda has these steps:
/// 1. It asks the friend to play, with a playful face, as it looks at it. If the friend refuses, the agenda stops.
/// 2. It makes the friend stand still in its mind.
/// 3. It tells the friend to turn to face the other creature, for a tenth of a second.
/// 4. It turns to face the other creature for a tenth of a second, with a playful face, as it looks at it.
/// 5. It tells the friend to point at the other creature for a second.
/// 6. It points at the other creature for a second, with a playful face.
/// 7. Half the time, it tells the friend to play the happy action. Half the time, it plays the happy action.
/// It does not let the mind of the friend go free. The end of the joint action does that.
[[nodiscard]] std::vector<Step> ExamineOtherCreatureWithFriend(uint32_t friendly, uint32_t other, const Random& random);
constexpr float k_PointAtExaminedSeconds = 1.0f;

/// Sitting with a friend on top of a hill, about a clear place on the hill. The creature sits a quarter of the radius
/// along the first axis of the map from the middle, and the friend the same distance the other way. The agenda has
/// these steps:
/// 1. It asks the friend to be its friend, as it looks at it. If the friend refuses, the agenda stops.
/// 2. When it cannot see the friend, it goes near the friend, to three times its own height, as it looks at it.
/// 3. It turns to face the friend for a tenth of a second, and flutters its eyelids as it looks at it.
/// 4. Half the time, it plays the happy action.
/// 5. It tells the friend to go to its place, to within two and a half times the height of the creature.
/// 6. It goes to its own place, to within the same distance, as it looks where it goes.
/// 7. It waits until the friend does not move, as it looks at it.
/// 8. It tells the friend to face down the slope, and to sit for ten seconds and up to five seconds more.
/// 9. It faces down the slope, and sits for ten seconds and up to five seconds more. This is the main step.
[[nodiscard]] std::vector<Step> SitOnTopOfHillWithFriend(const Friends& friends, glm::vec2 middle, float radius,
                                                         bool seesFriend, const Random& random);
/// The radius of the clear place on the hill, from the radius of each of the two on the ground
[[nodiscard]] float HillTogetherRadius(float radius, float friendRadius);
constexpr float k_HillTogetherRadii = 4.0f;
constexpr float k_HillPlaceWithinHeights = 2.5f;
constexpr float k_HillFriendNearHeights = 3.0f;

/// Eating from a field with a friend. The friend stands ten metres from the field one way along the first axis of the
/// map, and the creature ten metres the other way. The agenda has these steps:
/// 1. It asks the friend to be its friend, with a playful face, as it looks at it. If the friend refuses, the agenda
///    stops.
/// 2. It tells the friend to go to its place, to within the height of the creature.
/// 3. It goes to its own place, to within its own height.
/// 4. It tells the friend to turn to face the field. Then it turns to face the field. Each turn lasts a tenth of a
///    second.
/// 5. It tells the friend to take a handful of wheat from the field. Then it takes a handful.
/// 6. It tells the friend to eat. Then it eats. Its meal is the main step.
/// 7. It turns to face the friend for a tenth of a second, and plays the happy action.
/// 8. It lets the mind of the friend go free.
/// 9. It turns to face the friend for a tenth of a second, fluttering its eyelids, and plays the happy action.
[[nodiscard]] std::vector<Step> EatFromFieldWithFriend(const Friends& friends, uint32_t field, glm::vec2 fieldPoint);
constexpr float k_FieldPlaceMetres = 10.0f;

/// Playing a throwing game with a friend, with a toy. The agenda has these steps:
/// 1. It goes near the friend, to five times its own height, as it looks at it.
/// 2. It turns to face the friend for a tenth of a second, with a smile, as it looks at it.
/// 3. It asks the friend to play, with a playful face, as it looks at it. If the friend refuses, the agenda stops.
/// 4. It makes the friend stand still in its mind.
/// 5. It turns to face the friend for a tenth of a second, fluttering its eyelids, and plays the happy action.
/// 6. It tells the friend to turn to face it, for a tenth of a second.
/// 7. When its hand holds nothing, it picks up the toy, as it looks at it.
/// 8. It throws the toy at the friend, at 0.82 of the height of the friend. This is the main step.
/// 9. It waits for three seconds.
/// 10. It tells the friend to pick up the toy, and waits until the friend does not move, as it looks at it.
/// 11. It tells the friend to throw the toy at it, at 0.82 of its own height, and waits until the friend does not move.
/// 12. It lets the mind of the friend go free again.
/// The places are where the bodies of the two are.
[[nodiscard]] std::vector<Step> PlayThrowingGameWithFriend(const Friends& friends, uint32_t toy, glm::vec3 selfPlace,
                                                           glm::vec3 friendPlace, bool handFull);
constexpr float k_ThrowGameNearHeights = 5.0f;
constexpr float k_ThrowAtHeightFraction = 0.82f;
constexpr float k_WaitAfterThrowSeconds = 3.0f;

/// Giving a toy to a friend. The agenda has these steps:
/// 1. It asks the friend to be its friend, as it looks at it. If the friend refuses, the agenda stops.
/// 2. It tells the friend to point at it for five seconds.
/// 3. When its hand holds nothing, it picks up the toy, as it looks at it.
/// 4. It goes right up to the friend, as it looks at it.
/// 5. It turns to face the friend for a tenth of a second, with a face of compassion, fluttering its eyelids.
/// 6. It puts the toy down, with a smile. This is the main step.
/// 7. It waits until the toy is on the map, as it looks at it.
/// 8. It tells the friend to pick up the toy.
/// 9. It turns to face the friend for a tenth of a second, with a face of compassion, as it looks at it.
/// 10. It waits for five seconds with a smile.
/// 11. It waits until the friend is free, four turns at least.
/// 12. It tells the friend to do one of the four actions with what it holds, chosen at random.
[[nodiscard]] std::vector<Step> GiveFriendToy(uint32_t self, uint32_t friendly, uint32_t toy, bool handFull,
                                              const Random& random);
constexpr float k_FriendPointsAtGiverSeconds = 5.0f;
constexpr float k_WaitAfterGiftSeconds = 5.0f;
constexpr uint32_t k_WaitForFreeTurns = 4;

/// The citadel that a creature shows a friend: where its heart is on the ground, and the radius of the heart
struct Citadel
{
	glm::vec3 point {0.0f};
	float radius {0.0f};
};
/// Showing its citadel to a friend. The agenda has these steps:
/// 1. When it cannot see the friend, it goes near the friend, to three times its own height, with a face of compassion.
/// 2. It turns to face the friend for a tenth of a second, with a face of compassion, as it looks at it.
/// 3. It asks the friend to be shown something, as it looks at it. If the friend refuses, the agenda stops.
/// 4. It tells the friend to go near the citadel, to five times the radius of the friend and 1.2 times the radius of
///    the heart.
/// 5. It goes near the citadel, to five times its own radius and 1.2 times the radius of the heart.
/// 6. It waits until the friend does not move, as it looks at it, and grimaces.
/// 7. It points at the citadel for three seconds, and grimaces, as it looks at it. This is the main step.
/// 8. It tells the friend to turn to face the head of the creature.
/// 9. It waits until the friend is free, four turns at least.
/// 10. It tells the friend to turn to face the citadel, and to wait for five seconds and up to twenty seconds more.
/// 11. It waits for five seconds and up to twenty seconds more, with a face of compassion, as it looks at its feet.
[[nodiscard]] std::vector<Step> ShowFriendCitadel(const Friends& friends, const Citadel& citadel, float radius,
                                                  float friendRadius, glm::vec3 head, bool seesFriend, const Random& random);
constexpr float k_CitadelNearRadii = 5.0f;
constexpr float k_CitadelHeartRadii = 1.2f;
constexpr float k_PointAtCitadelSeconds = 3.0f;
constexpr float k_ShowCitadelWaitSeconds = 5.0f;
constexpr float k_ShowCitadelWaitExtraSeconds = 20.0f;

/// Whether a creature can see an object: when the object is less than ten metres away, or less than fifty times the
/// height of the object
[[nodiscard]] bool CanSee(float distance, float objectHeight);
constexpr float k_SeenWithinMetres = 10.0f;
constexpr float k_SeenWithinHeights = 50.0f;

} // namespace openblack::creature_mind
