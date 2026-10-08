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

#include <source_location>

#include <glm/vec3.hpp>

/// The game's random streams, as the original keeps them: a synced one (the same on every machine of a net game), a
/// local one, the particle systems' choice between the two and the C runtime's rand(). The state lives in the
/// GameRandomInterface service (Locator::gameRandom); the free functions below draw from it.
/// Details: docs/bw1-notes/engine-math.md "Random numbers".
///
/// The arithmetic is float, one operation per statement: the game runs its FPU at single precision, so every multiply
/// and add rounds to a float, and one operation per statement keeps the compiler from fusing them (FMA).
namespace openblack::game_random
{
constexpr uint32_t k_InitialSeed = 0x88F89F;           ///< Both seeds at the start of a new campaign
constexpr uint32_t k_CrtInitialSeed = 1;               ///< The C runtime's seed before any srand
constexpr uint32_t k_FloatRandRange = 0xFFFF;          ///< The draw behind every float random
constexpr uint32_t k_FloatRandScaleBits = 0x37800080u; ///< About 1/65535, as the game's float
constexpr uint32_t k_CrtRandomScaleBits = 0x38000100u; ///< About 1/32767, as the game's float
/// k_FloatRandScaleBits as a float (bit_cast, never a decimal literal)
[[nodiscard]] float FloatRandScale() noexcept;
/// k_CrtRandomScaleBits as a float
[[nodiscard]] float CrtRandomScale() noexcept;

/// The game's generator: s = ror32(s * 9377 + 0x24DF, 13), stored rotated; returns s % n (unsigned).
/// n != 0: the original divides by zero (every caller tests it first)
uint32_t SeededRandom(uint32_t n, uint32_t& seed) noexcept;
/// A float draw on a given seed: x == 0 or NaN -> 0 without a draw, else (u x x) x scale, u = SeededRandom(0xFFFF)
float FloatRand(float x, uint32_t& seed) noexcept;

namespace psys
{
/// Whether a particle effect belongs to the synced game or only to this machine
enum class NetGameType : uint8_t
{
	Local = 0,
	Synced = 1
};
} // namespace psys
} // namespace openblack::game_random

namespace openblack
{
struct GameRandomSeeds
{
	uint32_t synced {0};
	uint32_t local {0};
};

/// The stream particle draws use: none outside an effect's step (every draw is 0), else the effect's
enum class ParticleRandomStream : uint8_t
{
	None,
	Local,
	Synced
};
} // namespace openblack

namespace openblack
{
/// The state behind game_random: both seeds, the particle stream and the C runtime seed
class GameRandomInterface
{
public:
	virtual ~GameRandomInterface() = default;

	/// Both seeds at k_InitialSeed (a new campaign)
	virtual void Init() noexcept = 0;
	/// Both seeds at 0 (every later land and the skirmish playground)
	virtual void Reset() noexcept = 0;
	virtual void Load(const GameRandomSeeds& seeds) noexcept = 0;
	[[nodiscard]] virtual const GameRandomSeeds& Current() const noexcept = 0;

	/// 0 for 0 without a draw, else SeededRandom(n) on the synced seed. `where` names the caller in the trace
	virtual uint32_t GameRand(uint32_t n, std::source_location where) noexcept = 0;
	/// 0 for +-0 / NaN, else (u x x) x scale on the synced seed: signed like x, |r| <= 65534/65535 |x|
	virtual float GameFloatRand(float x, std::source_location where) noexcept = 0;
	/// 0 for 0, else SeededRandom(n) on the local seed (a negative n divides as a huge unsigned)
	virtual uint32_t LocalRand(int32_t n) noexcept = 0;
	/// As GameFloatRand on the local seed
	virtual float LocalFloatRand(float x) noexcept = 0;

	[[nodiscard]] virtual ParticleRandomStream GetParticleStream() const noexcept = 0;
	virtual void SetParticleStream(ParticleRandomStream stream) noexcept = 0;
	/// Counts a particle draw made outside an effect's step
	virtual void CountOutsideStep(const std::source_location& where) noexcept = 0;
	[[nodiscard]] virtual uint32_t OutsideStepDraws() const noexcept = 0;

	/// The C runtime's rand(): s = s * 0x343FD + 0x269EC3, (s >> 16) & 0x7FFF, the seed starting at 1
	virtual int32_t CrtRand() noexcept = 0;
	virtual void CrtSrand(uint32_t seed) noexcept = 0;
	[[nodiscard]] virtual uint32_t CrtSeed() const noexcept = 0;
};
} // namespace openblack

namespace openblack::game_random
{
void Init() noexcept;
void Reset() noexcept;
/// The seeds a saved game keeps, synced first. (openblack has no saved games yet: no caller)
[[nodiscard]] GameRandomSeeds Save() noexcept;
void Load(const GameRandomSeeds& seeds) noexcept;
[[nodiscard]] const GameRandomSeeds& Current() noexcept;

uint32_t GameRand(uint32_t n, std::source_location where = std::source_location::current()) noexcept;
float GameFloatRand(float x, std::source_location where = std::source_location::current()) noexcept;
/// GameFloatRand(b - a) + a, b - a rounded to a float first
float GameFloatRange(float a, float b, std::source_location where = std::source_location::current()) noexcept;
uint32_t LocalRand(int32_t n) noexcept;
float LocalFloatRand(float x) noexcept;

namespace psys
{
/// An effect's step draws from its own stream; at the end every draw goes back to 0, whatever was set before (no
/// nesting)
class StepScope
{
public:
	explicit StepScope(NetGameType type) noexcept;
	~StepScope();
	StepScope(const StepScope&) = delete;
	StepScope& operator=(const StepScope&) = delete;
	StepScope(StepScope&&) = delete;
	StepScope& operator=(StepScope&&) = delete;
};
[[nodiscard]] ParticleRandomStream Active() noexcept;
/// None: 0; Synced: GameRand(0xFFFF), drawn even for x == 0; Local: LocalRand(0xFFFF); then (u x scale) x x, the other
/// order from GameFloatRand
float FloatRand(float x, std::source_location where = std::source_location::current()) noexcept;
/// FloatRand(b - a) + a, b - a rounded first
float FloatRand(float a, float b, std::source_location where = std::source_location::current()) noexcept;
/// None: 0; Synced: GameRand(n); Local: LocalRand(n)
int32_t Rand(int32_t n, std::source_location where = std::source_location::current()) noexcept;
/// x, y, z = FloatRand(2) - 1 in that order, again while (z^2 + y^2) + x^2 > 1. (approximate) outside a step the
/// original would never end and never gets there: (-1, -1, -1) once, with a warning
glm::vec3 RandR3(std::source_location where = std::source_location::current()) noexcept;
} // namespace psys

namespace crt
{
int32_t Rand() noexcept;
void Srand(uint32_t seed) noexcept;
/// The seed rand() goes on from, for Debug/StateHash.h
[[nodiscard]] uint32_t Seed() noexcept;
/// ((rand() x scale) x (b - a)) + a
float Random(float a, float b) noexcept;
} // namespace crt
} // namespace openblack::game_random
