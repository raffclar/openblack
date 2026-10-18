/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// EXPERIMENT (not for merge): the rules of the volumetric clouds

#include "VolumetricClouds.h"

#include <cmath>
#include <cstdint>

#include <algorithm>
#include <array>
#include <functional>
#include <limits>
#include <numbers>
#include <numeric>
#include <thread>
#include <vector>

#include <glm/common.hpp>
#include <glm/exponential.hpp>
#include <glm/geometric.hpp>
#include <glm/trigonometric.hpp>

using namespace openblack::graphics;
using namespace openblack::graphics::volumetric_clouds;

namespace
{
/// The seeds of the noises, one for each so that they do not match
constexpr uint32_t k_BaseSeed = 17;
constexpr uint32_t k_DetailSeed = 101;
constexpr uint32_t k_WeatherSeed = 211;

/// The twelve directions of the gradients, to the middles of a cube's edges
constexpr std::array<glm::vec3, 12> k_Gradients {{
    {1.0f, 1.0f, 0.0f},
    {-1.0f, 1.0f, 0.0f},
    {1.0f, -1.0f, 0.0f},
    {-1.0f, -1.0f, 0.0f},
    {1.0f, 0.0f, 1.0f},
    {-1.0f, 0.0f, 1.0f},
    {1.0f, 0.0f, -1.0f},
    {-1.0f, 0.0f, -1.0f},
    {0.0f, 1.0f, 1.0f},
    {0.0f, -1.0f, 1.0f},
    {0.0f, 1.0f, -1.0f},
    {0.0f, -1.0f, -1.0f},
}};

uint32_t HashBits(glm::ivec3 cell, uint32_t seed)
{
	auto h = seed * 0x9E3779B9u;
	h ^= static_cast<uint32_t>(cell.x) * 0x85EBCA6Bu;
	h = (h << 13u) | (h >> 19u);
	h ^= static_cast<uint32_t>(cell.y) * 0xC2B2AE35u;
	h = (h << 13u) | (h >> 19u);
	h ^= static_cast<uint32_t>(cell.z) * 0x27D4EB2Fu;
	// The finish of MurmurHash3, which spreads every bit to every other
	h ^= h >> 16u;
	h *= 0x85EBCA6Bu;
	h ^= h >> 13u;
	h *= 0xC2B2AE35u;
	h ^= h >> 16u;
	return h;
}

int32_t Wrap(int32_t value, int32_t period)
{
	const int32_t r = value % period;
	return r < 0 ? r + period : r;
}

float Fade(float t)
{
	return t * t * t * (t * (t * 6.0f - 15.0f) + 10.0f);
}

float Smoothstep(float low, float high, float x)
{
	const float t = std::clamp((x - low) / (high - low), 0.0f, 1.0f);
	return t * t * (3.0f - 2.0f * t);
}

/// The feature points of a cellular noise, one in each cell, made once for many lookups
class FeatureGrid
{
public:
	FeatureGrid(int32_t cells, uint32_t seed)
	    : _cells(cells)
	{
		_points.resize(static_cast<size_t>(cells) * static_cast<size_t>(cells) * static_cast<size_t>(cells));
		for (int32_t z = 0; z < cells; ++z)
		{
			for (int32_t y = 0; y < cells; ++y)
			{
				for (int32_t x = 0; x < cells; ++x)
				{
					const glm::ivec3 cell {x, y, z};
					_points.at(Index(cell)) = {Hash(cell, seed), Hash(cell, seed + 1), Hash(cell, seed + 2)};
				}
			}
		}
	}

	/// 1 at a feature point, 0 at the distance of a cell or more, for a point from 0 to 1 along each axis
	[[nodiscard]] float At(glm::vec3 point) const
	{
		const auto scaled = point * static_cast<float>(_cells);
		const glm::ivec3 base {glm::floor(scaled)};
		float nearest = std::numeric_limits<float>::max();
		for (int32_t dz = -1; dz <= 1; ++dz)
		{
			for (int32_t dy = -1; dy <= 1; ++dy)
			{
				for (int32_t dx = -1; dx <= 1; ++dx)
				{
					const auto cell = base + glm::ivec3 {dx, dy, dz};
					const glm::ivec3 wrapped {Wrap(cell.x, _cells), Wrap(cell.y, _cells), Wrap(cell.z, _cells)};
					const auto feature = glm::vec3(cell) + _points[Index(wrapped)];
					const auto offset = feature - scaled;
					nearest = std::min(nearest, glm::dot(offset, offset));
				}
			}
		}
		return 1.0f - std::clamp(std::sqrt(nearest), 0.0f, 1.0f);
	}

private:
	[[nodiscard]] size_t Index(glm::ivec3 cell) const
	{
		return (static_cast<size_t>(cell.z) * static_cast<size_t>(_cells) + static_cast<size_t>(cell.y)) *
		           static_cast<size_t>(_cells) +
		       static_cast<size_t>(cell.x);
	}

	int32_t _cells;
	std::vector<glm::vec3> _points;
};

float Octaves(float a, float b, float c)
{
	return a * 0.625f + b * 0.25f + c * 0.125f;
}

uint8_t ToByte(float value)
{
	return static_cast<uint8_t>(std::clamp(value, 0.0f, 1.0f) * 255.0f + 0.5f);
}

/// Gradient noise in octaves from a period, 0 to 1
float PerlinOctaves(glm::vec3 point, int32_t period, int32_t octaves, uint32_t seed)
{
	float sum = 0.0f;
	float weight = 1.0f;
	float total = 0.0f;
	for (int32_t octave = 0; octave < octaves; ++octave)
	{
		const int32_t p = period << octave;
		sum += Perlin(point * static_cast<float>(p), p, seed + static_cast<uint32_t>(octave)) * weight;
		total += weight;
		weight *= 0.5f;
	}
	return std::clamp(sum / total * 0.75f + 0.5f, 0.0f, 1.0f);
}

/// Calls `slice` for each z from 0 to `count`, on as many threads as the machine has
void ForEachSlice(uint32_t count, const std::function<void(uint32_t)>& slice)
{
	const uint32_t threads = std::clamp(std::thread::hardware_concurrency(), 1u, 32u);
	std::vector<std::jthread> workers;
	workers.reserve(threads);
	for (uint32_t t = 0; t < threads; ++t)
	{
		workers.emplace_back([t, threads, count, &slice] {
			for (uint32_t z = t; z < count; z += threads)
			{
				slice(z);
			}
		});
	}
}

/// Each value of a channel replaced by its rank among all of them, 0 to 1, so that the values spread evenly. Then a
/// threshold at 1 - c keeps a share c of the texels.
void Equalise(std::vector<float>& values)
{
	std::vector<uint32_t> order(values.size());
	std::iota(order.begin(), order.end(), 0u);
	std::ranges::sort(order, [&values](uint32_t a, uint32_t b) { return values[a] < values[b]; });
	const auto last = static_cast<float>(std::max<size_t>(values.size() - 1, 1));
	for (size_t rank = 0; rank < order.size(); ++rank)
	{
		values[order[rank]] = static_cast<float>(rank) / last;
	}
}
} // namespace

float volumetric_clouds::Hash(glm::ivec3 cell, uint32_t seed)
{
	return static_cast<float>(HashBits(cell, seed) >> 8u) * (1.0f / 16777216.0f);
}

float volumetric_clouds::Perlin(glm::vec3 point, int32_t period, uint32_t seed)
{
	const glm::ivec3 base {glm::floor(point)};
	const auto f = point - glm::vec3(base);
	const auto gradient = [&](glm::ivec3 corner) {
		const auto cell = base + corner;
		const glm::ivec3 wrapped {Wrap(cell.x, period), Wrap(cell.y, period), Wrap(cell.z, period)};
		const auto& g = k_Gradients.at(HashBits(wrapped, seed) % k_Gradients.size());
		return glm::dot(g, f - glm::vec3(corner));
	};
	const glm::vec3 u {Fade(f.x), Fade(f.y), Fade(f.z)};
	const float x00 = glm::mix(gradient({0, 0, 0}), gradient({1, 0, 0}), u.x);
	const float x10 = glm::mix(gradient({0, 1, 0}), gradient({1, 1, 0}), u.x);
	const float x01 = glm::mix(gradient({0, 0, 1}), gradient({1, 0, 1}), u.x);
	const float x11 = glm::mix(gradient({0, 1, 1}), gradient({1, 1, 1}), u.x);
	return glm::mix(glm::mix(x00, x10, u.y), glm::mix(x01, x11, u.y), u.z);
}

float volumetric_clouds::Worley(glm::vec3 point, int32_t cells, uint32_t seed)
{
	const auto scaled = (point - glm::floor(point)) * static_cast<float>(cells);
	const glm::ivec3 base {glm::floor(scaled)};
	float nearest = std::numeric_limits<float>::max();
	for (int32_t dz = -1; dz <= 1; ++dz)
	{
		for (int32_t dy = -1; dy <= 1; ++dy)
		{
			for (int32_t dx = -1; dx <= 1; ++dx)
			{
				const auto cell = base + glm::ivec3 {dx, dy, dz};
				const glm::ivec3 wrapped {Wrap(cell.x, cells), Wrap(cell.y, cells), Wrap(cell.z, cells)};
				const glm::vec3 jitter {Hash(wrapped, seed), Hash(wrapped, seed + 1), Hash(wrapped, seed + 2)};
				const auto offset = glm::vec3(cell) + jitter - scaled;
				nearest = std::min(nearest, glm::dot(offset, offset));
			}
		}
	}
	return 1.0f - std::clamp(std::sqrt(nearest), 0.0f, 1.0f);
}

float volumetric_clouds::WorleyOctaves(glm::vec3 point, int32_t cells, uint32_t seed)
{
	return Octaves(Worley(point, cells, seed), Worley(point, cells * 2, seed), Worley(point, cells * 4, seed));
}

float volumetric_clouds::Remap(float value, float fromLow, float fromHigh, float toLow, float toHigh)
{
	return toLow + (value - fromLow) / (fromHigh - fromLow) * (toHigh - toLow);
}

std::vector<uint8_t> volumetric_clouds::BaseNoiseVolume(uint32_t side)
{
	std::vector<uint8_t> texels(static_cast<size_t>(side) * side * side * 4);
	// The cellular noise at five frequencies, each from its own grid of feature points
	const std::array<FeatureGrid, 5> grids {FeatureGrid(4, k_BaseSeed), FeatureGrid(8, k_BaseSeed), FeatureGrid(16, k_BaseSeed),
	                                        FeatureGrid(32, k_BaseSeed), FeatureGrid(64, k_BaseSeed)};
	ForEachSlice(side, [&](uint32_t z) {
		for (uint32_t y = 0; y < side; ++y)
		{
			for (uint32_t x = 0; x < side; ++x)
			{
				const auto p = (glm::vec3(x, y, z) + 0.5f) / static_cast<float>(side);
				std::array<float, 5> w {};
				std::ranges::transform(grids, w.begin(), [&p](const FeatureGrid& grid) { return grid.At(p); });
				const float low = Octaves(w[0], w[1], w[2]);
				// Perlin-Worley: the gradient noise's billows, with the round shapes of the cellular noise
				const float perlin = PerlinOctaves(p, 4, 4, k_BaseSeed);
				const float perlinWorley = std::clamp(Remap(perlin, low - 1.0f, 1.0f, 0.0f, 1.0f), 0.0f, 1.0f);
				const size_t at = ((static_cast<size_t>(z) * side + y) * side + x) * 4;
				texels[at + 0] = ToByte(perlinWorley);
				texels[at + 1] = ToByte(low);
				texels[at + 2] = ToByte(Octaves(w[1], w[2], w[3]));
				texels[at + 3] = ToByte(Octaves(w[2], w[3], w[4]));
			}
		}
	});
	return texels;
}

std::vector<uint8_t> volumetric_clouds::DetailNoiseVolume(uint32_t side)
{
	std::vector<uint8_t> texels(static_cast<size_t>(side) * side * side * 4);
	const std::array<FeatureGrid, 5> grids {FeatureGrid(2, k_DetailSeed), FeatureGrid(4, k_DetailSeed),
	                                        FeatureGrid(8, k_DetailSeed), FeatureGrid(16, k_DetailSeed),
	                                        FeatureGrid(32, k_DetailSeed)};
	ForEachSlice(side, [&](uint32_t z) {
		for (uint32_t y = 0; y < side; ++y)
		{
			for (uint32_t x = 0; x < side; ++x)
			{
				const auto p = (glm::vec3(x, y, z) + 0.5f) / static_cast<float>(side);
				std::array<float, 5> w {};
				std::ranges::transform(grids, w.begin(), [&p](const FeatureGrid& grid) { return grid.At(p); });
				const size_t at = ((static_cast<size_t>(z) * side + y) * side + x) * 4;
				texels[at + 0] = ToByte(Octaves(w[0], w[1], w[2]));
				texels[at + 1] = ToByte(Octaves(w[1], w[2], w[3]));
				texels[at + 2] = ToByte(Octaves(w[2], w[3], w[4]));
				texels[at + 3] = 255;
			}
		}
	});
	return texels;
}

std::vector<uint8_t> volumetric_clouds::WeatherNoise(uint32_t side)
{
	const size_t count = static_cast<size_t>(side) * side;
	std::vector<float> cover(count);
	std::vector<float> type(count);
	std::vector<float> change(count);
	std::vector<float> base(count);
	const FeatureGrid cells(6, k_WeatherSeed);
	const FeatureGrid fine(12, k_WeatherSeed);
	const FeatureGrid changeCells(5, k_WeatherSeed + 7);
	ForEachSlice(side, [&](uint32_t y) {
		for (uint32_t x = 0; x < side; ++x)
		{
			// A slice through the middle of the volumes: it tiles across x and y as the volumes do
			const glm::vec3 p {(static_cast<float>(x) + 0.5f) / static_cast<float>(side),
			                   (static_cast<float>(y) + 0.5f) / static_cast<float>(side), 0.5f};
			const size_t at = static_cast<size_t>(y) * side + x;
			// Groups of round cells over a billowing field, so that the clouds come in groups with sky between them
			const float billow = PerlinOctaves(p, 3, 4, k_WeatherSeed);
			const float round = cells.At(p) * 0.65f + fine.At(p) * 0.35f;
			cover[at] = billow * 0.55f + round * 0.45f;
			type[at] = PerlinOctaves(p, 2, 3, k_WeatherSeed + 3);
			change[at] = PerlinOctaves(p, 4, 3, k_WeatherSeed + 5) * 0.6f + changeCells.At(p) * 0.4f;
			base[at] = PerlinOctaves(p, 16, 2, k_WeatherSeed + 9);
		}
	});
	Equalise(cover);
	Equalise(change);
	std::vector<uint8_t> texels(count * 4);
	for (size_t i = 0; i < count; ++i)
	{
		texels[i * 4 + 0] = ToByte(cover[i]);
		texels[i * 4 + 1] = ToByte(type[i]);
		texels[i * 4 + 2] = ToByte(change[i]);
		texels[i * 4 + 3] = ToByte(base[i]);
	}
	return texels;
}

std::vector<uint8_t> volumetric_clouds::BlueNoise(uint32_t side)
{
	const int32_t n = static_cast<int32_t>(side);
	const size_t count = static_cast<size_t>(side) * side;
	// The energy that a point gives to the texels round it, over a torus
	constexpr float k_Sigma = 1.5f;
	std::vector<float> spread(count);
	for (int32_t y = 0; y < n; ++y)
	{
		for (int32_t x = 0; x < n; ++x)
		{
			const auto dx = static_cast<float>(std::min(x, n - x));
			const auto dy = static_cast<float>(std::min(y, n - y));
			spread[static_cast<size_t>(y * n + x)] = std::exp(-(dx * dx + dy * dy) / (2.0f * k_Sigma * k_Sigma));
		}
	}
	std::vector<float> energy(count, 0.0f);
	std::vector<bool> on(count, false);
	const auto toggle = [&](size_t at, bool value) {
		on[at] = value;
		const int32_t ax = static_cast<int32_t>(at) % n;
		const int32_t ay = static_cast<int32_t>(at) / n;
		const float sign = value ? 1.0f : -1.0f;
		for (int32_t y = 0; y < n; ++y)
		{
			for (int32_t x = 0; x < n; ++x)
			{
				const auto from = static_cast<size_t>(Wrap(y - ay, n) * n + Wrap(x - ax, n));
				energy[static_cast<size_t>(y * n + x)] += sign * spread[from];
			}
		}
	};
	const auto tightest = [&](bool value) {
		size_t best = 0;
		float bestEnergy = value ? -std::numeric_limits<float>::max() : std::numeric_limits<float>::max();
		for (size_t i = 0; i < count; ++i)
		{
			if (on[i] == value && (value ? energy[i] > bestEnergy : energy[i] < bestEnergy))
			{
				best = i;
				bestEnergy = energy[i];
			}
		}
		return best;
	};
	// A first pattern of a tenth of the texels, from a fixed sequence of numbers
	uint32_t random = 12345u;
	const size_t first = count / 10;
	for (size_t placed = 0; placed < first;)
	{
		random = random * 1664525u + 1013904223u;
		const size_t at = (random >> 8u) % count;
		if (!on[at])
		{
			toggle(at, true);
			++placed;
		}
	}
	// The points move from the tightest cluster to the largest void until the pattern is even
	for (size_t guard = 0; guard < count; ++guard)
	{
		const auto cluster = tightest(true);
		toggle(cluster, false);
		const auto gap = tightest(false);
		toggle(gap, true);
		if (gap == cluster)
		{
			break;
		}
	}
	std::vector<uint32_t> rank(count, 0);
	const auto start = on;
	const auto startEnergy = energy;
	// The ranks of the first points: the tightest cluster goes first, so it takes the highest rank of them
	for (size_t r = first; r-- > 0;)
	{
		const auto cluster = tightest(true);
		toggle(cluster, false);
		rank[cluster] = static_cast<uint32_t>(r);
	}
	on = start;
	energy = startEnergy;
	// The ranks of the rest: each fills the largest void that is left
	for (size_t r = first; r < count; ++r)
	{
		const auto gap = tightest(false);
		toggle(gap, true);
		rank[gap] = static_cast<uint32_t>(r);
	}
	std::vector<uint8_t> texels(count);
	for (size_t i = 0; i < count; ++i)
	{
		texels[i] = static_cast<uint8_t>(static_cast<size_t>(rank[i]) * 256 / count);
	}
	return texels;
}

float volumetric_clouds::LayerDepth(float type)
{
	return type < k_Cumulus ? glm::mix(k_StratusDepth, k_CumulusDepth, type * 2.0f)
	                        : glm::mix(k_CumulusDepth, k_CumulonimbusDepth, (type - k_Cumulus) * 2.0f);
}

float volumetric_clouds::HeightProfile(float type, float heightFraction)
{
	if (heightFraction <= 0.0f || heightFraction >= 1.0f)
	{
		return 0.0f;
	}
	const float h = heightFraction;
	// A flat sheet, a round tower with a flat bottom, and a tall tower that stays wide to its top
	const float stratus = Smoothstep(0.0f, 0.08f, h) * (1.0f - Smoothstep(0.3f, 0.9f, h));
	const float cumulus = Smoothstep(0.0f, 0.12f, h) * (1.0f - Smoothstep(0.45f, 1.0f, h));
	const float cumulonimbus = Smoothstep(0.0f, 0.06f, h) * (1.0f - Smoothstep(0.82f, 1.0f, h));
	const float t = std::clamp(type, 0.0f, 1.0f);
	return t < k_Cumulus ? glm::mix(stratus, cumulus, t * 2.0f) : glm::mix(cumulus, cumulonimbus, (t - k_Cumulus) * 2.0f);
}

float volumetric_clouds::HenyeyGreenstein(float cosAngle, float g)
{
	const float g2 = g * g;
	const float denominator = 1.0f + g2 - 2.0f * g * cosAngle;
	return (1.0f - g2) / (4.0f * std::numbers::pi_v<float> * denominator * std::sqrt(denominator));
}

float volumetric_clouds::DualLobePhase(float cosAngle)
{
	return glm::mix(HenyeyGreenstein(cosAngle, 0.8f), HenyeyGreenstein(cosAngle, -0.35f), 0.35f);
}

float volumetric_clouds::BeerPowder(float opticalDepth, float localDepth, float cosAngle)
{
	const float beer = std::exp(-opticalDepth);
	const float powder = 1.0f - std::exp(-2.0f * localDepth);
	// None of the powder towards the light, where the edges glow, and the most of it away from the light
	const float share = 0.8f * (0.5f - 0.5f * cosAngle);
	return beer * glm::mix(1.0f, powder, share);
}

float volumetric_clouds::MultipleScattering(float opticalDepth, float cosAngle)
{
	float sum = 0.0f;
	float extinction = 1.0f;
	float energy = 1.0f;
	float asymmetry = 1.0f;
	for (int octave = 0; octave < 3; ++octave)
	{
		const float phase =
		    glm::mix(HenyeyGreenstein(cosAngle, 0.8f * asymmetry), HenyeyGreenstein(cosAngle, -0.35f * asymmetry), 0.35f);
		sum += energy * std::exp(-opticalDepth * extinction) * phase;
		extinction *= 0.5f;
		energy *= 0.5f;
		asymmetry *= 0.5f;
	}
	return sum;
}

glm::vec2 volumetric_clouds::EaseTwoStage(glm::vec2 stages, float target, float seconds, float riseSeconds, float fallSeconds)
{
	if (seconds <= 0.0f)
	{
		return stages;
	}
	const auto ease = [seconds, riseSeconds, fallSeconds](float value, float towards) {
		const float constant = towards > value ? riseSeconds : fallSeconds;
		return value + (towards - value) * (1.0f - std::exp(-seconds / constant));
	};
	const float first = ease(stages.x, target);
	return {first, ease(stages.y, first)};
}

void CloudClimate::Reset(const CloudWeather& weather)
{
	_cover = glm::vec2(weather.cover);
	_type = glm::vec2(weather.type);
	_rain = glm::vec2(weather.rain);
	_wind = weather.wind;
	_choice = weather.choice;
	_storms.clear();
}

void CloudClimate::Step(float seconds, const CloudWeather& target, std::span<const StormInput> storms)
{
	if (seconds <= 0.0f)
	{
		return;
	}
	// A new choice of cover is put at once, with the storms as they are now, so that the sky shows the choice at once
	const bool snap = target.choice != _choice;
	_choice = target.choice;
	const auto ease = [seconds, snap](glm::vec2 stages, float towards) {
		return snap ? glm::vec2(towards) : EaseTwoStage(stages, towards, seconds, k_BuildUpSeconds, k_CalmDownSeconds);
	};
	_cover = ease(_cover, target.cover);
	_type = ease(_type, target.type);
	_rain = ease(_rain, target.rain);
	_wind += (target.wind - _wind) * (1.0f - std::exp(-seconds / k_WindSeconds));
	// The wind moves the clouds by its travel in this frame
	_drift += _wind * seconds;

	// The storms' cells follow their storms, and calm down once their storms go
	for (auto& cell : _storms)
	{
		cell.alive = false;
	}
	for (const auto& storm : storms)
	{
		auto found = std::ranges::find(_storms, storm.serial, &StormCell::serial);
		if (found == _storms.end())
		{
			if (_storms.size() >= k_MaxStorms)
			{
				continue;
			}
			_storms.push_back(StormCell {.serial = storm.serial});
			found = _storms.end() - 1;
		}
		found->centre = storm.centre;
		found->innerRadius = storm.innerRadius;
		found->outerRadius = storm.outerRadius;
		found->alive = true;
		found->strength = ease(found->strength, storm.strength);
		found->rain = ease(found->rain, storm.rain);
	}
	float strongest = 0.0f;
	for (auto& cell : _storms)
	{
		if (!cell.alive)
		{
			cell.strength = ease(cell.strength, 0.0f);
			cell.rain = ease(cell.rain, 0.0f);
		}
		strongest = std::max(strongest, cell.strength.y);
	}
	std::erase_if(_storms,
	              [](const StormCell& cell) { return !cell.alive && cell.strength.x < 0.002f && cell.strength.y < 0.002f; });
	// The shapes of the clouds change slowly, and faster in a storm
	_evolution += k_EvolutionRate * seconds * (1.0f + k_StormBoil * std::max(strongest, _rain.y));
}

CloudWeather CloudClimate::Current() const
{
	return {.cover = _cover.y, .type = _type.y, .rain = _rain.y, .wind = _wind, .choice = _choice};
}

CloudWeather volumetric_clouds::WeatherFor(float overcast, float rain, glm::vec2 cloudWind)
{
	const float o = std::clamp(overcast, 0.0f, 1.0f);
	const float r = std::clamp(rain, 0.0f, 1.0f);
	CloudWeather weather;
	weather.cover = k_FairCover + (0.97f - k_FairCover) * o;
	// Overcast with no rain flattens the clouds into a sheet; rain raises them into towers
	weather.type = std::clamp(
	    k_Cumulus - 0.5f * o * (1.0f - std::clamp(r * 2.0f, 0.0f, 1.0f)) + 0.5f * std::clamp(r * 1.5f, 0.0f, 1.0f), 0.0f, 1.0f);
	weather.rain = r;
	weather.wind = cloudWind;
	return weather;
}

glm::vec2 volumetric_clouds::CloudWind(glm::vec2 direction, float groundSpeed)
{
	// A calm day's wind at the ground is 3 world units a second; each unit a second more moves the clouds 8 more
	constexpr float k_CalmGround = 3.0f;
	constexpr float k_PerUnit = 8.0f;
	return direction * std::max(k_CalmWind + (groundSpeed - k_CalmGround) * k_PerUnit, 0.0f);
}

glm::vec3 volumetric_clouds::SunThroughAir(float elevation)
{
	// The relative mass of air along the path (Kasten and Young), and the air's extinction for each colour
	constexpr glm::vec3 k_Extinction {0.045f, 0.11f, 0.26f};
	const float degrees = std::max(glm::degrees(std::asin(std::clamp(elevation, -1.0f, 1.0f))), -2.0f);
	const float sine = std::sin(glm::radians(std::max(degrees, 0.0f)));
	const float mass = 1.0f / (sine + 0.50572f * std::pow(degrees + 6.07995f, -1.6364f));
	return glm::exp(-k_Extinction * (mass - 1.0f));
}

CloudWeather volumetric_clouds::ChosenWeather(CoverChoice choice, const CloudWeather& weather)
{
	auto chosen = weather;
	chosen.choice = choice;
	switch (choice)
	{
	case CoverChoice::Clear:
		chosen.cover = 0.08f;
		chosen.type = k_Cumulus;
		break;
	case CoverChoice::Scattered:
		chosen.cover = k_FairCover;
		chosen.type = k_Cumulus;
		break;
	case CoverChoice::Broken:
		chosen.cover = 0.64f;
		chosen.type = 0.6f;
		break;
	case CoverChoice::Overcast:
		chosen.cover = 0.95f;
		chosen.type = 0.15f;
		break;
	case CoverChoice::Weather:
	case CoverChoice::_Count:
		break;
	}
	return chosen;
}

float volumetric_clouds::ShadowWindowFor(float eyeHeight)
{
	const float doublings = std::ceil(std::log2(std::max(eyeHeight / k_ShadowWindowHeight, 1.0f)));
	return k_ShadowWindow * std::exp2(std::min(doublings, 6.0f));
}

glm::vec2 volumetric_clouds::ShadowOrigin(glm::vec2 eye, float window)
{
	const float texel = window / static_cast<float>(k_ShadowTexels);
	return glm::floor((eye - window * 0.5f) / texel) * texel;
}

float volumetric_clouds::FarthestCloudFor(float eyeHeight)
{
	const float height = std::max(eyeHeight, 0.0f);
	const float horizon = 1.1f * std::sqrt(height * (height + (2.0f * k_PlanetRadius)));
	const float near =
	    std::min(k_FarthestCloud + k_FarthestPerHeight * std::max(eyeHeight - k_CloudBase, 0.0f), k_FarthestCloudCap);
	// Past the height where the reach meets its cap, the reach eases out to the horizon over the same height again
	const float capHeight = (k_FarthestCloudCap - k_FarthestCloud) / k_FarthestPerHeight + k_CloudBase;
	const float t = std::clamp((height - capHeight) / capHeight, 0.0f, 1.0f);
	const float share = t * t * (3.0f - (2.0f * t));
	return near + (std::max(horizon, near) - near) * share;
}

void volumetric_clouds::PackStorms(std::span<const StormCell> cells, CloudUniforms& uniforms)
{
	uniforms.storms.fill(glm::vec4(0.0f));
	size_t count = 0;
	for (const auto& cell : cells)
	{
		if (count >= k_MaxStorms || CloudClimate::Strength(cell) < 0.001f)
		{
			continue;
		}
		uniforms.storms.at(count) = {cell.centre, cell.innerRadius, std::max(cell.outerRadius, cell.innerRadius + 1.0f)};
		uniforms.storms.at(k_MaxStorms + count) = {CloudClimate::Strength(cell), CloudClimate::Rain(cell), 0.0f, 0.0f};
		++count;
	}
	uniforms.cover.w = static_cast<float>(count);
}
