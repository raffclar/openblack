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
#include <optional>

namespace openblack::ecs::systems
{

/// The waterfall of the third land (see designed_waterfall). Only the frames make it: its model is a thing of the
/// drawn registry and its sound is a sound at a point, so it makes no change to the world.
class DesignedWaterfallSystemInterface
{
public:
	virtual ~DesignedWaterfallSystemInterface() = default;

	/// The land clears, for a new land or for a saved game: the sound of the waterfall goes with the things of the land.
	/// The model stays when the land after it has the same number. Then the waterfall stays silent, as in the game.
	virtual void Reset() = 0;
	/// Once a frame, with the number of the land and the frame's real and game time. On a frame that draws the land,
	/// the waterfall appears on the third land and goes from the other lands. Its water slides and its spray starts at
	/// the pace of the screen's refresh, by the game time, only on a frame that draws the land.
	virtual void UpdateFrame(int32_t landNumber, std::chrono::milliseconds frameRealTime,
	                         std::chrono::milliseconds frameGameTime, bool landDrawn) = 0;
	/// The model is on the land
	[[nodiscard]] virtual bool HasModel() const = 0;
	/// How far the water's texture has slid down the waterfall, from -1 to 0
	[[nodiscard]] virtual float GetSlide() const = 0;
	/// The number of the sound of the waterfall, if it has its sound (see SoundTagSystemInterface::KeepPointSound)
	[[nodiscard]] virtual std::optional<uint32_t> GetSound() const = 0;
};

} // namespace openblack::ecs::systems
