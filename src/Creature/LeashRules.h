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

#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

#include "Creature/CreatureDesires.h"
#include "Creature/CreatureIdleMind.h"
#include "Creature/LeashRope.h"
#include "Enums.h"

/// How the player leads a creature on a leash. There are three leashes: the learning leash (a plain rope), which makes
/// the creature watch and copy the player more keenly, and the aggression and compassion leashes, which make it angry or
/// kind for as long as it wears them. Held in the hand, the leash is as long as the creature is big; tied to something,
/// it is as long as the creature is far from it. Pulled taut, the creature stops what it was doing and walks to the
/// hand; pulled away from the same thing twice, it goes off wanting it for a while.
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

/// A rope pulled harder than this pulls the creature to the hand
constexpr float k_PullTension = 0.8f;
[[nodiscard]] bool ShouldPull(float tension);
/// What decides whether a held rope pulls the creature this turn
struct Pull
{
	float tension {0.0f};
	/// The leash works, as scripts set
	bool works {true};
	/// The leash is tied to something rather than held in the hand
	bool tied {false};
	/// The creature is carrying out an order given with the leash
	bool orderInForce {false};
	/// A script controls the creature
	bool scriptControlled {false};
	/// The creature is taking something the hand gives it
	bool takingFromHand {false};
};
/// A rope held in the hand and pulled taut pulls the creature to the hand, taking it out of whatever it is doing, a
/// fight included; not while it carries out a leash order, takes something from the hand, or a script controls it
[[nodiscard]] bool Pulls(const Pull& pull);

/// Pulled away from the same desire's action a second time, the desire is held back for this many seconds a pull
constexpr float k_SuppressSecondsPerPull = 30.0f;
/// How many times the creature has been pulled away from acting on each desire
struct PullMemory
{
	std::array<uint8_t, creature_desires::k_DesireCount> counts {};
};
/// The creature is pulled away from acting on a desire: on the second pull the desire is held back, for the returned
/// seconds, and the count starts again
[[nodiscard]] std::optional<float> RecordPull(PullMemory& memory, creature_desires::Desire desire);
/// The desire behind what the creature is doing, if any
[[nodiscard]] std::optional<creature_desires::Desire> DesireBehind(creature_mind::Activity activity,
                                                                   std::optional<creature_desires::Desire> shown);

/// Within this distance of the hand, a pull doesn't move the creature
constexpr float k_CloseToHand = 10.0f;
/// The hand has to move this far from where the creature is going for it to set off again
constexpr float k_HandMoved = 1.0f;
/// Still walking, it carries on while where it is going is this much closer to the hand than it is
constexpr float k_CloserShare = 0.5f;
enum class Lead : uint8_t
{
	/// Close enough to the hand already
	Stay,
	/// On its way to near enough where the hand is
	KeepGoing,
	/// Off to the hand
	GoToHand,
};
/// What a pull on the leash makes the creature do
[[nodiscard]] Lead DecideLead(const glm::vec3& creature, const glm::vec3& hand, std::optional<glm::vec2> destination,
                              bool walking);

/// How hard the creature is being pulled, for its speed: none at first, all once it is on its way, then fading by 0.95
/// a turn and gone below 0.3
constexpr float k_PullFade = 0.95f;
constexpr float k_PullGone = 0.3f;
[[nodiscard]] float FadePull(float pull);

/// Free of its home only within 140 of it, and only if its player has a temple
constexpr float k_HomeRange = 140.0f;
[[nodiscard]] bool FreeOfHome(float distanceFromHome, bool playerHasTemple);
/// Kept at home while it starts to grow up, within 12 of it
constexpr float k_HomeConfinement = 12.0f;
/// Kept within an area: a radius is set, unless it is on a leash that doesn't work
[[nodiscard]] bool IsConfined(float radius, bool leashed, bool leashWorks);
/// Strayed out of the area it is kept within
[[nodiscard]] bool OutsideArea(const glm::vec2& position, const glm::vec2& centre, float radius);

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
	/// Following the leash to the hand: the mind leaves its body to the leash
	bool obeying {false};
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
