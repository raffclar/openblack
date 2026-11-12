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
#include <map>
#include <optional>
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
	void AddForPlayer(liquid_particles::Interface interface, glm::vec3 position, glm::vec3 velocity, float size, uint32_t argb,
	                  liquid_particles::Kind kind) override;
	void SetPlayerPoint(liquid_particles::Interface interface, glm::vec3 point) override;
	void TurnPlayerSymbols(int32_t milliseconds, const glm::vec3& camera,
	                       const std::function<bool(liquid_particles::Interface)>& shows) override;
	[[nodiscard]] std::span<const liquid_particles::Particle>
	PlayerParticles(liquid_particles::Interface interface) const override;
	void UpdateFrame(float seconds, bool drawsDrops) override;
	void CollectDrawFrame(particles::draw::Frame& frame) const override;
	void Clear() override;
	void SaveState(reflection::BinaryWriter& out) const override;
	bool LoadState(reflection::BinaryReader& in) override;
	[[nodiscard]] std::span<const liquid_particles::Particle> Particles() const override { return _pool; }

private:
	/// The pool of another player, and the point that it is drawn at
	struct PlayerPool
	{
		std::vector<liquid_particles::Particle> particles;
		glm::vec3 point {0.0f};
		/// The angle of the symbol of the god over the particles, none when the symbol does not show
		std::optional<float> symbolAngle;
	};

	/// Adds a particle to a pool that has room for it
	void AddTo(std::vector<liquid_particles::Particle>& pool, glm::vec3 position, glm::vec3 velocity, float size, uint32_t argb,
	           liquid_particles::Kind kind);
	/// Adds the particles of a pool to the sprites of the frame, drawn in one piece at a point
	void CollectPool(particles::draw::Frame& frame, const std::vector<liquid_particles::Particle>& pool, glm::vec3 point) const;

	Sources _sources;
	std::vector<liquid_particles::Particle> _pool;
	/// The pools of the interfaces of the other players of a network game, in the order of their players and their
	/// observers
	std::map<liquid_particles::Interface, PlayerPool> _playerPools;
	/// The angle that all the symbols over the pools of the other players share
	float _symbolAngle {0.0f};
	frame_pace::Pace _drawPace;
	/// What the drops of liquid, the dust and smoke, and the motes are drawn as
	particles::Creator _liquidCreator;
	particles::Creator _puffCreator;
	particles::Creator _moteCreator;
};

} // namespace openblack::ecs::systems
