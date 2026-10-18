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

#include <glm/ext/vector_int2.hpp>
#include <glm/mat3x3.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

#include "CreatureRoute.h"

/// How a creature lets go of what it holds: thrown at something, tossed away, lobbed or put down, and how the thing then
/// flies until it comes to rest on the ground
namespace openblack::creature_throw
{
/// The pull of gravity on what is thrown, in units a second each second
constexpr float k_Gravity = 9.81f;

/// The throwing animations, flat and high, blended by how high the target is; and those that let go of what is held:
/// tossing it away, eating it and putting it down. The species also have a gentle lob animation, but the game never
/// plays it.
constexpr size_t k_HurlFlat = 91;
constexpr size_t k_HurlHigh = 92;
constexpr size_t k_Discard = 95;
constexpr size_t k_Eat = 96;
constexpr size_t k_PutDown = 97;

/// How an animation that lets go of a held thing releases it
enum class LetGo : uint8_t
{
	/// It tosses the thing away at the moment of the toss, with a share of the speed of the hand
	Toss,
	/// It puts the thing down at rest at the moment of the put-down
	PutDownStill,
	/// It does not release the thing: the thing stays in the hand
	Keep,
};
/// How an animation releases the held thing. Only the toss animation and the put-down animation release it.
[[nodiscard]] constexpr LetGo LetGoOf(size_t animation)
{
	switch (animation)
	{
	case k_Discard:
		return LetGo::Toss;
	case k_PutDown:
		return LetGo::PutDownStill;
	default:
		return LetGo::Keep;
	}
}

/// Tossing something away, it leaves with this share of the hand's own speed at that moment
constexpr float k_DiscardSpeedShare = 0.6f;
/// The hand's speed is measured over this many milliseconds of the animation before it lets go
constexpr float k_HandSpeedSpanMs = 100.0f;
/// A creature throws nothing at anything nearer than this share of its height, 15 units at size 1
constexpr float k_MinThrowHeightShare = 0.66f;
constexpr float k_HeightAtSizeOne = 15.0f;

/// The game's flight time takes the distance along the ground times this, one over g as the game keeps it
constexpr float k_FlightTimePerUnit = 0.10193679481744766f;
/// How long something that a creature throws at a target takes to get there: the square root of the distance along the
/// ground times one over g. The game takes the distance from the target to the creature, with the square of z first.
[[nodiscard]] float FlightTime(const glm::vec3& creature, const glm::vec3& target);

/// The height of the land at a map position (x and z in 16.16 map units)
using MapAltitude = std::function<float(int32_t x, int32_t z)>;

/// A thing that a creature throws at, and how it moves. The game leads a villager that walks and a creature that walks.
/// It aims at the middle of the height of anything else, where it stands.
struct ThrowTarget
{
	enum class Kind : uint8_t
	{
		/// Anything that is not a villager or a creature
		Thing,
		Villager,
		Creature,
	};
	Kind kind {Kind::Thing};
	/// Where it stands, in 16.16 map units
	glm::ivec2 mapPosition {0};
	/// The height of a thing or a villager from its model, or the height of a creature from its size
	float height {0.0f};
	/// A villager: the step that its walk makes each turn, in map units. The game leads it only when its state is not a
	/// final state.
	glm::ivec2 step {0};
	bool walking {false};
	/// A creature: where its body is, its heading, and its speed in units a second
	glm::vec3 position {0.0f};
	float heading {0.0f};
	float speed {0.0f};
};
/// The game turns in one second, for the lead of a villager
constexpr int32_t k_LeadTurnsPerSecond = 10;
/// Where a target is after some seconds
[[nodiscard]] glm::vec3 PlaceIn(const ThrowTarget& target, float seconds, const MapAltitude& altitude);

/// Where a creature throws at a target, and how long the throw takes
struct ThrowAim
{
	glm::vec3 point;
	float seconds;
};
/// The aim at a target that can move:
/// 1. The flight time to where the target stands, and where the target is after that time.
/// 2. The flight time to that place, and where the target is after that second time.
/// The creature throws at that last place, and the throw takes the second time.
[[nodiscard]] ThrowAim AimAt(const glm::vec3& creature, const ThrowTarget& target, const MapAltitude& altitude);

/// The velocity that takes something from where the hand lets go of it to the target in a time, falling as it goes
[[nodiscard]] glm::vec3 ReleaseVelocity(const glm::vec3& target, const glm::vec3& release, float seconds);

/// Whether a target is far enough along the ground to throw at, for a creature of a size
[[nodiscard]] bool FarEnoughToThrow(float groundDistance, float size);

/// How much of the high throw to blend into the flat one, 0 to 1. Each slope is height over distance ahead: the
/// target's, and the hand's where it lets go in the flat and in the high throw.
[[nodiscard]] float HighThrowWeight(float targetSlope, float flatSlope, float highSlope);

/// The hand's velocity in units a second, from where it is when it lets go and where it was a span of milliseconds
/// before
[[nodiscard]] glm::vec3 HandVelocity(const glm::vec3& atRelease, const glm::vec3& before, float spanMs);

/// The bones whose middle is the point where the hand holds a thing and lets go of it: the bones at the ends of the skeleton's
/// branches that start at the hand and at each bone after it under the same parent. In the creatures' skeletons the bones under
/// one parent follow each other in the order of their numbers.
[[nodiscard]] std::vector<uint32_t> TossPointBones(std::span<const uint32_t> parents, uint32_t hand);

/// Where a held thing is in the world and how it turns, from the hand
struct HeldPose
{
	glm::mat3 rotation {1.0f};
	glm::vec3 position {0.0f};
};
/// How a held thing sits in the hand of a creature. The thing turns with the hand bone, which is in the world:
/// - Its side axis is the opposite of the hand's first axis.
/// - Its up axis is the hand's third axis, and its forward axis is the opposite of the hand's second axis.
/// - In the other hand, its up axis and its forward axis change their direction.
/// Its origin is the hang distance below the middle of the finger ends, along its up axis. A negative hang puts it
/// above. Each axis of the hand is made a unit vector by the 3D engine's inverse square root. The up axis is scaled by
/// the thing's scale before it is made the hang distance long again.
[[nodiscard]] HeldPose HeldPlacement(const glm::mat3& hand, const glm::vec3& fingerEnds, bool mirrored, float hang,
                                     float thingScale);

/// Where a released thing starts. It keeps the turn that it had in the hand. A thing let go of (tossed or put down)
/// starts where it hangs. A thrown thing starts with its origin at the finger ends.
[[nodiscard]] HeldPose ReleaseStart(const HeldPose& held, const glm::vec3& fingerEnds, bool thrown);

/// The velocity something tossed away leaves with: a share of the hand's own, measured in the creature's space, flipped
/// to the other side when tossed by the other hand and turned with the creature
[[nodiscard]] glm::vec3 TossVelocity(const glm::vec3& handVelocity, bool mirrored, const glm::mat3& rotation, float share);

/// Going to throw into something, a creature stands no nearer to it than the distance it was given, the thing's radius
/// and twice its height together. It stands no further than three and a half times its height. When it backs away to
/// throw, it goes to within twice its height of the point that it backs away to.
constexpr float k_ThrowReachHeights = 3.5f;
/// Walking up to throw where it may go no nearer than it may stand off, it may stand off this much more
constexpr float k_ThrowWalkUpSpare = 0.01f;

/// What a creature does to get where it can throw into something from: nothing when it is there already, backing away
/// to a point, or walking up to the thing until within a ring about it
struct ThrowStand
{
	enum class Kind : uint8_t
	{
		There,
		BackOff,
		WalkUp,
	};
	Kind kind {Kind::There};
	glm::vec2 point {0.0f};
	float minDistance {0.0f};
	float maxDistance {0.0f};
};
/// Where a creature of a height goes to throw into a thing of a radius, keeping the distance it was given. The game works
/// from the map positions of the two. None when the creature stands right on the thing's centre, with no way to back away.
[[nodiscard]] std::optional<ThrowStand> WhereToThrowFrom(glm::vec2 creature, glm::vec2 target, float height, float targetRadius,
                                                         float keep);
/// The point that a creature backs away to for a throw. A point that the creature may stand on is used as it is. If not,
/// it is the nearest place that it may stand on, when that is less than the valid point reach away. None when the search
/// finds no place, or the place is too far: the move then fails (deviation 79: the game tests a point that the search
/// did not write).
[[nodiscard]] std::optional<glm::vec2> BackOffPoint(const creature_route::WalkableLand& land, glm::vec2 point);

} // namespace openblack::creature_throw
