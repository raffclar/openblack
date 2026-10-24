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
	/// This computer's left hand where the frames draw it, none off the land. Only the frames and the input of this computer
	/// read it.
	[[nodiscard]] virtual std::optional<glm::vec3> GetDrawnHandPosition() const noexcept { return std::nullopt; }
	/// Where the world has a player's camera, and the point that it looks at
	struct ReportedCamera
	{
		glm::vec3 origin;
		glm::vec3 focus;
	};
	/// Where the world has the player's camera: from the last report that a turn did, none before the first report. The
	/// world never reads the camera that the frames draw from.
	[[nodiscard]] virtual std::optional<ReportedCamera> GetReportedCamera() const noexcept { return std::nullopt; }
	/// At the turn: a player reports where the hand and the camera are (see player_commands::HandReport)
	virtual void Report([[maybe_unused]] PlayerNames player, [[maybe_unused]] const player_commands::HandReport& report) {}
	/// At the turn of a network game: a player reports the thing under the cursor (see player_commands::HandPick)
	virtual void ReportPick([[maybe_unused]] PlayerNames player, [[maybe_unused]] entt::entity object) {}
	/// The thing under the cursor of the player's hand from the last pick that a turn did. None in a single game.
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
