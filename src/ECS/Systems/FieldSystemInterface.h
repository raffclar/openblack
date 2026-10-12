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

#include <chrono>
#include <optional>

#include <entt/entity/entity.hpp>
#include <glm/vec3.hpp>

#include "3D/FieldCrop.h"

namespace openblack::ecs::components
{
struct Field;
}

namespace openblack::ecs::systems
{

/// The crops growing in the towns' fields (components::Field)
class FieldSystemInterface
{
public:
	virtual ~FieldSystemInterface() = default;

	/// Once a game turn: each field's crop grows on its own turn in every ten, by the land's alignment and the weather
	virtual void ProcessTurn(uint32_t turn) = 0;
	/// Once a frame: the crops that show ease towards the height their food puts them at
	virtual void Update(std::chrono::duration<float, std::milli> gameTime) = 0;

	/// How a field's crop looks, or nothing while it is too small to show
	[[nodiscard]] virtual std::optional<field_crop::Look> GetLook(const components::Field& field) const = 0;

	/// What a field wants of its farmers, and whether its crop is ripe
	[[nodiscard]] virtual field_crop::Activity GetActivity(entt::entity field) const = 0;
	[[nodiscard]] virtual bool IsRipe(entt::entity field) const = 0;

	/// The field of a town a villager at a place would best farm, by how much each wants farming and how near it is,
	/// with how good it is; none when no field wants farming
	struct FieldChoice
	{
		entt::entity field {entt::null};
		float score {0.0f};
	};
	[[nodiscard]] virtual std::optional<FieldChoice> BestFieldFor(entt::entity town, const glm::vec3& position) const = 0;

	/// A villager starts or stops farming a field
	virtual void AddFarmer(entt::entity field, entt::entity villager) = 0;
	virtual void RemoveFarmer(entt::entity field, entt::entity villager) = 0;

	/// Sows the field once more if it still wants sowing; whether it did
	virtual bool PlantCrop(entt::entity field) = 0;
	/// Takes food out of a field, for a farmer or the hand: what the taker gets. An emptied field's fire goes out and its
	/// town looks again at what it wants.
	virtual uint32_t RemoveFood(entt::entity field, float amount) = 0;
};

} // namespace openblack::ecs::systems
