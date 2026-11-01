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

#include <bit>
#include <optional>
#include <span>

#include <entt/entity/entity.hpp>
#include <glm/mat3x3.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

#include "Common/GameHeading.h"
#include "Enums.h"

/// How the scripts read and write a thing's properties, where the answer is a rule of its own
namespace openblack::script::property_rules
{

/// A building of a town, as the scripts weigh whether the town is wholly destroyed: its life (0 to 1), whether it is a
/// field, and how much of it is built (0 to 1)
struct TownBuilding
{
	float life {1.0f};
	bool field {false};
	float built {1.0f};
};

/// A building still stands while more than this much of it is built (or it is finished)
constexpr float k_StandingBuilt = 0.1f;

/// A town is wholly destroyed when none of its buildings stands: one stands while it has life, isn't a field and is
/// more than a tenth built. A town without buildings is destroyed.
[[nodiscard]] bool TownCompletelyDestroyed(std::span<const TownBuilding> buildings);

/// A thing's player as the scripts number them: 1 for a wholly destroyed town, whoever it belonged to; nothing for a
/// thing of no player or of the neutral player; otherwise the player's number counted from 1
[[nodiscard]] float PlayerProperty(std::optional<PlayerNames> player, bool destroyedTown);

/// Whether a thing is in a creature's hand, as the scripts ask: it is what the creature carries, or what it is eating
[[nodiscard]] bool InCreatureHand(entt::entity thing, entt::entity carried, std::optional<entt::entity> eating);

/// A creature's need that the scripts can set
enum class CreatureNeed : uint8_t
{
	Warmth,
	Energy,
	Itchiness,
	Poo,
	Exhaustion,
	Dehydration,
};

/// The value a creature's need takes when a script sets it: warmth is kept between -1 and 1, energy between 0 and 1,
/// the others are taken as they are
[[nodiscard]] float SetNeed(CreatureNeed need, float value);

/// The scripts give and take angles in degrees; the game turns them to and from radians with these factors
constexpr float k_RadiansToTurns = 0.159154937f;
constexpr float k_DegreesInATurn = 360.0f;
constexpr float k_DegreesToRadians = 0.0174532924f;
[[nodiscard]] float AngleToScript(float radians);
[[nodiscard]] float AngleFromScript(float degrees);

/// The angle about the upright a thing at `from` turns to so that it faces `to`, from 0 up to a whole turn: the way
/// from one to the other across the ground, measured from the x axis towards the z axis
constexpr float k_WholeTurnRadians = 6.28318548f;
[[nodiscard]] float FacingAngle(glm::vec2 from, glm::vec2 to);

/// The point at a distance from `to`, on the line from `from` through `to`, turned by an angle in degrees about the
/// upright. The angle turns the x axis towards the z axis. When the two points are the same, the line has no length,
/// and the point does not move.
[[nodiscard]] glm::vec3 TargetRelativePosition(glm::vec3 from, glm::vec3 to, float distance, float angleDegrees);

/// Whether a script may set a thing's life: a thing the scripts hold while they own the widescreen bars, and a thing
/// made indestructible, can't be brought to a hundredth of its life or under it
constexpr float k_LowestProtectedLife = 0.01f;
[[nodiscard]] bool CanSetLife(float life, bool heldDuringCutscene, bool indestructible);

/// A creature of size 1 stands this tall to the scripts
constexpr float k_CreatureHeightPerSize = 15.0f;
/// The size a creature takes for a height a script gives, as the game divides by its height per size
constexpr float k_CreatureSizePerHeight = 0.0666666701f;
[[nodiscard]] float CreatureHeight(float size);
[[nodiscard]] float CreatureSizeForHeight(float height);

/// A placed thing's angles in radians: its lean about the across axis, its turn about the upright axis and its lean
/// about the forward axis. The world's things are placed turned the other way by them.
struct Angles
{
	float x {0.0f};
	float y {0.0f};
	float z {0.0f};
};
/// The angles of a thing placed the way it faces, as the game finds them from its turn. A thing that does not lean keeps
/// its whole turn, past a quarter either way.
[[nodiscard]] Angles PlacedAngles(const glm::mat3& rotation);
/// The way a thing placed at these angles faces: the game's turn about z, then about x, then about y
[[nodiscard]] glm::mat3 PlacedRotation(const Angles& angles);

/// A living thing's angle as the scripts take it is the way it faces across the land, in radians from +x towards +z.
/// Villagers and animals keep their facing that way. A creature's heading is a quarter turn back from it: these turn one
/// into the other, the angle kept from 0 to a whole turn.
[[nodiscard]] float CreatureHeadingToLivingAngle(GameHeading heading);
[[nodiscard]] GameHeading LivingAngleToCreatureHeading(float angle);
/// Whether a living thing moved in its last turn: the game compares where it stood across the land, in whole map units
/// (a 65536th of ten metres), and not its height
[[nodiscard]] bool MovedAcross(const glm::vec3& before, const glm::vec3& after);

/// The scripts number the creature species as the game's creature tables do: the giant ape first, then the cow to the
/// gorilla in openblack's order. None for a number past the last.
[[nodiscard]] std::optional<CreatureType> CreatureTypeFromScript(uint32_t type);
[[nodiscard]] uint32_t ScriptCreatureType(CreatureType species);

/// A thing's belief in a player, as the scripts ask it: a town's belief in the player (none it was given is no belief);
/// anything else believes wholly in its own player and not at all in the others
[[nodiscard]] float BeliefForPlayer(bool town, std::optional<float> townBelief, std::optional<PlayerNames> owner,
                                    PlayerNames player);

/// A creature's speed as a script sets it is the share of its top speed it is asked to go at, from none to all of it
[[nodiscard]] float CreatureSpeedFraction(float speed);

/// A creature a script makes from another is as big and as strong as that one's own size and strength times the scale
/// the script gives
struct CopiedBody
{
	float size;
	float strength;
};
[[nodiscard]] CopiedBody CopyBody(float size, float strength, float scale);
/// One of the seven places for a player: whether the player of that place has a creature
struct PlayerPlace
{
	bool hasCreature;
};
/// Who a creature a script makes from another belongs to: the first of the seven players, in order, who has no creature
/// of their own. A place that nobody plays also gets the copy. It is none when each of the seven players has a creature.
[[nodiscard]] std::optional<PlayerNames> OwnerOfCopy(std::span<const PlayerPlace> places);

/// The velocity a script sets a thing off at towards a place: its speed along the line from the thing to the place, and
/// none when the thing is at the place already
[[nodiscard]] glm::vec3 HeadingVelocity(const glm::vec3& from, const glm::vec3& to, float speed);
/// A thing a script sets off from rest tumbles: about each of its axes it spins at a draw of 0 to 200 taken down by 100,
/// a hundredth of that, so -1 to 1
[[nodiscard]] float HeadingSpin(float draw);
/// The largest draw each axis of that spin takes
constexpr float k_HeadingSpinDraw = 200.0f;
/// The value that the spin takes away from the draw
constexpr float k_HeadingSpinMiddle = 100.0f;
/// The part of the draw that the spin keeps (the bits of the game's 0.01)
inline constexpr float k_HeadingSpinScale = std::bit_cast<float>(0x3C23D70Au);

/// The fall of a thing that a script throws at a place, in metres a second each second (the bits of the game's -9.81)
inline constexpr float k_TargetGravity = std::bit_cast<float>(0xC11CF5C3u);
/// The velocity that a script throws a thing at so that it gets to a place after a time, with no air drag: the
/// distance across the land in that time, and the height with the fall in that time added
[[nodiscard]] glm::vec3 TargetVelocity(const glm::vec3& from, const glm::vec3& to, float seconds);

} // namespace openblack::script::property_rules
