/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <array>
#include <chrono>
#include <vector>

#include <entt/entity/fwd.hpp>

#include "Common/VirtualInfluence.h"
#include "ECS/Systems/InfluenceSystemInterface.h"
#include "PlayerSlots.h"

#if !defined(LOCATOR_IMPLEMENTATIONS)
#error "ECS System implementations should only be included in Locator.cpp"
#endif

namespace openblack::ecs::systems
{

class InfluenceSystem final: public InfluenceSystemInterface
{
public:
	void Reset() override;
	void SaveState(reflection::BinaryWriter& out) const override;
	bool LoadState(reflection::BinaryReader& in) override;

	void ProcessTurn(uint32_t turn) override;
	void ProcessVirtualInfluence(PlayerNames player, uint32_t turn) override;
	void ProcessTownInfluence(entt::entity town) override;
	void MarkTownInfluence(entt::entity town) override;
	void BordersChanged() override { _bordersDirty = true; }
	void Update(std::chrono::duration<float, std::milli> gameTime) override;
	void ShowHandInfluence(std::chrono::duration<float, std::milli> gameTime) override;

	using InfluenceSystemInterface::PlayerInfluence;
	[[nodiscard]] float PlayerInfluence(PlayerNames player, const map_coords::MapCoords& position) const override;
	[[nodiscard]] float PlayerOwnInfluence(PlayerNames player, const map_coords::MapCoords& position) const override;
	[[nodiscard]] float HandPointInfluence(PlayerNames player, const map_coords::MapCoords& hand) const override;
	using InfluenceSystemInterface::PlayerRawInfluence;
	[[nodiscard]] float PlayerRawInfluence(PlayerNames player, const map_coords::MapCoords& position) const override;
	void HeldThingUsedOnLand(PlayerNames player) override;
	void SetInGameTurn(bool inGameTurn) override { _inGameTurn = inGameTurn; }
	void FixCitadelReaches() override;
	void LandLaidOut() override;

	[[nodiscard]] std::span<const influence::Circle> GetCircles() const override { return _circles; }
	[[nodiscard]] bool IsBorderShown(PlayerNames player) const override;
	void NoteInfluencePower(PlayerNames player) override;
	[[nodiscard]] float InfluencePower(PlayerNames player) const override;
	[[nodiscard]] float CitadelReachOf(PlayerNames player) const override;
	[[nodiscard]] glm::vec2 GetScrollOffset() const override { return influence::ScrollOffset(_scrollClock); }
	[[nodiscard]] std::span<const influence::Ripple> GetRipples() const override { return _ripples; }

private:
	/// A place for each player number of a network game with the most players
	static constexpr size_t k_Players = player_slots::k_PlayerNumbers;

	void ProcessCitadels();
	void DrawBorders();
	/// A reach that has moved since its border was drawn wants the border drawn again
	void NoteReach(float reach, float drawn);
	/// Whether the hand crossed a border shown since the last frame, sending out a ripple where it did
	bool CrossBorders(const glm::vec3& hand);
	/// Whether another player's shield keeps a player out of a place
	[[nodiscard]] static bool Shielded(PlayerNames player, const glm::vec3& point);
	[[nodiscard]] static bool Shielded(PlayerNames player, const map_coords::MapCoords& position);
	/// One turn of what a player's hand keeps past the border, with the hand where it is
	void ProcessVirtualInfluence(entt::entity playerEntity, PlayerNames player, glm::vec3 hand, uint32_t turn);
	/// The own influence of a player at a place, or where it is none or less, the influence that an ally lends there
	[[nodiscard]] float WithAllies(PlayerNames player, const map_coords::MapCoords& position, float own) const;
	/// Whether every player's influence covers the whole land now
	[[nodiscard]] static bool InfluenceEverywhere();
	/// What a player's hand keeps of their influence past the border, if they have a hand
	[[nodiscard]] static const virtual_influence::State* VirtualStateOf(PlayerNames player);

	std::vector<influence::Circle> _circles;
	std::array<bool, k_Players> _borderShown {};
	bool _bordersDirty {true};
	int32_t _scrollClock {0};
	float _scrollRemainder {0.0f};
	/// Newest first
	std::vector<influence::Ripple> _ripples;
	/// Whether the hand was inside each player's border at the last frame, once it has been somewhere, and where it was
	std::array<bool, k_Players> _handWasInside {};
	bool _handSeen {false};
	glm::vec3 _handBefore {0.0f};
	bool _inGameTurn {false};
};

} // namespace openblack::ecs::systems
