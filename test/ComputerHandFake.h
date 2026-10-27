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

#include <map>
#include <optional>
#include <vector>

#include <entt/entity/entity.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

#include "3D/MapCoords.h"
#include "ECS/ComputerHand.h"

/// A fake world for the tests of the steps of the hand of a computer god
namespace
{

using namespace openblack;
using namespace openblack::computer_hand;

constexpr auto k_Icon = static_cast<entt::entity>(5);
constexpr auto k_Store = static_cast<entt::entity>(6);
constexpr auto k_Pot = static_cast<entt::entity>(70);

/// A flat land with things at places that the test sets. It gives each part of the world of the steps.
class FakeWorld final: public MovesWorld, public ObjectsWorld, public CastsWorld, public CreatureWorld, public TotemWorld
{
public:
	[[nodiscard]] std::optional<map_coords::MapCoords> CoordsOf(entt::entity thing) const override
	{
		const auto found = places.find(thing);
		if (found == places.end())
		{
			return std::nullopt;
		}
		return map_coords::MapCoords {
		    .x = map_coords::ToFixed(found->second.x), .z = map_coords::ToFixed(found->second.y), .altitude = 0.0f};
	}
	[[nodiscard]] float HeightOf(entt::entity /*thing*/) const override { return 4.0f; }
	[[nodiscard]] float LandHeight(const map_coords::MapCoords& /*coords*/) const override { return 20.0f; }
	[[nodiscard]] bool IconHasSite(entt::entity icon) const override { return icon == k_Icon; }
	[[nodiscard]] bool IconCharging(entt::entity /*icon*/) const override { return charging; }
	bool StartChant(entt::entity /*icon*/, MagicType /*type*/) override { return startWorks; }
	[[nodiscard]] float ChantNeeded(entt::entity /*icon*/) const override { return needed; }
	[[nodiscard]] bool Angry(MagicType type) const override { return type == MagicType::Fireball; }
	bool Cast(MagicType type, entt::entity /*target*/, entt::entity /*icon*/, glm::vec3 /*hand*/) override
	{
		cast.push_back(type);
		return castWorks;
	}
	[[nodiscard]] float CastSeconds(MagicType /*type*/) const override { return 0.5f; }
	[[nodiscard]] uint32_t SeedGesture(MagicType /*type*/, int32_t stage) const override { return stage == 0 ? 1 : 9; }
	[[nodiscard]] float GestureLength(uint32_t gesture) const override { return gesture == k_GestureCircle ? 2.0f : 1.0f; }
	[[nodiscard]] glm::vec3 GesturePoint(uint32_t /*gesture*/, float share) const override
	{
		// A line from the bottom left of the square to its top right
		return {share, 0.0f, share};
	}
	void ShowGesture(uint32_t gesture, glm::vec3 corner, float size) override
	{
		shown.push_back(gesture);
		shownAt = corner;
		shownSize = size;
	}
	[[nodiscard]] bool HandHasRoom() const override { return held == entt::null; }
	void PutDownHeld(glm::vec3 /*hand*/) override
	{
		held = entt::null;
		++putDown;
	}
	[[nodiscard]] bool CanPickUp(entt::entity thing) const override { return places.contains(thing); }
	void PickUp(entt::entity thing, glm::vec3 /*hand*/) override { held = thing; }
	void UseHeldOn(entt::entity target, glm::vec3 /*hand*/) override
	{
		usedOn.push_back(target);
		held = entt::null;
	}
	void DropHeld(glm::vec3 hand) override
	{
		held = entt::null;
		droppedAt.push_back(hand);
	}
	[[nodiscard]] std::optional<uint32_t> HeldPotHolds(ResourceType type) const override
	{
		if (held != k_Pot || potType != type)
		{
			return std::nullopt;
		}
		return potAmount;
	}
	[[nodiscard]] uint32_t ResourceIn(entt::entity thing, ResourceType /*type*/) const override
	{
		const auto found = stock.find(thing);
		return found == stock.end() ? 0u : found->second;
	}
	bool TakeFromContainer(entt::entity container, ResourceType type, uint32_t amount, glm::vec3 /*hand*/) override
	{
		stock[container] -= amount;
		taken.push_back(amount);
		if (held == k_Pot)
		{
			potAmount += amount;
		}
		else if (makesPot)
		{
			held = k_Pot;
			potType = type;
			potAmount = amount;
		}
		return held == k_Pot;
	}

	std::map<entt::entity, glm::vec2> places;
	bool charging {false};
	bool startWorks {false};
	float needed {0.0f};
	bool castWorks {true};
	std::vector<MagicType> cast;
	std::vector<uint32_t> shown;
	glm::vec3 shownAt {0.0f};
	float shownSize {0.0f};
	entt::entity held {entt::null};
	int putDown {0};
	std::vector<entt::entity> usedOn;
	std::vector<glm::vec3> droppedAt;
	std::map<entt::entity, uint32_t> stock;
	std::vector<uint32_t> taken;
	bool makesPot {true};
	ResourceType potType {ResourceType::Food};
	uint32_t potAmount {0};
	/// The world of the steps, made of the parts of this fake
	World view {.moves = *this, .objects = *this, .casts = *this, .creature = *this, .totem = *this};
};

} // namespace
