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

#include <array>
#include <optional>

#include <entt/entity/entity.hpp>
#include <entt/fwd.hpp>
#include <glm/vec3.hpp>

#include "3D/MapCoords.h"
#include "Enums.h"

namespace openblack::player_commands
{
struct HandReport;
} // namespace openblack::player_commands

namespace openblack::hand_trace
{
class HandTrace;
} // namespace openblack::hand_trace

namespace openblack::ecs::systems
{
class HandSystemInterface
{
public:
	enum class Side : uint8_t
	{
		Left,
		Right,
		_Count
	};
	virtual bool Initialize() noexcept = 0;
	/// After the load of a saved world: the system finds the hands among the loaded entities
	virtual void FindAfterLoad() {}
	[[nodiscard]] virtual std::array<entt::entity, static_cast<size_t>(Side::_Count)> GetPlayerHands() const noexcept = 0;
	/// Where the world has the player's hands: the left hand from the last report that a turn did, none before the first
	/// report or off the land. The frames draw the hand from its Transform, which the world never reads.
	[[nodiscard]] virtual std::array<std::optional<glm::vec3>, static_cast<size_t>(Side::_Count)>
	GetPlayerHandPositions() const noexcept = 0;
	/// Where the world has the hands of a player, as GetPlayerHandPositions gives them for the first player. A player with
	/// no hands has none.
	[[nodiscard]] virtual std::array<std::optional<glm::vec3>, static_cast<size_t>(Side::_Count)>
	GetHandPositionsOf(PlayerNames player) const noexcept
	{
		return player == PlayerNames::PLAYER_ONE ? GetPlayerHandPositions()
		                                         : std::array<std::optional<glm::vec3>, static_cast<size_t>(Side::_Count)> {};
	}
	/// Where the world has the hands that this computer moves and draws (see GetOwnHands). Only the frames, the input and
	/// the inspector of this computer read it.
	[[nodiscard]] virtual std::array<std::optional<glm::vec3>, static_cast<size_t>(Side::_Count)>
	GetOwnHandPositions() const noexcept
	{
		return GetPlayerHandPositions();
	}
	/// This computer's left hand where the frames draw it, none off the land. Only the frames and the input of this computer
	/// read it.
	[[nodiscard]] virtual std::optional<glm::vec3> GetDrawnHandPosition() const noexcept { return std::nullopt; }
	/// Where the world has a player's camera, and the point that it looks at
	struct ReportedCamera
	{
		glm::vec3 origin;
		glm::vec3 focus;
	};
	/// Where the world has the camera of a player: from the last report of the player that a turn did, none before the
	/// first report or for a player with no hands. The world never reads the camera that the frames draw from.
	[[nodiscard]] virtual std::optional<ReportedCamera> GetReportedCamera([[maybe_unused]] PlayerNames player) const noexcept
	{
		return std::nullopt;
	}
	/// The hands of a player: the first player's are the hands above. The god of another player of a network game has
	/// hands from the first turn, which the game server's command makes; none before.
	[[nodiscard]] virtual std::optional<std::array<entt::entity, static_cast<size_t>(Side::_Count)>>
	GetHandsOf(PlayerNames player) const noexcept
	{
		return player == PlayerNames::PLAYER_ONE ? std::optional(GetPlayerHands()) : std::nullopt;
	}
	/// At the turn: makes the hands of a player that has none, and gives the hands of the player
	virtual std::array<entt::entity, static_cast<size_t>(Side::_Count)> MakeHandsOf([[maybe_unused]] PlayerNames player)
	{
		return GetPlayerHands();
	}
	/// The player whose hand this computer moves with its pointer: the first player, unless a network game gave this
	/// computer another god. Only the frames and the input of this computer read it.
	virtual void SetOwnPlayer([[maybe_unused]] PlayerNames player) {}
	/// The game server and an observer have no player of their own: no tap or turn takes them into a state that only
	/// the player at a computer has, for example the temple
	virtual void ClearOwnPlayer() {}
	/// The own player, or the first player for the frames of a computer with no player of its own
	[[nodiscard]] virtual PlayerNames GetOwnPlayer() const noexcept { return PlayerNames::PLAYER_ONE; }
	/// True only for the player of this computer. Always false on a computer with no player of its own.
	[[nodiscard]] virtual bool IsOwnPlayer(PlayerNames player) const noexcept { return player == GetOwnPlayer(); }
	/// The hands that this computer moves and draws: the hands of its own player, or the first player's while that
	/// player has none yet
	[[nodiscard]] virtual std::array<entt::entity, static_cast<size_t>(Side::_Count)> GetOwnHands() const noexcept
	{
		return GetHandsOf(GetOwnPlayer()).value_or(GetPlayerHands());
	}
	/// The velocity of the player's hand in units a second, from the reports of the turns (see hand_stillness::Step)
	[[nodiscard]] virtual glm::vec3 GetReportedHandVelocity() const noexcept { return glm::vec3(0.0f); }
	/// At the turn: a player reports where the hand and the camera are (see player_commands::HandReport)
	virtual void Report([[maybe_unused]] PlayerNames player, [[maybe_unused]] const player_commands::HandReport& report) {}
	/// At the turn: a player reports the thing under the cursor (see player_commands::HandPick)
	virtual void ReportPick([[maybe_unused]] PlayerNames player, [[maybe_unused]] entt::entity object) {}
	/// The thing under the cursor of the player's hand from the last pick that a turn did. None before the first pick.
	[[nodiscard]] virtual entt::entity GetReportedPick() const noexcept { return entt::null; }
	/// Each frame the hands catch up with their player's alignment once it has moved far enough, and blend their skin
	/// again as they go into or out of the player's influence, tested at the point picked under the cursor (see
	/// hand_morph::Pick and NextPoint)
	virtual void UpdateAlignmentMorph(std::optional<map_coords::MapCoords> picked, bool holdPoint) = 0;
	/// Once a game turn, before the creatures think: counts the turns that the player's hand stays in one place
	virtual void ProcessTurn() {}
	/// The game turns that the player's hand has stayed in one place (see hand_stillness::Step)
	[[nodiscard]] virtual uint32_t GetHandStillTurns() const noexcept { return 0; }
	/// The debugging trace of the player's hand frame by frame (see hand_trace::HandTrace), off unless asked for; none
	/// where the hands keep no trace
	[[nodiscard]] virtual hand_trace::HandTrace* GetTrace() noexcept { return nullptr; }
};
} // namespace openblack::ecs::systems
