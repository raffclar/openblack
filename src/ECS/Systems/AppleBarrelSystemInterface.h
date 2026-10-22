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

/// The apple barrel with the worm that the land draws beside the local player's storage pit nearest their temple
class AppleBarrelSystemInterface
{
public:
	virtual ~AppleBarrelSystemInterface() = default;

	/// Once a frame: the barrel and its worm go to the pit that carries them (made when one first does, gone while none
	/// does), and the worm's clip plays on by the frame's game time
	virtual void Update(std::chrono::milliseconds frameGameTime) = 0;
	/// Once a turn: the barrel and its worm are made when a pit first carries them, and go while none does. Only the turn
	/// makes and removes these entities, so that every computer keeps the same entities in the world.
	virtual void ProcessTurn() = 0;

	/// The barrel drawn now, none while no pit carries it
	[[nodiscard]] virtual entt::entity GetBarrel() const = 0;

	/// The barrel goes with the land; its worm carries on in its clip where it was
	virtual void Reset() = 0;
	/// After the load of a saved world: the system finds the barrel among the loaded entities, and its worm's place
	virtual void FindAfterLoad() = 0;
};

} // namespace openblack::ecs::systems
