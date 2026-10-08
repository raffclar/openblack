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

#include <array>
#include <functional>
#include <random>
#include <vector>

#include <entt/entity/entity.hpp>
#include <glm/vec3.hpp>

namespace openblack
{

/// The effects of the creature's room, set up the first time the room is drawn in a visit and moved on each frame it
/// is drawn: the flames of its fire and the smoke over it, and the spray and mist at the foot of its waterfall. The
/// flames and smoke are sprites of the room (components::Sprite), the mist domes of mist.l3d
/// (components::MistDome), which the temple's pass draws while the room is drawn.
class CreatureCaveEffects
{
public:
	/// A smoke's particles each: they rise from the smoke's place over 900 steps of age, 255 a second
	static constexpr size_t k_SmokeParticles = 10;
	static constexpr int32_t k_SmokeLife = 900;

	/// A number between two bounds. The room's own draws come from an engine of its own, seeded the same every visit:
	/// the game's are not known, and they must not move the game's random streams
	using Random = std::function<float(float min, float max)>;

	/// A smoke's particles and how they move
	struct Smoke
	{
		struct Particle
		{
			glm::vec3 position {0.0f};
			glm::vec3 velocity {0.0f};
			int32_t age {0};
			float angle {0.0f};
			bool spinsBack {false};
			bool hidden {true};
			entt::entity sprite {entt::null};
		};
		glm::vec3 place {0.0f};
		/// Which way the particles drift as they rise
		glm::vec3 drift {0.0f, 1.0f, 0.0f};
		/// RGB of the particles, which fade by age
		glm::vec3 colour {1.0f};
		float size {1.0f};
		/// Rising smoke is thrown up from its place and falls back, fading in as it starts
		bool rising {false};
		/// What is left over of the steps of age the frames have given, which the next frame adds to
		float ageRemainder {0.0f};
		std::array<Particle, k_SmokeParticles> particles;
	};

	/// The flames on the fire, the place the room plays its sound at
	explicit CreatureCaveEffects(glm::vec3 fire);
	~CreatureCaveEffects();
	CreatureCaveEffects(const CreatureCaveEffects&) = delete;
	CreatureCaveEffects& operator=(const CreatureCaveEffects&) = delete;
	CreatureCaveEffects(CreatureCaveEffects&&) = delete;
	CreatureCaveEffects& operator=(CreatureCaveEffects&&) = delete;

	/// Whether the textures the effects are drawn with are loaded, without which the room has none
	[[nodiscard]] static bool CanBeMade();

	/// A frame of the room drawn, of milliseconds, and the time in milliseconds the flames take their frames from
	void Update(uint32_t milliseconds, uint32_t tickCount);

	/// A smoke's step of its particles over milliseconds, without drawing them; a rising particle starting over is
	/// thrown up by the random given
	static void StepSmoke(Smoke& smoke, uint32_t milliseconds, const Random& random);
	/// The alpha, out of 255, of a smoke particle of an age
	[[nodiscard]] static uint8_t SmokeAlpha(int32_t age, bool rising);
	/// The frame of the flames' animation of a flame at a time, 0 to 31
	[[nodiscard]] static uint32_t FlameFrame(uint32_t tickCount, uint32_t flame);

private:
	[[nodiscard]] float Draw(float min, float max);
	[[nodiscard]] Smoke CreateSmoke();
	void ShowSmoke(const Smoke& smoke) const;

	/// The room's own engine: every visit draws the same
	static constexpr std::mt19937::result_type k_Seed = 0x0C4EA7E5;
	std::mt19937 _engine {k_Seed};
	Random _random;

	std::vector<entt::entity> _flames;
	std::vector<Smoke> _fireSmoke;
	std::vector<Smoke> _spray;
	std::vector<entt::entity> _mists;
};

} // namespace openblack
