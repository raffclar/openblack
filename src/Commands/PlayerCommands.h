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

#include <optional>
#include <variant>
#include <vector>

#include <entt/entity/entity.hpp>
#include <glm/vec3.hpp>

#include "Creature/CreatureMarks.h"
#include "Creature/LeashKeys.h"
#include "Enums.h"

/// The commands of the players. The game does not change the world at the time of the player's input. The input makes a
/// command, and the command changes the world at the start of a later game turn. Every player, also the one player of a
/// single game, sends the commands this way, so that all the computers of a network game do the same commands at the
/// same turn.
///
/// The steps of a command:
/// 1. The input puts the command in the open block of its player.
/// 2. Once a frame, after the turns of the frame, the game sends the open block. A block that becomes full is sent at
///    once.
/// 3. At the start of each turn, before the hand's own work of the turn, the game does the commands of all the blocks
///    that it sent, in the order that it sent them. Each command has the number of the player that sent it.
///
/// Thus a command never changes the world in the turn that runs in the same frame step as its input, and the world sees
/// it at the start of the next turn after the send.
namespace openblack::player_commands
{

/// A leash shortcut key: the leash key puts on or takes off the leash, and the two other keys change the leash
struct LeashKey
{
	creature_leash::LeashKey key;
};

/// A click on a creature, which puts the leash on the player's creature
struct LeashTap
{
	entt::entity creature;
};

/// The hand let go of a creature that it held: the creature feels how the hand stroked or slapped it
struct CreatureFeedback
{
	entt::entity creature;
	/// The sum of the strokes and the slaps, in the range from -1 to 1
	float amount;
};

/// The hand takes the thing that it waited for: the thing leaves the land, and the hand holds it
struct HandTake
{
	entt::entity object;
};

/// Where a thing starts when the hand lets go of it. The computer of the player that lets go makes these values from the
/// thing as that computer last drew it in the hand: a globe turns to the camera of that player. The command sends the
/// values to all the computers, thus each computer starts the same path.
struct ThrowStart
{
	/// The spin of the thing, as the bodies count the turn
	glm::vec3 angularMomentum {0.0f};
	/// The origin of the thing's model
	glm::vec3 origin {0.0f};
	/// The angles of the thing's turn about x, y and z. The game turns about z first, then about x, then about y.
	glm::vec3 angles {0.0f};
};

/// The hand lets go of the thing that it holds. It puts the thing down, or throws it at the velocity that the hand had
/// when the player let go of the button. The thing starts from the start in the command.
struct HandLetGo
{
	entt::entity object;
	glm::vec3 velocity;
	ThrowStart start {};
};

/// The hand taps a thing
struct HandTap
{
	entt::entity object;
};

/// The hand gives the thing that it holds to a creature
struct HandGive
{
	entt::entity creature;
	entt::entity object;
};

/// An order to the player's creature on its leash
struct LeashOrder
{
	enum class Kind : uint8_t
	{
		/// The creature acts on a thing
		OnThing,
		/// The creature goes to a place on the land
		AtPlace,
		/// The leash is tied to a thing
		Tie,
		/// The leash comes back to the hand from the thing that it is tied to
		Untie,
	};
	Kind kind;
	/// The thing of an order on a thing or of a tie
	entt::entity object {entt::null};
	/// The place of an order at a place
	glm::vec3 place {0.0f};
};

/// The tap of the player's hand on a leash that hangs at the temple
struct LeashPostTap
{
	entt::entity post;
};

/// The report of where the player's hand and camera are. The input of the player moves them, and the world reads them
/// only from the last report that a turn did.
struct HandReport
{
	/// The point under the cursor that the interface picks
	glm::vec3 collidePoint;
	/// The place of the hand
	glm::vec3 handPoint;
	/// The place of the camera
	glm::vec3 camera;
	/// The point that the camera looks at
	glm::vec3 focus;

	bool operator==(const HandReport& other) const = default;
};

/// The twist of a thing that the hand let go, after the hand moved a little: the turn turns the thing's body about
/// the level axis across the motion of the hand since the let go
struct HandTwist
{
	entt::entity object;
	/// How far the hand moved since it let go of the thing
	glm::vec3 moved;
};

/// The press of the hand on a thing that it locks on to: the hand scoops from a pile, a field or a fish farm, or holds a
/// creature to stroke or slap it. The lock stays until the hand sends the end of the lock (HandUnlock).
struct HandLock
{
	entt::entity object;
};

/// The end of the lock of the hand on a thing (HandLock), when the player lets go of the button
struct HandUnlock
{
	entt::entity object;
};

/// The press of the hand that holds a thing on a target that the thing is used on: a pot goes into a store or a pile,
/// a thing goes whole to an altar, a gate stone plinth or a store
struct HandApply
{
	entt::entity target;
};

/// The hand let go of a town's totem at a share of worship: the town takes that share
struct TotemShare
{
	entt::entity totem;
	/// The share of the town's people that go to worship, from 0 to 1
	float share;
};

/// A hard slap of the hand bruised a creature: the slapping player's own creature loses some life, and the slapped
/// creature, when it is that creature, takes the wound where the slap met its skin
struct CreatureBruise
{
	entt::entity creature;
	/// Where the slap met the skin of the slapped creature, which the frame measured; none for a miss
	std::optional<creature_marks::Mark> wound;
};

/// A press of the hand on the ground of the arena while the hand directs the fight of the player's creature. The turn
/// makes the creature step towards the point, from where the creature stands and the way that it faces then.
struct FightGround
{
	entt::entity creature;
	/// The point on the ground that the hand pressed
	glm::vec3 point;
	/// The Move button makes the step in place of the queued moves; the Action button adds it after them
	bool replace;
};

/// A move of the player's fighting creature that the hand or a gesture of the player gives: a press on the opponent or
/// on the player's creature, the special move or a miracle of the fight. The turn adds the move to the queue of the
/// sending player's creature.
struct FightMove
{
	/// What the hand pressed on: the opponent or the player's own creature, or the creature of a gesture
	entt::entity target;
	/// The kind of the move (a creature_fight::Move::Kind) and its value: the animation or the miracle
	uint8_t kind;
	uint32_t value;
	/// The move goes in place of the queued moves (the Move button), or after them
	bool replace;
};

/// The player let go of the button that pressed on a body in a fight: the blow that waits last in the queue of the
/// sending player's creature gets the charge
struct FightCharge
{
	/// How long the button was held, in milliseconds
	uint32_t heldMs;
};

/// A stroke or a slap of the creature that the hand holds, in a network game: every computer plays the creature's
/// reaction at the same turn
struct CreatureReaction
{
	entt::entity creature;
	/// The animation of the reaction and whether it plays mirrored
	uint32_t animation;
	bool mirrored;
	/// The face that the reaction pulls, if it pulls one: the face (a creature_face::Face), how long and why (a
	/// creature_face::Cue)
	bool pullsFace;
	uint8_t face;
	float faceMs;
	uint8_t cue;
	/// The share of the way through what the body plays that the reaction can stop it at
	float interruptsAfter;
};

/// A click of the hand in a network game: the turn writes the click that the scripts read, so that the scripts of
/// every computer read the same click
struct HandClick
{
	/// The thing that the hand clicked, or none
	entt::entity thing;
	/// The click is the let go of a press: with no thing, the place on the land is clicked, or with no place the thing
	/// is forgotten
	bool release;
	/// The place on the land of a let go, in the fixed units of the map
	bool hasPlace;
	int32_t placeX;
	int32_t placeZ;
};

/// A gesture of the hand on the miracle in the hand: a power-up, a scribble or a shake. The turn powers the seed up,
/// stops its power-up or drops it.
struct SeedGesture
{
	enum class Kind : uint8_t
	{
		PowerUp,
		Scribble,
		Shake,
	};
	Kind kind;
	/// The level of a power-up
	int8_t level;
};

/// The player of the command left a network game, or did not come back in time. Only the game server sends it. The
/// world defeats the player's god at the turn, and the god's slot stays closed.
struct PlayerLeft
{
};

/// The thing under the cursor of the player's hand in a network game, sent when it changes. The turn keeps it with the
/// report of the hand, so that every computer counts the same time that the hand stays over the creature.
struct HandPick
{
	/// The thing under the cursor, or none
	entt::entity object;
};

/// The player of the command is a human god of the network game from its start. Only the game server sends it, for each
/// human in the first turn. The world gives the player hands of its own, makes the player when the land has none, and
/// gives each slot with no human a computer god.
struct PlayerJoined
{
};

/// The miracle selection chose a miracle: the player's icons give the miracle of the seed to the hand, or one of them
/// starts to charge it. The turn checks again that an icon may do it.
struct MiracleRequest
{
	SpellSeedType seed;
};

/// The R gesture or the R key: the player's icons give the last miracle that went into the player's hand again, or one
/// of them starts to charge it. The turn reads that miracle and checks again that an icon may do it.
struct MiracleRepeat
{
};

/// The cast of the miracle in the player's hand, as the hand of that player found it: on a thing, or at a point on the
/// land. A miracle that the hand keeps locked on sends one cast at each turn that the button stays down. The turn checks
/// again that the miracle may be cast there, then casts it with the hand as the command gives it. Thus every computer
/// casts the same miracle at the same turn.
struct HandCast
{
	/// The thing that the miracle is cast on, or none for a cast on the land
	entt::entity object {entt::null};
	/// The point on the land: the point under the hand, or the middle of the circle that the player drew
	glm::vec3 point {0.0f};
	/// The size of the miracle: the radius of the circle that the player drew, or the plain size
	float size {0.0f};
	/// The place of the hand at the cast
	glm::vec3 handPosition {0.0f};
	/// The direction that the camera looks in at the cast
	glm::vec3 cameraForward {0.0f, 0.0f, 1.0f};
	/// The smoothed velocity of the hand at the cast
	glm::vec3 handVelocity {0.0f};
	/// The spin that the movement of the hand gives the miracle
	float spin {0.0f};
};

/// The player let go of the button that kept a miracle locked on in the hand: the miracle stops, and the seed keeps what
/// the miracle has left
struct HandCastUnlock
{
	/// The seed in the hand
	entt::entity seed {entt::null};
};

/// The gesture of the leash picker chose a leash: the creature of the player changes to the leash of this type, if it
/// knows that leash
struct LeashPick
{
	LeashType type;
};

/// A scribble of the hand shakes off the thing that the hand holds. The thing falls from where the hand holds it, with
/// no speed, and the turn does not plant it again.
struct HandShakeOff
{
};

/// The hand of the player gripped the land: the turn makes a puff of dust at the point on the ground. Thus every computer
/// makes the same dust at the same turn.
struct GripDust
{
	/// The point on the ground under the hand at the grip
	glm::vec3 point {0.0f};
};

/// What a command does
using Action = std::variant<LeashKey, LeashTap, CreatureFeedback, HandTake, HandLetGo, HandTap, HandGive, LeashOrder,
                            LeashPostTap, HandReport, HandTwist, HandLock, HandUnlock, HandApply, TotemShare, CreatureBruise,
                            FightGround, FightMove, FightCharge, CreatureReaction, HandClick, SeedGesture, PlayerLeft, HandPick,
                            PlayerJoined, MiracleRequest, MiracleRepeat, HandCast, HandCastUnlock, LeashPick, HandShakeOff,
                            GripDust>;

/// A command and the player that sent it
struct Command
{
	PlayerNames player;
	Action action;
};

/// The commands that the players made and that the world did not see yet. The queue keeps no other game state, so that
/// the game and the tests use it both.
class CommandQueue
{
public:
	/// The most commands in one block. The block goes at once when it is full.
	static constexpr size_t k_BlockSize = 6;

	/// Put a command in the open block, with the number of the player that sends it. Send the block if it is full.
	void Submit(PlayerNames sender, Action action);
	/// Send the open block, if it has commands. The commands then wait for the start of the next turn.
	void Send();
	/// Take the commands of all the blocks that the queue sent, in the order that it sent them
	[[nodiscard]] std::vector<Command> TakeSent();
	/// Remove all the commands, those that wait and those in the open block
	void Clear();

	/// The number of commands in the open block
	[[nodiscard]] size_t OpenCount() const { return _open.size(); }
	/// The number of commands that the queue sent and that wait for the start of a turn
	[[nodiscard]] size_t SentCount() const;
	/// The number of blocks that the queue sent and that wait for the start of a turn
	[[nodiscard]] size_t SentBlocks() const { return _sent.size(); }

private:
	std::vector<Command> _open;
	std::vector<std::vector<Command>> _sent;
};

} // namespace openblack::player_commands
