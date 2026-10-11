/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <chrono>
#include <span>

#include "3D/WaterRings.h"

namespace openblack::ecs::systems
{

/// The rings on the water where things splash (see water_rings)
class WaterRingSystemInterface
{
public:
	virtual ~WaterRingSystemInterface() = default;

	/// As a new land opens: no rings
	virtual void Reset() = 0;
	/// A new ring, unless there are as many as there can be; false then
	virtual bool Add(const water_rings::Ring& ring) = 0;
	/// Once a frame, with the frame's real and game time: the rings grow and fade with the game time, which stops while
	/// the game is paused, at the pace of water_rings::Step, and go
	virtual void Update(std::chrono::milliseconds frameRealTime, std::chrono::milliseconds frameGameTime) = 0;
	[[nodiscard]] virtual std::span<const water_rings::Ring> GetRings() const = 0;
};

} // namespace openblack::ecs::systems
