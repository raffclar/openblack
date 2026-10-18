/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "LiquidParticles.h"

#include <numbers>

#include "3D/MapCoords.h"
#include "Common/X87.h"

namespace openblack::liquid_particles
{
namespace
{
/// The land's fixed point has 65536 units to a 10 metre cell
constexpr float k_FixedPerCell = 65536.0f;
constexpr float k_CellsPerMetre = std::bit_cast<float>(0x3DCCCCCDu);
/// The pulse of a mote turns through a full circle each second
constexpr float k_TwoPi = std::bit_cast<float>(0x40C90FDBu);
/// The frames of a mote change eight times a second, those of dust and smoke twice a second
constexpr float k_MoteFramesPerSecond = -8.0f;
constexpr float k_PuffFramesPerSecond = -2.0f;
constexpr int32_t k_FramesPerLook = 16;

/// The C remainder of a whole number by 16, which keeps the sign of the number
constexpr int32_t Remainder16(int32_t value)
{
	return value % k_FramesPerLook;
}

/// A drop of liquid falls until it meets the land, then slides on the land
void StepLiquid(Particle& drop, float seconds, const AltitudeAt& altitude, const CrtRandom& random)
{
	if (drop.onLand)
	{
		const float kept = x87::Pow(k_SlideKeptPerSecond, static_cast<double>(seconds));
		drop.velocity.x = kept * drop.velocity.x;
		drop.velocity.y = kept * drop.velocity.y;
		drop.velocity.z = kept * drop.velocity.z;
		drop.position += seconds * drop.velocity;
		drop.position.y = altitude(ToMapUnits(drop.position.x), ToMapUnits(drop.position.z));
		return;
	}
	drop.velocity.y = drop.velocity.y - (seconds * k_Gravity);
	drop.position += seconds * drop.velocity;
	const float land = altitude(ToMapUnits(drop.position.x), ToMapUnits(drop.position.z));
	if (drop.position.y > land)
	{
		return;
	}
	// The drop starts to slide, and lives its full time again from now
	const float perDraw = drop.size * k_SlidePerDraw;
	const float least = drop.size * k_SlideLeast;
	drop.velocity.x = (static_cast<float>(random() % k_SlideDraws) * perDraw) - least;
	const auto alongZ = random() % k_SlideDraws;
	drop.velocity.y = 0.0f;
	drop.age = 0.0f;
	drop.onLand = true;
	drop.position.y = land;
	drop.velocity.z = (static_cast<float>(alongZ) * perDraw) - least;
}
} // namespace

int32_t StartVariant(Kind kind, const CrtRandom& random)
{
	return kind == Kind::Liquid ? 0 : Remainder16(random());
}

int32_t ToMapUnits(float metres)
{
	return map_coords::FtoL((metres * k_FixedPerCell) * k_CellsPerMetre);
}

bool Step(Particle& particle, float seconds, const AltitudeAt& altitude, const CrtRandom& random)
{
	particle.age += seconds;
	// A particle goes when its age is more than its life
	if (particle.age > Life(particle.kind))
	{
		return false;
	}
	if (particle.kind == Kind::Liquid)
	{
		StepLiquid(particle, seconds, altitude, random);
		return true;
	}
	particle.position += seconds * particle.velocity;
	return true;
}

void StepPool(std::vector<Particle>& pool, float seconds, const AltitudeAt& altitude, const CrtRandom& random)
{
	size_t index = 0;
	while (index < pool.size())
	{
		if (Step(pool[index], seconds, altitude, random))
		{
			++index;
			continue;
		}
		pool[index] = pool.back();
		pool.pop_back();
	}
}

float DrawnSize(const Particle& particle)
{
	if (particle.kind == Kind::Liquid)
	{
		return particle.size;
	}
	const float age = particle.age;
	if (IsAdditive(particle.kind))
	{
		float fade = (k_OtherLife - age) * 0.5f;
		if (age < k_MoteGrowth)
		{
			fade = fade * (age * (1.0f / k_MoteGrowth));
		}
		const float pulse = x87::Add(x87::Sin(static_cast<double>(age * k_TwoPi)), 2.0);
		return ((pulse * k_MotePulse) * particle.size) * fade;
	}
	const float life = Life(particle.kind);
	float scale = (life - age) / life;
	const float growth = life * k_PuffGrowth;
	if (growth > age)
	{
		scale = scale * (age / growth);
	}
	return scale * particle.size;
}

int32_t Frame(const Particle& particle)
{
	switch (particle.kind)
	{
	case Kind::Liquid:
		return k_LiquidFirstFrame + particle.variant;
	case Kind::Mote:
		return Remainder16(particle.variant - map_coords::FtoL(particle.age * k_MoteFramesPerSecond));
	case Kind::MoteReversed:
		return (k_FramesPerLook - 1) - Remainder16(particle.variant - map_coords::FtoL(particle.age * k_MoteFramesPerSecond));
	default:
		return k_FramesPerLook + Remainder16(particle.variant - map_coords::FtoL(particle.age * k_PuffFramesPerSecond));
	}
}

} // namespace openblack::liquid_particles
