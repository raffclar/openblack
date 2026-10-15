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

#include <functional>
#include <optional>
#include <span>
#include <vector>

#include <glm/vec2.hpp>

#include "Creature/CreatureIdleMind.h"

/// What a creature does when it has no way to its destination:
/// - When the route search finds no way, the creature gives up what it does. It leaves the thing that it went to alone
///   for a time and makes a sound of refusal.
/// - When the first turn of the search stops far short of the destination, the creature is shut in. It clears one of
///   the things that the search met: it throws the thing, kicks it or casts an attack miracle at it. Then it walks on
///   to the destination.
namespace openblack::creature_clear_way
{
/// The creature keeps at most this many of the things that it met and can clear
constexpr size_t k_MostToClear = 16;
/// One time in this many, it clears any of the things that it keeps. Else it clears one of the first few.
constexpr uint32_t k_AnyOneIn = 4;
constexpr size_t k_FirstFew = 3;
/// After it clears the way, it walks on to within its height of the destination, or within this many metres when its
/// height is less
constexpr float k_LeastWalkOnDistance = 8.0f;

/// What the creature knows about a thing in its way
struct Obstacle
{
	/// The heart of a temple, a storage pit or a town centre: the creature never clears these
	bool templeHeart {false};
	bool storagePit {false};
	bool townCentre {false};
	/// Whether the creature can pick the thing up, and whether it can kick the thing
	bool canPickUp {false};
	bool canKick {false};
};

/// A miracle, in the order of the miracle types, as the creature knows it
struct Miracle
{
	bool known {false};
	/// The desire that the miracle serves, and the creature action that casts it (0 for none)
	uint32_t desire {0};
	uint32_t action {0};
};

/// The actions of the action table that clear a thing: a throw and a kick
struct Actions
{
	uint32_t throwAround {0};
	uint32_t kick {0};
};

/// The action that clears a thing out of the way, none when the creature cannot clear it:
/// 1. The creature does not clear the heart of a temple, a storage pit or a town centre.
/// 2. It throws a thing that it can pick up.
/// 3. It kicks a thing that it can kick.
/// 4. It casts the first miracle that it knows which serves its anger.
[[nodiscard]] std::optional<uint32_t> ClearingAction(const Obstacle& obstacle, std::span<const Miracle> miracles,
                                                     const Actions& actions);

/// The things that the creature keeps from the things that its search met, in the order that it met them: only the
/// things that it can clear, at most k_MostToClear of them
[[nodiscard]] std::vector<uint32_t> ThingsToClear(std::span<const uint32_t> met,
                                                  const std::function<bool(uint32_t)>& clearable);

/// The index of the thing that the creature clears, none when it keeps no thing. The random function gives a whole
/// number below its argument.
[[nodiscard]] std::optional<size_t> Choose(size_t count, const std::function<uint32_t(uint32_t)>& random);

/// How near the creature walks on to the destination, by its height
[[nodiscard]] float WalkOnDistance(float height);

/// The walk on to the destination after the creature clears the way
[[nodiscard]] std::vector<creature_mind::Step> WalkOn(glm::vec2 destination, float height);
} // namespace openblack::creature_clear_way
