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
#include <optional>
#include <vector>

#include "Creature/CreatureDesires.h"
#include "Creature/LeashRope.h"
#include "Enums.h"

/// How the player leads a creature on a leash. There are three leashes: the learning leash (a plain rope), which makes
/// the creature watch and copy the player more keenly, and the aggression and compassion leashes, which make it angry or
/// kind for as long as it wears them. Held in the hand, the leash is as long as the creature is big; tied to something,
/// it is as long as the creature is far from it. When the creature is further than the full length of the leash from the
/// hand, or from the thing that holds the leash, it stops what it does and walks back.
namespace openblack::creature_leash
{

/// The leashes in the order the citadel hangs them and the creature knows them
constexpr std::array<LeashType, 3> k_Types = {LeashType::Evil, LeashType::Rope, LeashType::Good};
/// A leash's place in k_Types, or none for no leash
[[nodiscard]] std::optional<size_t> IndexOf(LeashType type);
[[nodiscard]] const char* Name(LeashType type);

/// The rope's length at rest and pulled fully taut
struct Lengths
{
	float slack;
	float max;
};
/// Held in the hand, the leash is as long as the creature is big: with s fifteen times its size, slack is 0.7 s + 22
/// and full length 3 s + 32
[[nodiscard]] Lengths InHand(float creatureSize);
/// Tied to something that stays put, the leash reaches one and a half times as far as the creature is from it, between
/// 180 and 360, half of that at rest
[[nodiscard]] Lengths TiedToStatic(float distance);
/// Tied to something that moves, the leash is seven times the creature's height, at most 40, half of that at rest
[[nodiscard]] Lengths TiedToMobile(float creatureHeight);

/// The rope's look for each leash: which band of the leash texture it shows, and its width. The compassion leash is
/// thicker.
[[nodiscard]] leash_rope::Look LookFor(LeashType type);

/// The desire a leash makes dominant every turn the creature wears it, and whenever the player picks it while it is on:
/// anger on the aggression leash and compassion on the compassion leash; nothing on the learning leash, which instead
/// lets go of a desire made dominant before. Taking the leash off doesn't end it: the other desires stay held down
/// until their time is up.
[[nodiscard]] std::optional<creature_desires::Desire> ForcedDesireFor(LeashType type);
/// How long the leash's desire holds the creature's other desires (but its bodily needs) down, in seconds
constexpr float k_ForcedDesireSeconds = 36000.0f;
/// How long a creature tied to a village it wants to impress holds its other desires down, in seconds
constexpr float k_ImpressTownSeconds = 120.0f;

/// What a creature tied to a village's centre (or its totem) knows of the village
struct TiedTown
{
	/// The leash isn't the aggression leash
	LeashType leash {LeashType::Rope};
	/// The creature's strongest desire is already to impress
	bool impressing {false};
	/// The creature belongs to a player, and the village to the creature's player
	bool creatureHasPlayer {false};
	bool ownersTown {false};
	/// The village's belief in the creature's player, and the most it believes in any other player
	float beliefInOwner {0.0f};
	float mostOtherBelief {0.0f};
};
/// Tied to a village on any leash but aggression, the creature is made to want to impress it, unless that is already
/// its strongest desire or the village is firmly its player's: theirs, and believing in them more than half as much as
/// in any other player (any village of its own when it has no player)
[[nodiscard]] bool ImpressesTown(const TiedTown& town);

/// Each sighting of a miracle counts once, three times while the creature wears the learning leash
[[nodiscard]] uint32_t MiracleSightingWeight(bool learningLeash);

/// The data that tells if the creature walks back into the area that holds it in this turn
struct Confinement
{
	/// The radius of the area. The area does not hold the creature when the radius is 0.
	float radius {0.0f};
	bool leashed {false};
	/// The leash works, as the scripts set it
	bool leashWorks {true};
	/// The creature does an order that the player gave with the leash
	bool orderInForce {false};
	/// The creature copies an action of the player
	bool mimicking {false};
	/// A script controls the creature
	bool scriptControlled {false};
	/// The creature uses a teleport stone
	bool teleporting {false};
	/// The distance from the creature to the centre of the area
	float distance {0.0f};
	/// A creature can walk on the land at the centre of the area
	bool centreOpen {true};
	/// The distance from the place that the creature walks back to, to the centre of the area. It is empty when the
	/// creature does not walk back.
	std::optional<float> returnToCentre;
};
/// When the creature walks back already, it walks back again only if the centre is this far from the place that it
/// walks to: half the radius, but not less than 5
constexpr float k_ReturnShare = 0.5f;
constexpr float k_ReturnLeast = 5.0f;
/// The creature walks back when it is further than the radius from the centre, unless one of these is true:
/// - the area does not hold it, or it wears a leash that does not work
/// - it does an order of the leash, it copies the player, a script controls it, or it uses a teleport stone
/// - a creature cannot walk on the land at the centre
/// - it walks back already, to a place near the centre
[[nodiscard]] bool WalksBack(const Confinement& confinement);
/// The creature walks back faster when it is further away. At twice the radius or more, it adds 0.8 to its walk speed.
constexpr float k_ReturnMostExtraSpeed = 0.8f;
[[nodiscard]] float ReturnExtraSpeed(float distance, float radius);
/// The creature stops within the radius of the centre, or within its height if that is more
[[nodiscard]] float ReturnArrival(float radius, float height);

/// Free of its home only within 140 of it, and only if its player has a temple
constexpr float k_HomeRange = 140.0f;
[[nodiscard]] bool FreeOfHome(float distanceFromHome, bool playerHasTemple);
/// Kept at home while it starts to grow up, within 12 of it
constexpr float k_HomeConfinement = 12.0f;
/// Kept within an area: a radius is set, unless it is on a leash that doesn't work
[[nodiscard]] bool IsConfined(float radius, bool leashed, bool leashWorks);

/// Leashed to another creature on the aggression leash, the other gets angry too within eight times the leashed
/// creature's height
constexpr float k_AngerOtherReach = 8.0f;
/// Leashed to another creature, how nice each finds the other goes down (aggression) or up (compassion) by 0.1 at the
/// first turn, then whenever more than 600 turns have passed since the other creature's feelings were last touched so
constexpr uint32_t k_AttitudeTurns = 600;
constexpr float k_AttitudeStep = 0.1f;
/// How much nicer each finds the other each time, none on the learning leash
[[nodiscard]] float AttitudeChange(LeashType type);
/// Whether the feelings are touched this turn, given the turn the other creature's were last touched by a leash
[[nodiscard]] bool AttitudeDue(std::optional<uint32_t> lastTurn, uint32_t turn);

/// A lesson the creature learns about something the player showed it on a leash: how much more the desire is what to
/// act on it with
struct Lesson
{
	creature_desires::Desire desire;
	float change;
};
/// On the aggression leash, what the player shows the creature teaches it anger (+1) over compassion (-1), or over
/// befriending when it is another creature; the compassion leash teaches the reverse; the learning leash nothing
[[nodiscard]] std::vector<Lesson> LessonsFor(LeashType type, bool objectIsCreature);

/// What the leash tells the creature's mind, for its planner and its learning to act on
struct MindHooks
{
	/// The turn a creature leashed to this one last touched how nice the two find each other
	std::optional<uint32_t> attitudeTurn;
	/// Wearing the learning leash in its player's hand, it copies the player's actions that need it
	bool learningInHand {false};
	/// How much each miracle seen counts towards learning it
	uint32_t miracleSightingWeight {1};
	/// Things the player showed it, by entity number, and the leash they were shown on
	struct Shown
	{
		uint32_t object;
		LeashType type;
		std::vector<Lesson> lessons;
	};
	std::vector<Shown> shown;
	/// Things it was told to act on, by entity number
	std::vector<uint32_t> actOn;
	/// How much nicer it finds other creatures, by entity number
	struct Attitude
	{
		uint32_t creature;
		float change;
	};
	std::vector<Attitude> attitudes;
};

} // namespace openblack::creature_leash
