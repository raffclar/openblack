/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <entt/entity/entity.hpp>
#include <glm/vec3.hpp>

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
	/// The hand throws what it holds from where it is, at the speed
	virtual void ThrowHeld(glm::vec3 hand, glm::vec3 velocity) = 0;
};

} // namespace openblack::computer_hand
