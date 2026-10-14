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

#include <array>
#include <optional>

#include <glm/vec3.hpp>

/// The rules for what a creature does with its feet and its body to the things round it: a stomp, a kick, and the
/// spheres round the parts of its body that knock things about while it lies down, gets up, faints or fights.
namespace openblack::creature_stomp
{
/// A creature of size 1 is this tall
constexpr float k_HeightPerSize = 15.0f;
/// Before a stomp, the creature plays its angry animation one time in this number
constexpr uint32_t k_AngryBeforeStompLots = 5;
/// The crush that a stomp gives to a living thing that it lands on
constexpr float k_StompCrush = 1.0f;

/// The rule that decides if a creature can stomp on a kind of thing
enum class StompRule : uint8_t
{
	/// Fields, fish farms, vortices, teleport stones and anything that is not on the land
	Never,
	/// What lies about: mobile things, pots, piles, rocks, dead trees and the statics
	Always,
	/// Villagers and animals while they live
	WhileAlive,
	/// A bird only while it is lower over the land than a quarter of the height of the creature
	LowBird,
	/// Trees, features, other creatures and the like only while they are on the map and shorter than a fifth of the
	/// height of the creature
	Short,
	/// A building only while it is lower than a quarter of the height of the creature
	LowBuilding,
};
/// What the rule of a thing needs to know
struct StompTarget
{
	StompRule rule {StompRule::Never};
	bool dead {false};
	/// On the map: not in a hand, in the air or in a tornado
	bool onMap {true};
	float height {0.0f};
	float heightAboveLand {0.0f};
};
constexpr float k_ShortShare = 0.2f;
constexpr float k_LowShare = 0.25f;
/// Whether a creature of a size can stomp on a thing
[[nodiscard]] bool CanBeStompedOn(const StompTarget& target, float creatureSize);

/// The rule that decides if a creature can kick a kind of thing
enum class KickRule : uint8_t
{
	Never,
	/// A building or a field: only when it is fully built and not on fire
	Building,
	/// Any other thing on the land, by the rules of KickTarget
	Thing,
};
/// What the rule of a thing needs to know
struct KickTarget
{
	KickRule rule {KickRule::Never};
	/// Another creature: always, when the leash of the kicker is not tied to it
	bool creature {false};
	/// The leash of the kicker is tied to the thing
	bool leashTied {false};
	bool canFly {false};
	bool immovable {false};
	bool indestructible {false};
	/// The kicker holds the thing in its hand
	bool heldByKicker {false};
	float weight {0.0f};
	bool built {true};
	bool onFire {false};
};
/// A thing that weighs this number of times the weight of the creature, or more, is too heavy to kick
constexpr float k_KickWeightTimes = 2.0f;
/// Whether a creature of a weight can kick a thing
[[nodiscard]] bool CanBeKicked(const KickTarget& target, float creatureWeight);

/// The hit that a kick gives another creature: one tenth of the hit effect, and one tenth more as the kicker is tall
/// for the other creature, up to its full height
constexpr float k_KickHitShare = 0.1f;
[[nodiscard]] float KickHitStrength(float kickerHeight, float otherHeight);

/// A blow on a building: where it lands, how fast it pushes, and how far round it breaks
struct Blow
{
	glm::vec3 point {0.0f};
	glm::vec3 velocity {0.0f};
	float reach {0.0f};
};
/// What a kick on a building needs to know. The three draws are from the synced random numbers: from 0 to 10, then two
/// from 0 to 99.
struct BuildingKick
{
	glm::vec3 creature {0.0f};
	/// Where the foot of the kick is, and the way the kick goes (a length of 1)
	glm::vec3 foot {0.0f};
	glm::vec3 direction {0.0f};
	/// The way that the creature faces, in radians
	float yaw {0.0f};
	float creatureSize {0.0f};
	/// The radius of the building on the ground
	float buildingRadius {0.0f};
	uint32_t sideDraw {0};
	uint32_t firstHeightDraw {0};
	uint32_t secondHeightDraw {0};
};
/// A kick breaks a building with two blows, both at the middle between the creature and its foot. The first goes the
/// way of the kick, the second goes sideways from the way the creature faces. Each lands at a random height above the
/// land, not higher than the reach of the blow of the creature. The land height is the height of the land at that
/// middle point.
[[nodiscard]] std::array<Blow, 2> KickBlows(const BuildingKick& kick, float landHeight);
/// The reach of the blow of a creature on a building, for each unit of its size
constexpr float k_BlowReachPerSize = 3.75f;
constexpr float k_KickReachShare = 0.6f;
constexpr float k_KickBlowSpeed = 2.0f;
/// The draws of a kick on a building: the side from 0 to 10, and each height from 0 to 99
constexpr uint32_t k_KickSideDraws = 11;
constexpr uint32_t k_KickHeightDraws = 100;

/// The parts of the body that knock things about, each one a sphere round a bone
enum class Part : uint8_t
{
	Head,
	Groin,
	RightHand,
	LeftHand,
	RightFoot,
	LeftFoot,
	RightArmpit,
	LeftArmpit,
};
constexpr size_t k_PartCount = 8;
/// The radius of the sphere round a part of the body, for a creature of a size. The head and the limbs are big for a
/// small creature, as they are drawn.
[[nodiscard]] float SphereRadius(Part part, float creatureSize);

/// What a body action does to the spheres as it starts
enum class Switch : uint8_t
{
	/// No change
	Keep,
	On,
	Off,
};
/// The body action that starts, as the spheres know it
struct BodyStart
{
	/// The body stands, or walks, and plays no action
	bool stands {false};
	/// The animation that starts, and if it is the start of a sequence, its loop, or its end
	std::optional<size_t> animation;
	enum class Phase : uint8_t
	{
		Once,
		Start,
		Loop,
		End,
	};
	Phase phase {Phase::Once};

	constexpr bool operator==(const BodyStart&) const = default;
};
/// The spheres come on as the creature lies down to sleep, gets up from sleep, falls in a faint, and starts a fight or
/// a cast in a fight. They go off as it sleeps, lies in a faint, takes its stance in a fight, or stands.
[[nodiscard]] Switch SwitchFor(const BodyStart& start);

/// A part of the body knocks things only when it moved further than this number of times the size of the creature since
/// its last check
constexpr float k_MoveShare = 0.75f;
[[nodiscard]] bool MovedEnough(const glm::vec3& from, const glm::vec3& to, float creatureSize);

/// What a sphere can do to a thing in its cell
enum class Victim : uint8_t
{
	/// Nothing: trees, fields, storage pits, the town centre, creatures, birds, pieces of buildings and immovable things
	None,
	/// A villager or an animal, knocked through the air
	Living,
	/// A building, broken where the sphere hits it
	Building,
	/// Another thing that can fly, knocked through the air when part of its model is in the sphere
	Thing,
};
/// What the spheres need to know of a thing
struct VictimFacts
{
	bool hasModel {false};
	bool indestructible {false};
	bool immovable {false};
	/// Kinds that the spheres pass by: creatures, the heart of the temple, trees, storage pits, the town centre and the
	/// pieces of broken buildings
	bool passedBy {false};
	bool villager {false};
	/// An animal that is not a bird
	bool animal {false};
	/// Villagers, animals, birds and creatures
	bool living {false};
	/// A building other than a field
	bool building {false};
	bool canFly {false};
};
[[nodiscard]] Victim VictimOf(const VictimFacts& facts);
/// A villager, an animal or a thing goes when its distance on the ground is less than three tenths of the sphere radius
/// added to its own radius. A building breaks when its distance is less than the two radii added.
constexpr float k_KnockReachShare = 0.3f;
[[nodiscard]] bool InReach(Victim victim, float groundDistance, float sphereRadius, float thingRadius);
/// The speed of a knocked thing is this number of times the move of the part
constexpr float k_KnockSpeedPerMove = 7.0f;
[[nodiscard]] glm::vec3 KnockVelocity(const glm::vec3& move);
/// The blow on a building goes the way the part moved, at this speed
constexpr float k_TrampleBlowSpeed = 4.0f;
[[nodiscard]] Blow TrampleBlow(const glm::vec3& point, const glm::vec3& move, float sphereRadius);
} // namespace openblack::creature_stomp
