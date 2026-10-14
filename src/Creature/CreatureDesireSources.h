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

#include <functional>
#include <optional>

#include <glm/vec3.hpp>

/// The sources of a creature's desires that follow what it is and what happens to it, rather than fading after events
/// push them: the personality it is born with, how much the player looks at it and plays with it, how far it is from
/// home and how many magic mushrooms it has eaten. Each new creature's mind is also spread at random from its species'
/// values. Everything here is pure.
namespace openblack::creature_desire_sources
{

/// A random number from 0 up to the given most
using UpTo = std::function<float(float most)>;

/// The sources these rules read, by their place in the game's source table
namespace types
{
constexpr uint32_t k_ImpressFromSeeingThings = 1;
constexpr uint32_t k_InnateKindness = 5;
constexpr uint32_t k_InnateAggression = 11;
constexpr uint32_t k_InnateLethargy = 26;
constexpr uint32_t k_IdleWithPlayer = 27;
constexpr uint32_t k_InnateFriendliness = 35;
constexpr uint32_t k_Loneliness = 36;
constexpr uint32_t k_LackOfInteraction = 37;
constexpr uint32_t k_InnateCommunicativeness = 39;
constexpr uint32_t k_GoHome = 49;
constexpr uint32_t k_GetHigh = 55;
constexpr uint32_t k_HangAroundAtHome = 56;
constexpr uint32_t k_MentalIllness = 57;
} // namespace types

/// The personality a creature is born with, 0 to 1 for its species and then spread at random
struct InnateTraits
{
	float kindness {0.0f};
	float aggression {0.0f};
	float lethargy {0.0f};
	float friendliness {0.0f};
	float communicativeness {0.0f};
	/// A sixth trait that no desire reads
	float sixth {0.0f};
	/// How much things on the land, mountains and water catch its eye
	float objectsLookWeight {0.0f};
	float mountainsLookWeight {0.0f};
	float waterLookWeight {0.0f};
};

/// A trait is spread by up to this either way, and a look weight by up to the second
constexpr float k_TraitSpread = 0.1f;
constexpr float k_LookWeightSpread = 0.4f;

/// A value spread by up to `spread` either way and then held between 0 and 1, as the game does it: the spread value is
/// drawn again for each test of the limits and once more to keep. So the kept value is a new draw, which can fall
/// outside 0 to 1 when an earlier draw passed the test.
[[nodiscard]] float SpreadTrait(float base, float spread, const UpTo& upTo);
/// A new creature's personality: its species' values, each spread at random
[[nodiscard]] InnateTraits SpreadTraits(const InnateTraits& species, const UpTo& upTo);

/// A source's starting value or threshold, spread by up to `spread` either way and held between 0 and 1
[[nodiscard]] float SpreadSource(float value, float spread, const UpTo& upTo);
/// The time a desire takes to grow is spread by up to a fifth either way
constexpr float k_IncreaseSpread = 0.2f;
[[nodiscard]] float SpreadIncreaseSeconds(float seconds, const UpTo& upTo);
/// A new creature starts each action this many turns or fewer, at random, since it last did it
constexpr uint32_t k_NewActionTurnsRange = 120;

/// How much attention the player pays the creature, counted in game turns
struct Attention
{
	/// Turns in a row the player's camera has looked at it
	uint32_t turnsWatched {0};
	/// Turns in a row the player's camera has not looked at it, which make it lonely
	uint32_t turnsIgnored {0};
	/// Turns since the player last taught it or told it what to do on the leash
	uint32_t turnsSinceInteraction {0};
};

/// One game turn of attention, unless the hand holds the creature: the counts of turns looked at or not go on or start
/// again, and the turns since the last interaction go on
void CountTurn(Attention& attention, bool lookedAt);

/// Loneliness: whole seconds unwatched over a minute, held between 0 and 1
constexpr uint32_t k_LonelySeconds = 60;
/// Lack of interaction: whole seconds since the last interaction over two minutes, held between 0 and 1
constexpr uint32_t k_UninterestedSeconds = 120;
[[nodiscard]] float CountSource(uint32_t turns, uint32_t seconds, uint32_t turnsPerSecond);
/// The turns a count is set to when a script sets its source: the value of that many seconds, cut to whole turns
[[nodiscard]] uint32_t TurnsForSource(float value, uint32_t seconds, uint32_t turnsPerSecond);

/// How interesting the creature finds its player: the hand's recent movement and what it holds
struct PlayerInterest
{
	/// It finds the player most interesting at first
	float value {1.0f};
	/// The distance the hand has moved lately, fading
	float handMovement {0.0f};
	/// Where the hand was at the last turn, none before the first
	std::optional<glm::vec3> lastHand;
};
/// The hand's movement counts up to this, and fades by this each turn
constexpr float k_MostHandMovement = 60.0f;
constexpr float k_HandMovementFade = 0.98f;
/// A held thing's radius across the land counts up to this
constexpr float k_MostHeldRadius = 30.0f;

/// How interesting the player is this turn, from where the hand is and the radius of what it holds (none when it holds
/// nothing); 0 for a creature without a player, whose hand is not passed
[[nodiscard]] float InterestNow(PlayerInterest& interest, const std::optional<glm::vec3>& hand,
                                std::optional<float> heldRadius);
/// A player more interesting than before is so at once; less interesting, the creature loses half its interest a turn
void UpdateInterest(PlayerInterest& interest, float now);

/// What the readable sources depend on
struct State
{
	const InnateTraits* innate {nullptr};
	const Attention* attention {nullptr};
	float interest {0.0f};
	uint32_t turnsPerSecond {10};
	/// The computer plays the creature's player
	bool computerPlayer {false};
	/// The creature's age, in the body's age steps
	uint32_t age {0};
	/// How far it is from its home across the land, none without a home
	std::optional<float> homeDistance;
	/// Its stage of growing up's distance from home at which it most wants to go home
	float goHomeDistance {0.0f};
	uint32_t mushroomsEaten {0};
	/// What it has seen of other players' and neutral villages
	float impressFromSeeing {0.0f};
};

/// The value of a source these rules read, or none for the others
[[nodiscard]] std::optional<float> Read(uint32_t type, const State& state);

/// What a script sets a source to changes what the source follows
struct Written
{
	InnateTraits* innate {nullptr};
	Attention* attention {nullptr};
	PlayerInterest* interest {nullptr};
	uint32_t* mushroomsEaten {nullptr};
	uint32_t turnsPerSecond {10};
};
/// A script set a source to a value: the trait or count behind the source follows. Setting the player's interest sets
/// it to nothing, and setting the source to get high sets a hundred magic mushrooms eaten, whatever the value.
void Write(uint32_t type, float value, const Written& written);

/// The creature saw a village of another player, or of no player, again: impress from seeing things goes up by a half
constexpr float k_ImpressPerSighting = 0.5f;
[[nodiscard]] float ImpressAfterSighting(float impress);

} // namespace openblack::creature_desire_sources
