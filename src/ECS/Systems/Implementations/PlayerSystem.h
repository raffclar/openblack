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
#include <optional>
#include <string>
#include <unordered_map>

#include "Common/VirtualInfluence.h"
#include "ECS/Components/Alignment.h"
#include "ECS/Components/Player.h"
#include "ECS/StatisticsRules.h"
#include "ECS/Systems/PlayerSystemInterface.h"

#if !defined(LOCATOR_IMPLEMENTATIONS)
#error "ECS System implementations should only be included in Locator.cpp"
#endif

namespace openblack::ecs::systems
{
class PlayerSystem final: public PlayerSystemInterface
{
public:
	void RegisterPlayers() override;
	void AddPlayer(entt::entity playerEntity) override;
	[[nodiscard]] std::optional<entt::entity> FindPlayer(PlayerNames playerName) const override;
	void AddCreature(entt::entity creature) override;
	void MakePrimaryCreature(entt::entity creature) override;
	[[nodiscard]] std::optional<entt::entity> GetPrimaryCreature(PlayerNames name) const override;
	void KeepForNextLand() override;
	void ForgetKept() override;
	[[nodiscard]] bool KeepAlignmentAt(uint32_t turn) override;
	void TakeUpKept(entt::entity playerEntity) override;
	void SetAlly(PlayerNames player, PlayerNames other, float strength) override;
	[[nodiscard]] float AllyStrength(PlayerNames player, PlayerNames other) const override;

	[[nodiscard]] bool IsComputerPlayer(PlayerNames name) const override;

private:
	/// Remembers that the owner of the creature got it, after the creatures they got before or before all of them
	void Acquire(entt::entity creature, bool primary);
	/// What a player keeps from land to land
	struct Kept
	{
		std::optional<components::Alignment> alignment;
		std::array<float, 8> damageFrom {};
		uint32_t windResistance {0};
		components::Player::Miracles miracles;
		/// What their hand keeps of their influence past the border, and whether a script switched it off
		std::optional<virtual_influence::State> virtualInfluence;
		/// What the frames showed of it: the hum and the mana path
		virtual_influence::Shown virtualInfluenceShown;
		/// Their histories of influence and population, which run on over the whole game
		statistics::History influenceHistory;
		statistics::History populationHistory;
	};

	std::unordered_map<PlayerNames, entt::entity> _players;
	/// Each player's as of the last land they were on
	std::unordered_map<PlayerNames, Kept> _kept;
	/// The game turn the local player's alignment was last kept with their profile at
	uint32_t _alignmentKeptTurn {0};
};
} // namespace openblack::ecs::systems
