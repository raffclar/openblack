/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <functional>
#include <vector>

#include "Common/FramePace.h"
#include "ECS/Systems/LiquidParticleSystemInterface.h"
#include "Particles/ParticleEffect.h"

#if !defined(LOCATOR_IMPLEMENTATIONS)
#error "ECS System implementations should only be included in Locator.cpp"
#endif

namespace openblack::ecs::systems
{

class LiquidParticleSystem final: public LiquidParticleSystemInterface
{
public:
	/// What the pool reads from the game: the C library's random numbers, the height of the land, and whether a frame
	/// that draws the drops comes at the pace of the screen's refresh
	struct Sources
	{
		liquid_particles::CrtRandom random;
		liquid_particles::AltitudeAt altitude;
		std::function<bool(frame_pace::Pace&)> drawDue;
	};

	/// The pool with the game's random numbers, the land of the terrain system and the game's frame time
	LiquidParticleSystem();
	explicit LiquidParticleSystem(Sources sources);

	void Add(glm::vec3 position, glm::vec3 velocity, float size, uint32_t argb, liquid_particles::Kind kind) override;
	void UpdateFrame(float seconds, bool drawsDrops) override;
	void CollectDrawFrame(particles::draw::Frame& frame) const override;
	void Clear() override;
	[[nodiscard]] std::span<const liquid_particles::Particle> Particles() const override { return _pool; }

private:
	Sources _sources;
	std::vector<liquid_particles::Particle> _pool;
	frame_pace::Pace _drawPace;
	/// What the drops of liquid, the dust and smoke, and the motes are drawn as
	particles::Creator _liquidCreator;
	particles::Creator _puffCreator;
	particles::Creator _moteCreator;
};

} // namespace openblack::ecs::systems
