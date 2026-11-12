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

#include "Common/GameHeading.h"
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

/// The velocity that takes something from where the hand lets go of it to the target in a time, falling as it goes.
/// The game takes the fall as the time squared, times a half, times minus g. It takes the way less the fall on each
/// axis, and multiplies it by one over the time.
[[nodiscard]] glm::vec3 ReleaseVelocity(const glm::vec3& target, const glm::vec3& release, float seconds);

/// Whether a target is far enough along the ground to throw at, for a creature of a size
[[nodiscard]] bool FarEnoughToThrow(float groundDistance, float size);

/// How much of the high throw to blend into the flat one: (target slope - flat slope) / (high slope - flat slope), with
/// no limit. A slope is the height over the distance along the ground.
[[nodiscard]] float HighThrowWeight(float targetSlope, float flatSlope, float highSlope);

/// The hand in the flat and the high throw, measured one time from the two animations at the throw time
struct HurlHands
{
	/// The middle of the finger ends at the throw time, in the model's space at size 1
	glm::vec3 flat {0.0f};
	glm::vec3 high {0.0f};
	/// How fast the hand moves, from HurlHandSpeed
	float flatSpeed {0.0f};
	float highSpeed {0.0f};

	/// The slope of each throw. The game keeps each speed as a distance along the ground and a height that are the same
	/// value, with the height of the flat throw below the ground. The slopes are thus -1 and 1 (not a number when the
	/// hand does not move).
	[[nodiscard]] float FlatSlope() const { return -flatSpeed / flatSpeed; }
	[[nodiscard]] float HighSlope() const { return highSpeed / highSpeed; }
};
/// How fast the hand moves up and forwards in a throw: the length of the change of y and z over the last 100 ms before
/// the throw time (the squares of z first), times 10 and times 0.707
[[nodiscard]] float HurlHandSpeed(const glm::vec3& atThrow, const glm::vec3& before);
/// The slope that the game gives for the point where the hand lets go, at the start of a throw
constexpr float k_HurlReleaseSlope = 0.5f;
/// Where the hand lets go in the world: the blend of the two hands by the weight of the slope, flipped to the other side
/// for the other hand, made the creature's size and turned by the heading. The game keeps the cosine and the sine of
/// the heading as floats.
[[nodiscard]] glm::vec3 HurlReleasePoint(const HurlHands& hands, const glm::vec3& position, GameHeading heading, float scale,
                                         bool mirrored, float slope);

/// The speed along the ground and the height speed of a throw, from which the game chooses the blend of the two throws
struct HurlLift
{
	float speed {0.0f};
	/// The height speed, kept from minus the speed to the speed
	float rise {0.0f};
};
/// At the start of a throw, the game finds where the hand lets go (at the heading to the target, with the slope 0.5)
/// and the velocity from there to the target in the flight time. No limit keeps the time above 0.
[[nodiscard]] HurlLift LiftOf(const HurlHands& hands, const glm::vec3& position, float scale, bool mirrored,
                              const glm::vec3& target, float seconds);
/// The weight of the high throw in the blend: the slope of the throw against the slopes of the two hands
[[nodiscard]] float HurlHighWeight(const HurlHands& hands, const HurlLift& lift);
/// The milliseconds that the throw animations move on in a turn. Near the throw time the hand moves at the speed of the
/// throw: the turn's milliseconds times the throw's speed along the ground times 1.6, over the blended hand speed times
/// the scale. Further away the step goes to the clip step of the turn, which it reaches 400 ms from the throw time.
[[nodiscard]] int32_t HurlStepMs(const HurlHands& hands, const HurlLift& lift, float highWeight, float scale, int32_t turnMs,
                                 int32_t clipStepMs, int32_t timeMs, int32_t throwMs);
/// Whether the body ends the throw before the hand lets go of the thing. The body ends the throw when the time and the
/// step of the turn get to the length of the flat throw. It tests this before the step that lets go. Thus, when one step
/// goes past both the throw time and the end, the hand keeps the thing. A throw that let go already ends as usual.
[[nodiscard]] bool EndsBeforeRelease(int32_t timeMs, int32_t stepMs, int32_t flatLengthMs, bool released);

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
/// to the other side when tossed by the other hand, made the creature's size and turned by its heading. The game
/// multiplies by the share before the size. It keeps the cosine of the heading as a float and the sine wide.
[[nodiscard]] glm::vec3 TossVelocity(const glm::vec3& handVelocity, bool mirrored, GameHeading heading, float scale,
                                     float share);

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
/// The distances of a throw at a place, in heights of the creature: the creature stands this far from the place, and
/// when it backs away it may stop up to this far from the point that it backs away to
constexpr float k_ThrowAtPointHeights = 8.0f;
constexpr float k_ThrowAtPointBackOffHeights = 3.0f;
/// Where a creature of a height goes to throw at a place, keeping the distance it was given. Nearer than that distance it
/// backs away to that distance from the place. Further than eight times its height it walks up to that distance. The game
/// works from the map position of the creature. As in the game, a creature right on the place gets a point to back away to
/// that is not a number (vanilla-bugs 1890).
[[nodiscard]] std::optional<ThrowStand> WhereToThrowAt(glm::vec2 creature, glm::vec2 place, float height, float keep);
/// The point that a creature backs away to for a throw. A point that the creature may stand on is used as it is. If not,
/// it is the nearest place that it may stand on, when that is less than the valid point reach away. None when the search
/// finds no place, or the place is too far: the move then fails (deviation 79: the game tests a point that the search
/// did not write).
[[nodiscard]] std::optional<glm::vec2> BackOffPoint(const creature_route::WalkableLand& land, glm::vec2 point);

} // namespace openblack::creature_throw
