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
#include <numbers>
#include <optional>
#include <string_view>

#include <glm/vec3.hpp>

#include "Enums.h"

/// The player's hand giving what it holds to a creature: the hand offers it while over a creature that would take it,
/// turning it over towards the creature; a press of the Action button gives it; the creature turns to the player,
/// reaches out and takes it from the hand, which is drawn to meet the creature's hand; then the creature decides by its
/// desires what to do with what it was given.
namespace openblack::creature_giving
{
/// Over a creature that would take what the hand holds, the hand offers it for this long after leaving the creature
constexpr int32_t k_OfferMs = 1100;
/// The held thing turns back once less than this much of the offer is left, unless the creature is reaching for it
constexpr int32_t k_OfferTurnsBackMs = 300;
/// How long the held thing takes to turn over, and back
constexpr float k_OfferTurnSeconds = 0.4f;
/// Within this many milliseconds of the moment the creature takes it, the hand is drawn to the creature's hand, this
/// much of the way for each millisecond nearer
constexpr float k_DrawMs = 500.0f;
constexpr float k_DrawPerMs = 0.002f;
/// Moving the hand further than this many times the creature's size from where it should be calls the giving off
constexpr float k_CallOffSizes = 15.0f;
/// The animation the creature takes a thing from the hand with
constexpr size_t k_TakeAnimation = 86;
/// The desires weighed for what to do with a thing given, below which none counts
constexpr float k_LeastDesire = 0.1f;
/// The creature's help raised when it couldn't pick up what it is offered: a stacked miscellaneous message of this
/// number
constexpr uint32_t k_CantPickUpHelp = 7;

/// The offer, counted down on the frame's real time
struct Offer
{
	int32_t ms {0};
	/// The hand is over a creature that would take what it holds: it offers it for the full time again
	void Renew() { ms = k_OfferMs; }
	/// A frame without a creature that would take it: the offer runs down, unless the creature is already reaching for
	/// the thing. Whether it ran out this frame.
	bool RunDown(int32_t frameMs, bool reaching);
	[[nodiscard]] bool Active() const { return ms > 0; }
};

/// How far round the held thing turns while it is offered: the palm turns over, a thing held at the side or by the
/// fingers a quarter turn, the other way for the right hand; none when the offer is nearly over and the creature isn't
/// reaching. None when the hold keeps its last turn (magic, grain, fingers).
[[nodiscard]] std::optional<float> OfferTurn(HoldType hold, bool rightHanded, const Offer& offer, bool reaching);

/// Whether a thing can be given to a creature
enum class Verdict : uint8_t
{
	Give,
	/// Not to this creature: another god's not allied to it, a spell seed, or a creature without a player
	No,
	/// The creature couldn't pick it up; its help says so
	CantPickUp,
};
struct GiveFacts
{
	bool creatureHasPlayer {false};
	bool giverHasPlayer {false};
	/// The giver is the creature's own player, or allied to it
	bool friendly {false};
	bool spellSeed {false};
	bool creatureCanPickUp {false};
};
[[nodiscard]] Verdict MayGive(const GiveFacts& facts);

/// Where the hand is drawn while the creature reaches, before it rises for what it holds: within half a second of the
/// moment of taking, from the creature's hand towards where the hand should be, the further the more milliseconds from
/// that moment, and lowered by the rest of the way times how far what it holds hangs below it. None further away.
[[nodiscard]] std::optional<glm::vec3> DrawToCreature(glm::vec3 creatureHand, glm::vec3 target, float msFromTake, float hang);

/// Whether the hand has gone too far from the creature reaching for what it holds
[[nodiscard]] bool CallsOff(glm::vec3 hand, glm::vec3 target, float creatureSize);

/// What a creature does with a thing it was given
enum class Use : uint8_t
{
	Examine,
	Eat,
	Throw,
	/// What it would do led to it on the leash
	AsLed,
};
/// The desires it weighs, in the game's order: curiosity examines, hunger eats, wanting to play with the player throws
struct Weighed
{
	float desire {0.0f};
	float usefulness {0.0f};
};
struct GiftFacts
{
	Weighed curiosity;
	Weighed hunger;
	Weighed playWithPlayer;
	/// It has learnt it can't eat things of this kind
	bool cantEatKind {false};
	/// It has been given a thing of this kind before
	bool givenKindBefore {false};
};
/// The use chosen, and whether it is the first of the kind it was given with no desire for it, which it is to remember
struct Choice
{
	Use use {Use::Examine};
	bool remembersKind {false};
};
/// The desire with the best score (its value times how useful the thing is to it) wins, hunger only for a kind it can
/// eat. With no winner, or no desire at least a tenth strong, the first thing of a kind is examined and later ones are
/// acted on as when led to them.
[[nodiscard]] Choice ChooseUse(const GiftFacts& facts);

/// The plan that the creature kept, as it was when it started its last reaction, or when the hand gave it a thing
/// while it had no reaction
struct KeptPlanFacts
{
	/// The plan has a desire, a thing to act on and an action
	bool complete {false};
	/// The desire of the plan is about a whole thing, and the plan has no such thing
	bool missesWhole {false};
	/// The action needs a thing to use, and the plan has no such thing
	bool missesThingToUse {false};
	/// The action acts on a thing, and it needs a thing to use
	bool actsOnThing {false};
	bool needsThingToUse {false};
	/// The thing that the creature was given suits the action as the thing to act on, or as the thing to use
	bool giftSuitsActOn {false};
	bool giftSuitsUse {false};
};
/// How the creature uses the thing that it was given for the plan that it kept
enum class KeptPlanUse : uint8_t
{
	/// The thing does not suit the plan: the creature chooses a use of its own
	None,
	/// The creature continues the plan with the thing as the thing that it acts on
	ActOn,
	/// The creature continues the plan with the thing as the thing that it uses
	Use,
};
/// The creature continues a complete plan, when the action acts on a thing and the given thing suits it. The creature
/// tries the thing to act on first, then the thing to use.
[[nodiscard]] KeptPlanUse UseForKeptPlan(const KeptPlanFacts& facts);
/// A creature must have grown up to at least this stage to cast a one-shot globe that it was given
constexpr uint32_t k_GlobeCastPhase = 8;
/// The desire of compassion, the only desire for which the creature casts a globe with the action for compassion
constexpr uint32_t k_CompassionDesire = 1;
/// The two actions that cast the globe that the creature holds
constexpr std::string_view k_CastGlobeAggressive = "CastOneOffSpellInHandAggressive";
constexpr std::string_view k_CastGlobeCompassionate = "CastOneOffSpellInHandCompassiona";

/// The thing at which the creature casts a one-shot globe that it was given
enum class GlobeTarget : uint8_t
{
	/// The thing that is most useful for the desire, of the things that it believes in
	MostUseful,
	/// The creature itself
	Itself,
	/// The nearest other creature that it believes in
	NearestCreature,
};
/// How the creature uses a one-shot globe that it was given
struct GlobeCast
{
	/// The desire that the miracle serves
	uint32_t desire {0};
	std::string_view action;
	GlobeTarget target {GlobeTarget::MostUseful};
};
/// When the creature is given a one-shot globe, it casts the miracle if it has grown up enough. The first miracle of the
/// globe's seed gives the desire. The creature casts with the action for compassion only for the desire of compassion.
/// It casts a creature spell at itself for compassion, or else at the nearest creature. It casts another miracle at the
/// thing that is most useful for the desire. None when it is too young: it uses the globe as any other gift.
[[nodiscard]] std::optional<GlobeCast> CastGlobe(uint32_t phase, MagicType firstMiracle, uint32_t desire);
} // namespace openblack::creature_giving
