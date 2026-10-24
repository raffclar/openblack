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

#include "ECS/Components/ComputerPlayer.h"
#include "ECS/ComputerHand.h"
#include "ECS/ComputerMind.h"
#include "Enums.h"

#if !defined(LOCATOR_IMPLEMENTATIONS)
#error "Locator interface implementations should only be included in Locator.cpp, use interface instead."
#endif

/// The mind and the hand of a computer god in the game's own world
namespace openblack::ecs::systems::computer_world
{

/// The steps of the hand in the game's world
class GameHandWorld final: public computer_hand::World
{
public:
	GameHandWorld(PlayerNames player, components::ComputerPlayer& state);

	[[nodiscard]] std::optional<map_coords::MapCoords> CoordsOf(entt::entity thing) const override;
	[[nodiscard]] float HeightOf(entt::entity thing) const override;
	[[nodiscard]] float LandHeight(const map_coords::MapCoords& coords) const override;
	[[nodiscard]] bool IconHasSite(entt::entity icon) const override;
	[[nodiscard]] bool IconCharging(entt::entity icon) const override;
	bool StartChant(entt::entity icon, MagicType type) override;
	[[nodiscard]] float ChantNeeded(entt::entity icon) const override;
	[[nodiscard]] bool Angry(MagicType type) const override;
	bool Cast(MagicType type, entt::entity target, entt::entity icon, glm::vec3 hand) override;
	[[nodiscard]] float CastSeconds(MagicType type) const override;
	[[nodiscard]] uint32_t SeedGesture(MagicType type, int32_t stage) const override;
	[[nodiscard]] float GestureLength(uint32_t gesture) const override;
	[[nodiscard]] glm::vec3 GesturePoint(uint32_t gesture, float share) const override;
	void ShowGesture(uint32_t gesture, glm::vec3 corner, float size) override;
	[[nodiscard]] bool HandHasRoom() const override;
	void PutDownHeld(glm::vec3 hand) override;
	[[nodiscard]] bool CanPickUp(entt::entity thing) const override;
	void PickUp(entt::entity thing, glm::vec3 hand) override;
	void UseHeldOn(entt::entity target, glm::vec3 hand) override;

private:
	PlayerNames _player;
	components::ComputerPlayer& _state;
};

/// The rules of the god's needs in the game's world
class GameMindWorld final: public computer_mind::World
{
public:
	GameMindWorld(PlayerNames player, components::ComputerPlayer& state);

	float Intensity(computer_mind::Mind& mind, uint16_t node) override;
	bool Perform(computer_mind::Mind& mind, uint16_t node) override;
	[[nodiscard]] bool ActionBusy() const override;
	bool StepAction() override;
	void ClearAction() override;
	void StartAction() override;
	[[nodiscard]] bool ActionLocked() const override;
	void UnlockAction() override;
	bool ForceQueued() override;
	[[nodiscard]] bool HeldByHelp() const override;

private:
	/// The town that wants food or wood most is the node's object. How much it wants it.
	float TownWanting(computer_mind::Mind& mind, uint16_t node, TownDesireInfo desire);
	/// How much the god wants to cast the miracle on the store of the town above the node, or on the thing above it
	float ShallCast(computer_mind::Mind& mind, uint16_t node, MagicType type, ResourceType resource);
	/// The worship site's icon that the god wants to charge most is the node's object. How much it wants it.
	float ChargeUpSpell(computer_mind::Mind& mind, uint16_t node);
	/// The field of the god's towns that the god wants to take food from most
	float FoodFromField(computer_mind::Mind& mind, uint16_t node);
	/// The hand takes the thing that the chosen leaf found and drops it on the store of the town
	bool PickUpForTown(computer_mind::Mind& mind, uint16_t node);
	/// The god charges the miracle of the node's icon and casts it on the store of the town above
	bool CastFromIcon(computer_mind::Mind& mind, uint16_t node);
	/// The god's worshippers chant for the miracle of the node's icon
	bool PerformCharge(computer_mind::Mind& mind, uint16_t node);
	/// The hand goes to the icon, or makes the miracle's gesture when it is far, before the worshippers chant
	void ReachIcon(entt::entity icon, MagicType type);
	/// The share of the miracle's charge that the god can have, with its icon; the request counts once more
	float ChargeLevel(MagicType type, entt::entity& icon);
	/// How much the god wants a place: all inside its influence
	[[nodiscard]] float PlacePull(const map_coords::MapCoords& place) const;

	PlayerNames _player;
	components::ComputerPlayer& _state;
	GameHandWorld _hand;
};

/// Before the mind's turn: the god forgets its miracles that are gone, and stops an action whose things are gone
void Validate(components::ComputerPlayer& state);
/// The god reports where its hand and its camera are, as a player of a network game reports them. Only a god that has
/// hands reports.
void ReportHand(PlayerNames player, glm::vec3 hand, glm::vec3 camera);
/// Where the hand of the god starts on the land: at its temple
[[nodiscard]] std::optional<glm::vec3> StartingHand(PlayerNames player);

} // namespace openblack::ecs::systems::computer_world
