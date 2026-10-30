/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <cmath>
#include <cstdint>

#include <array>
#include <optional>

#include <glm/vec3.hpp>

#include "3D/MapCoords.h"
#include "Commands/PlayerCommands.h"
#include "Online/GameOrigin.h"

/// The rules that decide when the computer of a player sends the report of its hand and its camera. The report has two
/// parts: the hand part (the collide point and the hand) and the camera part (the camera and its focus). A part goes
/// only when it moved enough since the last report that sent it. Thus a camera that moves by very small values at each
/// frame sends no report, and the turns of two runs get the same commands. After a send, the report waits for some turns,
/// but a miracle that follows the hand gets a report at each turn.
namespace openblack::hand_report
{

/// A point moved when one of its axes moved by more than this value. The test compares the float difference with this
/// float as a double.
inline constexpr double k_MoveLimit = static_cast<double>(0.01f);

/// The parts of the report that go in a turn
enum class Parts : uint8_t
{
	/// Nothing moved enough: no report goes
	None,
	/// The hand part and the camera part
	All,
	/// The collide point and the hand only
	Hand,
	/// The camera and its focus only
	Camera,
};

/// The map cell across x and z that a point is in, as the 16-bit high word of each 16.16 coordinate
[[nodiscard]] constexpr std::array<uint16_t, 2> MapCell(glm::vec3 point)
{
	const auto cell = [](float metres) {
		return static_cast<uint16_t>(static_cast<uint32_t>(map_coords::ToFixed(metres)) >> 16U);
	};
	return {cell(point.x), cell(point.z)};
}

/// The collide point moved when it went into another map cell. Its height has no effect.
[[nodiscard]] constexpr bool CollidePointMoved(glm::vec3 last, glm::vec3 now)
{
	return MapCell(last) != MapCell(now);
}

/// A point moved when one of its axes moved by more than the limit. An axis that is not a number does not move.
[[nodiscard]] inline bool PointMoved(glm::vec3 last, glm::vec3 now)
{
	const auto moved = [](float a, float b) {
		const float difference = a - b;
		return static_cast<double>(std::fabs(difference)) > k_MoveLimit;
	};
	return moved(last.x, now.x) || moved(last.y, now.y) || moved(last.z, now.z);
}

/// The parts of the report that moved enough since the last report that sent each part
[[nodiscard]] inline Parts MovedParts(const player_commands::HandReport& last, const player_commands::HandReport& now)
{
	const bool hand = CollidePointMoved(last.collidePoint, now.collidePoint) || PointMoved(last.handPoint, now.handPoint);
	const bool camera = PointMoved(last.focus, now.focus) || PointMoved(last.camera, now.camera);
	if (hand && camera)
	{
		return Parts::All;
	}
	if (hand)
	{
		return Parts::Hand;
	}
	return camera ? Parts::Camera : Parts::None;
}

/// The report that the turns get, or none when nothing moved enough. A part that did not move enough keeps the values of
/// the last report, so that the turns read the values that the player last sent for that part.
[[nodiscard]] inline std::optional<player_commands::HandReport> ToSend(const player_commands::HandReport& last,
                                                                       const player_commands::HandReport& now)
{
	switch (MovedParts(last, now))
	{
	case Parts::All:
		return now;
	case Parts::Hand:
		return player_commands::HandReport {
		    .collidePoint = now.collidePoint,
		    .handPoint = now.handPoint,
		    .camera = last.camera,
		    .focus = last.focus,
		};
	case Parts::Camera:
		return player_commands::HandReport {
		    .collidePoint = last.collidePoint,
		    .handPoint = last.handPoint,
		    .camera = now.camera,
		    .focus = now.focus,
		};
	case Parts::None:
		break;
	}
	return std::nullopt;
}

/// The turns that the report waits after a send: 3 in a game of the internet lobby, else 1. A game alone has no network
/// game and waits 1 turn.
[[nodiscard]] constexpr uint8_t TurnsToWaitAfterSend(std::optional<online::GameOrigin> origin) noexcept
{
	return origin == online::GameOrigin::InternetLobby ? 3 : 1;
}

/// What the report rules read of a running miracle
struct ReportedSpell
{
	/// The miracle is cast in the hand and its seed is still linked to it
	bool castFromHand {false};
	/// The miracle closed down
	bool closedDown {false};
	/// A person casts it with the hand of a player
	bool humanCasting {false};
	/// The player of the miracle is the player of this computer
	bool ownPlayer {false};
	/// The miracle is a flock that made fewer animals than its total
	bool flockMakesAnimals {false};
};

/// The miracle needs a report at each turn, so that it follows the hand of the player of this computer at each turn:
/// - a flock that a person casts, while it makes fewer animals than its total;
/// - a miracle that a person casts in the hand, while its seed is linked to it and it did not close down.
[[nodiscard]] constexpr bool NeedsReportEachTurn(const ReportedSpell& spell) noexcept
{
	const bool ownHuman = spell.humanCasting && spell.ownPlayer;
	return ownHuman && (spell.flockMakesAnimals || (spell.castFromHand && !spell.closedDown));
}

/// The report waits in this turn: it sent in one of the last turns to wait, and no miracle needs a report at each turn
[[nodiscard]] constexpr bool WaitsThisTurn(uint8_t turnsToWait, bool spellNeedsReport) noexcept
{
	return turnsToWait != 0 && !spellNeedsReport;
}

/// The turns to wait after this turn:
/// - one turn less after a turn that waits;
/// - the full wait after a turn that sends;
/// - no change after a turn that tests and does not send.
[[nodiscard]] constexpr uint8_t TurnsToWaitAfter(uint8_t turnsToWait, bool waited, bool sent,
                                                 std::optional<online::GameOrigin> origin) noexcept
{
	if (waited)
	{
		return static_cast<uint8_t>(turnsToWait - 1);
	}
	return sent ? TurnsToWaitAfterSend(origin) : turnsToWait;
}

} // namespace openblack::hand_report
