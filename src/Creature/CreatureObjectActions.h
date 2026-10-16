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

/// The animations of what it does to what it holds while keeping it: stroke, shake, smell, examine
constexpr size_t k_FirstKeepAnimation = 100;
constexpr size_t k_KeepAnimationCount = 4;
constexpr size_t k_StrokeObject = 100;
constexpr size_t k_ExamineObject = 103;
/// Pointing, low and high to the left and right
constexpr std::array<size_t, 4> k_PointAnimations {208, 209, 210, 211};
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

/// How the creature's town sees what it does, which its villagers react to: as nothing much, with fear, or with
/// respect
enum class TownAttitude : uint8_t
{
	None,
	Fear,
	Respect,
};
[[nodiscard]] std::string_view Name(TownAttitude attitude);
/// How the town sees an action: eating is fearsome when it is a villager eaten, holding a villager always is
[[nodiscard]] TownAttitude AttitudeTo(Kind kind, bool targetIsVillager, bool holdingVillager);
/// How long the town keeps a view once the creature stops, in seconds: fear for thirty, respect for ten
[[nodiscard]] float AttitudeSeconds(TownAttitude attitude);

/// How an action is going
enum class Status : uint8_t
{
	Running,
	/// It has hold of what it reached for, or the blow has landed, and plays on to the end of the animation
	Contact,
	Done,
	Failed,
};

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
/// continues until the thing is in reach or the action fails: there is no limit to the walks.
enum class ApproachStep : uint8_t
{
	/// The thing is in reach: the animations start
	Begin,
	/// Near enough, it only turns so that the thing lies where its reach is middling
	Turn,
	/// It walks again to the thing
	Walk,
};
[[nodiscard]] ApproachStep NextApproachStep(bool inReach, bool nearEnoughToTurn);

/// The kicking animation
constexpr size_t k_KickAnimation = 117;
/// A thing kicked flies off at this many times the creature's size
constexpr float k_KickSpeedPerSize = 45.0f;
/// How the creature leans with the land under it: the rise across its width, by the heights a radius to either side of
/// it, times this share, at most this much either way
constexpr float k_LeanShare = 0.6f;
constexpr float k_MostLean = 0.3f;
/// How the creature leans along the land's two axes, from the land's heights a radius to either side of it along each
[[nodiscard]] glm::vec2 LeanOf(float plusX, float minusX, float plusZ, float minusZ, float radius);
/// How a thing it kicks starts to fly: half up and half ahead of the creature, the ahead part tipped up or down with the
/// creature's lean, at the kick's speed for its size
[[nodiscard]] glm::vec3 KickVelocity(glm::vec2 forward, glm::vec2 lean, float size);

/// Whether a throw is over: its animation has played out, and, thrown into a store, what it threw has stopped flying
[[nodiscard]] bool ThrowOver(bool animationOver, bool waitsForLanding, bool thrownFlying);

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
	/// Never: creatures, buildings, fields, features other than magic mushrooms, lanterns, shields, vortices and teleport
	/// stones
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
	/// Anything else that can be picked up (villagers, animals, things lying about, pots, rocks, dead trees, trees):
	/// the whole rule
	Whole,
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
};
/// How a creature decides on picking up a kind of thing
[[nodiscard]] KindRule RuleOf(Thing thing);
/// Whether a hand may hold a kind of thing at all
[[nodiscard]] bool Holdable(Thing thing);
/// Whether a kind of thing is alive: villagers, animals, birds and creatures
[[nodiscard]] bool IsLiving(Thing thing);
/// Whether a creature may throw things at a thing to knock it down: a living thing while it lives, and whatever is
/// built or set into the land (shields too), but no tree, lantern, vortex or teleport stone, nor anything lying about
[[nodiscard]] bool CanBeStoned(Thing thing, bool dead);
/// Whether a thing does damage when a creature throws it: the statics, rocks and dead trees, but not the toys
[[nodiscard]] bool DoesThrowingDamage(Thing thing, bool toy);
/// Whether a creature may throw a thing to impress: anything but the living, vortices and teleport stones, as long as
/// the thing's height is less than two thirds (0.64) of the creature's weight. The game sets the thing's height against
/// the creature's weight, so in practice any such thing a creature can pick up will do.
[[nodiscard]] bool CanBeThrownToImpress(Thing thing, float thingHeight, float creatureWeight);
constexpr float k_ThrowToImpressWeightShare = 0.64f;
/// Whether a creature may throw a thing into the sea in play: what lies about, the statics, rocks, dead trees and trees
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
	/// A hand may hold it at all, so a hand may give it to a creature and a creature may catch it: villagers, animals,
	/// things lying about, pots and piles, statics and rocks, dead trees and trees
	bool holdable {false};
	/// One of the toys
	bool toy {false};
	/// A bird low enough over the land
	bool low {false};
	/// A kind of pile that may fly about
	bool mayFly {false};
};
/// Whether a creature may pick the thing up from where it is, by its kind's own rule
[[nodiscard]] bool MayPickUpKind(const KindFacts& kind, const PickUpFacts& facts);
/// Whether a creature may take the thing from a hand that gives it
[[nodiscard]] bool MayTakeKindFromHand(const KindFacts& kind, const PickUpFacts& facts);
/// Whether a creature may catch the thing flying past
[[nodiscard]] bool MayCatchKind(const KindFacts& kind, const PickUpFacts& facts);

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
