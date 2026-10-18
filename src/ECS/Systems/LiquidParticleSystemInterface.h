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

#include "Particles/LiquidParticles.h"

namespace openblack::particles::draw
{
struct Frame;
}

namespace openblack::ecs::systems
{

/// The one shared pool of liquid particles (rules in liquid_particles): drops of blood and sick, and the dust, smoke
/// and motes that landings, footsteps, blows and creatures make
class LiquidParticleSystemInterface
{
public:
	virtual ~LiquidParticleSystemInterface() = default;
	/// Adds a particle at a point with a speed, a full size, a colour (0xAARRGGBB) and a kind. The pool adds nothing when
	/// it is full.
	virtual void Add(glm::vec3 position, glm::vec3 velocity, float size, uint32_t argb, liquid_particles::Kind kind) = 0;
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
