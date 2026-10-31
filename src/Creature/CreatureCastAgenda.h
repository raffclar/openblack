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

#include <array>
#include <functional>
#include <optional>
#include <span>
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
	/// A spell that the creature casts on another creature to amuse it
	Amusing,
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

/// The start, loop and end of the pose in which the creature scatters a miracle from its hands. It casts food and water
/// in this pose.
inline constexpr std::array<size_t, 3> k_ScatterPose {46, 47, 48};
/// The creature casts food from this distance from the thing
inline constexpr float k_FoodCastDistance = 2.0f;
/// The creature casts food from this distance from a need flag of a worship site
inline constexpr float k_FoodCastNeedFlagDistance = 20.0f;
/// To stop its own fire, the creature goes to a clear area. The area is this many times as wide as the creature is
/// tall.
inline constexpr float k_WaterOnItselfClearHeights = 1.7f;
/// The creature casts a miracle to impress from this many times its height. One time in this many it points at the
/// thing first, and in the other times it points after the cast. One time in this many it does not beckon.
inline constexpr float k_ImpressCastHeights = 1.5f;
inline constexpr uint32_t k_ImpressPointLots = 2;
inline constexpr uint32_t k_ImpressBeckonLots = 3;
/// The creature points at the thing for this long
inline constexpr float k_ImpressPointSeconds = 1.0f;
/// The miracles that a creature can cast to impress, in the order that it examines them
inline constexpr std::array<uint32_t, 4> k_ImpressiveMiracles {10, 16, 11, 24};
/// The miracles that let a creature choose to impress with a miracle. The list has food before the others, but the
/// creature never casts food to impress.
inline constexpr std::array<uint32_t, 5> k_ImpressMiraclesItMayKnow {14, 10, 16, 11, 24};

/// The creature casts food at a thing from near it. It does not show how it feels first, and it does not get away
/// from the thing. It turns, draws the gesture (0 for none) and casts in the scatter pose. It stays further from a need
/// flag of a worship site.
[[nodiscard]] std::vector<Step> CastFoodAt(uint32_t magicType, uint32_t gesture, uint32_t object, bool needFlag = false);

/// The creature casts a shield at a thing from where it stands. It turns to face the thing and casts.
[[nodiscard]] std::vector<Step> CastShieldAt(uint32_t magicType, uint32_t object);

/// The creature casts water at a thing. It goes near to twice its height, turns, and casts in the scatter pose. It does
/// not draw a gesture.
[[nodiscard]] std::vector<Step> CastWaterAt(uint32_t magicType, uint32_t object, float height);

/// The creature stops its own fire with water. It goes to the middle of a clear area, to within twice its height.
/// It casts the water at itself in the scatter pose. Then its fire stops.
[[nodiscard]] std::vector<Step> CastWaterOnItself(uint32_t magicType, uint32_t self, glm::vec2 clearArea, float height);

/// The creature casts a forest from this many times its height, and holds the miracle this long
inline constexpr float k_ForestCastHeights = 2.5f;
inline constexpr float k_ForestHoldSeconds = 4.0f;

/// The creature casts a forest at a place. It goes near to two and a half times its height from the thing (a villager),
/// and turns to face it. Then it casts the forest at the place in the scatter pose and holds it four seconds. It does
/// not draw a gesture.
[[nodiscard]] std::vector<Step> CastForestAt(uint32_t magicType, uint32_t object, glm::vec3 place, float height);

/// The creature casts a miracle to impress the people of a thing (a villager). It goes near to one and a half times its
/// height and gets away to twice its height. One time in two it points at the thing first. Two times in three it
/// beckons. It turns and casts the miracle, if it has one. If it did not point first, it points now.
[[nodiscard]] std::vector<Step> CastToImpress(std::optional<uint32_t> magicType, uint32_t object, float height,
                                              const Random& random);

/// The miracle that a creature casts to impress, by its place in k_ImpressiveMiracles. The creature must know the
/// miracle. Of these, it picks the miracle that any creature cast to impress the longest time ago. For equal times, it
/// picks the last. None if it knows none of them.
[[nodiscard]] std::optional<size_t> PickImpressiveMiracle(const std::array<bool, 4>& known,
                                                          const std::array<uint32_t, 4>& castTurn);

/// Whether a creature can choose to impress with a miracle, by its state for each of k_ImpressMiraclesItMayKnow. For one
/// of them, the creature must know it and can pay for it. It must also have seen it more than half the times that it
/// must see it to learn it.
struct ImpressiveMiracleState
{
	bool known {false};
	bool canPay {false};
	float seen {0.0f};
	float needed {0.0f};
};
[[nodiscard]] bool CanCastToImpress(std::span<const ImpressiveMiracleState> miracles);

/// The first and the last magic types of the spells on a creature. The creature casts one of these to amuse another
/// creature.
inline constexpr uint32_t k_FirstAmusingSpell = 26;
inline constexpr uint32_t k_LastAmusingSpell = 41;
/// To amuse another creature, the creature casts from this many times its height
inline constexpr float k_AmusingCastHeights = 4.0f;
/// A spell that the creature can cast to amuse: it knows the spell and can pay for it. The gesture is 0 for none.
struct AmusingSpell
{
	uint32_t magicType {0};
	uint32_t gesture {0};
};
/// The creature casts a spell to amuse another creature. It picks one of the spells at random. Then one time in five it
/// shows that it feels playful. It goes near to four times its height and gets away to twice its height. It turns,
/// draws the gesture and casts. With no spell, it only shows that it feels playful.
[[nodiscard]] std::vector<Step> CastAmusingAt(std::span<const AmusingSpell> spells, uint32_t object, float height,
                                              const Random& random);
/// Whether another creature seems friendly to the creature, by the desire that each creature wants most to do about the
/// other. It does not seem friendly when the creature is angry with it or afraid of it, or when the other creature is
/// angry with the creature. None is a creature that wants nothing of the other.
[[nodiscard]] bool SeemsFriendly(std::optional<creature_desires::Desire> mine, std::optional<creature_desires::Desire> theirs);

/// A creature goes this near the thing that it casts a one-shot miracle at
inline constexpr float k_OneShotCastDistance = 50.0f;
/// A one-shot miracle that the creature casts at a thing, by entity numbers
struct OneShotCast
{
	/// The first miracle of the seed in the globe
	uint32_t magicType {0};
	uint32_t target {0};
	/// The creature's height
	float height {0.0f};
	/// The globe that it picks up first, none when it holds the globe or its hand holds something else
	std::optional<uint32_t> pickUp;
	/// The globe that goes from the world after the cast, none for a globe in its hand
	std::optional<uint32_t> used;
	/// Whether it gets away from the thing to twice its height before it turns
	bool getAway {true};
	/// The face that it pulls as it goes near the thing and as it turns to face the thing
	creature_face::Cue face {creature_face::Cue::Compassion};
};
/// The creature casts a one-shot miracle at a thing. The steps are these:
/// 1. It picks up the globe, when it must. It looks at the globe as it goes to the globe.
/// 2. It goes near the thing, and looks at the thing as it goes. It gets away from the thing when it must, and looks at
///    the thing.
/// 3. It turns to face the thing, and looks at the thing.
/// 4. What its hand holds goes from the world.
/// 5. It casts the miracle from the casting pose.
/// 6. The globe that it used goes from the world.
[[nodiscard]] std::vector<Step> CastOneShotAt(const OneShotCast& cast);

/// What a creature does when its try at a miracle fizzles: it is embarrassed, then one time in two it is sad too. The
/// sadness grows by a random number from 0 to this.
inline constexpr float k_SadnessFromFizzle = 0.3f;
/// A random number from 0 to the given value
using FizzleFloatRandom = std::function<float(float most)>;
struct FizzleReaction
{
	std::vector<Step> agenda;
	/// How much its sadness grows, none when it is not sad
	std::optional<float> sadness;
};
[[nodiscard]] FizzleReaction ReactToFizzle(const Random& random, const FizzleFloatRandom& floatRandom);

/// What a creature keeps of its tries that fizzled, by entity numbers: the thing of the last try at a thing that
/// fizzled, and whether it has yet to show its feelings about a try that fizzled
struct FizzleRecord
{
	std::optional<uint32_t> lastThing;
	bool pending {false};

	bool operator==(const FizzleRecord&) const = default;
};
/// The record after a thing goes from the world. When the thing is the thing of the last try that fizzled, the creature
/// forgets the record: it keeps no thing for a later try at a place, and it does not show the feelings that it has yet
/// to show.
[[nodiscard]] FizzleRecord ForgetGoneThing(const FizzleRecord& record, uint32_t gone);

} // namespace openblack::creature_mind
