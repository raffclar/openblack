/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <optional>

#include <entt/entity/entity.hpp>

#include "Enums.h"

/// What the hand of a human who leaves a network game lets go of, at the turn of the leave
namespace openblack::ecs::leaver_hand
{

/// The parts of the world that the hand of a human who leaves touches
class LeaverHandWorldInterface
{
public:
	virtual ~LeaverHandWorldInterface() = default;
	/// The leash of the player's creature comes off when the player holds it. Whether it came off.
	virtual bool TakeOffLeash(PlayerNames player) = 0;
	/// Each icon at the worship sites of the player's temple that charges a miracle for the hand of the player stops
	virtual void CancelCharges(PlayerNames player) = 0;
	/// The miracle seed that the hand of the player holds, if there is one
	[[nodiscard]] virtual std::optional<entt::entity> HeldSeed(PlayerNames player) const = 0;
	/// The thing that the hand of the player holds, if there is one
	[[nodiscard]] virtual std::optional<entt::entity> HeldThing(PlayerNames player) const = 0;
	/// Whether the world can still deal with the thing
	[[nodiscard]] virtual bool IsAvailable(entt::entity thing) const = 0;
	/// The seed goes from the world, and a miracle that still runs from it closes down
	virtual void DeleteSeed(entt::entity seed) = 0;
	/// The thing goes from the world at once. The hand that held it then holds nothing.
	virtual void DeleteThing(entt::entity thing) = 0;
};

/// What the hand of the player did at the leave
struct Release
{
	bool leashOff {false};
	std::optional<entt::entity> deleted;
};

/// The hand of a human who leaves a network game lets go, in this order:
/// 1. The leash of the player's creature comes off when the player holds it.
/// 2. The miracles that charge for the hand of the player stop.
/// 3. The held seed or thing goes from the world when the world can still deal with it. It does not fall.
/// Every computer does this at the turn of the leave.
[[nodiscard]] Release ReleaseHand(LeaverHandWorldInterface& world, PlayerNames player);

/// The same, with the systems of the game
[[nodiscard]] Release ReleaseHandInGame(PlayerNames player);

} // namespace openblack::ecs::leaver_hand
