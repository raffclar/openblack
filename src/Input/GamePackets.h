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

#include <entt/entity/entity.hpp>
#include <glm/vec3.hpp>

#include "3D/MapCoords.h"

// The game's packets as single player runs them: the interface sends them into a buffer that flushes by itself at 6
// packets, the loop flushes once an iteration after the turns (into the session's list) and the next turn applies them
// first thing. Nothing is applied in the turn that sent it. Wiki: docs/bw1-notes/engine-loop.md.
namespace openblack::game_packets
{

/// The packet's type byte, which picks its handler
enum class Type : uint8_t
{
	ApplyToObject = 0x11,   ///< Applies the held object to an object
	ApplyToMapCoord = 0x12, ///< Applies the held object to a map point, then the throw
	PlaceInHand = 0x13,     ///< Puts an object in the hand
	HandAndCamera = 0x15,
	Hand = 0x16,
	Camera = 0x17,
	ApplyUnlock = 0x1A,
	StartLockedSelect = 0x1B, ///< Starts a locked select
	EndLockedSelect = 0x1C,   ///< Ends a locked select
	ThrowHeld = 0x1D,         ///< Throws the held object
	Give = 0x1F,
	Tap = 0x20,            ///< Taps an object
	SpotVisual = 0x2B,     ///< Spawns a spot visual
	ReleaseImpulse = 0x40, ///< The released object's spin
	ThrowData = 0x4D,      ///< Copied into the interface status's throw data
	/// How the hand treated a creature, to its mind (data[0])
	CreatureFeedback = 0x59,
	/// A click on the player's own creature: their leash key
	CreatureLeashClick = 0x5F,
	SpellIconCharge = 0x6A,
};

/// A decompressed packet: the fields the interface's packets use
struct Packet
{
	Type type {Type::Tap};
	entt::entity object {entt::null}; ///< Null to the handler when the object is gone
	glm::vec3 position {0.0f};        ///< A MapCoords or the gesture's point
	int32_t value {0};                ///< HandAndCamera / Hand / Camera: the ping ring index
	/// HandAndCamera / Hand: the action collide's MapCoords
	map_coords::MapCoords coords {};
	/// ThrowData: velocity, angular momentum, position, YXZ angles; ApplyToObject / ApplyToMapCoord: the gesture
	/// (size, point); HandAndCamera / Hand / Camera: the hand, the camera position and focus
	std::array<float, 16> data {};
};

/// Into the buffer; the buffer flushes by itself once it holds 6
void Push(const Packet& packet);
/// Once an iteration of the game loop: the buffer goes to the session's list
void Flush();
/// The session's list in order through the handlers; a packet whose object no longer exists reaches its handler with
/// entt::null (the handler takes its null path, mostly EndAction)
void DispatchQueuedPackets();

/// The handler of one packet type (each owner registers its own: the hand all its types)
using Handler = std::function<void(const Packet& packet)>;
void SetHandler(Type type, Handler handler);

/// Packets still waiting (the buffer and the list), for the tests and the traces
[[nodiscard]] size_t Pending();
/// A new game or land: nothing waits ((inferred) the session list is emptied with the game)
void Reset();
/// The handlers' owner goes (the hand system's destructor): no handler is left pointing at it
void ClearHandlers();

} // namespace openblack::game_packets
