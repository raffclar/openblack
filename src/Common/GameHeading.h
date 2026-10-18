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

#include <numbers>

#include "Common/GUtilsAngle.h"
#include "Common/X87Angle.h"

namespace openblack
{

/// The heading of a creature or another object, as the game keeps it.
///
/// The heading is a float angle in radians about the vertical axis. At 0 the object faces -z. At a quarter turn it faces
/// +x. The renderer turns the other way, so a yaw is the negative of the heading. Change between the two only where the
/// heading goes to the renderer or comes from it.
class GameHeading
{
public:
	constexpr GameHeading() = default;
	constexpr explicit GameHeading(float radians)
	    : _radians(radians)
	{
	}

	/// The heading of a direction on the land, as the game calculates and keeps it. It is 0 for a direction that is too
	/// short to turn to.
	[[nodiscard]] static GameHeading Of(float x, float z) { return GameHeading(x87::RoundToFloat(x87::GameHeadingOf(x, z))); }

	/// The heading of a yaw that the renderer turns a model by
	[[nodiscard]] static constexpr GameHeading FromYaw(float yaw) { return GameHeading(-yaw); }
	/// The yaw that the renderer turns a model by
	[[nodiscard]] constexpr float Yaw() const { return -_radians; }

	[[nodiscard]] constexpr float Radians() const { return _radians; }

	/// The game angle (2048 to the circle) of the heading
	[[nodiscard]] uint16_t GameAngle() const { return static_cast<uint16_t>(gutils::ConvertScawenAngleToGameAngle(_radians)); }

	/// An angle that the game brings back into the half turns each side of 0 with one full turn at most. An angle more
	/// than a half turn from 0 on one side moves one full turn to the other side. Other angles do not change.
	[[nodiscard]] static constexpr float Wrap(float angle)
	{
		constexpr float k_HalfTurn = std::numbers::pi_v<float>;
		constexpr float k_FullTurn = 2.0f * std::numbers::pi_v<float>;
		if (angle > k_HalfTurn)
		{
			return angle - k_FullTurn;
		}
		if (angle < -k_HalfTurn)
		{
			return angle + k_FullTurn;
		}
		return angle;
	}

	/// How far this heading turns from another one: this heading less the other, wrapped one time
	[[nodiscard]] constexpr float TurnFrom(GameHeading other) const { return Wrap(_radians - other._radians); }

	[[nodiscard]] constexpr bool operator==(const GameHeading&) const = default;

private:
	float _radians {0.0f};
};

} // namespace openblack
