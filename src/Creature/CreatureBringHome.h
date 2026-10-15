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

#include <optional>
#include <vector>

#include <glm/vec2.hpp>

#include "Creature/CreatureIdleMind.h"

/// The actions with which a creature brings things to its home: a thing that is far from its home, food, and a handful
/// of wheat from a field. The creature puts the thing down at its home. The agendas and the rules here are pure, free
/// of the game.
namespace openblack::creature_bring_home
{
using creature_mind::Step;

/// The creature brings home only a thing that is more than this many metres from its home
constexpr float k_AwayFromHome = 50.0f;
/// When it brings a thing home, it stops within this many metres of its home, or nearer when its height is less
constexpr float k_NearestHome = 5.0f;
/// It turns to face a field for a tenth of a second before it takes wheat from the field
constexpr float k_FaceFieldSeconds = 0.1f;

/// Whether the creature can bring a thing home: the thing is more than k_AwayFromHome from its home, the thing is not
/// alive, and the creature can pick the thing up
[[nodiscard]] bool CanBeBroughtHome(float metresFromHome, bool living, bool canPickUp);

/// How near to its home the creature goes when it brings a thing home: its height, but not more than k_NearestHome
[[nodiscard]] float BringHomeWithin(float height);

/// The field that the creature takes wheat from, and where the field is
struct Field
{
	uint32_t field {0};
	glm::vec2 point {0.0f};
};

/// Bringing a thing home. When its hand is empty, the creature picks up the thing (pickUp) and looks at it as it goes
/// to it. Then it walks home to within BringHomeWithin of its height, and puts the thing down there. A living thing is
/// then made still: it starts to die (see Step::Kind::RenderImmobile). Only an order brings a living thing home.
[[nodiscard]] std::vector<Step> BringHome(std::optional<uint32_t> pickUp, uint32_t thing, bool living, glm::vec2 home,
                                          float height);

/// Bringing food home. When its hand does not hold food, the creature picks up the food (pickUp), and living food is
/// made still at once. Then it walks home to within its height, and puts the food down there.
[[nodiscard]] std::vector<Step> BringFoodHome(std::optional<uint32_t> pickUp, bool living, glm::vec2 home, float height);

/// Taking wheat from a field to its home. When its hand does not hold food, the creature walks to within its height of
/// the field and turns to face the field. Then the plan acts on the creature, and the creature takes a handful of wheat
/// from the field. After that, it walks home to within its height, and puts the wheat down there.
[[nodiscard]] std::vector<Step> TakeFoodFromFieldHome(const std::optional<Field>& take, glm::vec2 home, float height);

} // namespace openblack::creature_bring_home
