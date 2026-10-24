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

/// The player of the command left a network game, or did not come back in time. Only the game server sends it. The
/// world defeats the player's god at the turn, and the god's slot stays closed.
struct PlayerLeft
{
};

/// The player of the command is a human god of the network game from its start. Only the game server sends it, for each
/// human in the first turn. The world gives the player hands of its own, makes the player when the land has none, and
/// gives each slot with no human a computer god.
struct PlayerJoined
{
};

/// What a command does
using Action =
    std::variant<LeashKey, LeashTap, CreatureFeedback, HandTake, HandLetGo, HandTap, HandGive, LeashOrder, LeashPostTap,
                 HandReport, HandTwist, HandLock, HandUnlock, HandApply, TotemShare, CreatureBruise, PlayerLeft, PlayerJoined>;

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
