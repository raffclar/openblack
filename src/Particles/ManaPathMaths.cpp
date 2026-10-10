/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "ManaPathMaths.h"

#include <cmath>

#include <algorithm>
#include <numbers>

#include <glm/geometric.hpp>

#include "3D/MapCoords.h"

using namespace openblack::particles;

namespace
{
/// Each spark's weave is between 0.6 and 1.4 times the file's frequency and size
constexpr float k_DrawBase = 0.6f;
/// Where along the noise a spark starts, out of this many lattice steps
constexpr float k_PhaseRange = 256.0f;
/// The weave grows over the first fifth of the path and dies away over the last
constexpr float k_WeaveGrown = 0.2f;
constexpr float k_WeaveFading = 0.8f;
constexpr float k_WeaveRate = 5.0f;
/// The ground is read at the land's fixed point grid, sixty-five thousand steps to ten metres
constexpr float k_FixedPerCell = 65536.0f;
constexpr float k_CellsPerMetre = 0.1f;
} // namespace

mana_path::Path mana_path::MakePath(glm::vec3 from, glm::vec3 to, const Settings& settings, const Draws& draws)
{
	Path path {
	    .from = from,
	    .to = to,
	    .amplitude = (draws.amplitude + k_DrawBase) * settings.noiseAmplitude,
	    .frequency = (draws.frequency + k_DrawBase) * settings.noiseFrequency,
	    .phase = draws.phase * k_PhaseRange,
	};
	// Across the ground: the length and the way across, a quarter turn from the way along
	float dx = to.x - from.x;
	float dz = to.z - from.z;
	if (dx != 0.0f || dz != 0.0f)
	{
		path.length = std::sqrt(dx * dx + dz * dz);
		const float inverse = 1.0f / path.length;
		dx *= inverse;
		dz *= inverse;
	}
	path.side = {dz, 0.0f, -dx};
	// The curve leaves and arrives along the path, as long as the distance the steady speed covers in the time to travel
	auto tangent = to - from;
	if (tangent != glm::vec3(0.0f))
	{
		tangent *= settings.timeToTravel * settings.speed / glm::length(tangent);
	}
	path.cubic = from * 2.0f + to * -2.0f + tangent + tangent;
	path.square = from * -3.0f + to * 3.0f + tangent * -2.0f + tangent * -1.0f;
	path.linear = tangent;
	path.constant = from;
	return path;
}

float mana_path::Heading(const Path& path)
{
	const float across = path.length != 0.0f ? 1.0f / path.length : 0.0f;
	const float dx = (path.to.x - path.from.x) * across;
	const float dz = (path.to.z - path.from.z) * across;
	return std::atan2(dz, dx) + std::numbers::pi_v<float> / 2.0f;
}

float mana_path::Progress(const Path& path, const Settings& settings, float age)
{
	if (settings.constantSpeed)
	{
		return age * settings.speed / path.length;
	}
	return age / settings.timeToTravel;
}

float mana_path::WeaveShare(float along)
{
	if (along < k_WeaveGrown)
	{
		return along < 0.0f ? 0.0f : along * k_WeaveRate;
	}
	if (along <= k_WeaveFading)
	{
		return 1.0f;
	}
	if (along > 1.0f)
	{
		return 0.0f;
	}
	return 1.0f - (along - k_WeaveFading) * k_WeaveRate;
}

glm::vec3 mana_path::PointOnPath(const Path& path, const Settings& settings, float progress)
{
	if (settings.constantSpeed)
	{
		return path.from + (path.to - path.from) * std::clamp(progress, 0.0f, 1.0f);
	}
	const float t = progress;
	return path.cubic * (t * t * t) + path.square * (t * t) + path.linear * t + path.constant;
}

float mana_path::WeaveNoiseAt(const Path& path, float along)
{
	return path.frequency * path.length * along + path.phase;
}

glm::vec2 mana_path::Woven(const Path& path, glm::vec3 point, float along, float noise)
{
	const float offset = WeaveShare(along) * noise * path.amplitude;
	return {point.x + offset * path.side.x, point.z + offset * path.side.z};
}

glm::vec2 mana_path::GroundPoint(glm::vec2 xz)
{
	const auto fixed = [](float metres) {
		return map_coords::ToMetres(map_coords::FtoL(metres * k_FixedPerCell * k_CellsPerMetre));
	};
	return {fixed(xz.x), fixed(xz.y)};
}
