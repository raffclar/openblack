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
#include <string_view>
#include <vector>

#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

#include "Creature/CreatureDesires.h"
#include "Creature/CreatureIdleMind.h"
#include "Enums.h"

/// Directing a creature with the leash. With the leash held in the hand and not tied, a tap of the Action button gives
/// the creature an order: on the land it goes there (or drinks, looks at its reflection, works a field, or goes home to
/// sleep), on a thing it acts on that thing. A double tap ties the leash to the thing instead, and a double tap on what
/// it is tied to, or on the creature, unties it. Every order the creature takes is answered with a nod, and marked: a
/// ring of sparkles over the place everyone sees, and for its player a throbbing yellow ring holding the species'
/// footprint.
namespace openblack::creature_leash_orders
{

/// What a tap of the Action button does with the leash
enum class Tap : uint8_t
{
	/// The leash has nothing to do with it: the tap does what it would without one
	Normal,
	/// An order to act on the thing tapped, or to go to the place tapped
	OrderOnThing,
	OrderOnLand,
	/// The leash is tied to the thing tapped, or untied back to the hand
	Tie,
	Untie,
	/// Nothing at all happens
	Nothing,
};

/// The player's leash as a tap finds it
struct Leash
{
	/// The player's creature wears the leash, held by this player
	bool worn {false};
	/// What it is tied to, by entity number, if it is
	std::optional<uint32_t> tiedTo;
	/// The creature, by entity number
	uint32_t creature {0};
	/// The creature is in the middle of a game with another creature, which no order breaks into
	bool playingGame {false};
};

/// What was tapped
struct Tapped
{
	/// The thing tapped, by entity number, or none for the land
	std::optional<uint32_t> object;
	/// The interface may act on it, and the leash may be tied to it: leash posts and worship icons are neither
	bool leashTarget {true};
	/// A miracle bubble that can be taken only once
	bool miracleBubble {false};
};

/// What a single tap does. Held untied, the leash gives an order on a thing or on the land. The creature itself isn't
/// ordered: its own taps put the leash on and take hold of it.
[[nodiscard]] Tap OnTap(const Leash& leash, const Tapped& tapped);
/// What the second tap of a double tap does. Tied, a double tap on what the leash is tied to, or on the creature,
/// unties it, and on anything else does nothing. Untied, it ties the leash to the thing tapped; a miracle bubble that
/// can be taken only once is taken as usual instead.
[[nodiscard]] Tap OnDoubleTap(const Leash& leash, const Tapped& tapped);

/// The Windows double click the game takes: a second press within half a second, within a small box of the first
constexpr uint32_t k_DoubleTapMilliseconds = 500;
constexpr float k_DoubleTapSlop = 2.0f;
/// Tells double taps from the presses of the Action button
class DoubleTaps
{
public:
	/// Whether this press, when and where on the screen, is the second of a double tap
	[[nodiscard]] bool OnPress(uint32_t milliseconds, glm::vec2 screen);

private:
	struct Press
	{
		uint32_t milliseconds;
		glm::vec2 screen;
	};
	std::optional<Press> _last;
};

/// The hand's tooltips for the leash, by their place among the game's tooltips. Each shows the Action button's mouse.
constexpr uint32_t k_FocusCreatureTip = 47;
constexpr uint32_t k_DetachLeashTip = 48;
constexpr uint32_t k_AttachLeashTip = 49;
/// What the hand is over, for the leash's tooltip
struct Hovered
{
	/// The thing under the hand, by entity number, if any
	std::optional<uint32_t> object;
	/// The leash may be tied to it, and it is not a miracle bubble taken only once
	bool leashTarget {false};
	bool miracleBubble {false};
	/// How far the player's creature has grown up
	uint32_t developmentPhase {0};
};
/// The tooltip the hand shows with the leash held: "Focus Creature" over nothing, "Attach Leash" over something the
/// leash could be tied to once the creature is old enough, "Detach Leash" over what it is tied to
[[nodiscard]] std::optional<uint32_t> ToolTipFor(const Leash& leash, const Hovered& hovered);
/// The leash can be tied to things once the creature has grown up past this stage
constexpr uint32_t k_TyingPhase = 3;

/// What the creature's state allows of an order
struct Body
{
	/// The leash works, as scripts set
	bool leashWorks {true};
	float life {1.0f};
	/// How exhausted it is, 0 to 1
	float exhaustion {0.0f};
};
/// Whether the creature takes orders: not when the leash doesn't work, it has no life left, or it is wholly exhausted
[[nodiscard]] bool TakesOrders(const Body& body);
/// A creature this exhausted or more takes no orders
constexpr float k_TooExhausted = 1.0f;

/// The farthest from the place it is told to go that the creature stops, and how its own height counts: it stops within
/// its height, up to this
constexpr float k_MaxArrival = 8.0f;
/// Told to go somewhere again while still carrying out an order, it runs, then waits at the place for at least this
/// many seconds and up to this many more
constexpr float k_RunWaitSeconds = 2.0f;
constexpr float k_RunWaitExtraSeconds = 3.0f;
/// A tap this near its home sends it home to sleep, when it is tired enough and its player has a citadel
constexpr float k_HomeSleepReach = 12.0f;
constexpr float k_SleepExhaustion = 0.1f;
/// Going home to sleep, it stops within its height of home, up to this
constexpr float k_MaxSleepArrival = 5.0f;
/// Carrying something, it throws it at the place from within this many times its own height
constexpr float k_ThrowReachHeights = 8.0f;
/// The farthest a place counts as reachable when the nearest point it can stand at is looked for
constexpr float k_NearestReachableSearch = 1000.0f;
/// It is told which desire it acts on when sent farther than this
constexpr float k_TellDesireDistance = 40.0f;

/// What the creature finds at the place tapped
struct Ground
{
	/// It carries something
	bool carrying {false};
	/// Where it can stand nearest the place: the place itself, the nearest point, or nowhere
	std::optional<glm::vec2> reachable;
	/// The place itself can be stood at
	bool placeReachable {true};
	/// The place is in the sea
	bool water {false};
	/// It wants water, and its body needs some
	bool thirsty {false};
	/// A field lies at the place
	bool field {false};
	/// Its player has a citadel, how tired it is, whether it is already off to sleep, and how far the place is from its
	/// home
	bool playerHasCitadel {false};
	float exhaustion {0.0f};
	bool sleeping {false};
	std::optional<float> distanceFromHome;
	LeashType leash {LeashType::Rope};
	/// An earlier order is still being carried out
	bool orderInForce {false};
	/// Its height
	float height {0.0f};
};

/// An order on the land, as the creature takes it
struct GroundOrder
{
	enum class Kind : uint8_t
	{
		/// It can't get there: it doesn't go, and the help says so
		Inaccessible,
		Drink,
		LookAtReflection,
		/// It acts on the field at the place, as if the field had been tapped
		ActOnField,
		SleepAtHome,
		MoveTo,
		/// Carrying something, it takes it there and puts it down, or throws it there
		PutDownAt,
		ThrowAt,
	};
	Kind kind {Kind::MoveTo};
	/// Where it goes: the place, or the nearest it can reach
	glm::vec2 point {0.0f};
	/// How near the point it stops
	float arrival {0.0f};
	/// It runs there and then waits a while
	bool runAndWait {false};
	/// It answers with a nod and its acknowledging sound
	bool acknowledged {true};
	/// The place is marked
	bool marked {true};
};
/// What tapping the land with the leash tells the creature to do
[[nodiscard]] GroundOrder OnGround(glm::vec2 place, const Ground& ground);

/// What the creature makes of the thing tapped
struct Thing
{
	/// A forest, or a fish farm
	bool forest {false};
	bool fishFarm {false};
	/// For a fish farm: it is hungry, knows how to fish, and isn't full
	bool hungry {false};
	bool knowsFishing {false};
	float energy {1.0f};
	/// It carries something, and it could lift the thing
	bool carrying {false};
	bool liftable {false};
	/// The thing is another creature, and how far it is from the creature
	bool creature {false};
	float distance {0.0f};
	LeashType leash {LeashType::Rope};
	float height {0.0f};
};
/// What the creature tries, in turn, until one is done: the first it can carry out is its order
enum class Attempt : uint8_t
{
	/// A forest tapped is an order to go to it, as a tap of the land there
	GoToForest,
	FishAndEat,
	/// Its desires choose what to do to the thing, with what it carries or without
	DesiresUsingCarried,
	Desires,
	/// On the aggression leash, another creature near enough is fought
	Fight,
	/// With nothing else to do, it picks up what it can lift and holds it, or looks at anything else
	Hold,
	Look,
};
/// The attempts an order on a thing makes, in order
[[nodiscard]] std::vector<Attempt> OnThing(const Thing& thing);
/// On the aggression leash, another creature within this many times the creature's height is fought
constexpr float k_FightReachHeights = 8.0f;
/// Answering an order on a fish farm, the creature doesn't nod
[[nodiscard]] bool Acknowledges(const std::vector<Attempt>& attempts);

/// The actions of the game's table the orders are carried out as
constexpr std::string_view k_MoveAction = "MoveToPos";
constexpr std::string_view k_DrinkAction = "DrinkFromTheSea";
constexpr std::string_view k_ReflectionAction = "LookAtReflection";
constexpr std::string_view k_SleepAction = "Sleep";
constexpr std::string_view k_HoldAction = "HoldObject";
constexpr std::string_view k_LookAction = "ExamineByLooking";
constexpr std::string_view k_FightAction = "Fight";
constexpr std::string_view k_FishAction = "FishAndEat";

/// The agendas of the orders, for the creature's mind to carry out.
/// Walking to a point, looking at it as it goes, stopping within a distance; running instead, and waiting there some
/// seconds after
[[nodiscard]] std::vector<creature_mind::Step> MoveTo(glm::vec2 point, float arrival, bool run, float waitSeconds);
/// How long it waits after running to the place: the least and a random part of the extra seconds, counted in whole
/// game turns
[[nodiscard]] float RunWaitSeconds(float extraSeconds, float turnsPerSecond);
/// Walking home and sleeping there, then the sleep's own agenda
[[nodiscard]] std::vector<creature_mind::Step> SleepAtHome(glm::vec2 home, float height,
                                                           std::vector<creature_mind::Step> sleep);
/// Taking what it carries to a point and putting it down there, or throwing it at the point from near enough
[[nodiscard]] std::vector<creature_mind::Step> PutDownAt(glm::vec2 point, float height);
[[nodiscard]] std::vector<creature_mind::Step> ThrowAt(glm::vec2 point, float height);
/// Holding something: picking it up if it doesn't hold it already, turning to face the player, playing with it once if
/// it has hardly looked such a thing over before (by a random choice of how, 0 to 3), then waiting holding it, facing the
/// player, for the seconds given
[[nodiscard]] std::vector<creature_mind::Step> Hold(uint32_t object, bool alreadyHeld, glm::vec2 camera,
                                                    std::optional<uint32_t> playWith, float waitSeconds);
/// Looking at its reflection: going to the water, looking down a second, a dazed look, looking down a while, over to
/// the shore, a while there, the dazed look again, back to the water and looking down a while; each walk stops within
/// twice its height. The waits are counted in whole game turns.
[[nodiscard]] std::vector<creature_mind::Step> LookAtReflection(glm::vec2 water, glm::vec2 shore, float height,
                                                                float turnsPerSecond);
/// It plays with what it holds once if it has done so fewer times than this with things of that kind
constexpr uint32_t k_HoldPlayLimit = 2;
/// How long it holds the thing, in seconds: at least this, and up to this many more
constexpr float k_HoldSeconds = 40.0f;
constexpr float k_HoldExtraSeconds = 10.0f;
/// How long it holds the thing, with a random part of the extra seconds, in whole game turns
[[nodiscard]] float HoldSeconds(float extraSeconds, float turnsPerSecond);

/// The target marker.
/// The sparkles over the target last this many game turns before fading
constexpr int k_SparkleTurns = 20;
/// The ring hangs this far above the land point, or above the top of the thing
constexpr float k_MarkerLift = 2.0f;
/// Where the ring hangs: above a thing's top, by its point on the land and its height, or above a point of the land
[[nodiscard]] glm::vec3 MarkerOverThing(glm::vec3 point, float height);
[[nodiscard]] glm::vec3 MarkerOverLand(glm::vec3 point);
/// The ring throbs: its half width and half height each swing from 0.2 to 1.2 once a second, the height a fifth of a
/// second behind the width, on a clock of milliseconds that goes round every two seconds
constexpr uint32_t k_PulsePeriodMs = 2000;
constexpr uint32_t k_PulseLagMs = 200;
[[nodiscard]] glm::vec2 MarkerSize(uint32_t clockMs);
/// The footprint inside the ring is this much the ring's size, turned a quarter turn
constexpr float k_FootprintShare = 0.7f;
/// The sheet's picture of the plain ring, and the footprint of a species, by its row in the game's creature tables
constexpr uint8_t k_RingCell = 56;
[[nodiscard]] uint8_t FootprintCell(size_t speciesRow);

} // namespace openblack::creature_leash_orders
