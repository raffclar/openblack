/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <compare>
#include <cstdint>

#include <functional>
#include <span>

#include <glm/vec3.hpp>

#include "ECS/Systems/SavedStateInterface.h"
#include "Enums.h"
#include "Particles/LiquidParticles.h"

namespace openblack::particles::draw
{
struct Frame;
}

namespace openblack::liquid_particles
{
/// The interface of another player of a network game that has a pool of particles on this computer: the player, and
/// the number of the observer for each observer of the god of the observers. Each interface has its own pool.
struct Interface
{
	PlayerNames player {PlayerNames::PLAYER_ONE};
	/// The number of the observer, from 1. 0 for the player of a god.
	uint8_t observer {0};

	[[nodiscard]] auto operator<=>(const Interface&) const = default;
};
} // namespace openblack::liquid_particles

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
	/// Adds a particle to the pool of an interface of another player of a network game, which only shows the world on
	/// this computer. Each of those pools has its own limit, moves on with the shared pool, and is drawn in one piece at
	/// its point (see SetPlayerPoint). The world save keeps none of them.
	virtual void AddForPlayer([[maybe_unused]] liquid_particles::Interface interface, [[maybe_unused]] glm::vec3 position,
	                          [[maybe_unused]] glm::vec3 velocity, [[maybe_unused]] float size, [[maybe_unused]] uint32_t argb,
	                          [[maybe_unused]] liquid_particles::Kind kind)
	{
	}
	/// The point that the pool of an interface of another player is drawn at, for the order of the things that show what
	/// is behind them
	virtual void SetPlayerPoint([[maybe_unused]] liquid_particles::Interface interface, [[maybe_unused]] glm::vec3 point) {}
	/// Once a frame, before the draw: whether each pool of another player that has particles shows the symbol of its god
	/// over them (see player_sparkles::ShowsSymbol). Each symbol that shows turns the angle that all of them share by
	/// the milliseconds of the frame's game time (see player_sparkles::SymbolAngle).
	virtual void TurnPlayerSymbols([[maybe_unused]] int32_t milliseconds,
	                               [[maybe_unused]] const std::function<bool(liquid_particles::Interface)>& shows)
	{
	}
	/// The particles of the pool of an interface of another player
	[[nodiscard]] virtual std::span<const liquid_particles::Particle>
	PlayerParticles([[maybe_unused]] liquid_particles::Interface interface) const
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
