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

#include <span>

#include <glm/vec3.hpp>

#include "ECS/Systems/SavedStateInterface.h"
#include "Enums.h"
#include "Particles/LiquidParticles.h"

namespace openblack::particles::draw
{
struct Frame;
}

namespace openblack::ecs::systems
{

/// The one shared pool of liquid particles (rules in liquid_particles): drops of blood and sick, and the dust, smoke
/// and motes that landings, footsteps, blows and creatures make
class LiquidParticleSystemInterface: public SavedStateInterface
{
public:
	virtual ~LiquidParticleSystemInterface() = default;
	/// Adds a particle at a point with a speed, a full size, a colour (0xAARRGGBB) and a kind. The pool adds nothing when
	/// it is full.
	virtual void Add(glm::vec3 position, glm::vec3 velocity, float size, uint32_t argb, liquid_particles::Kind kind) = 0;
	/// Adds a particle to the pool of another player of a network game, which only shows the world on this computer.
	/// Each of those pools has its own limit, moves on with the shared pool, and is drawn in one piece at its point (see
	/// SetPlayerPoint). The world save keeps none of them.
	virtual void AddForPlayer([[maybe_unused]] PlayerNames player, [[maybe_unused]] glm::vec3 position,
	                          [[maybe_unused]] glm::vec3 velocity, [[maybe_unused]] float size, [[maybe_unused]] uint32_t argb,
	                          [[maybe_unused]] liquid_particles::Kind kind)
	{
	}
	/// The point that the pool of another player is drawn at, for the order of the things that show what is behind them
	virtual void SetPlayerPoint([[maybe_unused]] PlayerNames player, [[maybe_unused]] glm::vec3 point) {}
	/// The particles of the pool of another player
	[[nodiscard]] virtual std::span<const liquid_particles::Particle> PlayerParticles([[maybe_unused]] PlayerNames player) const
	{
		return {};
	}
	/// Each frame, by the frame's game time: every particle moves on. Then, when the frame draws the drops, each drop of
	/// liquid takes a new random frame.
	virtual void UpdateFrame(float seconds, bool drawsDrops) = 0;
	/// Adds the particles to the sprites of the frame, in the order of the pool
	virtual void CollectDrawFrame(particles::draw::Frame& frame) const = 0;
	/// Removes all the particles
	virtual void Clear() = 0;
	[[nodiscard]] virtual std::span<const liquid_particles::Particle> Particles() const = 0;
};

} // namespace openblack::ecs::systems
