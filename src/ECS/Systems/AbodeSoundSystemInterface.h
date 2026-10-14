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

namespace openblack::ecs::systems
{

/// The sounds that the buildings of the towns make by themselves (see abode_sounds)
class AbodeSoundSystemInterface
{
public:
	virtual ~AbodeSoundSystemInterface() = default;
	/// Once a frame, with the game time of the frame: each building that the game draws can make its sound
	virtual void Update(std::chrono::duration<float, std::milli> gameTime) = 0;
	/// The number of sounds that the buildings made since the game started, for the inspector
	[[nodiscard]] virtual uint32_t GetSoundCount() const = 0;
};

} // namespace openblack::ecs::systems
