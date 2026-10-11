/*******************************************************************************
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

#include <entt/fwd.hpp>
#include <glm/vec3.hpp>

#include "Enums.h"

namespace openblack::ecs::systems
{

/// The stores of food and wood and their piles, shared by the miracles that pour into them, the hand that gives and
/// scoops, and the physics when something thrown hits one: what each stores, what it takes of what it is given, what
/// giving and taking count for in its town, and the things it takes whole
class ResourceStoreSystemInterface
{
public:
	virtual ~ResourceStoreSystemInterface() = default;

	/// What an object would give a store: its kind of resource and how much of it, nothing for what isn't a resource
	struct ObjectResource
	{
		ResourceType type {ResourceType::None};
		uint32_t amount {0};
		/// It poisons what it is given to: a toadstool, or a poisoned pot
		bool poisoned {false};
	};
	[[nodiscard]] virtual ObjectResource ResourceOf(entt::entity object) const = 0;

	/// Whether something stores a resource: a storage pit stores anything; a building going up stores wood on its site;
	/// a pile only through the store it is part of
	[[nodiscard]] virtual bool IsStore(entt::entity store, ResourceType type) const = 0;
	/// The store, or the pile's store, takes what it will of an amount; giving it counts for the giver in its town. A
	/// building site takes wood into its pile nearest where it is given. What it took.
	virtual uint32_t AddToStore(entt::entity store, ResourceType type, uint32_t amount, std::optional<PlayerNames> giver,
	                            bool poisoned, std::optional<glm::vec3> at) = 0;
	/// A pile takes what it will of an amount of its own resource, with its thud; a poisoned gift poisons it. What it took.
	virtual uint32_t AddToPile(entt::entity pile, ResourceType type, uint32_t amount, bool poisoned) = 0;
	/// What is taken from a pile: a store's pile gives what its store can spare, the rest coming from the store's other
	/// piles, and the taking counts against the taker in the store's town. What was taken.
	virtual uint32_t TakeFromPile(entt::entity pile, ResourceType type, uint32_t amount, std::optional<PlayerNames> taker) = 0;
	/// A store takes a thing whole, for all the resource it is worth, and the thing goes; whether it took it. Thrown by a
	/// player, the giving is that player's.
	virtual bool TakeObject(entt::entity store, entt::entity object, std::optional<PlayerNames> giver) = 0;
	/// A resource poured at a point goes to the stores and piles of it about the point, each taking what it will; what is
	/// left makes a new pile of the player's unless the point is in the water. A power-up's food sparkles over its new
	/// pile. A poisoned pour poisons every pile it goes into or makes. Whether anything was taken or made.
	virtual bool PourAt(ResourceType type, glm::vec3 point, uint32_t amount, bool speedUp, PlayerNames player,
	                    bool poisoned) = 0;
	/// The store a pile is part of, none for a pile on its own
	[[nodiscard]] virtual std::optional<entt::entity> StoreOf(entt::entity pile) const = 0;

	/// How much of a resource a thing says it holds, as a script or a villager asks it: a store or building its count, a
	/// pile its store's count or else its own, a building site the wood in its piles, anything else what it is worth of
	/// that resource. A villager says none, whatever it carries.
	[[nodiscard]] virtual uint32_t GetResource(entt::entity object, ResourceType type) const = 0;
	/// A thing takes an amount of a resource as it takes it from a script or a villager, with nobody's giving counted:
	/// a store into its piles, a pile into its store or itself, a building going up into its site (a temple's site into
	/// the pile nearest a place, making its piles if it has none), a villager into its load. What it took, which for a
	/// villager is always none.
	virtual uint32_t AddResource(entt::entity object, ResourceType type, uint32_t amount, std::optional<glm::vec3> at) = 0;
	/// A thing gives up to an amount of a resource, with nobody's taking counted: a store from its piles, a pile from its
	/// store or itself, a building going up from its site's piles in turn. What it gave.
	virtual uint32_t RemoveResource(entt::entity object, ResourceType type, uint32_t amount) = 0;
};

} // namespace openblack::ecs::systems
