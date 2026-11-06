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

#include <optional>

#include "Help/TurnCheer.h"

namespace openblack::ecs::systems
{

/// The advisors cheer the player on as this computer's world camera turns or tilts, while the turning or the tilting
/// lesson asks for it (see help::turn_cheer). The system keeps the measures of the camera's turning and tilting speeds
/// and the cheering. The turns of the world
/// and the scripts do not read it, and the dedicated server does not make it.
class TurnCheerSystemInterface
{
public:
	virtual ~TurnCheerSystemInterface() = default;

	/// A script starts the cheering, which goes by the turning or the tilting speed. The measure of that speed starts
	/// again from still.
	virtual void Start(help::turn_cheer::Watch watch) = 0;
	/// A script stops the cheering
	virtual void Stop() = 0;
	/// Once a step of the camera: the measure takes in the angle that the player turned the camera by in the step, or
	/// slows towards still when the player did not turn it
	virtual void MeasureTurn(std::optional<float> turned, float seconds) = 0;
	/// Once a step of the camera, or once for each of its tilts in the step: the measure takes in the angle that the
	/// player tilted the camera by, or slows towards still when the player did not tilt it
	virtual void MeasureTilt(std::optional<float> tilted, float seconds) = 0;
	/// One game turn of cheering: the line that the advisors say, if any
	[[nodiscard]] virtual std::optional<int32_t> ProcessTurn(uint32_t gameTurn, const help::turn_cheer::Random& random) = 0;

	/// How fast the player turns the world camera, in radians a second
	[[nodiscard]] virtual float GetTurnSpeed() const = 0;
	/// How fast the player tilts the world camera, in radians a second
	[[nodiscard]] virtual float GetTiltSpeed() const = 0;
	[[nodiscard]] virtual const help::turn_cheer::Cheering& GetCheering() const = 0;
};

} // namespace openblack::ecs::systems
