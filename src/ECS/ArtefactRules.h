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
#include <span>

#include "Enums.h"

// The rules of artefacts: rocks and other loose things a player puts down gently by a town, which its people dance round.
// Dances in the owner's own town raise its worth, ever more slowly once it is worth more than 1; while it dances in a
// town that isn't its owner's, that town gives the owner belief now and then. Worth more than 1 it impresses the people
// who see it more, and glows; worth more than 2 its owner's symbol floats over it. Pure rules, tested on their own.

namespace openblack::ecs::artefact_rules
{

/// Worth more than this, an artefact glows, impresses more and impresses a town it is brought to
inline constexpr float k_GlowingWorth = 1.0f;
/// Worth more than this, its owner's symbol floats over it
inline constexpr float k_SymbolWorth = 2.0f;
/// The owner's symbol is drawn at this scale
inline constexpr float k_SymbolScale = 0.5f;
/// A villager wants to join a dance less the farther it is, out to this many metres
inline constexpr float k_JoiningReach = 250.0f;
/// A villager waiting for a dance farther than this from it walks closer
inline constexpr float k_WaitingReach = 12.0f;
/// The room a waiting villager keeps clear about itself, its own width times this
inline constexpr float k_WaitingRoom = 1.2f;
/// A villager walking closer heads this far either side of straight towards itself from the artefact, at most
inline constexpr float k_WaitingSpread = 0.785398185253f;
inline constexpr float k_WaitingHalfSpread = 0.392699092626f;
/// Room is looked for in steps this far apart
inline constexpr float k_WaitingRoomStep = 1.0f;
/// What a dance's worth gain is multiplied by before it is added
inline constexpr float k_WorthGainScale = 300.0f;
/// A town's belief in its owner counts fully up to this, a third of the rest beyond, and no more than the cap
inline constexpr float k_TownBeliefFull = 2.0f;
inline constexpr float k_TownBeliefCap = 4.0f;
/// The artefact power-up of a worship site comes every this many turns
inline constexpr uint32_t k_SitePowerUpTurns = 1000;

/// The dance of an object as wide as twice its ground radius: the small object dance under 7 m, the medium one under
/// 10 m, otherwise the dance for objects wider than 20 m (the one for objects under 20 m is never chosen)
[[nodiscard]] DanceInfo DanceForRadius(float radius);

/// Whether an artefact worth so much glows, impressing more
[[nodiscard]] bool Glows(float worth);
/// Whether its owner's symbol floats over it
[[nodiscard]] bool ShowsSymbol(float worth);
/// How many times as impressive the artefact makes its object: 1 + its worth once it glows, else 1
[[nodiscard]] float ImpressiveModifier(float worth);
/// Whether an artefact, worth so much, impresses the town (or site) it is put down by: only when it glows and it last
/// belonged to another
[[nodiscard]] bool WillImpress(float worth, bool sameTown);

/// What a villager's wish to join an artefact's dance comes from
struct JoiningInputs
{
	/// The artefact's dance has begun since its town last looked
	bool dancing {false};
	/// How far the villager is from the artefact, in metres
	float distance {0.0f};
	/// The dance's dancers (those on their way among them) and the most its kind takes; none when it has no dance yet
	std::optional<uint32_t> dancers;
	uint32_t maximumDancers {0};
	/// Whether the dance is under way, and its length and the turns since it started
	bool running {false};
	uint32_t duration {0};
	uint32_t sinceStart {0};
};
/// How much a villager wants to join an artefact's dance: not at all while it is dancing; otherwise less the farther
/// it is (out to 250 m) and the fuller the dance already is
[[nodiscard]] float JoiningDesire(const JoiningInputs& in, float distanceModifier);

/// What a villager waiting for a dance does this turn
enum class Waiting : uint8_t
{
	/// It walks to a clear spot nearer, on its own side of the artefact
	WalkCloser,
	/// It looks for a clear spot near by, the spot it stands on being crowded
	MakeRoom,
	/// It waits on
	Wait,
	/// It has waited its time: the dance starts
	StartDance,
};
/// A waiting villager's turn: from farther than 12 m it walks closer; there, on its first turn it makes itself room if
/// it has none, and once it has waited its time the dance starts
[[nodiscard]] Waiting WaitingTurn(float distance, uint16_t waited, bool roomHere, uint16_t timeToWait);

/// The turns of an artefact's dance left, none below 0, its town having seen it start at a turn
[[nodiscard]] uint32_t DanceTimeLeft(uint32_t duration, uint32_t turn, uint32_t started);

/// How much a town's belief in its owner counts towards its artefacts' worth: all of it up to 2, a third of the rest
/// beyond, 4 at most; 1 for a town of the neutral player or no one
[[nodiscard]] float TownModifier(std::optional<float> beliefInOwner);

/// What a dance finished in its owner's own town adds to an artefact's worth
struct WorthInputs
{
	float townModifier {1.0f};
	/// The kind's artefact multiplier, from its table
	float artefactMultiplier {0.0f};
	/// The dance's dancers and the most its kind takes
	uint32_t dancers {0};
	uint32_t maximumDancers {0};
	/// The turns since its town saw it start, and its length
	uint32_t danced {0};
	uint32_t duration {0};
	/// The land's artefact speed, 1 unless a script sets it
	float artefactSpeed {1.0f};
};
/// The gain: the town's modifier, the multiplier, the share of the most dancers it had, the share of the dance's length
/// danced and the land's speed
[[nodiscard]] float DanceGain(const WorthInputs& in);
/// Its worth after a gain: the gain times 300 added, divided by its worth once it is worth more than 1
[[nodiscard]] float AddToWorth(float worth, float gain);

/// What the belief a town gives an artefact's owner comes from
struct BeliefInputs
{
	float worth {0.0f};
	/// The object's impressiveness: its kind's, times the land's balance for impressiveness
	float impressiveness {0.0f};
	uint32_t dancers {0};
	/// The town's adults and children
	uint32_t population {0};
	/// The town table's multiplier for belief from villagers dancing round artefacts
	float beliefMultiplier {0.0f};
};
/// The belief: its worth once it glows (else none) times the object's impressiveness times the dancers per head of the
/// town's people times the multiplier (1 in place of those last two for a town without people)
[[nodiscard]] float DancingBelief(const BeliefInputs& in);
/// Whether the town gives an owner belief again: the turns since it last did, with a random part up to a quarter of the
/// gap, past the gap
[[nodiscard]] bool BeliefGiftDue(uint32_t sinceLastGift, uint32_t randomPart, uint32_t gap);
/// The random part is drawn below this: a quarter of the gap
[[nodiscard]] uint32_t GiftRandomLimit(uint32_t gap);

/// What every artefact at a worship site gains at the site's power-up: the land's artefact speed times the site's table
/// multiplier times its dancers over the player's people (none for a player without people)
[[nodiscard]] float SitePowerUp(float artefactSpeed, float powerUpMultiplier, uint32_t dancers, uint32_t playerPopulation);

/// A thing near where a wonder is planned, as the planning sees it
struct WonderNeighbour
{
	float distance {0.0f};
	/// Its artefact's worth (0 for one that isn't an artefact) and its impressiveness
	float artefactWorth {0.0f};
	float impressiveness {0.0f};
};
/// A planned wonder's power: the town's desire for a wonder, plus, for each thing nearer than its reach (the desire
/// times 15 m, at least 15 m), its artefact's worth and a tenth of its impressiveness, at least a quarter. The game
/// means to look in the cells round the place but looks in the place's own cell each time, so the things in that cell
/// are counted once for each cell its reach covers: `passes`.
[[nodiscard]] float WonderPower(float wonderDesire, std::span<const WonderNeighbour> inCell, uint32_t passes);
/// How far a wonder's planning reaches for a desire
[[nodiscard]] float WonderReach(float wonderDesire);
/// A planned wonder's scale from its power: its power (5 at most) times one less a tenth of it
[[nodiscard]] float WonderScale(float power);

} // namespace openblack::ecs::artefact_rules
