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

/// The rules that decide when the computer of a player sends the report of its hand and its camera. The report has two
/// parts: the hand part (the collide point and the hand) and the camera part (the camera and its focus). A part goes
/// only when it moved enough since the last report that sent it. Thus a camera that moves by very small values at each
/// frame sends no report, and the turns of two runs get the same commands.
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

} // namespace openblack::hand_report
