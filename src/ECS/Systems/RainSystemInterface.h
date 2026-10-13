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

#include <chrono>
#include <span>
#include <vector>

#include <glm/vec3.hpp>

#include "3D/Rain.h"

namespace openblack::ecs::systems
{

/// The rain the world shows where the weather rains: its streaks, how high they reach and how fast they fall
class RainSystemInterface
{
public:
	virtual ~RainSystemInterface() = default;

	/// New streaks, falling as in calm air
	virtual void Reset() = 0;
	/// Once a frame: the rain's fall follows the storm nearest the camera, and the streaks move on if they were drawn.
	/// The real time of the frame tells when the rain makes its next splashes, which it also makes while the game is
	/// paused.
	virtual void Update(float seconds, std::chrono::milliseconds frameRealTime, const glm::vec3& camera) = 0;
	/// The rain to draw this frame over the land blocks in view about the camera. When the splashes are due, the rain
	/// also makes its splashes on these blocks. Taking some counts the streaks as drawn.
	[[nodiscard]] virtual std::vector<rain::Tile> TakeTiles(const rain::View& view) = 0;
	/// How many splashes the rain made since the last reset; when the rings are full, the rain makes none
	[[nodiscard]] virtual uint64_t GetSplashCount() const = 0;
	[[nodiscard]] virtual std::span<const rain::Streak> GetStreaks() const = 0;
	/// How high the streaks reach over the ground
	[[nodiscard]] virtual float GetHeight() const = 0;
	/// How high and how fast the rain falls, which the snow falls as too
	[[nodiscard]] virtual rain::Fall GetFall() const = 0;
};

} // namespace openblack::ecs::systems
