/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "landgen/Swamps.h"

#include <cmath>
#include <cstddef>

#include <algorithm>
#include <array>
#include <utility>

#include "landgen/Random.h"

using namespace openblack::landgen;
using namespace openblack::landgen::swamps;

namespace
{
constexpr float k_MetresPerCorner = 10.0f;
/// The area of the land round one corner, in square kilometres
constexpr float k_SquareKmPerCorner = k_MetresPerCorner * k_MetresPerCorner / 1.0e6f;

/// A puff of the sheet of the story, at a distance in metres from its first puff
struct SheetPuff
{
	float dx;
	float dz;
	uint32_t colour;
	float size;
	float edgeShrink;
};

// The dark green mist of the fourth land of the story: a group of four puffs, a pair of puffs and a line of five flat
// puffs, over a sand flat near the sea by bushes and oaks
constexpr std::array<SheetPuff, 11> k_Sheet {{
    {0.00f, 0.00f, 0xFF50632Fu, 0.902520f, 1.0f},
    {2.92f, 1.31f, 0xC250632Fu, 1.470488f, 1.0f},
    {3.34f, 3.53f, 0x7E50632Fu, 1.916748f, 1.0f},
    {6.43f, 2.42f, 0xFF50632Fu, 0.902520f, 1.0f},
    {38.92f, -32.17f, 0xAE50632Fu, 0.659106f, 1.0f},
    {40.25f, -31.83f, 0x8650632Fu, 1.389350f, 1.0f},
    {48.27f, 34.14f, 0x8650632Fu, 1.389350f, 3.780488f},
    {20.96f, 54.04f, 0x8650632Fu, 1.389350f, 3.780488f},
    {37.45f, 47.19f, 0x8650632Fu, 1.389350f, 3.780488f},
    {51.44f, 20.32f, 0x8650632Fu, 1.389350f, 3.780488f},
    {51.37f, 1.79f, 0x8650632Fu, 1.389350f, 3.780488f},
}};

/// The ground under the sheet of the story is between 2.7 m and 14.7 m high. No sheet goes over ground that rises more.
constexpr float k_SheetRise = 12.06f;

/// The farthest puff of the sheet from its first puff, in metres
float ReachOfSheet()
{
	float reach = 0.0f;
	for (const auto& puff : k_Sheet)
	{
		reach = std::max(reach, std::sqrt((puff.dx * puff.dx) + (puff.dz * puff.dz)));
	}
	return reach;
}

/// The random numbers of a point of a grid, from the seed of the stage and the place of the point only
Random RandomAt(uint64_t stageSeed, int i, int j)
{
	return Random(static_cast<uint64_t>(Hash(stageSeed, i, j)) | (static_cast<uint64_t>(Hash(stageSeed, j, i)) << 32u));
}

/// The corner of a place in metres, on the map
std::pair<int, int> CornerOf(const features::Ground& ground, float x, float z)
{
	const int side = ground.biomes.side;
	// NOLINTBEGIN(bugprone-incorrect-roundings): A change gives a different land.
	return {std::clamp(static_cast<int>((x / k_MetresPerCorner) + 0.5f), 0, side - 1),
	        std::clamp(static_cast<int>((z / k_MetresPerCorner) + 0.5f), 0, side - 1)};
	// NOLINTEND(bugprone-incorrect-roundings)
}

/// A place in metres is over the free ground of a swamp
bool SwampAt(const features::Ground& ground, float x, float z)
{
	if (!features::IsFree(ground, x / k_MetresPerCorner, z / k_MetresPerCorner))
	{
		return false;
	}
	const auto [i, j] = CornerOf(ground, x, z);
	return ground.biomes.At(i, j) == biomes::Biome::Swamp;
}

/// Whether the whole sheet fits with its first puff at a place in metres
bool SheetFits(const features::Ground& ground, float x, float z)
{
	float lowest = 0.0f;
	float highest = 0.0f;
	for (size_t k = 0; k < k_Sheet.size(); ++k)
	{
		const float px = x + k_Sheet[k].dx;
		const float pz = z + k_Sheet[k].dz;
		if (!SwampAt(ground, px, pz))
		{
			return false;
		}
		const float height = features::SpotAt(ground, px / k_MetresPerCorner, pz / k_MetresPerCorner).height;
		lowest = k == 0 ? height : std::min(lowest, height);
		highest = k == 0 ? height : std::max(highest, height);
	}
	return highest - lowest <= k_SheetRise;
}

/// A candidate first puff of a sheet, with its random key
struct Candidate
{
	uint32_t key;
	int i;
	int j;
};
} // namespace

size_t swamps::PuffsInSheet()
{
	return k_Sheet.size();
}

uint32_t swamps::SheetsFor(float landKm2, const Settings& settings)
{
	if (settings.landPerSheet <= 0.0f || landKm2 <= 0.0f)
	{
		return 0;
	}
	return static_cast<uint32_t>(std::floor(landKm2 / settings.landPerSheet));
}

namespace
{
/// The sheets of the story over the swamps
void PlaceSheets(const features::Ground& ground, uint64_t seed, const Settings& settings, Result& result)
{
	const auto land = static_cast<float>(std::ranges::count_if(
	                      ground.biomes.values, [](biomes::Biome biome) { return biome != biomes::Biome::Sea; })) *
	                  k_SquareKmPerCorner;
	const auto most = std::min(settings.most, SheetsFor(land, settings));
	if (most == 0 || settings.spacing <= 0.0f)
	{
		return;
	}
	const auto siteSeed = StageSeed(seed, 70);
	const auto last = static_cast<float>(ground.biomes.side - 1);
	const int points = static_cast<int>(last / settings.spacing);
	// Only the points on a swamp can start a sheet
	std::vector<Candidate> candidates;
	for (int i = 0; i < points; ++i)
	{
		for (int j = 0; j < points; ++j)
		{
			// NOLINTBEGIN(bugprone-incorrect-roundings): A change gives a different land.
			const int ci =
			    std::clamp(static_cast<int>((static_cast<float>(i) * settings.spacing) + 0.5f), 0, ground.biomes.side - 1);
			const int cj =
			    std::clamp(static_cast<int>((static_cast<float>(j) * settings.spacing) + 0.5f), 0, ground.biomes.side - 1);
			// NOLINTEND(bugprone-incorrect-roundings)
			if (ground.biomes.At(ci, cj) == biomes::Biome::Swamp)
			{
				candidates.push_back({.key = RandomAt(siteSeed, i, j).Next(), .i = i, .j = j});
			}
		}
	}
	std::ranges::sort(candidates, [](const Candidate& a, const Candidate& b) {
		return a.key != b.key ? a.key < b.key : (a.i != b.i ? a.i < b.i : a.j < b.j);
	});

	// Two sheets keep apart by twice the reach of a sheet, so that no puffs of two sheets mix
	const float gap = 2.0f * ReachOfSheet();
	for (const auto& candidate : candidates)
	{
		if (result.sheets.size() >= most)
		{
			break;
		}
		auto random = RandomAt(siteSeed, candidate.i, candidate.j);
		random.Next();
		const float x = (static_cast<float>(candidate.i) + random.NextUnit()) * settings.spacing * k_MetresPerCorner;
		const float z = (static_cast<float>(candidate.j) + random.NextUnit()) * settings.spacing * k_MetresPerCorner;
		const bool near = std::ranges::any_of(result.sheets, [x, z, gap](const Sheet& other) {
			const float dx = other.x - x;
			const float dz = other.z - z;
			return (dx * dx) + (dz * dz) < gap * gap;
		});
		if (near || !SheetFits(ground, x, z))
		{
			continue;
		}
		result.sheets.push_back({.x = x, .z = z});
		for (const auto& puff : k_Sheet)
		{
			result.mists.push_back({.x = x + puff.dx,
			                        .z = z + puff.dz,
			                        .altitude = 0.0f,
			                        .colour = puff.colour,
			                        .size = puff.size,
			                        .edgeShrink = puff.edgeShrink});
		}
	}
}

/// Whether a place in metres is in the middle of a swamp: the swamp is under it and at the distance of one corner on
/// each side, so that no puff of the cover stands alone over a small wet spot
bool InSwampMiddle(const features::Ground& ground, float x, float z)
{
	constexpr std::array<std::pair<float, float>, 5> k_Around {{{0.0f, 0.0f},
	                                                            {k_MetresPerCorner, 0.0f},
	                                                            {-k_MetresPerCorner, 0.0f},
	                                                            {0.0f, k_MetresPerCorner},
	                                                            {0.0f, -k_MetresPerCorner}}};
	return std::ranges::all_of(
	    k_Around, [&ground, x, z](const auto& offset) { return SwampAt(ground, x + offset.first, z + offset.second); });
}

/// The cover of flat puffs over the whole of each swamp. A sheet of the story keeps its own puffs.
void PlaceCover(const features::Ground& ground, uint64_t seed, const Settings& settings, Result& result)
{
	if (settings.coverSpacing <= 0.0f)
	{
		return;
	}
	const auto coverSeed = StageSeed(seed, 71);
	const float extent = static_cast<float>(ground.biomes.side - 1) * k_MetresPerCorner;
	const int points = static_cast<int>(extent / settings.coverSpacing);
	const float clear = settings.coverSpacing * 0.5f;
	const auto sheetPuffs = result.mists.size();
	for (int i = 0; i < points; ++i)
	{
		for (int j = 0; j < points; ++j)
		{
			if (result.mists.size() - sheetPuffs >= settings.mostCoverPuffs)
			{
				return;
			}
			// Each puff is in the middle half of its square, so that the gaps between the puffs stay near the spacing
			auto random = RandomAt(coverSeed, i, j);
			const float x = (static_cast<float>(i) + 0.25f + (0.5f * random.NextUnit())) * settings.coverSpacing;
			const float z = (static_cast<float>(j) + 0.25f + (0.5f * random.NextUnit())) * settings.coverSpacing;
			if (!InSwampMiddle(ground, x, z))
			{
				continue;
			}
			const bool nearSheet =
			    std::any_of(result.mists.begin(), result.mists.begin() + static_cast<std::ptrdiff_t>(sheetPuffs),
			                [x, z, clear](const cursed::Mist& puff) {
				                const float dx = puff.x - x;
				                const float dz = puff.z - z;
				                return (dx * dx) + (dz * dz) < clear * clear;
			                });
			if (nearSheet)
			{
				continue;
			}
			const auto& flat = k_Sheet.back();
			result.mists.push_back({.x = x,
			                        .z = z,
			                        .altitude = 0.0f,
			                        .colour = flat.colour,
			                        .size = flat.size * (settings.coverSpacing / k_CoverBaseSpacing),
			                        .edgeShrink = flat.edgeShrink});
			++result.coverPuffs;
		}
	}
}
} // namespace

Result swamps::Place(const features::Ground& ground, uint64_t seed, const Settings& settings)
{
	Result result;
	PlaceSheets(ground, seed, settings, result);
	PlaceCover(ground, seed, settings, result);
	return result;
}
