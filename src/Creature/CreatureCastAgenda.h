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

#include "CreatureIdleMind.h"

// How a creature casts a miracle at something. One time in five it first shows how it feels: angry before a lightning
// bolt or another miracle that attacks, kind before a helpful miracle, playful before one it casts on another creature.
// Then it goes near the thing, gets away from it to a distance, turns to face it and draws the miracle's gesture, if it
// has one. Last it takes up the casting pose and casts the miracle as the pose's loop begins, holds it three seconds and
// lets it go as the pose ends.
//
// A lightning bolt is cast from fifty away, backing off to ten more than the creature's height; a helpful miracle from
// twice its height; a creature spell from five times its height, backing off to twice it. The creature casts a
// tornado, the lightning storm and the power-ups of the explosion from five times its height, backing off to twice it.
// It casts an explosion from a hundred away, backing off to twice its height, and shows its anger first one time in
// three.
//
// The creature casts a fireball in a different way. First it goes to a clear area near where it stands, as wide as
// five times its radius, to within twice its height of the middle of the area. Then one time in five it shows its
// anger. At a living thing it then gets away to twice its height. Then it turns, draws the gesture and casts. Pure,
// tested on its own.

namespace openblack::creature_mind
{

/// The kinds of casting, by the miracles cast that way
enum class CastStyle : uint8_t
{
	/// The lightning bolts
	Lightning,
	/// The heals, and other miracles that help
	Helpful,
	/// The spells cast on another creature
	Playful,
	/// The tornado, the lightning storm and the power-ups of the explosion
	Aggressive,
	/// The explosion
	Explosion,
};

/// It shows how it feels first one time in this many
inline constexpr uint32_t k_CastEmoteLots = 5;
/// The casting pose's start, loop and end
inline constexpr std::array<size_t, 3> k_CastPose {40, 41, 42};
/// The miracle is held this long once cast
inline constexpr float k_CastHoldSeconds = 3.0f;
/// Turning to face what it casts at, it holds still this long once facing it
inline constexpr float k_CastSettleSeconds = 0.1f;
/// A lightning bolt is cast from this far, backing off to this much more than its height
inline constexpr float k_LightningCastDistance = 50.0f;
inline constexpr float k_LightningBackOff = 10.0f;
/// An explosion is cast from this far, and its creature shows its anger first one time in this many
inline constexpr float k_ExplosionCastDistance = 100.0f;
inline constexpr uint32_t k_ExplosionEmoteLots = 3;
/// The clear area for a fireball is this many times as wide as the radius of the creature
inline constexpr float k_FireballClearRadii = 5.0f;

/// Casting a miracle of a magic type, by its number, with a gesture to draw (0 for none), at an object by its entity's
/// number, by a creature of a height
[[nodiscard]] std::vector<Step> CastAt(CastStyle style, uint32_t magicType, uint32_t gesture, uint32_t object, float height,
                                       const Random& random);

/// Casting a fireball, by its magic type's number, with a gesture to draw (0 for none), at an object by its entity's
/// number, by a creature of a height. The creature goes first to the middle of a clear area near it. It gets away from
/// the object only when the object is living.
[[nodiscard]] std::vector<Step> CastFireballAt(uint32_t magicType, uint32_t gesture, uint32_t object, bool objectLiving,
                                               glm::vec2 clearArea, float height, const Random& random);

/// What a creature does when its try at a miracle fizzles: it is embarrassed, then one time in two it is sad too. The
/// sadness grows by the chance from 0 to 1 times this.
inline constexpr float k_SadnessFromFizzle = 0.3f;
struct FizzleReaction
{
	std::vector<Step> agenda;
	/// How much its sadness grows, none when it is not sad
	std::optional<float> sadness;
};
[[nodiscard]] FizzleReaction ReactToFizzle(const Random& random, float chance);

} // namespace openblack::creature_mind
