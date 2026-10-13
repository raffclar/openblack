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

#include <vector>

#include <glm/vec2.hpp>

#include "Creature/CreatureIdleMind.h"

/// The agendas of actions a creature carries out the same way whether it chose them or a script forced them: hurling one
/// thing at another, sleeping at home, looking something over, smiling or waving at someone, being frightened on the
/// spot, putting down what it holds, and lying dead for good.
namespace openblack::creature_mind
{
/// Hurling something at a thing: with its hand empty, a sixth of the time it shows its anger, amazed, then picks up what
/// it is to throw; then it goes to where it can throw at the thing and throws at it, angry, watching it
[[nodiscard]] std::vector<Step> HurlAt(uint32_t thrown, uint32_t target, glm::vec2 targetPoint, float height, bool handFull,
                                       const Random& random);
constexpr uint32_t k_AngryBeforeHurlingLots = 6;

/// Sleeping at home: half the time a sleepy yawn first; then walking home until within its height (no further than 5),
/// facing down the slope, sleeping until rested and getting up dazed with a grimace
[[nodiscard]] std::vector<Step> SleepAtHome(glm::vec2 home, float height, const Random& random);
constexpr uint32_t k_YawnBeforeSleepingLots = 2;
constexpr float k_FurthestFromHomeToSleep = 5.0f;

/// A poo at home: walking home until within two times its height, then the steps of a poo, which it does by itself
[[nodiscard]] std::vector<Step> PooAtHome(glm::vec2 home, float height, uint32_t self, const Random& random);
/// It poos at home only when it is this near to its home, in metres
constexpr float k_NearHomeToPoo = 20.0f;
constexpr float k_PooAtHomeHeights = 2.0f;

/// Sleeping by a thing, at a clear place found about it: walking there until within its own radius, half the time a
/// sleepy yawn, facing down the slope, sleeping until rested and getting up dazed with a grimace
[[nodiscard]] std::vector<Step> SleepByObject(glm::vec2 place, float radius, const Random& random);
/// The clear place is as wide as four times the creature's height
constexpr float k_SleepByObjectHeights = 4.0f;

/// Eating from a pile of food: walking to the pile until within a fifth more than both their radii, turning to face it
/// a tenth of a second, taking a handful of food from it, and eating that
[[nodiscard]] std::vector<Step> EatFromPile(uint32_t pile, glm::vec2 pilePoint, float pileRadius, float radius);
constexpr float k_EatFromPileReach = 1.2f;
constexpr float k_FacePileSeconds = 0.1f;

/// Looking something over: going near it (two and a half times its height away, or the thing's height when that is
/// more), turning to face it for two seconds, watching its foot for 1.1 more, then taking in what kind of thing it is
[[nodiscard]] std::vector<Step> ExamineByLooking(uint32_t thing, float height, float thingHeight);
constexpr float k_ExamineHeights = 2.5f;
constexpr float k_ExamineFaceSeconds = 2.0f;
constexpr float k_ExamineWatchSeconds = 1.1f;

/// Looking another creature over: going near it (four times its own height away), turning to face it a tenth of a
/// second, curious, then watching it, curious, for five seconds and up to ten more, after which it has looked it over
[[nodiscard]] std::vector<Step> InspectCreature(uint32_t other, float height);
constexpr float k_InspectHeights = 4.0f;
constexpr float k_InspectFaceSeconds = 0.1f;
constexpr float k_InspectSeconds = 5.0f;
constexpr float k_InspectExtraSeconds = 10.0f;

/// Looking at a thing flying through the air: half the time it first points at it two seconds, amazed, then it watches
/// it, amazed, while it flies, for up to two seconds and a draw of up to two more each turn; its head follows the thing
/// where it is, unless the creature holds it
[[nodiscard]] std::vector<Step> LookAtFlyingObject(uint32_t thing, const Random& random);
constexpr uint32_t k_PointAtFlyingLots = 2;
constexpr float k_PointAtFlyingSeconds = 2.0f;

/// Looking at the player's hand: watching it, curious, its head on the hand, for whole seconds more than those of a
/// turn and a half, unless the hand picking something up has the creature watch longer (five seconds at most)
[[nodiscard]] std::vector<Step> LookAtHand();
constexpr uint32_t k_LookAtHandTurns = 15;

/// Smiling at a friend: going near it (three times its height away) and facing it four seconds and up to four more,
/// smiling at it with fluttering eyelids; then half the time a happy jump
[[nodiscard]] std::vector<Step> SmileAt(uint32_t friendly, float height, float chance, const Random& random);
constexpr float k_SmileHeights = 3.0f;
constexpr float k_SmileSeconds = 4.0f;
constexpr float k_SmileExtraSeconds = 4.0f;
constexpr uint32_t k_HappyAfterSmilingLots = 2;

/// Showing off to the thing that the desire is about: going near it (three times its height away), turning to face it
/// for a tenth of a second while it watches it, then playing its look-at-me and, as random(2) gives 0 or not, its
/// impress or its summon. With no thing it plays the actions where it stands.
[[nodiscard]] std::vector<Step> ShowImpressiveAnimation(std::optional<uint32_t> thing, float height, const Random& random);
constexpr float k_ShowOffHeights = 3.0f;
constexpr float k_ShowOffFaceSeconds = 0.1f;

/// Waving at a thing: going near it (three and a half times its height away), smiling; turning to face it 1.4 seconds,
/// watching it; then a friendly wave
[[nodiscard]] std::vector<Step> WaveAt(uint32_t thing, float height);
constexpr float k_WaveHeights = 3.5f;
constexpr float k_WaveFaceSeconds = 1.4f;

/// Being frightened on the spot: a frightened start, three seconds looking frightened, and another start
[[nodiscard]] std::vector<Step> BeFrightenedOnTheSpot();
constexpr float k_FrightenedSeconds = 3.0f;

/// Putting down what it holds, as it does when told to
[[nodiscard]] std::vector<Step> PutDown();

/// Lying dead for good: falling down as in a faint and lying there with its eyes closed for ten days, unless something
/// else is given
[[nodiscard]] std::vector<Step> DeadForever();
constexpr float k_DeadForeverSeconds = 864000.0f;
/// Pointing out a lesson's highlight to the player: when the camera is more than 30 away it first looks at the camera
/// a second, curious, and plays its look-at-me; then it points at the highlight a second, amazed, and looks at the
/// camera two seconds more
[[nodiscard]] std::vector<Step> PointOutHighlight(uint32_t highlight, float cameraDistance);
constexpr float k_CallCameraDistance = 30.0f;
constexpr size_t k_LookAtMeAnimation = 69;
constexpr float k_CallCameraSeconds = 1.0f;
constexpr float k_PointOutSeconds = 1.0f;
constexpr float k_AfterPointingOutSeconds = 2.0f;

/// Having had a good look at a kind of thing, a creature is only a hundredth as keen to satisfy its curiosity with a
/// thing of that kind, unless the thing is doing something it finds interesting
constexpr float k_ObservedCuriosityShare = 0.01f;
[[nodiscard]] float CuriosityShare(bool observedKind, bool doingSomethingInteresting);
} // namespace openblack::creature_mind
