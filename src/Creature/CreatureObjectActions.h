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
#include <string_view>

#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

#include "3D/AllMeshes.h"

/// What a creature does with the things about it: picking them up, holding, examining, eating, putting down, tossing
/// away and throwing them, knocking them down, and pointing at them. Each is played on the body by animations, and
/// whatever it is done to is taken hold of or let go of at a moment of the animation particular to the species.
namespace openblack::creature_object_actions
{
enum class Kind : uint8_t
{
	/// Walking up to something and reaching for it with the four reaching animations blended
	PickUp,
	/// Letting go of what it holds: setting it down where the hand is, or tossing it away
	PutDown,
	Discard,
	/// Eating what it holds
	Eat,
	/// Stroking, shaking, smelling or examining what it holds, which it keeps hold of
	Keep,
	/// Throwing what it holds at a point
	Throw,
	/// Walking up to something and striking it with the four striking animations blended
	Destroy,
	/// Pointing at something for a while
	Point,
	/// Catching something flying at it with the four catching animations blended
	Catch,
	/// Reaching out to take what the player's hand gives it, which it takes from the hand at a moment of the animation
	TakeFromHand,
	/// Kicking a thing in front of it, which flies off as the foot meets it
	Kick,
};
[[nodiscard]] std::string_view Name(Kind kind);
/// Whether an action that plays its animations doubles its pace when the creature is told to hurry. Picking up, letting
/// go, eating, keeping hold, knocking down and pointing do. Throwing, catching, taking from the hand and kicking keep
/// their pace.
[[nodiscard]] bool Hurries(Kind kind);
/// Whether the hand divides the time of an action by the length of the stand animation, to know how far the action is
/// through. The game keeps the animation of the stand for picking up, knocking down, throwing, pointing, taking from the
/// hand and kicking. Letting go and keeping hold keep their own animation.
[[nodiscard]] bool SharesByStand(Kind kind);
/// What the stop of the plan that asked for an action does to the action
enum class PlanStop : uint8_t
{
	/// The body did not start the action, so the action stops
	Stops,
	/// The body plays the action to its end, and the moment of the action occurs
	PlaysOn,
	/// The arm plays the end of its point with no hold. A turn to the point ends first.
	EndsPoint,
};
/// What a plan stop does to an action of the kind. The body starts an action when it plays its animations, or when it
/// turns on the spot for the action: to get a thing in reach, to throw or to point. Only a dance, a fight, the static
/// loop and a point stop with the plan. Each other action that the body started plays on.
[[nodiscard]] PlanStop OnPlanStop(Kind kind, bool playing, bool turnsForIt);

/// What a plan stop does to the thing of an eat
enum class EatStop : uint8_t
{
	/// The thing stays in the hand
	Keeps,
	/// The creature swallows the thing at once
	Swallows,
	/// The thing in the hand goes out of the world, uneaten. The creature keeps the food that it took in at the start.
	Deletes,
};
/// What a plan stop does to the thing of an eat. After the bite, the creature swallows the thing at once. Before the
/// bite, while the eat animation plays, the thing in the hand goes out of the world.
[[nodiscard]] EatStop OnEatStop(bool playing, bool bitten, bool holds);

/// Whether the arm that holds a thing shows the hold pose over the body, while the body plays the animation of an
/// action, or no action (none). The animations that move the held thing themselves replace the hold pose: picking up,
/// letting go or eating, stroking, shaking, smelling or examining, the throw itself, catching and taking from the god's
/// hand. Walking or turning to the thing first, knocking down, kicking and pointing keep it.
[[nodiscard]] bool ShowsHoldPose(std::optional<Kind> playing);

/// The animations of what it does to what it holds while keeping it: stroke, shake, smell, examine
constexpr size_t k_FirstKeepAnimation = 100;
constexpr size_t k_KeepAnimationCount = 4;
constexpr size_t k_StrokeObject = 100;
constexpr size_t k_ExamineObject = 103;
/// It points for this long, and turns to face what it points at first when it is more than this far round
constexpr float k_PointSeconds = 5.0f;
constexpr float k_PointTurnRadians = 0.4712f;
/// It turns to face what it throws at first when it is more than this far round
constexpr float k_ThrowTurnRadians = 0.01f;

/// What it needs in its hands to do it
enum class Hands : uint8_t
{
	/// Something to act on
	Holding,
	/// Nothing, or what it holds is put down first
	Empty,
	/// Its hands as they are
	Either,
};
[[nodiscard]] Hands HandsFor(Kind kind);

/// How an action is going
enum class Status : uint8_t
{
	Running,
	/// It has hold of what it reached for, or the blow has landed, and plays on to the end of the animation
	Contact,
	Done,
	Failed,
};

/// Whether the animations of an action make the pose of the body for the turn, in place of the clips of the walk. This
/// is true only while the action plays its animations and has not ended. While the creature walks or turns to the
/// thing, the walk makes the pose.
[[nodiscard]] bool PosesBody(bool playing, Status status);

/// Whether the thing in the hand of a creature counts as what the creature carries, while the hand does an action of
/// this kind and status (none for no action). As a throw starts, the thing stops being what the creature carries. The
/// hand keeps the thing until the release, and then lets it go. Until then the mind, the scripts, the towns and the
/// thing itself see that the creature carries nothing.
[[nodiscard]] bool CarriesHeld(std::optional<Kind> kind, Status status, bool released);

/// Why an action failed, to the extent that the mind must know
enum class Failure : uint8_t
{
	/// It did not fail, or it failed for a different cause
	Other,
	/// It found no way to the thing: the creature cannot stand where the thing is, or it can no longer pick the thing up
	NoWay,
	/// On the walk to the thing, the search found no route
	NoRoute,
};
/// Whether the creature does not choose the thing again for a time after the action failed. A pick-up, a stomp or a kick
/// that found no way or no route to its thing does this.
[[nodiscard]] bool LeavesThingAlone(Kind kind, Failure failure);
/// Whether the creature makes a sound of refusal when the action failed: only when the search found no route
[[nodiscard]] bool RefusesAloud(Failure failure);

/// What a creature that walks up to a thing to pick it up or stomp on it does on a turn when it stands still. It
/// continues until the thing is in reach or the action fails: there is no limit to the walks. It never turns in place
/// to get the thing in reach.
enum class ApproachStep : uint8_t
{
	/// The thing is in reach: the animations start
	Begin,
	/// It cannot see the thing yet: it walks near to the thing, and stops at twice their two radii from it
	WalkNear,
	/// It sees the thing out of reach: it walks to the thing, and stops at its straight reach from it
	WalkInReach,
};
/// The next step from what the creature finds now:
/// - seen: the thing is less than ten metres away, or less than fifty times its height (creature_mind::CanSee);
/// - flatDistance: the distance along the ground from the creature to the thing;
/// - straightReach: how far the hand or foot gets in the reaching animations (creature_reach::StraightReach);
/// - inRange: the thing lies where the reaching animations get to (creature_reach::Blend::inRange).
[[nodiscard]] ApproachStep NextApproachStep(bool seen, float flatDistance, float straightReach, bool inRange);
/// What a creature that walks up to a thing to pick it up or stomp on it does on a turn when it moves
enum class MovingApproachStep : uint8_t
{
	/// It turns on the spot to get the thing in reach. This turn is the start of the reach, so the walk-up does nothing:
	/// no new walk, no count of the turns with no progress, and no new test of the thing.
	WaitForTurn,
	/// It is on its way to the thing: it aims at the thing again
	AimAgain,
};
/// The next step while the creature moves. turnsToReach: it turns on the spot because the thing was outside the corners
/// of its reach.
[[nodiscard]] MovingApproachStep NextMovingApproachStep(bool turnsToReach);
/// Whether a kept measure of where a hand or foot gets in the animations is correct for the body now. measuredFor is
/// the revision of the body shape at the measure, or none when there is no measure. Each time the animations follow a
/// new body shape, the next use measures the hand or foot again. The reach, the catch and the toss all use this rule.
[[nodiscard]] bool MeasureHolds(std::optional<uint32_t> measuredFor, uint32_t bodyRevision);
/// How far from the thing the walk near to it stops: twice the creature's radius and the thing's radius added
[[nodiscard]] float NearWalkDistance(float creatureRadius, float thingRadius);
/// A thing higher than this above the creature, in metres, can be too high to pick up
constexpr float k_MostPickUpRise = 5.0f;
/// Whether a thing in reach is too high to pick up: the difference of the heights of the two above the ground is
/// more than k_MostPickUpRise and more than the distance between them. The pick-up then stops.
[[nodiscard]] bool TooHighToPickUp(float rise, float distance);

/// Whether a walk up to a thing to pick it up or stomp on it moves the creature. In the game, this walk has no limit.
/// When the thing is in the circle of a building, or is not where the creature believes it is, each new walk ends at
/// once where it started. Thus the creature never gives up. openblack gives up as a walk near a thing does
/// (creature_cast_moves::CheckStall). It records where the creature is every k_StallTurns turns. On the last turn of
/// that span, it gives up when the creature moved less than k_StallDistance. A creature under the control of a script
/// never gives up. The walk then fails as a walk to a place where the creature cannot stand fails (deviation 90).
/// A turn on which one route search of the walk continues, before the creature has a route, does not count: the search
/// can take many turns to a far thing, and the game has no limit on it. A search that starts again on each turn, to a
/// new place of a thing that moves, counts as before.
struct ApproachStall
{
	/// The turns of the walk so far
	uint32_t turns {0};
	/// Where the creature was at the last record
	glm::vec2 note {0.0f};
	/// Where the route search went to on the last turn, while the creature had no route
	std::optional<glm::vec2> searchedFor;
};
/// Counts one turn of the walk, with the creature here. Gives true when the creature gives up. `searchingFor` is the
/// destination of the route search when the creature has no route yet. A turn on which the search to the same
/// destination as on the last turn continues does not count and does not give up.
bool StallsOnTurn(ApproachStall& stall, glm::vec2 here, bool scripted, std::optional<glm::vec2> searchingFor);

/// The kicking animation
constexpr size_t k_KickAnimation = 117;
/// A thing kicked flies off at this many times the creature's size
constexpr float k_KickSpeedPerSize = 45.0f;
/// How a thing it kicks starts to fly: half up and half ahead of the creature, the ahead part tipped up or down with the
/// creature's lean, at the kick's speed for its size. The scale is the creature's drawn scale
/// The way of a kick, of length 1: half up and half ahead of the creature, the ahead part tipped up or down with the
/// creature's lean. A zero way stays zero. The scale is the creature's drawn scale
[[nodiscard]] glm::vec3 KickDirection(glm::vec2 forward, glm::vec2 lean, float scale);
[[nodiscard]] glm::vec3 KickVelocity(glm::vec2 forward, glm::vec2 lean, float scale, float size);

/// Whether a throw is over: its animation has played out, and, thrown into a store, what it threw has stopped flying
[[nodiscard]] bool ThrowOver(bool animationOver, bool waitsForLanding, bool thrownFlying);

/// The time of a reach for a pick-up or a stomp in the turn that the thing gets out of reach: one step less than the
/// time before the turn, and not less than zero
[[nodiscard]] float WithdrawStart(float timeBeforeMs, float stepMs);
/// The time of a reach that lost its thing, one turn later: one step less. Nothing when the step is longer than the
/// time that is left: then the reach is at its start, and the action fails.
[[nodiscard]] std::optional<float> WithdrawStep(float timeMs, float stepMs);

/// Whether a creature about to swallow what it holds brings it back up instead, uneaten: a thing made indestructible,
/// or one the scripts hold while they own the widescreen bars, can't be eaten. The creature then lets it fall and is
/// sick, and isn't hungry for a while.
[[nodiscard]] bool BringsBackUp(bool indestructible, bool heldDuringCutscene);
/// A creature only lifts what weighs at most this share of its own weight
constexpr float k_MostLiftedWeightShare = 0.8f;
/// How big and heavy the thing is against the creature
struct LiftFacts
{
	/// The creature's and the thing's radii across the land
	float creatureRadius {0.0f};
	float thingRadius {0.0f};
	/// How far the creature's hand gets at its furthest stretch when picking up, measured straight
	float reach {0.0f};
	float creatureWeight {0.0f};
	float thingWeight {0.0f};
};
/// Whether a creature can lift the thing: the two radii together within its reach, and the thing no heavier than its
/// share of the creature's weight
[[nodiscard]] bool CanLift(const LiftFacts& facts);

/// What decides whether a creature may pick up a thing whose kind can be picked up
struct PickUpFacts
{
	/// A script said it can't be picked up, as a land's script says of a villager it is showing the player
	bool cannotBePickedUp {false};
	/// A villager inside its home
	bool villagerAtHome {false};
	/// A villager on its way to worship at, or worshipping at, the creature's own player's worship site
	bool worshippingForCreaturesPlayer {false};
	/// A script holds a reference to the thing
	bool inScript {false};
	/// The creature belongs to a computer player, and isn't under a script's control
	bool computerCreatureNotScripted {false};
	/// A rock, static or dead tree that has been made a town's artefact
	bool townArtefact {false};
	/// It stands in the map, out of any hand, flight or tornado; or it is what the creature already holds
	bool inMapOrHeldByCreature {true};
	/// The creature's leash is on and tied to the thing
	bool leashTiedToIt {false};
	/// How big and heavy it is against the creature; none when the creature's reach can't be measured, as for a
	/// creature not posed yet
	std::optional<LiftFacts> lift;
};
/// Whether a creature may take the thing wherever it is, as from a hand that gives it: never what its leash is tied to,
/// nor what it can't lift, nor one a script said can't be picked up or a villager at home
[[nodiscard]] bool MayPickUpWhereverItIs(const PickUpFacts& facts);
/// Whether a creature may pick the thing up from where it is: as wherever it is, and never a town's artefact, a thing
/// out of the map that it isn't holding already, or a villager off to worship for the creature's own player; and a
/// computer player's creature, when no script controls it, leaves alone whatever a script holds. A player's creature
/// may pick up what a script holds once the script allows it.
[[nodiscard]] bool MayPickUp(const PickUpFacts& facts);
/// Whether a flying thing may be caught at all, before the weights are compared: only the thing's own say counts, not
/// one a script said can't be picked up nor a villager at home
[[nodiscard]] bool MayBeCaught(const PickUpFacts& facts);

/// How a creature decides whether it may pick up a kind of thing
enum class KindRule : uint8_t
{
	/// Never: creatures, buildings, fields, features other than magic mushrooms, lanterns, shields, vortices, teleport
	/// stones and building pieces
	Never,
	/// Always, whatever else holds: the football and a creed
	Always,
	/// A static other than a rock: only when it is one of the toys, whatever else holds
	Toy,
	/// A bird: only when it is low enough over the land, whatever else holds
	Bird,
	/// A magic mushroom: unless a script said it can't be picked up
	Mushroom,
	/// A pile of food or wood: unless a script said it can't be picked up, and only a kind of pile that may fly about
	Pile,
	/// Anything else that can be picked up (villagers, animals, things lying about, pots, rocks, dead trees, trees,
	/// reward chests): the whole rule
	Whole,
	/// A scaffold: only when a hand can take it, and then the whole rule
	Scaffold,
};

/// The kinds of thing that differ in how a creature picks them up
enum class Thing : uint8_t
{
	/// Anything not named below: buildings, fields, features and the like
	Other,
	Creature,
	Villager,
	/// An animal other than a bird
	Animal,
	Bird,
	/// The football
	Ball,
	Creed,
	/// Anything else lying about
	MobileObject,
	Pot,
	/// A pile of food or wood
	Pile,
	Rock,
	Bonfire,
	DeadTree,
	/// A standing tree
	Tree,
	/// A magic mushroom growing on the land
	MagicMushroom,
	/// Any other static: only the toys among them are picked up
	Static,
	/// Statics with kinds of their own that no hand holds
	Lantern,
	Shield,
	/// Statics with kinds of their own that a hand could hold but a creature never picks up
	Vortex,
	TeleportStone,
	/// A thing built or set into the land that stones can knock about: a building, temple, totem or worship site, a
	/// feature other than a magic mushroom, an animated static or flowers
	Fixture,
	/// A piece that broke off a building. It is a rock that a creature never picks up from the land.
	BuildingPiece,
	/// A reward chest
	Reward,
	/// A scaffold of a workshop
	Scaffold,
};
/// How a creature decides on picking up a kind of thing
[[nodiscard]] KindRule RuleOf(Thing thing);
/// Whether the kind of a thing lets something pick it up at all. A creature that catches a thing or takes it from a
/// hand asks only this, not the kind's rule. Buildings, features, lanterns and shields say no. Creatures say yes.
[[nodiscard]] bool CanBePickedUp(Thing thing);
/// Whether a kind of thing is alive: villagers, animals, birds and creatures
[[nodiscard]] bool IsLiving(Thing thing);
/// Whether a kind of thing is a rock for the creature's work with rocks: a rock, a bonfire or a piece that broke off a
/// building. The creature breaks it in two and holds it as a rock.
[[nodiscard]] bool IsRock(Thing thing);
/// Whether a creature may throw things at a thing to knock it down: a living thing while it lives, and whatever is
/// built or set into the land (shields and building pieces too). Not a tree, lantern, vortex or teleport stone, nor
/// anything lying about
[[nodiscard]] bool CanBeStoned(Thing thing, bool dead);
/// Whether a thing does damage when a creature throws it: the statics, rocks, building pieces and dead trees, but not
/// the toys
[[nodiscard]] bool DoesThrowingDamage(Thing thing, bool toy);
/// Whether a creature may throw a thing to impress: anything but the living, vortices and teleport stones, as long as
/// the thing's height is less than two thirds (0.64) of the creature's weight. The game sets the thing's height against
/// the creature's weight, so in practice any such thing a creature can pick up will do.
[[nodiscard]] bool CanBeThrownToImpress(Thing thing, float thingHeight, float creatureWeight);
constexpr float k_ThrowToImpressWeightShare = 0.64f;
/// Whether a creature may throw a thing into the sea in play: what lies about (reward chests and scaffolds too), the
/// statics, rocks, building pieces, dead trees and trees
[[nodiscard]] bool CanBeThrownInTheSea(Thing thing);

/// The toys among the statics, known by their models
enum class Toy : uint8_t
{
	Ball,
	BowlingBall,
	Cuddly,
	Die,
	Skittle,
};
/// Which toy a static's model makes it, if any
[[nodiscard]] std::optional<Toy> ToyOf(MeshId mesh);

/// What the kind of a thing says about its being picked up by a creature
struct KindFacts
{
	Thing thing {Thing::Other};
	KindRule rule {KindRule::Never};
	/// Its kind lets something pick it up at all, so a hand may give it to a creature and a creature may catch it:
	/// all but buildings, features, lanterns and shields
	bool canBePickedUp {false};
	/// One of the toys
	bool toy {false};
	/// A bird low enough over the land
	bool low {false};
	/// A kind of pile that may fly about
	bool mayFly {false};
	/// A scaffold that a hand can take now
	bool handCanTake {false};
};
/// Whether a creature may pick the thing up from where it is, by its kind's own rule
[[nodiscard]] bool MayPickUpKind(const KindFacts& kind, const PickUpFacts& facts);
/// Whether a creature may take the thing from a hand that gives it
[[nodiscard]] bool MayTakeKindFromHand(const KindFacts& kind, const PickUpFacts& facts);
/// Whether a creature may catch the thing flying past
[[nodiscard]] bool MayCatchKind(const KindFacts& kind, const PickUpFacts& facts);

/// What happens to the living state of a thing at the moment a creature's hand closes on it
enum class HoldStart : uint8_t
{
	/// The thing has no living state of its own: it is only held
	NoChange,
	/// A person or an animal remembers what it did and goes into its state for the hand
	IntoHandState,
	/// It cannot go into its state for the hand: the hand lets it go and the take fails
	Refused,
};
/// The change to the living state of a thing as a creature picks it up, catches it or takes it from the player's hand.
/// A creature held by another creature keeps its own state.
[[nodiscard]] HoldStart HoldStartFor(bool living, bool creature, bool acceptsHandState);

/// What happens to a thing that left a creature's hand at the bite of an eat, when the eat ends
enum class EatenThingEnd : uint8_t
{
	/// It is still to be swallowed as the eat animation ends
	Wait,
	/// The eat ended or stopped: the creature swallows it now
	Swallow,
	/// The creature is gone: the thing comes back into sight and falls
	Fall,
};
/// The end of a thing between the bite and the swallow. The game left such a thing out of sight for good when the eat
/// stopped early; openblack swallows it as the eat would have (deviations list).
[[nodiscard]] EatenThingEnd EatenThingEndFor(bool eatGoesOn, bool creatureExists);

/// How long a creature that brought its food back up isn't hungry for
constexpr float k_NotHungryAfterBringingUpSeconds = 20.0f;

/// What decides if a creature eats a thing, other than the kind of the thing
struct EatFacts
{
	/// The creature may pick up the thing by the full rule
	bool mayPickUp {false};
	/// The creature brought up a meal of the same sort of object before
	bool refusedSort {false};
	/// The thing is a villager of the creature's player, and the creature is a computer player's that no script controls
	bool computersOwnVillager {false};
};
/// Whether a kind of thing is a toy: the football, or a static toy
[[nodiscard]] bool IsToy(const KindFacts& kind);
/// Whether a kind of thing is a static, which a creature holds and eats by its own rule
[[nodiscard]] bool IsStatic(Thing thing);
/// Whether a creature may eat a thing: a thing that it may pick up and that is not a toy. The creature does not eat a
/// sort of thing that it brought up before.
[[nodiscard]] bool MayEat(const KindFacts& kind, const EatFacts& facts);
/// Whether a creature may eat the thing that it holds, with no pick-up first. A static that is not a toy is food in the
/// hand. Other things must agree with the full pick-up rule.
[[nodiscard]] bool MayEatHeld(const KindFacts& kind, bool mayPickUp);
/// A meal with less food than this comes back up, unless it is a magic mushroom or a pile of food
constexpr float k_LeastFoodKeptDown = 5.0f;
/// Whether a creature brings up a meal after it eats it. The creature then is sick and is not hungry for a time.
[[nodiscard]] bool BringsMealBackUp(float food, bool magicMushroom, bool foodPile, bool poisoned);
/// Whether a creature that brings up a meal also stops to eat that sort of thing. Poisoned food does not stop it.
[[nodiscard]] constexpr bool RefusesSortAfter(bool poisoned)
{
	return !poisoned;
}
} // namespace openblack::creature_object_actions
