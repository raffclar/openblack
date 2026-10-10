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

/// Looking something over: going near it (two and a half times its height away, or the thing's height when that is
/// more), turning to face it for two seconds and watching its foot for 1.1 more
[[nodiscard]] std::vector<Step> ExamineByLooking(uint32_t thing, float height, float thingHeight);
constexpr float k_ExamineHeights = 2.5f;
constexpr float k_ExamineFaceSeconds = 2.0f;
constexpr float k_ExamineWatchSeconds = 1.1f;

/// Smiling at a friend: going near it (three times its height away) and facing it four seconds and up to four more,
/// smiling at it with fluttering eyelids; then half the time a happy jump
[[nodiscard]] std::vector<Step> SmileAt(uint32_t friendly, float height, float chance, const Random& random);
constexpr float k_SmileHeights = 3.0f;
constexpr float k_SmileSeconds = 4.0f;
constexpr float k_SmileExtraSeconds = 4.0f;
constexpr uint32_t k_HappyAfterSmilingLots = 2;

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
} // namespace openblack::creature_mind
