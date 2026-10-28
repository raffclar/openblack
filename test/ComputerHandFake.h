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
#include <set>
#include <utility>
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
	[[nodiscard]] float InfluenceAt(const map_coords::MapCoords& coords) const override
	{
		// The god has influence west of the border
		return map_coords::ToMetres(coords.x) < influenceBorder ? 1.0f : 0.0f;
	}
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
	bool CastAt(MagicType type, glm::vec3 place, entt::entity /*icon*/, glm::vec3 /*hand*/, float magnitude) override
	{
		cast.push_back(type);
		castAt.push_back(place);
		magnitudes.push_back(magnitude);
		return castWorks;
	}
	[[nodiscard]] bool UnderSpell(entt::entity creature) const override { return creature == underSpell; }
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

	void ThrowHeld(glm::vec3 hand, glm::vec3 velocity) override
	{
		held = entt::null;
		thrownFrom.push_back(hand);
		thrownAt.push_back(velocity);
	}
	bool ForceAction(entt::entity creature, uint32_t action, entt::entity object) override
	{
		if (!speeds.contains(creature))
		{
			return false;
		}
		ordered.push_back({creature, action, object});
		forced.insert(creature);
		return true;
	}
	[[nodiscard]] bool PlanForced(entt::entity creature) const override { return forced.contains(creature); }
	void ForceDominantDesire(entt::entity creature, int32_t desire, float seconds) override
	{
		dominant.push_back({creature, desire, seconds});
	}
	[[nodiscard]] std::optional<CreatureSpeeds> SpeedsOf(entt::entity creature) const override
	{
		const auto found = speeds.find(creature);
		return found != speeds.end() ? std::optional(found->second) : std::nullopt;
	}
	void DrawForShortJourney() override { ++shortJourneyDraws; }
	void SetRequiredSpeed(entt::entity creature, float speed) override { required[creature] = speed; }
	[[nodiscard]] bool HasPlayer(entt::entity creature) const override { return !playerless.contains(creature); }
	void SetLeashType(entt::entity creature, LeashType type) override { leashTypes.push_back({creature, type}); }
	void TakeOffTiedLeash(entt::entity creature) override { takenOff.push_back(creature); }
	bool TieLeash(entt::entity creature, entt::entity object) override
	{
		if (!places.contains(object))
		{
			return false;
		}
		tied.push_back({creature, object});
		return true;
	}
	void Feedback(entt::entity creature, float feedback) override { feedbacks.push_back({creature, feedback}); }
	[[nodiscard]] float DrawFeedbackSeconds() override
	{
		++feedbackDraws;
		return feedbackDraw;
	}
	void LetGoForCast(MagicType type, const map_coords::MapCoords& hand) override
	{
		letGoTypes.push_back(type);
		letGoHands.push_back(hand);
	}
	void RaiseTotem(entt::entity totem, float share) override { raised.push_back({totem, share}); }

	/// An order that the god gave to a creature, and a desire that it made dominant
	struct Order
	{
		entt::entity creature {entt::null};
		uint32_t action {0};
		entt::entity object {entt::null};
	};
	struct Dominant
	{
		entt::entity creature {entt::null};
		int32_t desire {0};
		float seconds {0.0f};
	};
	std::map<entt::entity, CreatureSpeeds> speeds;
	std::vector<Order> ordered;
	std::set<entt::entity> forced;
	std::vector<Dominant> dominant;
	int shortJourneyDraws {0};
	std::map<entt::entity, float> required;
	std::set<entt::entity> playerless;
	std::vector<std::pair<entt::entity, LeashType>> leashTypes;
	std::vector<entt::entity> takenOff;
	std::vector<std::pair<entt::entity, entt::entity>> tied;
	std::vector<std::pair<entt::entity, float>> feedbacks;
	int feedbackDraws {0};
	float feedbackDraw {0.0f};
	std::vector<MagicType> letGoTypes;
	std::vector<map_coords::MapCoords> letGoHands;
	std::vector<std::pair<entt::entity, float>> raised;

	std::map<entt::entity, glm::vec2> places;
	std::vector<glm::vec3> castAt;
	std::vector<float> magnitudes;
	entt::entity underSpell {entt::null};
	float influenceBorder {1.0e9f};
	std::vector<glm::vec3> thrownFrom;
	std::vector<glm::vec3> thrownAt;
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
