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

/// The desire to steal: a creature takes a thing that is not in the influence of its player, and brings the thing to
/// its temple. The rules and the agenda here are pure, free of the game.
namespace openblack::creature_steal
{
using creature_mind::Step;

/// The source of the desire to steal
inline constexpr uint32_t k_StealSource = 60;
/// Each time the creature sees a thing again that it can steal, the source of the desire to steal grows by this
inline constexpr float k_StealableSightingAmount = 0.3f;

/// What the creature knows about a thing when it decides if it can steal the thing
struct Facts
{
	/// The thing is a toy for this creature: the football, or a static toy
	bool toy {false};
	/// The thing is a scaffold
	bool scaffold {false};
	/// The thing is a one-shot miracle that lies on the land
	bool oneShot {false};
	/// The creature can pick the thing up
	bool canPickUp {false};
	/// The influence of the player of the creature at the place of the thing
	float ownInfluence {0.0f};
};

/// Whether the creature can steal the thing. The thing must be a toy, a scaffold or a one-shot miracle. The creature
/// must be able to pick it up, and the thing must be out of the influence of the player of the creature.
[[nodiscard]] bool IsStealable(const Facts& facts);

/// How near to its home the creature goes to put down a thing that it stole: two times its height
[[nodiscard]] float PutDownWithin(float height);

/// Stealing a thing and taking it to the temple. When its hand is empty, the creature picks up the thing (pickUp) and
/// looks at it as it goes to it. Then it walks home to within PutDownWithin of its height, and puts the thing down there.
/// The walk is the main step.
[[nodiscard]] std::vector<Step> StealAndPutByTemple(std::optional<uint32_t> pickUp, glm::vec2 home, float height);
} // namespace openblack::creature_steal
