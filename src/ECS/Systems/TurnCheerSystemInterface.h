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

/// The advisors cheer the player on as this computer's world camera turns, while the turning lesson asks for it (see
/// help::turn_cheer). The system keeps the measure of the camera's turning speed and the cheering. The turns of the world
/// and the scripts do not read it, and the dedicated server does not make it.
class TurnCheerSystemInterface
{
public:
	virtual ~TurnCheerSystemInterface() = default;

	/// A script starts the cheering. The measure of the turning speed starts again from still.
	virtual void Start() = 0;
	/// A script stops the cheering
	virtual void Stop() = 0;
	/// Once a step of the camera: the measure takes in the angle that the player turned the camera by in the step, or
	/// slows towards still when the player did not turn it
	virtual void MeasureTurn(std::optional<float> turned, float seconds) = 0;
	/// One game turn of cheering: the line that the advisors say, if any
	[[nodiscard]] virtual std::optional<int32_t> ProcessTurn(uint32_t gameTurn, const help::turn_cheer::Random& random) = 0;

	/// How fast the player turns the world camera, in radians a second
	[[nodiscard]] virtual float GetTurnSpeed() const = 0;
	[[nodiscard]] virtual const help::turn_cheer::Cheering& GetCheering() const = 0;
};

} // namespace openblack::ecs::systems
