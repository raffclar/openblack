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

#include <entt/entity/entity.hpp>

namespace openblack::ecs::systems
{

/// The apple barrel with the worm that the land draws beside the storage pit of the player at this computer that is
/// nearest to the temple of that player
class AppleBarrelSystemInterface
{
public:
	virtual ~AppleBarrelSystemInterface() = default;

	/// Once a frame: the barrel and its worm go to the pit that carries them (made when one first does, gone while none
	/// does), and the worm's clip plays on by the frame's game time. They are things of the drawn registry: each computer
	/// draws them for its own player, and the synced world does not know of them.
	virtual void Update(std::chrono::milliseconds frameGameTime) = 0;

	/// The barrel drawn now, a thing of the drawn registry; none while no pit carries it
	[[nodiscard]] virtual entt::entity GetBarrel() const = 0;

	/// The barrel goes with the land; its worm carries on in its clip where it was
	virtual void Reset() = 0;
	/// After the load of a saved world: the frames make the barrel again, and its worm carries on in its clip where it was
	virtual void FindAfterLoad() = 0;
};

} // namespace openblack::ecs::systems
