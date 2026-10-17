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
#include <functional>
#include <memory>
#include <optional>
#include <span>
#include <string_view>
#include <utility>
#include <vector>

#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

#include "Creature/CreatureDesires.h"
#include "Creature/CreatureFace.h"
#include "Creature/CreatureFriendship.h"
#include "Creature/CreaturePlayerAttention.h"

/// What a creature does, decided once a game turn. It works through an agenda of steps: it waits while it looks about,
/// plays an action, sits for a while or goes to a place. Each step pulls a face for what it does or feels as it starts.
/// It pulls the face again every few seconds while the step continues: when idle, a random face for three seconds.
///
/// The planner and the forced plans give the agendas. When the agenda is done and no plan gives a new agenda, the
/// creature does nothing. If the creature is exhausted, starved or has no life, it faints where it is and gets up a
/// short time later.
namespace openblack::creature_mind
{
/// Each step pulls a face as it starts, and again this often for as long as it lasts: every four seconds and two turns
constexpr float k_FaceRepeatSeconds = 4.2f;
/// After each face it pulls, the creature's variety of faces moves on by up to this much less one
constexpr uint32_t k_FaceVarietyStep = 7;
/// Idle waits are one second and up to one more
constexpr float k_IdleWaitSeconds = 1.0f;
/// Being idle is waiting then yawning, this many times
constexpr int k_IdleRepeats = 2;
/// Sitting lasts ten seconds and up to four more
constexpr uint32_t k_SitSeconds = 10;
constexpr uint32_t k_SitExtraSeconds = 5;
/// Hanging around at home, the creature walks home and stops at its height from home, or at this distance when the
/// creature is taller
constexpr float k_HangAroundHomeReach = 5.0f;
/// At home it waits for two seconds and up to three more, in whole turns
constexpr float k_HangAroundWaitSeconds = 2.0f;
constexpr float k_HangAroundWaitExtraSeconds = 3.0f;
/// With a temple, the creature does one of these lots at random. The first lot is a tired yawn. The next seven send
/// it out from home, and the last two make it sit and look at the temple.
constexpr uint32_t k_HangAroundLots = 10;
constexpr uint32_t k_HangAroundOutLots = 7;
/// It walks out from home, away from the temple, for this part of the radius of the area that holds it. It stops at
/// this distance from that point.
constexpr float k_HangAroundOutPart = 0.8f;
constexpr float k_HangAroundOutReach = 2.0f;
/// Out from home, it faces a point this far from home, away from the temple. Half the time it sits, else it points
/// at the point for this many seconds.
constexpr float k_HangAroundViewDistance = 50.0f;
constexpr float k_HangAroundPointSeconds = 1.0f;
/// Sitting while it hangs around lasts ten seconds and up to five more
constexpr float k_HangAroundSitSeconds = 10.0f;
constexpr float k_HangAroundSitExtraSeconds = 5.0f;
/// When home and the temple are this near along each ground axis, the creature goes out along the x axis
constexpr float k_HangAroundSameSpot = 0.001f;
/// Off to sleep, the creature yawns first one time in this many; off for a poo, it shows it needs one first one time in
/// this many
constexpr uint32_t k_YawnBeforeSleepLots = 2;
constexpr uint32_t k_ShowPooLots = 3;
/// A creature about to eat a thing at once shows that it is hungry first one time in this many
constexpr uint32_t k_ShowHungerBeforeEatLots = 2;
/// A poo and being sick each last this many seconds
constexpr float k_PooSeconds = 4.0f;
constexpr float k_PukeSeconds = 4.0f;
/// Fainted, it lies out cold four seconds for each unit of its size, and eight more
constexpr float k_FaintSecondsPerSize = 4.0f;
constexpr float k_FaintExtraSeconds = 8.0f;
/// Looking something over, it puts it down gently most times, out of this many, and tosses it away the rest
constexpr uint32_t k_PutDownLots = 10;
constexpr uint32_t k_TossLots = 2;
/// Throwing something about, it throws it this far away and up to this much further, in a random direction
constexpr float k_ThrowAroundDistance = 30.0f;
constexpr uint32_t k_ThrowAroundExtra = 21;
/// Turning to face a store before throwing into it, it holds still this long
constexpr float k_FaceStoreSeconds = 0.1f;
/// Going up to something to act at it, it stops this close
constexpr float k_ApproachDistance = 12.0f;
/// Waving at a friend, it turns to face the friend and holds still this long first
constexpr float k_ApproachFaceSeconds = 1.4f;
/// Following a villager to watch it: for up to 30 whole seconds, keeping within 15 of it; then holding for one second
/// and up to three more
constexpr float k_FollowWatchingSeconds = 30.0f;
constexpr float k_FollowWatchingDistance = 15.0f;
constexpr float k_FollowWatchingHoldSeconds = 1.0f;
constexpr float k_FollowWatchingExtraHoldSeconds = 3.0f;
/// Off to hurl something, it shows its anger first one time in this many
constexpr uint32_t k_AngryBeforeHurlLots = 6;

/// What the creature is doing
enum class Activity : uint8_t
{
	None,
	BeIdle,
	Sit,
	ShowDesire,
	/// Walking somewhere nearby and sitting there
	HangAround,
	/// Something it was told to do
	Told,
	/// Seeing to its body: eating, drinking, sleeping, having a poo, being sick, and lying out cold
	Eat,
	Drink,
	Sleep,
	Poo,
	Puke,
	Faint,
	/// Doing things with what is nearby: picking something up to look it over, throwing it about, hurling it at
	/// something, and putting down what it holds
	Examine,
	PlayWithObject,
	Hurl,
	PutDown,
	/// Something else the planner chose to satisfy a desire: an emote, looking at or following something, running away,
	/// destroying something, greeting something
	Planned,
};
[[nodiscard]] std::string_view Name(Activity activity);

/// Something done with the creature's hands to a thing about it
struct ObjectOrder
{
	enum class Kind : uint8_t
	{
		PickUp,
		PutDown,
		Discard,
		Eat,
		/// Stroking, shaking, smelling or examining what it holds, by the animation
		Keep,
		/// Throwing what it holds at a point, or at a point as far from where the creature is as the point is from the
		/// origin
		Throw,
		ThrowNearby,
		Destroy,
		/// Pointing at a point
		PointAt,
		/// Catching a thing flying at it
		Catch,
		/// Bringing food out of the sea: a handful of it appears at its feet, sized to the creature, and it picks it up.
		/// Nothing is taken from the fish farm it stands by.
		FishFromSea,
		/// Throwing what it holds into a store at the store's own place, taking as long to fly there as it would take to
		/// fall the distance along the ground
		ThrowInStore,
		/// The creature takes a handful of food from a thing: a pile of food, the food pile of a store, a field or a
		/// tree. The size of the handful follows the size of the creature. The handful holds no more food than the store
		/// of food of the thing. The handful appears near the creature, and the creature lifts it.
		TakeFromPile,
		/// Reaching out to take what the player's hand gives it, with its left hand when mirrored
		TakeFromHand,
		/// Kicking the object, which flies off ahead of it
		Kick,
	};
	/// The look of a handful from a thing: wheat, or fruit from a tree; or wood from a pile of wood, as an evergreen
	/// branch, a hardwood branch or logs
	enum class Handful : uint8_t
	{
		Wheat,
		Fruit,
		EvergreenBranch,
		HardwoodBranch,
		Logs,
	};
	Kind kind {Kind::PickUp};
	Handful handful {Handful::Wheat};
	/// What it acts on, by its entity's number. Pointing at an object, it points at wherever the object is, turn by
	/// turn.
	std::optional<uint32_t> object;
	glm::vec2 point {0.0f};
	size_t animation {0};
	/// Pointing: how high the point is, when it isn't on the ground, and how long it points, when not for its usual while
	std::optional<float> pointHeight;
	std::optional<float> seconds;
	/// Taking from the player's hand with its left hand
	bool mirrored {false};
	/// Throwing: the creature aims at the place of the object as the throw starts, not at the point
	bool atObject {false};
};

/// Going somewhere, or turning to face something
struct Movement
{
	enum class Kind : uint8_t
	{
		/// To a point, or to a point as far from where the creature is as the point is from the origin
		ToPoint,
		Nearby,
		/// Up to an object, or following it until the step's time is up
		ToObject,
		Follow,
		/// Running away from a point
		FleeFrom,
		TurnToFace,
		/// Going near an object, keeping the step's distance from it; getting away from it to that distance; turning
		/// to face it and holding still a moment, the step's seconds. Each goes on until the creature's movement says
		/// it is done or has failed.
		GoNearObject,
		GetAwayFromObject,
		TurnToFaceObject,
		/// Turning so that its back is to an object, then waiting until the turn ends. Right on top of the object it
		/// does not turn
		TurnToFaceAwayFromObject,
		/// Going to where it can throw into an object: nearer than its height three times over plus the object's radius
		/// it backs away, further than three and a half times its height it walks up; the step's distance is the
		/// creature's height
		ToThrowPosition,
		/// Turning to face a point as a script asks: within an eighth of a half turn of facing it, or right on top of it,
		/// it counts as facing it already and doesn't turn
		FacePoint,
		/// Turning to face the lowest of the ground around it, done once it faces it, or at once when it stands on top of
		/// it
		FaceDownSlope,
	};
	Kind kind {Kind::ToPoint};
	glm::vec2 point {0.0f};
	/// The object walked up to or followed, by its entity's number
	std::optional<uint32_t> object;
	bool run {false};
	/// An extra fraction of the top speed, added to the walk speed in place of the run. The creature walks back into the
	/// area that holds it at such a speed.
	std::optional<float> extraSpeed;
	/// Arriving anywhere from min to max from the point, or keeping within max of what it follows
	float minDistance {0.0f};
	float maxDistance {0.0f};
	/// When the creature cannot stand at the point, the walk to the point stops the rest of the agenda. The walks of a
	/// leash order, of a script and of a step that walks again in its turns do not stop it. These walks count as done.
	bool giveUpIfUnreachable {true};
};

/// A miracle cast at a thing, by its magic type's number
struct CastOrder
{
	uint32_t magicType {0};
	/// What it is cast at, by its entity's number
	std::optional<uint32_t> object;
	/// The place that it is cast at, when the miracle goes to a place and not to a thing
	std::optional<glm::vec3> place;
};

/// One stage of a creature's dance with the villagers of a town
struct DanceOrder
{
	enum class Kind : uint8_t
	{
		/// Make a new dance of a row of the dance table at a point. The creature's last dance goes.
		Set,
		/// Call the free grown up people of a villager's town to the dance, at least the step's count of them
		Summon,
		/// Stop the head that looks about. No later step of the agenda turns the head.
		StopLooking,
		/// Send each called villager to its place in the dance, at a run
		SendToPlaces,
		/// Wait until none of the dancers moves
		WaitUntilStill,
		/// Start the dance from the start of its clock
		Start,
		/// Tell each called villager to dance and worship. The step fails when its villager is gone.
		TellToDance,
		/// Join the dance and dance in it for the step's count of turns, then stop the dance
		Dance,
		/// Stop the dance, at the end of the last step
		Stop,
		/// Remove the creature's last dance, the turn before it makes a new one
		Clear,
	};
	Kind kind {Kind::Set};
	/// Where the dance is, and its row of the dance table
	glm::vec2 point {0.0f};
	uint32_t type {0};
	/// The villager the dance is for, by its entity's number
	std::optional<uint32_t> villager;
	/// The fewest villagers to call, or the turns to dance
	uint32_t count {0};
};

/// What a step does to the creature's body
enum class Effect : uint8_t
{
	None,
	/// Has eaten what it held
	Eat,
	Drink,
	Poo,
	/// Did a fart
	Fart,
	Puke,
	/// Wakes up rested
	Slept,
	/// Comes round from a faint
	CameRound,
	/// Has looked something over, thrown something about, and hurled something
	Examined,
	ThrewAbout,
	Hurled,
	/// The action it is carrying out counts as done here, before the step that finishes it
	Completed,
	/// Has had a good look at a thing (the effect's object), and is far less curious about things of its kind from now on
	Observed,
	/// Has picked up the food it took from a pile, which the pile now gives up
	TookFromPile,
	/// Has taken what the player's hand gave it (the step's object), and decides what to do with it
	TookFromHand,
	/// Has looked another creature (the effect's object) over, and waits a while before it does again
	Inspected,
	/// Has stomped on a thing (the effect's object). When the stomp is the last step of the agenda and the thing is
	/// dead, the creature counts the kill, and the action sees to its desire.
	Stomped,
	/// Gets up from a faint with four tenths of its life
	Revived,
	/// Has stopped a run away from a thing (the effect's object) or from the player's hand. When it is far enough from
	/// it, the run sees to the desire of the plan.
	RanAway,
};

/// How the eyes should look
enum class Eyes : uint8_t
{
	Unchanged,
	/// Drooping and slowly blinking, as if just woken up. The lids stay down until it stops what it does.
	Sleepy,
	/// Calm again, as after its eyes were closed. The lids and the blinking stay as they were.
	Normal,
	Closed,
	/// Wide open and darting about
	Stoned,
	/// Wide open, as when frightened
	Wide,
};

/// What a creature's head is made to look at: a thing (its top, or its foot), a point, or the player's camera
struct Gaze
{
	std::optional<uint32_t> object;
	bool bottom {false};
	/// The thing itself where it is, as up in the air, rather than its top or foot
	bool itself {false};
	/// The player's hand nearest the creature
	bool hand {false};
	/// The player: the hand when it moved much lately, else the camera
	bool player {false};
	std::optional<glm::vec3> point;
	bool camera {false};
	/// The look of a creature that walks to the point: at the point, at what it watches, and a sweep from side to side
	/// (see creature_look::LookWhileGoing). Without a point, the look uses the top of the object, or else the place that
	/// the creature walks to.
	bool whileGoing {false};
	/// The look of a creature that runs away from the object or the hand: it looks back at it for a time, then looks
	/// about for a time, and so on
	bool runningAway {false};
	/// The ground a few metres ahead of where its body faces (see k_FeetLookMetres)
	bool feet {false};
};
/// Looking at its feet, it looks at the ground this many metres ahead of where its body faces
constexpr float k_FeetLookMetres = 4.0f;
/// The look while the creature walks to the place of its walk
[[nodiscard]] inline Gaze GoingLook()
{
	return {.whileGoing = true};
}
/// The look while the creature walks to a point, or throws something at it
[[nodiscard]] inline Gaze GoingLook(glm::vec2 point)
{
	return {.point = glm::vec3(point.x, 0.0f, point.y), .whileGoing = true};
}

struct Step
{
	enum class Kind : uint8_t
	{
		/// Waiting while looking about
		Wait,
		/// An action played once
		Action,
		/// A start, a loop for a while and an end, such as sitting down, sitting while looking about, and getting up
		Static,
		/// Going somewhere until it arrives or gives up, or for the step's seconds when they are more than 0
		Move,
		/// Doing something with a thing until it is done; if it can't, the rest of the agenda is given up
		Object,
		/// Casting a miracle: the start of the casting pose, then the miracle as its loop begins, held for the step's
		/// seconds counted in whole turns, then let go of as the end plays; if it can't be cast, the rest of the agenda
		/// is given up
		Cast,
		/// Drawing a gesture in the air with its hand, by the gesture's number in the step's animation, until the
		/// creature's movement says it is done or has failed
		Gesture,
		/// Waiting for the step's object to be back on the map (out of any hand, flight or tornado); if the object is
		/// gone, the rest of the agenda is given up
		WaitInMap,
		/// The creature puts water on the step's object at once. Only the plain "put out fire" action has this step.
		Douse,
		/// The creature's own fire stops at once: the creature cools to the temperature of the air
		CoolDown,
		/// Breaking the step's object, a rock, in two at once; when it isn't a rock, the rest of the agenda is given up
		Shatter,
		/// Mending the step's object, a building, by a fifth of its life at once
		Repair,
		/// Doing away with the step's object at once, as with the tree used up mending a building
		UseUp,
		/// Doing away at once with what its hand holds now, as with a one-shot miracle that it casts
		UseUpHeld,
		/// Watching its player's camera: turning to it whenever it is more than a quarter turn away, until whole seconds
		/// of turns more than the step's seconds have gone by, or the camera is right overhead or more than 100 away. A
		/// creature without a player is done at once. With the step's turns set it watches for that many turns whatever
		/// the camera does, turning to it unless it is right overhead.
		WatchCamera,
		/// Taking in what the step's object is, done at once
		Observe,
		/// Swapping minds with another creature: every spell on it is brought to its end until none is on it or on the
		/// other creature; then it plays the step's animation (not while its body plays another), starting the beam
		/// between them, and turn by turn it takes on the shape the swap gives it until it has
		SwapMind,
		/// Playing the step's animation on top of the body, as talking: it starts unless something already plays there,
		/// and the step is done once nothing plays on top of the body
		Overlay,
		/// Looking another creature (the step's object) over: as it starts it picks a time of the step's seconds and up
		/// to the step's extra seconds more, counted in whole turns from the next, after which it has looked it over
		Inspect,
		/// Watching a thing flying through the air (the step's object): each turn, from the one it starts on, it goes on
		/// while the thing is still really in the air and the whole seconds it has watched come under a limit picked
		/// afresh each turn, from two seconds to two seconds more: not in the first three seconds, half the time each
		/// turn in the fourth, never after
		WatchFlying,
		/// Watching the player's hand: each turn, from the one it starts on, it turns to face the hand when it is more
		/// than a quarter turn away (unless right under it), and once it isn't turning it is done when the whole
		/// seconds it has watched come to more than those of the step's turns, at most five; a creature without a
		/// player is done at once
		WatchHand,
		/// Communicating its state to the player: as it starts, it plays the animation of what it shows, once. With
		/// nothing to show, the step is done at once.
		Communicate,
		/// Stopping where it is, done once its body plays no action. It starts even while the body moves.
		StopMoving,
		/// A stage of a dance with the villagers of a town (see DanceOrder)
		Dance,
		/// Pulling a face, by the face's animation, held for the step's seconds. It is done at once, and the next step
		/// starts on the next turn.
		Face,
		/// Following the point of focus of its player's camera, for the step's seconds counted in whole turns, until it
		/// is within the movement's distance of the point (see creature_emote::FollowCameraTurn)
		FollowCamera,
		/// Healing the step's object at once: it gets all its life back, and a villager goes home or an animal wanders
		Heal,
		/// The creature heals itself at once: its life becomes full, and its wounds and blood age as if much time went by
		HealItself,
		/// Following a villager (the step's object) to watch it do a skill the creature does not know: each turn while
		/// the villager does it, up to the step's seconds counted in whole seconds, it walks up to the villager, stopping
		/// within the step's movement distance, whenever it is further and still. Then it holds where it is for one to four
		/// seconds, picked at random, and the step is done.
		FollowWatching,
		/// The step that follows the player's hand. From the next turn, for the step's seconds in whole turns, the creature
		/// walks to the hand when the hand is further than its radius and a little more (see
		/// creature_player_attention::FollowHandTurn). The step is done when the creature is near the hand or the time is
		/// up. If the hand is off the map, the creature stops the rest of the agenda.
		FollowHand,
		/// The step that watches the player while the creature waits for orders after a leash order to a place. The
		/// creature turns to the hand or to the camera (see creature_player_attention::WatchPlayerTurn). The step is done
		/// when the creature does not wait for orders.
		WatchPlayer,
		/// After this step, the plan acts on the creature, not on the thing that it chose. The step ends at once.
		ActOnItself,
		/// Fighting another creature (the step's object): as it starts, it asks that creature to fight. If the other
		/// creature does not agree, the rest of the agenda is given up. Else the step lasts until the fight has ended.
		Fight,
		/// Raising or lowering the step's object, a town's totem: from the next turn, each turn the share of the town's
		/// people at worship moves by a step, until the totem is all the way up or down. If the totem has no town that
		/// can worship, the rest of the agenda is given up.
		RaiseTotem,
		LowerTotem,
		/// Making the step's object, a villager, a breeder at once
		MakeDisciple,
		/// Kissing: once its body plays no action, it plays the step's animation once, with whatever its hand holds
		Kiss,
		/// Dancing alone: as it starts, once its body plays no action, the creature starts a dance with moves of one
		/// second each. The step lasts for the step's seconds counted in whole turns. The dance stops when the step
		/// ends, or when the creature does something else.
		PracticeDance,
		/// Making the step's object still. While the object flies, the step waits. Then a living object starts to die at
		/// once, without the work of a death, and the step ends. For an object that is not alive, the step ends at once.
		RenderImmobile,
		/// Asking another creature (the step's object) to be its partner in a joint action, for the step's request.
		/// When the other creature agrees, the two are partners and the step is done at once. When it refuses, the rest
		/// of the agenda is given up.
		RequestPartner,
		/// Telling the partner (the step's object) to do the step's order at once. The partner stops what it does, and
		/// does the order and then waits, watching this creature. The step is done at once.
		Order,
		/// Making the partner (the step's object) stand still in its mind, or letting its mind go free again, at once
		Demobilise,
		Remobilise,
	};
	Kind kind {Kind::Wait};
	float seconds {0.0f};
	/// Up to how many seconds more an inspect step may last, picked as it starts
	float extraSeconds {0.0f};
	/// Watching the camera for a set number of turns, rather than until the camera goes or the step's seconds are up
	std::optional<uint32_t> turns;
	size_t animation {0};
	/// Played with drooping, slowly blinking eyes, as if just woken up. The eyes stay so until it stops what it does.
	bool sleepyEyes {false};
	Movement movement {};
	/// A static step's start, loop and end
	std::array<size_t, 3> sequence {};
	/// A static step's loop lasts until the creature is rested, rather than for the step's seconds
	bool untilRested {false};
	/// A static step's loop lasts until the creature is better (see k_FewestRestTurns). The body heals and rests during
	/// the loop, and its fire stops each turn.
	bool untilBetter {false};
	/// A static step's loop holds the last frame of its start, as lying out cold
	bool holdLoop {false};
	/// A static step plays left to right half the time, chosen as it starts
	bool mirrorAtRandom {false};
	/// Played with the eyes closed
	bool closedEyes {false};
	/// What the step does to the body: when an action is done (eating, as it starts), or a static step's loop ends
	Effect effect {Effect::None};
	/// The object eaten, waited for or doused, by its entity's number
	std::optional<uint32_t> object;
	/// What an object step does
	ObjectOrder order {};
	/// What a cast step casts
	CastOrder cast {};
	/// The face pulled as the step starts
	creature_face::Cue face {creature_face::Cue::None};
	/// What its head looks at while the step lasts, in place of looking about
	std::optional<Gaze> gaze;
	/// How its eyes look from the step's start until it next takes up something else to do: stoned (wide and darting
	/// about) or wide open, as when frightened
	std::optional<Eyes> heldEyes;
	/// It flutters its eyelids, blinking far more often, from the step's start until it next takes up something else
	bool flutterEyelids {false};
	/// What a dance step does
	DanceOrder dance {};
	/// A static step in which the creature faints. As it starts, those near it react as to a death. The step's seconds
	/// count from the turn that it lies still.
	bool faint {false};
	/// The main step of the agenda: the step that does what the action is for. The player sees the action only at or
	/// after this step (see MainStep).
	bool main {false};
	/// What a step that asks for a partner asks the other creature to do
	creature_friendship::RequestKind request {creature_friendship::RequestKind::Play};
	/// The step that an order step tells the partner to do
	std::shared_ptr<const Step> ordered;
	/// The step of the game that this step stands for, as a saved game keeps it in the plan of a creature that a script
	/// controls. Only the steps that such a plan has keep it.
	struct Saved
	{
		/// The number of the game's step
		uint32_t kind {0};
		uint32_t integer {0};
		glm::vec3 point {0.0f};
		float real {0.0f};
	};
	std::optional<Saved> saved;
};

struct IdleMind
{
	Activity activity {Activity::None};
	std::vector<Step> agenda;
	size_t step {0};
	bool stepStarted {false};
	float stepSeconds {0.0f};
	bool sitEnding {false};
	/// Seconds until the step's face is pulled again, and the count that varies the faces it pulls
	float faceSeconds {0.0f};
	/// It stopped what it did: its eyes look as a rested creature's eyes do again (see Commands::restEyes)
	bool eyesToClear {false};
	uint32_t faceVariety {0};
	/// The desire it showed last
	std::optional<creature_desires::Desire> shown;
	/// Told to wake up, or to end whatever static step it is in
	bool wakeWanted {false};
	/// Counts the agendas planned, so that a new one can be told from the last
	uint32_t serial {0};
	/// The agenda was given up before its end, as a step it needed couldn't be done
	bool gaveUp {false};
	/// A script asked if the creature played and so stopped the agenda. The plan stays, with no steps, until the
	/// creature chooses a new plan or a script gives it a new order.
	bool stoppedByScript {false};
	/// The mind of a creature free to think saw the stopped plan in a turn before this one
	bool stoppedSeen {false};
	/// The thing in its hand that a step of this agenda used up before the last step, which the agenda does not lose
	std::optional<uint32_t> usedUpHeld;
	/// A cast step: whether its miracle has been cast, and the turns it is held for yet
	bool castDone {false};
	uint32_t castTurns {0};
	/// An inspect step: the turns it watches for yet
	uint32_t inspectTurns {0};
	/// A step watching a flying thing: the turns it has watched it
	uint32_t watchTurns {0};
	/// A step that follows to watch: whether it has stopped watching and holds, and the turns it holds for yet (it
	/// counts its watching turns with the flying watch's)
	bool followHolding {false};
	uint32_t followHoldTurns {0};
	/// Whether it has set off back towards the villager, so that a walk that fails gives the agenda up
	bool followMoved {false};
	/// For a step that follows the hand: the number of turns that are left
	uint32_t followHandTurns {0};
	/// A step that rests to get better: the turns since it started, the first turn included
	uint32_t restTurns {0};
	/// A dance step: the turns it dances yet, and whether its dance stops next turn
	uint32_t danceTurns {0};
	bool danceEnding {false};
	/// A step that makes a dance: whether it ordered the new dance yet
	bool danceSet {false};
	/// A step that follows the camera: the turns it follows for yet
	uint32_t followCameraTurns {0};
	/// The step that moved on to its next part this turn: it started, its loop ended, or it ended. Each turn of thought
	/// clears it first.
	std::optional<size_t> movedOn;
};

/// How what its hands were told to do is going
enum class HandsState : uint8_t
{
	/// Told nothing, or it was stopped
	Idle,
	Busy,
	Done,
	Failed,
};

/// How going near, getting away from or turning to face an object is going
enum class SubMove : uint8_t
{
	Running,
	Done,
	Failed,
};

/// The places that the creature uses when it hangs around at home
struct HangAroundPlaces
{
	/// Its home, on the ground
	glm::vec3 home {0.0f};
	/// The heart of its player's temple, none when its player has no temple
	std::optional<glm::vec3> temple;
	/// The radius of the area that holds it, 0 when no area holds it
	float confinementRadius {0.0f};
	/// Its height
	float height {0.0f};
};
/// What a step needs in the hands of the creature when the step starts
enum class HandsNeed : uint8_t
{
	/// Nothing: the step starts with the hands as they are
	Any,
	/// Empty hands: the creature lets go of the thing that it holds first
	Free,
	/// Empty hands, or the thing that the step picks up. When the creature holds that thing already, it does not do the
	/// step. When it holds a different thing, it puts that thing down first.
	FreeOrTarget,
	/// A thing in the hand. When the hand is empty, the creature stops the rest of the agenda.
	Held,
};
/// What a step needs in the hands of the creature:
/// - Actions, static actions, casts, the water on a fire, swaps of minds, messages to the player, fights, catches and
///   the steps that take food or a thing from the hand need empty hands.
/// - A pick-up needs empty hands or the thing that it picks up.
/// - The steps that eat, keep, toss away or throw the held thing need a thing in the hand. So do the step that makes a
///   breeder and the step that uses up the held thing.
[[nodiscard]] HandsNeed HandsNeededFor(const Step& step);

/// How the creature lets go of the thing that it holds before a step that needs empty hands
enum class LetGo : uint8_t
{
	/// It puts the thing down or it tosses the thing away, as PutsDownGently tells
	AsItChooses,
	/// It puts the thing down
	PutDown,
};
/// Whether a creature puts down the thing that it holds before a step, and does not toss the thing away. It puts down a
/// creature. It puts down a different thing when the first desire with a plan for the thing is Impress, Compassion or
/// Curiosity. With no such plan, it tosses the thing away.
[[nodiscard]] bool PutsDownGently(bool heldIsCreature, std::optional<creature_desires::Desire> use);

/// What the mind knows this turn
struct Senses
{
	float seconds {0.0f};
	/// Whether the body plays an action or is on the move, and whether it is in the loop of an action, such as sitting
	bool bodyBusy {false};
	bool bodyLooping {false};
	/// Whether the body dances and does not play the finish of the dance yet
	bool bodyDancing {false};
	/// Whether it is on its way somewhere or turning, and where it stands
	bool moving {false};
	/// How going near, getting away from or turning to face an object, or drawing a gesture, is going, by the
	/// creature's movement
	SubMove subMove {SubMove::Running};
	glm::vec2 position {0.0f};
	/// What it shows when a step communicates its state
	creature_desires::Communication communication {};
	/// Whether it has slept enough to wake
	bool rested {false};
	/// Whether its life is full and it is not tired at all
	bool better {false};
	HandsState hands {HandsState::Idle};
	/// The thing that the creature holds, by the number of its entity
	std::optional<uint32_t> held;
	/// Whether the object a step waits for is on the map, none when the object is gone
	std::optional<bool> objectInMap;
	/// Whether the object a step watches fly is still really in the air
	bool objectInTheAir {false};
	/// Whether the living object of a step that makes it still is in its state of flying
	bool objectFlying {false};
	/// What picks the faces it pulls
	creature_face::Feelings feelings {};
	/// Its player's camera as the creature sees it, none for a creature without a player: how far it is, how far along
	/// the ground, where it is on the ground, and how far round from the way the creature faces it is (radians, either
	/// way)
	struct CameraView
	{
		float distance {0.0f};
		float groundDistance {0.0f};
		glm::vec2 point {0.0f};
		float turn {0.0f};
		/// For the camera only: the game turns that the player's hand has stayed in one place
		uint32_t handStillTurns {0};
	};
	std::optional<CameraView> camera;
	/// The player's hand nearest it, seen the same way, while a step watches it; none for a creature without a player
	std::optional<CameraView> hand;
	/// A swap of minds: whether no spell is on it or on the other creature any more, and whether its body has taken on
	/// the shape the swap gives it
	struct SwapView
	{
		bool spellsGone {false};
		bool settled {false};
	};
	SwapView swap {};
	/// Whether something plays on top of the body, as talking or nodding
	bool overlayPlaying {false};
	/// Whether its body plays an action, whether it moves or not
	bool bodyPlaying {false};
	/// Its dance with villagers, while a dance step is next or under way
	struct DanceView
	{
		/// Whether its dance is still there, and whether a villager of the dance moved in the last turn
		bool exists {false};
		bool dancersMoving {false};
		/// Whether the creature dances in a group of the dance, and that group's move: its number in the dance files
		/// and its two arguments
		bool inGroup {false};
		uint32_t action {0};
		uint32_t first {0};
		float second {0.0f};
		/// The way the dance faces (radians), the middle of the creature's group, and its own place in the group
		float danceAngle {0.0f};
		/// How many times their keyed speed the groups of the dance move at
		float danceRate {1.0f};
		glm::vec2 groupCentre {0.0f};
		glm::vec2 place {0.0f};
		/// How far it is from its place, in metres, and the way it faces (radians, as its movement measures it)
		float placeDistance {0.0f};
		float heading {0.0f};
	};
	std::optional<DanceView> dance;
	/// The villager a step follows to watch, while that step is next or under way: whether it still does a skill the
	/// creature does not know, and how far it is along the ground, none once it has gone
	struct FollowedView
	{
		bool interesting {false};
		float distance {0.0f};
	};
	std::optional<FollowedView> followed;
	/// The point of focus of its player's camera on the land, while a step follows the camera; none without a player
	std::optional<glm::vec3> cameraFocus;
	/// Whether that point is less than a sixth of a turn from the way its body faces, or in its own map cell
	bool cameraFocusAhead {false};
	/// The player's hand while a step follows the hand, and the player while a step watches the player
	std::optional<creature_player_attention::FollowHandView> followHand;
	std::optional<creature_player_attention::WatchPlayerView> watchPlayer;
	/// Whether it is in a fight now
	bool fighting {false};
	/// The share of the town's people at worship, while a step moves the town's totem; none when the totem has no town
	/// that can worship
	std::optional<float> totemShare;
};

/// Watching the camera ends once it is this far away, or this near along the ground; it turns to the camera when it is
/// more than this far round from it
constexpr float k_WatchCameraFurthest = 100.0f;
constexpr float k_WatchCameraOverhead = 0.1f;
constexpr float k_WatchCameraTurn = 1.5707963f;

/// Whether a creature that watches the camera or the hand starts to turn to it this turn. It turns when the watched
/// point is more than a quarter turn round from it and not right overhead. A body that does an action or moves does not
/// start a new turn: it completes the turn that it does now.
[[nodiscard]] constexpr bool StartsWatchTurn(const Senses::CameraView& view, bool bodyBusy)
{
	return !bodyBusy && view.groundDistance >= k_WatchCameraOverhead &&
	       (view.turn > k_WatchCameraTurn || view.turn < -k_WatchCameraTurn);
}

/// What the mind tells the body
struct Commands
{
	std::optional<size_t> playOnce;
	/// The action played once, or the start, loop and end, plays left to right
	bool mirrored {false};
	/// A start, loop and end to play, the loop held on its last frame or not, and the end of the loop playing now
	std::optional<std::array<size_t, 3>> startSequence;
	bool holdLoop {false};
	bool endSit {false};
	/// A dance to start, with the milliseconds of each move; the dance to stop; only a start, loop and end to stop
	std::optional<float> startDance;
	bool endDance {false};
	bool endStaticLoop {false};
	/// A face to pull, or to relax the face pulled
	std::optional<creature_face::Request> face;
	bool relaxFace {false};
	/// It stopped what it did: its eyes go calm, blink as usual and open by how rested it is (see
	/// creature_eyes::RestedOpenness). This comes before the eyes of this turn.
	bool restEyes {false};
	Eyes eyes {Eyes::Unchanged};
	/// Whether the head turns to whatever is interesting this turn
	bool lookAbout {false};
	/// Where to go, or to stop going
	std::optional<Movement> move;
	bool stopMoving {false};
	/// What happens to the body this turn, and the object it eats
	Effect effect {Effect::None};
	std::optional<uint32_t> effectObject;
	/// The step that finished this turn is the last step of the agenda
	bool lastStep {false};
	/// The step that finished this turn lets the end of the plan decrease the desire of the plan
	bool seesToDesire {false};
	/// What to do with a thing
	std::optional<ObjectOrder> object;
	/// The creature lets go of the thing that it holds, because the next step needs empty hands
	std::optional<LetGo> letGo;
	/// A gesture to draw, by its number. A dance gives it each turn of its move, and the drawing continues only on these
	/// turns.
	std::optional<uint32_t> gesture;
	bool gestureInDance {false};
	/// The arm stops pointing, at a move of a dance that is not a gesture
	bool endPoint {false};
	/// An animation to play on top of the body
	std::optional<size_t> playOverlay;
	/// A miracle to cast, and whether to let go of the one it holds
	std::optional<CastOrder> cast;
	bool releaseCast {false};
	/// An object to douse with water, by its entity's number
	std::optional<uint32_t> douse;
	/// The creature's own fire stops
	bool coolDown {false};
	/// The creature heals itself (see Step::Kind::HealItself)
	bool healItself {false};
	/// A rock to break in two, by its entity's number
	std::optional<uint32_t> shatter;
	/// A fight to start with another creature, by its entity's number
	std::optional<uint32_t> startFight;
	/// Sitting down this turn, it rests a little
	bool restSitting {false};
	/// How often its eyes now blink, when that changes
	std::optional<int32_t> blinkIntervalMs;
	/// A building to mend, and a thing to do away with, by their entities' numbers
	std::optional<uint32_t> repair;
	std::optional<uint32_t> useUp;
	/// A living thing that starts to die at once, without the work of a death, by its entity's number
	std::optional<uint32_t> renderImmobile;
	/// What its hand holds goes from the world this turn
	bool useUpHeld {false};
	/// The plan acts on the creature from this turn
	bool actOnItself {false};
	/// What the head looks at this turn, for a step that gives it something to look at
	std::optional<Gaze> gaze;
	/// A swap of minds: bring every spell on it to its end, start the beam to the other creature, take on a turn more of
	/// the shape the swap gives it
	bool finishSpells {false};
	bool startSwapBeam {false};
	bool easeSwap {false};
	/// The creature communicated its state this turn, and what it showed
	std::optional<creature_desires::Communication> communicated;
	/// A stage of a dance with villagers to carry out this turn
	std::optional<DanceOrder> dance;
	/// A thing to heal at once, by its entity's number
	std::optional<uint32_t> heal;
	/// The share of the town's people at worship that a totem (by its entity's number) is set to this turn
	struct TotemShare
	{
		uint32_t totem {0};
		float share {0.0f};
	};
	std::optional<TotemShare> totem;
	/// A villager to make a breeder at once, by its entity's number
	std::optional<uint32_t> makeDisciple;
	/// The creature faints this turn, so those near it react as to a death
	bool fainted {false};
	/// A creature to ask to be its partner, and what for
	struct PartnerRequest
	{
		uint32_t creature {0};
		creature_friendship::RequestKind kind {creature_friendship::RequestKind::Play};
	};
	std::optional<PartnerRequest> requestPartner;
	/// An order to give the partner: the partner, by its entity's number, and the step it is to do
	struct PartnerOrder
	{
		uint32_t partner {0};
		std::shared_ptr<const Step> step;
	};
	std::optional<PartnerOrder> order;
	/// The partner to make stand still in its mind (true) or to let go free again (false)
	std::optional<std::pair<uint32_t, bool>> demobilise;
};

/// random(n) is a whole number from 0 to n - 1
using Random = std::function<uint32_t(uint32_t)>;

/// Fluttering its eyelids, the creature blinks every 0.625 to 1.875 seconds
constexpr int32_t k_FlutterBlinkIntervalMs = 1250;

/// The limit of a watch of a flying thing: two seconds, and a draw of up to two more each turn
constexpr float k_WatchFlyingSeconds = 2.0f;
constexpr float k_WatchFlyingExtraSeconds = 2.0f;

/// Being idle: wait a second or two looking about, then a tired yawn with sleepy eyes, twice
[[nodiscard]] std::vector<Step> BeIdle(const Random& random);
/// Sitting for ten to fourteen seconds
[[nodiscard]] Step SitDown(const Random& random);
/// Hanging around at home. The creature walks home and waits there while it looks about. Then it does one of these:
/// - With no temple, or one time in ten, it yawns with sleepy eyes.
/// - Seven times in ten, it walks out from home, away from the temple. It faces away from the temple and sits or
///   points. Then it turns to the temple and sits.
/// - Two times in ten, it turns to the temple and sits.
[[nodiscard]] std::vector<Step> HangAround(const HangAroundPlaces& places, float turnSeconds, const Random& random);
/// Sleeping on the spot: half the time a tired yawn first, then sleeping with its eyes closed until rested, then a dazed
/// look about with sleepy eyes
[[nodiscard]] std::vector<Step> Sleep(const Random& random);
/// Pick up the food, look at it and eat it. It eats what it holds when it can eat that, with no pick-up.
[[nodiscard]] std::vector<Step> Eat(uint32_t food, bool holdingEatable);
/// Eat a thing at once: it shows that it is hungry half the time, picks the thing up and eats it. It eats what it holds
/// when it can eat that, with no pick-up.
[[nodiscard]] std::vector<Step> EatAlive(uint32_t food, bool holdingEatable, const Random& random);
/// Picking something up, stroking, shaking, smelling or examining it, then mostly putting it down and sometimes tossing it
/// away. When its hand holds something, it does not pick the thing up: it examines what it holds.
[[nodiscard]] std::vector<Step> ExamineByPickingUp(uint32_t object, bool handFull, const Random& random);
/// Picking something up and throwing it about, 30 to 50 units away in a random direction
[[nodiscard]] std::vector<Step> ThrowAbout(uint32_t object, const Random& random);
/// A sixth of the time showing its anger, then picking something up and hurling it at a point
[[nodiscard]] std::vector<Step> Hurl(uint32_t object, glm::vec2 target, const Random& random);
/// Putting down what it holds
[[nodiscard]] std::vector<Step> PutDownHeld();
/// The creature walks to the shoal of a fish farm until it is in a distance of the shoal. Then it brings food out of
/// the sea and eats the food. The step that brings the food needs empty hands.
[[nodiscard]] std::vector<Step> FishAndEat(glm::vec2 shoal, float arriveWithin);
/// A walk to the shoal of a fish farm, to bring food out of the sea: the creature stops in a distance of the shoal
struct FishingTrip
{
	glm::vec2 shoal {0.0f};
	float arriveWithin {0.0f};
};
/// Fishing when it has no food in its hand, then going to where it can throw into a storage pit, turning to it and
/// throwing the food in; the action counts as done as it turns
[[nodiscard]] std::vector<Step> GiveFishToStore(const std::optional<FishingTrip>& trip, uint32_t store, float height);
/// Fishing when it has no food in its hand, then walking to a place until within its height of it and putting the food
/// down there
[[nodiscard]] std::vector<Step> TakeFishTo(const std::optional<FishingTrip>& trip, glm::vec2 place, float height);
/// The thing a creature poos by, by its entity's number. The creature can poo by itself
struct PooBy
{
	uint32_t object {0};
	bool itself {false};
};
/// A poo, a third of the time showing it needs one first. With a thing to poo by, it goes up to the thing and looks at it,
/// then turns its back to it. It does not look at itself. Only the squat pulls a face.
[[nodiscard]] std::vector<Step> Poo(const Random& random, std::optional<PooBy> by);
/// A fart: the creature does the same steps as for a poo, but it makes a cloud, not a lump
[[nodiscard]] std::vector<Step> Fart(const Random& random, std::optional<PooBy> by);
/// Being sick on the spot
[[nodiscard]] std::vector<Step> Puke();
/// Fainting where it stands, lying out cold four seconds for each unit of its size and eight more, then getting up. A
/// creature that faints in the world is carried home by the fights; this agenda is for a world without them.
[[nodiscard]] std::vector<Step> Faint(float size);
/// Eating what it holds
[[nodiscard]] std::vector<Step> EatHeld();
/// Taking a thing the player's hand gives it: turning to the player's camera, then reaching out to take it from the
/// hand, looking at it and curious all along
[[nodiscard]] std::vector<Step> TakeFromHand(uint32_t object, glm::vec2 camera, bool leftHand);
/// What it does with a thing it was given and holds: looks it over, curious; eats it; or throws it at the player's
/// camera, looking at the camera
[[nodiscard]] std::vector<Step> ExamineGiven(uint32_t object);
[[nodiscard]] std::vector<Step> EatGiven();
[[nodiscard]] std::vector<Step> ThrowGiven(glm::vec3 camera);
/// Playing an action once on the spot
[[nodiscard]] std::vector<Step> Emote(size_t animation);
/// Communicating its state to the player: turning to face the camera, then showing what it feels. Its head looks at
/// the camera all the time.
[[nodiscard]] std::vector<Step> Communicate(glm::vec2 camera);
/// Turning to face a point, then playing an action
[[nodiscard]] std::vector<Step> FaceAndEmote(glm::vec2 point, size_t animation);
/// Walking up to something, then playing an action at it
[[nodiscard]] std::vector<Step> ApproachAndEmote(uint32_t object, size_t animation);
/// Turning to face something and watching it for some seconds
[[nodiscard]] std::vector<Step> LookAt(glm::vec2 point, float seconds);
/// Running away from a point
[[nodiscard]] std::vector<Step> RunFrom(glm::vec2 point);
/// The creature walks up to a thing and stomps on it, with an angry face. Its head looks at the thing as it walks.
[[nodiscard]] std::vector<Step> DestroyThing(uint32_t object);
/// Waiting a while, looking about
[[nodiscard]] std::vector<Step> LookAbout(float seconds);
/// A creature that rests to get better gets up only when its life is full, it is not tired at all, and the step has
/// lasted more than this many turns
constexpr uint32_t k_FewestRestTurns = 30;
/// Whether the creature is asleep, or out cold, in its current step
[[nodiscard]] bool IsAsleep(const IdleMind& mind);
/// Whether the creature rests to get better in its current step, until it starts to get up
[[nodiscard]] bool IsResting(const IdleMind& mind);
[[nodiscard]] bool IsUnconscious(const IdleMind& mind);

/// A new agenda in place of the current one
void Plan(IdleMind& mind, Activity activity, std::vector<Step> agenda);
/// Pulls a face for a reason, if the reason calls for one, and moves the variety of faces on
[[nodiscard]] std::optional<creature_face::Request> PullFace(IdleMind& mind, creature_face::Cue cue, const Senses& senses,
                                                             const Random& random);
/// The world carried out a dance stage that the mind ordered: the step is done, or the rest of the agenda is no use
void DanceOrderDone(IdleMind& mind, bool done);
/// What the head looks at while the step lasts. A step that picks a thing up, or goes near a thing, looks at that
/// thing as the creature walks, unless the step gives a look of its own. Without a look, the head looks about.
[[nodiscard]] std::optional<Gaze> LookOf(const Step& step);
/// One game turn of the mind
[[nodiscard]] Commands Think(IdleMind& mind, const Senses& senses, const Random& random);
/// The main step of an agenda: the last step marked as main. A step that casts a miracle is always a main step. When no
/// step is a main step, the last step is the main step. An empty agenda gives 0.
[[nodiscard]] size_t MainStep(std::span<const Step> agenda);
/// Whether a step of an agenda is at or after the main step. When the creature shows the player what it wants, every
/// step counts. An empty agenda has no such step.
[[nodiscard]] bool AtOrPastMainStep(std::span<const Step> agenda, size_t step, bool showingDesire);
/// Whether a creature goes on watching a flying thing: while the thing is still flying and the whole seconds of the
/// turns it has watched come under the limit
[[nodiscard]] bool KeepsWatchingFlying(uint32_t turns, uint32_t turnsPerSecond, float limit, bool flying);
/// Whether a creature watching the hand has watched long enough: its whole seconds are more than the whole seconds of
/// the step's turns, but never more than five
[[nodiscard]] bool WatchedHandEnough(uint32_t turns, uint32_t limitTurns, uint32_t turnsPerSecond);
/// The longest a creature watches the hand, in whole seconds
constexpr float k_LongestHandWatchSeconds = 5.0f;
} // namespace openblack::creature_mind
