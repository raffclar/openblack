/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <optional>
#include <vector>

#include <glm/vec2.hpp>

#include "Creature/CreatureIdleMind.h"

/// The agendas of the odd actions of a creature: it shows a lesson that it learnt, it looks confused at the player, and
/// it behaves strangely after too many magic mushrooms
namespace openblack::creature_odd
{
using creature_mind::Random;
using creature_mind::Step;

/// The creature scratches its head once and smiles
[[nodiscard]] std::vector<Step> ShowLearntLesson();

/// The creature turns to the camera, scratches its head and then looks confused, puzzled and with its eyes on the
/// camera. Without a camera the creature does not turn.
[[nodiscard]] std::vector<Step> LookConfused(std::optional<glm::vec2> camera);

/// The seconds that a creature that behaves strangely stares at the camera, and the seconds that it lies in a faint
/// at the least and the seconds more that it can lie at random
constexpr float k_StrangeStareSeconds = 5.0f;
constexpr float k_StrangeFaintSeconds = 5.0f;
constexpr float k_StrangeFaintExtraSeconds = 5.0f;
/// The life that a creature has when it gets up from that faint
constexpr float k_StrangeReviveLife = 0.4f;

/// The creature stares at the camera with stoned eyes and is happy. One time in two it then faints, lies still for a
/// time and gets up. At the end it looks confused two times, the first time with sleepy eyes.
[[nodiscard]] std::vector<Step> BehaveStrangely(float turnsPerSecond, const Random& random);

} // namespace openblack::creature_odd
