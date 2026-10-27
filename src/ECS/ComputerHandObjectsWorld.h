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

#include <entt/entity/entity.hpp>
#include <glm/vec3.hpp>

#include "Enums.h"

namespace openblack::computer_hand
{

/// What the hand does with things in the game: it takes them, holds them, uses them and lets go of them
class ObjectsWorld
{
public:
	virtual ~ObjectsWorld() = default;
	/// The hand of the god has room for a thing
	[[nodiscard]] virtual bool HandHasRoom() const = 0;
	/// The hand puts down what it holds where the hand is
	virtual void PutDownHeld(glm::vec3 hand) = 0;
	/// The thing can go into the hand now
	[[nodiscard]] virtual bool CanPickUp(entt::entity thing) const = 0;
	/// The god places the thing in its hand. A field gives a handful of its food, then all the food that is left in it.
	virtual void PickUp(entt::entity thing, glm::vec3 hand) = 0;
	/// The hand uses what it holds on the thing: food or wood goes into a store
	virtual void UseHeldOn(entt::entity target, glm::vec3 hand) = 0;
	/// The hand lets go of what it holds where it is, with no speed
	virtual void DropHeld(glm::vec3 hand) = 0;
	/// What the pot in the hand holds of the food or the wood. None when the hand holds no pot of it.
	[[nodiscard]] virtual std::optional<uint32_t> HeldPotHolds(ResourceType type) const = 0;
	/// The food or the wood that a thing has: a pile of a store answers for its store
	[[nodiscard]] virtual uint32_t ResourceIn(entt::entity thing, ResourceType type) const = 0;
	/// The hand takes food or wood out of a container. It adds the amount to the pot in its hand, or it makes a new pot
	/// in the hand at the container. False when the hand gets no pot.
	virtual bool TakeFromContainer(entt::entity container, ResourceType type, uint32_t amount, glm::vec3 hand) = 0;
};

} // namespace openblack::computer_hand
