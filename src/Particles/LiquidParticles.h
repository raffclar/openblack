/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <cstddef>
#include <cstdint>

#include <bit>
#include <functional>
#include <vector>

#include <glm/vec3.hpp>

/// The rules of the liquid particles: one shared pool of small sprites from the blobs sheet. Drops of liquid (blood and
/// sick) fall, meet the land and slide on it. Dust, smoke and motes fly in straight lines, grow and shrink away.
namespace openblack::liquid_particles
{

/// The kinds of particle. Each kind has its own life, size, frames and draw state.
enum class Kind : uint8_t
{
	/// A drop of liquid. It falls until it meets the land, then slides on it, slowing. It lives three seconds from each
	/// of those starts. It shows one of eight frames at random each time the game draws it, keeps its size, and hides
	/// what is behind it.
	Liquid = 0,
	/// A light mote that lives two seconds and adds its colour to what is behind it
	Mote = 1,
	/// The same mote, but its frames play in the opposite order
	MoteReversed = 2,
	/// Smoke that lives two seconds
	Smoke = 3,
	/// Dust that a landing, a footstep or a fart throws up. It lives one second.
	Dust = 4,
};

/// One particle of the pool
struct Particle
{
	glm::vec3 position {0.0f};
	glm::vec3 velocity {0.0f};
	/// Its full size
	float size {0.0f};
	/// Seconds of game time it has lived
	float age {0.0f};
	/// Which of its sixteen frames it starts on. A drop of liquid keeps its frame of the last draw here.
	int32_t variant {0};
	/// 0xAARRGGBB
	uint32_t argb {0};
	Kind kind {Kind::Dust};
	/// A drop of liquid that has met the land and slides on it
	bool onLand {false};
};

/// The most particles there can be at one time. The pool makes no particle when it is full.
inline constexpr size_t k_MostParticles = 1024;
/// How long a drop of liquid, a puff of dust and any other particle live, in seconds of game time
inline constexpr float k_LiquidLife = 3.0f;
inline constexpr float k_DustLife = 1.0f;
inline constexpr float k_OtherLife = 2.0f;
/// A drop of liquid falls at this many units a second each second (the bits of the game's 9.81)
inline constexpr float k_Gravity = std::bit_cast<float>(0x411CF5C3u);
/// The speed of a sliding drop keeps a quarter of itself each second
inline constexpr float k_SlideKeptPerSecond = 0.25f;
/// As it meets the land, a drop takes a speed along x and along z of a draw of 0 to 99 times this part of its size,
/// less 30 of its size. The part is one bit less than 0.6.
inline constexpr float k_SlidePerDraw = std::bit_cast<float>(0x3F199999u);
inline constexpr float k_SlideLeast = 30.0f;
inline constexpr int32_t k_SlideDraws = 100;
/// The first of the eight frames a drop of liquid shows
inline constexpr int32_t k_LiquidFirstFrame = 32;
/// Dust and smoke grow over the first eighth of their life, then shrink away
inline constexpr float k_PuffGrowth = 0.125f;
/// A mote grows over its first quarter second, shrinks away over two seconds, and pulses once a second
inline constexpr float k_MoteGrowth = 0.25f;
inline constexpr float k_MotePulse = 0.33f;

/// Gives the next number of the C library's random numbers, as the game draws them
using CrtRandom = std::function<int32_t()>;
/// The height of the land at a point of the map, in 16.16 fixed point of the 10 metre cells
using AltitudeAt = std::function<float(int32_t x, int32_t z)>;

/// How long a particle of a kind lives, in seconds of game time
[[nodiscard]] constexpr float Life(Kind kind)
{
	switch (kind)
	{
	case Kind::Liquid:
		return k_LiquidLife;
	case Kind::Dust:
		return k_DustLife;
	default:
		return k_OtherLife;
	}
}

/// Whether a particle of a kind adds its colour to what is behind it, rather than blends over it
[[nodiscard]] constexpr bool IsAdditive(Kind kind)
{
	return kind == Kind::Mote || kind == Kind::MoteReversed;
}

/// The frame a new particle starts on. A drop of liquid starts on the first frame and draws no number. Any other kind
/// draws one random number.
[[nodiscard]] int32_t StartVariant(Kind kind, const CrtRandom& random);

/// A distance in metres as the land's fixed point: times 65536, times a tenth, cut towards zero
[[nodiscard]] int32_t ToMapUnits(float metres);

/// A particle after more seconds of game time. A drop of liquid asks the land under it for its height, and draws two
/// numbers for its slide (along x, then along z) as it meets the land. False when the particle has gone.
bool Step(Particle& particle, float seconds, const AltitudeAt& altitude, const CrtRandom& random);

/// Steps each particle of the pool in order. A particle that goes takes the last particle in its place, and that one
/// steps next.
void StepPool(std::vector<Particle>& pool, float seconds, const AltitudeAt& altitude, const CrtRandom& random);

/// The size that the game draws a particle at now
[[nodiscard]] float DrawnSize(const Particle& particle);

/// The frame of the blobs sheet that a particle shows now. Dust and smoke show one of the sixteen frames after the
/// first sixteen, changing twice a second. Motes show one of the first sixteen, changing eight times a second. A drop
/// of liquid shows its frame of the last draw.
[[nodiscard]] int32_t Frame(const Particle& particle);

/// The frame that a drop of liquid shows for a random number that the game draws for each draw of the drop
[[nodiscard]] constexpr int32_t LiquidFrame(int32_t random)
{
	return random & 7;
}

} // namespace openblack::liquid_particles
