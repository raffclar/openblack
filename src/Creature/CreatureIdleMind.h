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
#include <optional>
#include <string_view>
#include <vector>

#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

#include "Creature/CreatureDesires.h"
#include "Creature/CreatureFace.h"

/// What a creature does with itself while it has nothing better to do, decided once a game turn. It works through an
/// agenda of steps: waiting while it looks about, playing an action, sitting for a while, going somewhere. When the
/// agenda runs out it picks the next: showing the player its strongest desire, at most once a minute; else sitting down
/// for a while; else hanging around, which is walking somewhere nearby and sitting there; else being idle, which is
/// waiting a second or two then a tired yawn, twice. Each step pulls a face for what it does or feels as it starts, and
/// again every few seconds while it lasts: idling, a random face for three seconds.
///
/// Only the planner chooses the actions for the needs and the other desires: eat, drink, sleep, poo, examine, play and
/// hurl. If the creature is exhausted, starved or has no life, it faints where it is and gets up a short time later.
///
/// Only this choice of the next agenda is the idle policy; a planner that weighs desires against the actions that
/// satisfy them can choose agendas in its place, and the steps play out the same way.
namespace openblack::creature_mind
{
/// The seconds between showing desires
constexpr float k_ShowDesireSeconds = 60.0f;
/// Each step pulls a face as it starts, and again this often for as long as it lasts: every four seconds and two turns
constexpr float k_FaceRepeatSeconds = 4.2f;
/// After each face it pulls, the creature's variety of faces moves on by up to this much less one
constexpr uint32_t k_FaceVarietyStep = 7;
/// Within this many seconds of being stroked or slapped, the creature shows its pleasure or sorrow at it
constexpr float k_FeedbackSeconds = 10.0f;
/// Idle waits are one second and up to one more
constexpr float k_IdleWaitSeconds = 1.0f;
/// Being idle is waiting then yawning, this many times
constexpr int k_IdleRepeats = 2;
/// Sitting lasts ten seconds and up to four more
constexpr uint32_t k_SitSeconds = 10;
constexpr uint32_t k_SitExtraSeconds = 5;
/// A desire weaker than this isn't worth showing. The game shows desires when its planner picks the desire to show how
/// it is over all others; this stands in for that.
constexpr float k_MinDesireShown = 0.2f;
/// The idle activities are chosen at random among this many lots: the first is sitting down, the last hanging
/// around, the rest being idle. The game chooses between its idle activities by the strength of the desire to rest and
/// what the creature has learnt; this stands in for that.
constexpr uint32_t k_ActivityLots = 4;
/// Hanging around, the creature walks this far away and up to this much further, in a random direction
constexpr float k_HangAroundDistance = 20.0f;
constexpr uint32_t k_HangAroundExtra = 21;
/// Off to sleep, the creature yawns first one time in this many; off for a poo, it shows it needs one first one time in
/// this many
constexpr uint32_t k_YawnBeforeSleepLots = 2;
constexpr uint32_t k_ShowPooLots = 3;
/// A poo and being sick each last this many seconds
constexpr float k_PooSeconds = 4.0f;
constexpr float k_PukeSeconds = 4.0f;
/// Fainted, it lies out cold this long before it gets up. The game's time is not known; this stands in for it.
constexpr float k_FaintSeconds = 10.0f;
/// It drinks from anywhere this close to the water's edge
constexpr float k_DrinkReach = 5.0f;
/// Looking something over, it puts it down gently most times, out of this many, and tosses it away the rest
constexpr uint32_t k_PutDownLots = 10;
constexpr uint32_t k_TossLots = 2;
/// Throwing something about, it throws it this far away and up to this much further, in a random direction
constexpr float k_ThrowAroundDistance = 30.0f;
constexpr uint32_t k_ThrowAroundExtra = 21;
/// Turning to face a store before throwing into it, it holds still this long
constexpr float k_FaceStoreSeconds = 0.1f;
/// Going up to something to act at it, it stops this close; following something, it keeps this close
constexpr float k_ApproachDistance = 12.0f;
constexpr float k_FollowDistance = 25.0f;
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
		/// Taking food from a pile: a handful sized to the creature, no more than the pile holds, appears at its feet and
		/// it picks it up
		TakeFromPile,
		/// Reaching out to take what the player's hand gives it, with its left hand when mirrored
		TakeFromHand,
		/// Kicking the object, which flies off ahead of it
		Kick,
	};
	Kind kind {Kind::PickUp};
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
	/// Arriving anywhere from min to max from the point, or keeping within max of what it follows
	float minDistance {0.0f};
	float maxDistance {0.0f};
	/// Going to a point where it can't stand gives up the rest of the agenda, rather than counting as arrived
	bool giveUpIfUnreachable {false};
};

/// A miracle cast at a thing, by its magic type's number
struct CastOrder
{
	uint32_t magicType {0};
	/// What it is cast at, by its entity's number
	std::optional<uint32_t> object;
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
};

/// How the eyes should look
enum class Eyes : uint8_t
{
	Unchanged,
	/// Drooping and slowly blinking, as if just woken up
	Sleepy,
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
	std::optional<glm::vec3> point;
	bool camera {false};
};

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
		/// Dousing the step's object with water at once, as a creature does after casting the water miracle at a fire
		Douse,
		/// Breaking the step's object, a rock, in two at once; when it isn't a rock, the rest of the agenda is given up
		Shatter,
		/// Mending the step's object, a building, by a fifth of its life at once
		Repair,
		/// Doing away with the step's object at once, as with the tree used up mending a building
		UseUp,
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
	};
	Kind kind {Kind::Wait};
	float seconds {0.0f};
	/// Up to how many seconds more an inspect step may last, picked as it starts
	float extraSeconds {0.0f};
	/// Watching the camera for a set number of turns, rather than until the camera goes or the step's seconds are up
	std::optional<uint32_t> turns;
	size_t animation {0};
	/// Played with drooping, slowly blinking eyes, as if just woken up
	bool sleepyEyes {false};
	Movement movement {};
	/// A static step's start, loop and end
	std::array<size_t, 3> sequence {};
	/// A static step's loop lasts until the creature is rested, rather than for the step's seconds
	bool untilRested {false};
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
	/// Its eyes look as a step held them, and look as usual again once it takes up something else to do
	bool eyesHeld {false};
	bool eyesToClear {false};
	uint32_t faceVariety {0};
	/// Seconds until it may show a desire again
	float showDesireSeconds {0.0f};
	/// The desire it showed last
	std::optional<creature_desires::Desire> shown;
	/// Told to wake up, or to end whatever static step it is in
	bool wakeWanted {false};
	/// Counts the agendas planned, so that a new one can be told from the last
	uint32_t serial {0};
	/// The agenda was given up before its end, as a step it needed couldn't be done
	bool gaveUp {false};
	/// A cast step: whether its miracle has been cast, and the turns it is held for yet
	bool castDone {false};
	uint32_t castTurns {0};
	/// An inspect step: the turns it watches for yet
	uint32_t inspectTurns {0};
	/// A step watching a flying thing: the turns it has watched it
	uint32_t watchTurns {0};
};

/// The nearest water, and where the creature can stand at its edge to drink
struct WaterSpot
{
	glm::vec2 shore;
	glm::vec2 water;
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

/// What the mind knows this turn
struct Senses
{
	float seconds {0.0f};
	/// Whether the body plays an action or is on the move, and whether it is in the loop of an action, such as sitting
	bool bodyBusy {false};
	bool bodyLooping {false};
	/// Whether it is on its way somewhere or turning, and where it stands
	bool moving {false};
	/// How going near, getting away from or turning to face an object, or drawing a gesture, is going, by the
	/// creature's movement
	SubMove subMove {SubMove::Running};
	glm::vec2 position {0.0f};
	std::optional<creature_desires::Desire> strongest;
	/// Seconds since the player last stroked or slapped it, and which
	std::optional<float> feedbackSeconds;
	bool feedbackWasStroke {false};
	/// Whether it has slept enough to wake
	bool rested {false};
	HandsState hands {HandsState::Idle};
	/// Whether the object a step waits for is on the map, none when the object is gone
	std::optional<bool> objectInMap;
	/// Whether the object a step watches fly is still really in the air
	bool objectInTheAir {false};
	/// What picks the faces it pulls
	creature_face::Feelings feelings {};
	/// Whether it chooses what to do next by itself when its agenda runs out; under a script's control it only does
	/// what it is given
	bool choosesNext {true};
	/// Its player's camera as the creature sees it, none for a creature without a player: how far it is, how far along
	/// the ground, where it is on the ground, and how far round from the way the creature faces it is (radians, either
	/// way)
	struct CameraView
	{
		float distance {0.0f};
		float groundDistance {0.0f};
		glm::vec2 point {0.0f};
		float turn {0.0f};
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
};

/// Watching the camera ends once it is this far away, or this near along the ground; it turns to the camera when it is
/// more than this far round from it
constexpr float k_WatchCameraFurthest = 100.0f;
constexpr float k_WatchCameraOverhead = 0.1f;
constexpr float k_WatchCameraTurn = 1.5707963f;

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
	/// A face to pull, or to relax the face pulled
	std::optional<creature_face::Request> face;
	bool relaxFace {false};
	Eyes eyes {Eyes::Unchanged};
	/// Whether the head turns to whatever is interesting this turn
	bool lookAbout {false};
	/// Where to go, or to stop going
	std::optional<Movement> move;
	bool stopMoving {false};
	/// What happens to the body this turn, and the object it eats
	Effect effect {Effect::None};
	std::optional<uint32_t> effectObject;
	/// What to do with a thing
	std::optional<ObjectOrder> object;
	/// A gesture to draw, by its number
	std::optional<uint32_t> gesture;
	/// An animation to play on top of the body
	std::optional<size_t> playOverlay;
	/// A miracle to cast, and whether to let go of the one it holds
	std::optional<CastOrder> cast;
	bool releaseCast {false};
	/// An object to douse with water, by its entity's number
	std::optional<uint32_t> douse;
	/// A rock to break in two, by its entity's number
	std::optional<uint32_t> shatter;
	/// Sitting down this turn, it rests a little
	bool restSitting {false};
	/// How often its eyes now blink, when that changes
	std::optional<int32_t> blinkIntervalMs;
	/// A building to mend, and a thing to do away with, by their entities' numbers
	std::optional<uint32_t> repair;
	std::optional<uint32_t> useUp;
	/// What the head looks at this turn, for a step that gives it something to look at
	std::optional<Gaze> gaze;
	/// A swap of minds: bring every spell on it to its end, start the beam to the other creature, take on a turn more of
	/// the shape the swap gives it
	bool finishSpells {false};
	bool startSwapBeam {false};
	bool easeSwap {false};
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
/// Hanging around: walking 20 to 40 units away in a random direction, then sitting there
[[nodiscard]] std::vector<Step> HangAround(const Random& random);
/// Sleeping on the spot: half the time a tired yawn first, then sleeping with its eyes closed until rested, then a dazed
/// look about with sleepy eyes
[[nodiscard]] std::vector<Step> Sleep(const Random& random);
/// Picking food up, examining it and eating it
[[nodiscard]] std::vector<Step> Eat(uint32_t food);
/// Picking something up, stroking, shaking, smelling or examining it, then mostly putting it down and sometimes tossing it
/// away
[[nodiscard]] std::vector<Step> ExamineByPickingUp(uint32_t object, const Random& random);
/// Picking something up and throwing it about, 30 to 50 units away in a random direction
[[nodiscard]] std::vector<Step> ThrowAbout(uint32_t object, const Random& random);
/// A sixth of the time showing its anger, then picking something up and hurling it at a point
[[nodiscard]] std::vector<Step> Hurl(uint32_t object, glm::vec2 target, const Random& random);
/// Putting down what it holds
[[nodiscard]] std::vector<Step> PutDownHeld();
/// Walking to the water's edge, turning to the water and drinking
[[nodiscard]] std::vector<Step> Drink(glm::vec2 shore, glm::vec2 water);
/// Walking to a fish farm's shoal until within a distance of it, putting down what it holds first when it holds
/// anything, bringing food out of the sea, and eating it
[[nodiscard]] std::vector<Step> FishAndEat(glm::vec2 shoal, float arriveWithin, bool putDownFirst);
/// Going to a fish farm's shoal to bring food out of the sea: until within a distance of it, putting down what it holds
/// first when it holds anything
struct FishingTrip
{
	glm::vec2 shoal {0.0f};
	float arriveWithin {0.0f};
	bool putDownFirst {false};
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
/// then turns its back to it. It does not look at itself
[[nodiscard]] std::vector<Step> Poo(const Random& random, std::optional<PooBy> by);
/// A fart: the creature does the same steps as for a poo, but it makes a cloud, not a lump
[[nodiscard]] std::vector<Step> Fart(const Random& random, std::optional<PooBy> by);
/// Being sick on the spot
[[nodiscard]] std::vector<Step> Puke();
/// Fainting where it stands, lying out cold a while, then getting up
[[nodiscard]] std::vector<Step> Faint();
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
/// Turning to face a point, then playing an action
[[nodiscard]] std::vector<Step> FaceAndEmote(glm::vec2 point, size_t animation);
/// Walking up to something, then playing an action at it
[[nodiscard]] std::vector<Step> ApproachAndEmote(uint32_t object, size_t animation);
/// Turning to face something and watching it for some seconds
[[nodiscard]] std::vector<Step> LookAt(glm::vec2 point, float seconds);
/// Following something about for some seconds
[[nodiscard]] std::vector<Step> FollowFor(uint32_t object, float seconds);
/// Running away from a point
[[nodiscard]] std::vector<Step> RunFrom(glm::vec2 point);
/// Walking up to something and destroying it
[[nodiscard]] std::vector<Step> DestroyThing(uint32_t object);
/// Waiting a while, looking about
[[nodiscard]] std::vector<Step> LookAbout(float seconds);
/// Whether the creature is asleep, or out cold, in its current step
[[nodiscard]] bool IsAsleep(const IdleMind& mind);
[[nodiscard]] bool IsUnconscious(const IdleMind& mind);

/// A new agenda in place of the current one
void Plan(IdleMind& mind, Activity activity, std::vector<Step> agenda);
/// The next agenda when the current runs out
void ChooseNext(IdleMind& mind, const Senses& senses, const Random& random);
/// Pulls a face for a reason, if the reason calls for one, and moves the variety of faces on
[[nodiscard]] std::optional<creature_face::Request> PullFace(IdleMind& mind, creature_face::Cue cue, const Senses& senses,
                                                             const Random& random);
/// One game turn of the mind
[[nodiscard]] Commands Think(IdleMind& mind, const Senses& senses, const Random& random);
/// Whether a creature goes on watching a flying thing: while the thing is still flying and the whole seconds of the
/// turns it has watched come under the limit
[[nodiscard]] bool KeepsWatchingFlying(uint32_t turns, uint32_t turnsPerSecond, float limit, bool flying);
/// Whether a creature watching the hand has watched long enough: its whole seconds are more than the whole seconds of
/// the step's turns, but never more than five
[[nodiscard]] bool WatchedHandEnough(uint32_t turns, uint32_t limitTurns, uint32_t turnsPerSecond);
/// The longest a creature watches the hand, in whole seconds
constexpr float k_LongestHandWatchSeconds = 5.0f;
} // namespace openblack::creature_mind
