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

#include <glm/vec2.hpp>

#include "Enums.h"

/// How towns feel about the creatures they have seen, and how their villagers react to a creature. A creature makes a
/// reaction of its own that follows it all its life. The first time a town's villager takes it up, the town starts to
/// watch the creature, curious about it. The town then reads, every turn, what the creature last finished doing to the
/// thing it means to act on next: harming the land or eating one of the people frightens the town, gifts, help and
/// kind miracles earn its respect. A feeling lapses into indifference some seconds after the creature last showed it.
/// Villagers curious about a creature stand and look at it, then step closer and closer to inspect it; those who fear
/// it run from it; those who respect it go up to it, face it and bow for a few seconds. Indifferent, they go back to what
/// they were doing. Pure rules, tested without the game.
namespace openblack::ecs::creature_attitude
{

/// How a town feels about a creature
enum class Attitude : uint8_t
{
	/// Watching a creature it has only just seen
	Curious = 0,
	/// Nothing to feel about it any more
	Indifferent = 1,
	Fear = 3,
	Respect = 4,
};

/// What finishing an action of the creature's table shows the towns
enum class Shown : uint8_t
{
	Nothing,
	Fear,
	Respect,
	/// Eating, or handling to eat: frightening when done to one of the people, nothing otherwise
	Eating,
};

/// What finishing an action, by its number in the creature's action table, shows the towns
[[nodiscard]] Shown ShownBy(uint32_t action);

/// The attitude a creature's finished action shows a town, about the thing it means to act on next: none when it shows
/// nothing (the town's feeling is left to lapse)
[[nodiscard]] std::optional<Attitude> AttitudeShown(uint32_t action, bool thingIsVillager);

/// A town's feeling about one creature, and when it last changed and last lapsed, in game turns
struct Feeling
{
	Attitude attitude {Attitude::Curious};
	uint32_t shownTurn {0};
	uint32_t lapsedTurn {0};
};

/// A town's feeling about a creature it starts to watch now
[[nodiscard]] Feeling StartWatching(uint32_t turn);

/// How many whole seconds after it was last shown a feeling lapses: curiosity 15, fear 30, respect 10
[[nodiscard]] float LapseSeconds(Attitude attitude);

/// A town turn of its feeling about a creature: what the creature showed (if anything) takes the place of the feeling,
/// and a new fear starts its time again even when the town feared the creature already; then a feeling shown longer
/// ago than its lapse, counted in whole seconds, gives way to indifference. Whether the feeling changed to another one
/// that the creature showed (the creature's help tells its player so).
bool Turn(Feeling& feeling, std::optional<Attitude> shown, uint32_t turn, uint32_t turnsPerSecond);

/// What a villager weighs about a creature's reaction
struct Watch
{
	/// The villager is in a dance
	bool dancing {false};
	/// The creature is under the invisibility spell
	bool creatureInvisible {false};
	/// The creature is within the reaction's reach for running from it
	bool withinReach {false};
	bool hasTown {false};
	/// The villager's town's feeling, curious when it doesn't watch the creature yet
	Attitude attitude {Attitude::Curious};
	/// The villager's town and the creature belong to the same player
	bool sameOwner {false};
	bool disciple {false};
	/// For the state the villager works towards, how much its town wants what that state does for it, times how much the
	/// state does; none for a state that serves no want of the town's
	std::optional<float> wanted;
	/// The priorities of the reaction table's rows for a creature, for running from a thing and for following one
	uint32_t creaturePriority {0};
	uint32_t fleePriority {0};
	uint32_t followPriority {0};
};

/// How urgently a villager heeds a creature's reaction, before nearness counts, 0 to 255. None for a dancer, for an
/// invisible creature or one out of reach. Without a town, by the creature row's priority. Curious, half as much again;
/// frightened, twice the priority of running from a thing; indifferent, not at all. Respecting it, by the priority of
/// following a thing, when the creature is another player's, or the villager isn't a disciple and isn't busy with
/// something its town wants.
[[nodiscard]] uint8_t VillagerPriority(const Watch& watch);

/// What a villager's time with a creature's reaction depends on
struct Timing
{
	/// The creature row's turns
	uint32_t tableTurns {0};
	float maxReactionDistance {0.0f};
	float howImportantIsDistance {0.0f};
	/// How far the villager is from the creature, in metres
	float distance {0.0f};
	/// How impressive the creature's last finished action was
	ImpressiveType impressive {ImpressiveType::CreatureNotImpressive};
	/// The villager's town's feeling about the creature, none for a villager without a town
	std::optional<Attitude> attitude;
};

/// How many turns a villager reacts to a creature: the row's turns, more the nearer it is, twice as long for a quite
/// impressive creature and three times for a very impressive one, four times again for a town that fears it
[[nodiscard]] uint32_t TurnsToReact(const Timing& timing);

/// How many turns a villager waits before reacting to a creature again: the row's turns, less the nearer it is, half as
/// long for a quite impressive creature and a third for a very impressive one; none at all while the creature means to do
/// something impressive, or while the villager's town fears or respects it
[[nodiscard]] uint32_t TurnsBeforeReactingAgain(const Timing& timing, bool meansSomethingImpressive);

/// A curious villager first looks at the creature from this far away, and steps this much closer each time it has looked
/// long enough; it gives up once the distance it means to keep is under a metre
inline constexpr float k_FirstLookMetres = 20.0f;
inline constexpr float k_StepCloserMetres = 8.0f;
inline constexpr float k_CloseEnoughMetres = 1.0f;
/// It looks for 1.5 seconds and up to 3 more the first time, up to 7 more after each step closer
inline constexpr float k_LookSeconds = 1.5f;
inline constexpr float k_FirstLookExtraSeconds = 3.0f;
inline constexpr float k_LaterLookExtraSeconds = 7.0f;
/// It bows to a creature it respects for 4 seconds, and turns to face it again when it has moved more than an eighth of a
/// turn round
inline constexpr float k_RespectSeconds = 4.0f;
inline constexpr int32_t k_RespectDrift = 0x100;
/// A walk to or from the creature is over within this of its goal
inline constexpr float k_ArrivedMetres = 0.5f;
/// A frightened villager runs from a creature 1.2 times the reaction's reach, and stops once it is 1.5 times that away;
/// it sets off at half its fleeing speed one turn in this many while the creature means to act on something
inline constexpr float k_FleeReachFactor = 1.2f;
inline constexpr float k_FleeGiveUpFactor = 1.5f;
inline constexpr uint32_t k_FleeChance = 10;

/// Whole turns in some seconds, truncated
[[nodiscard]] int32_t TurnsIn(float seconds, uint32_t turnsPerSecond);

/// Where a curious villager steps to, to stand some distance from the creature: straight towards it by the whole metres
/// it is too far, along the map's angle to it. None when it is within a metre of that distance already.
[[nodiscard]] std::optional<glm::vec2> StepCloser(glm::vec2 villager, glm::vec2 creature, float keep);

/// Where a villager goes to bow to a creature: from where it stands, towards the creature, by its own and the creature's
/// radii together
[[nodiscard]] glm::vec2 BowingPlace(glm::vec2 villager, glm::vec2 creature, float villagerRadius, float creatureRadius);

/// Where a frightened villager runs to: straight away from the creature, so far
[[nodiscard]] glm::vec2 FleePoint(glm::vec2 villager, glm::vec2 creature, float distance);

/// Whether a villager bowing to a creature, facing a game angle, must turn to face it again: the angle to the creature
/// differs from its own by more than the drift, the numbers taken as they are, without going round the circle
[[nodiscard]] bool DriftedFrom(uint16_t facing, uint16_t towardsCreature);

} // namespace openblack::ecs::creature_attitude
