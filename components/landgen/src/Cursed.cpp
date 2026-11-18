/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "landgen/Cursed.h"

#include <cmath>

#include <algorithm>
#include <array>
#include <optional>
#include <span>
#include <utility>
#include <vector>

#include "landgen/Random.h"

using namespace openblack::landgen;
using namespace openblack::landgen::cursed;
using features::Feature;
using features::Kind;

namespace
{
constexpr float k_MetresPerCorner = 10.0f;
constexpr float k_FullTurn = 6.28318531f;

/// A thing of a place of the story, at a distance in metres from the first puff of its stack
struct StampRock
{
	float dx;
	float dz;
	uint8_t type;
	/// The pose as the land script gives it: the height above the land, the turns about the three axes and the scale
	float altitude;
	float xAngle;
	float yAngle;
	float zAngle;
	float scale;
};
struct StampPuff
{
	float dx;
	float dz;
	uint32_t colour;
	float size;
};
struct Stamp
{
	std::span<const StampPuff> puffs;
	std::span<const StampRock> rocks;
	/// The slopes of the middle of the stack, as the rise for each metre. The stack of the story stands on such ground.
	float flattest;
	float steepest;
};

// The near black mist of the first land of the story: three puffs on one spot of a heather slope at 32 degrees, with
// large boulders and a sharp rock of limestone round them
constexpr std::array<StampPuff, 3> k_BlackPuffs {{
    {0.00f, 0.00f, 0xC90A0400u, 0.456260f},
    {0.16f, -0.14f, 0x990A0400u, 0.740244f},
    {-1.04f, -0.33f, 0x570A0400u, 1.145935f},
}};
constexpr std::array<StampRock, 5> k_BlackRocks {{
    {-12.81f, -5.43f, 41, -0.154262f, -0.468245f, -0.303575f, 0.136743f, 3.269f},
    {-2.50f, 4.93f, 25, 4.215774f, -0.483854f, 2.621433f, -0.024075f, 4.822f},
    {-14.57f, 6.54f, 17, 0.288852f, 0.283111f, 2.752570f, -0.771995f, 2.545f},
    {-0.11f, 15.27f, 17, 1.091764f, -0.121891f, 1.704498f, -0.626537f, 5.547f},
    {13.47f, 9.03f, 37, 0.092043f, 0.081783f, -0.020354f, 0.603467f, 1.0f},
}};
// The dark mist of the fourth land of the story: three puffs over a clump of seven volcanic boulders on rock at 101 m,
// on a slope of 14 degrees
constexpr std::array<StampPuff, 3> k_VolcanicPuffs {{
    {0.00f, 0.00f, 0xAC212B29u, 0.699675f},
    {-0.94f, 0.60f, 0xAC212B29u, 0.415691f},
    {-0.75f, 0.12f, 0x84212B29u, 1.186504f},
}};
constexpr std::array<StampRock, 7> k_VolcanicRocks {{
    {-1.08f, -0.18f, 27, 0.087353f, -0.487141f, 0.304064f, 0.489208f, 1.299f},
    {1.01f, -2.54f, 19, 0.267404f, -0.343718f, 0.581325f, 0.660574f, 1.839f},
    {3.37f, -1.40f, 19, 0.289244f, -0.759303f, 1.007594f, 0.260368f, 1.0f},
    {3.08f, 1.26f, 27, 1.671802f, 1.012975f, 1.175620f, -2.107772f, 1.0f},
    {3.61f, 5.07f, 23, 0.150900f, 0.326238f, 0.434364f, -0.309946f, 1.0f},
    {4.78f, 2.32f, 27, 0.494445f, 0.267807f, -0.020360f, -0.595780f, 1.059f},
    {0.41f, 5.36f, 19, 0.0f, 0.415690f, -0.056329f, 0.390019f, 1.839f},
}};
// The slopes of 20, 25 and 45 degrees as the rise for each metre
constexpr float k_TwentyDegrees = 0.3640f;
constexpr float k_TwentyFiveDegrees = 0.4663f;
constexpr float k_FortyFiveDegrees = 1.0f;
constexpr Stamp k_BlackStamp {k_BlackPuffs, k_BlackRocks, k_TwentyDegrees, k_FortyFiveDegrees};
constexpr Stamp k_VolcanicStamp {k_VolcanicPuffs, k_VolcanicRocks, 0.0f, k_TwentyFiveDegrees};

/// The steepest ground of a boulder of a stack, as the rise for each metre (50 degrees: the steepest boulder of the
/// stacks of the story), and the most that a boulder is above or below the stack, in metres
constexpr float k_BoulderSteepest = 1.1918f;
constexpr float k_StampRise = 10.0f;

const Stamp& StampOf(Stack stack)
{
	return stack == Stack::Black ? k_BlackStamp : k_VolcanicStamp;
}

/// The farthest thing of a stamp from its first puff, in metres
float ReachOf(const Stamp& stamp)
{
	float reach = 0.0f;
	for (const auto& rock : stamp.rocks)
	{
		reach = std::max(reach, std::sqrt((rock.dx * rock.dx) + (rock.dz * rock.dz)));
	}
	return reach;
}

// The group of burnt trees of the first land of the story: the place of each tree from the first, in metres, and its
// scale
struct StampTree
{
	float dx;
	float dz;
	float scale;
};
constexpr std::array<StampTree, 3> k_BurntGroup {{{0.0f, 0.0f, 1.716f}, {9.55f, 0.0f, 1.0f}, {26.07f, 41.02f, 1.095f}}};
/// The fewest and the most burnt trees of a place
constexpr uint32_t k_FewestBurnt = 10;
constexpr uint32_t k_MostBurnt = 40;
/// The steepest ground of a burnt tree of the story, as the rise for each metre
constexpr float k_BurntSteepest = 0.7590f;
/// No burnt tree stands nearer than this to another feature, in metres
constexpr float k_BurntGap = 1.5f;

// The craters of the fifth land of the story: their turns in radians and their scales. Each crater has one puff of
// grey smoke at its middle, which shrinks when the camera sees it edge on. The craters stand in a line, 95 m to 121 m
// apart.
struct StampCrater
{
	float angle;
	float scale;
};
constexpr std::array<StampCrater, 5> k_Craters {{
    {-4.953f, 3.999f},
    {-2.094f, 3.999f},
    {-2.094f, 3.759f},
    {0.0f, 3.759f},
    {-5.793f, 3.999f},
}};
constexpr uint32_t k_SmokeColour = 0x6D505050u;
constexpr float k_SmokeSize = 0.577968f;
constexpr float k_SmokeEdgeShrink = 2.642276f;
/// The steepest ground of a crater of the story, as the rise for each metre (40 degrees)
constexpr float k_CraterSteepest = 0.8391f;
constexpr float k_CraterNearest = 95.0f;
constexpr float k_CraterFarthest = 121.0f;
/// The most craters of a place
constexpr uint32_t k_MostCraters = 2;
/// The most stacks of dark mist of a place
constexpr uint32_t k_MostStacks = 3;
/// Nothing else stands nearer than this to the middle of a crater, in metres. The nearest thing to a crater of the
/// story is 37 m away.
constexpr float k_CraterClearing = 20.0f;
/// The stacks and the craters of a place keep this far apart, in metres
constexpr float k_PartGap = 25.0f;
/// A part of a place tries this many points before it stops
constexpr uint32_t k_Tries = 12;
/// A big forest keeps a circle of this radius clear at scale 1, in metres
constexpr float k_BigForestClearing = 33.0f;

/// The random numbers of a point of a grid, from the seed of the stage and the place of the point only
Random RandomAt(uint64_t stageSeed, int i, int j)
{
	return Random(static_cast<uint64_t>(Hash(stageSeed, i, j)) | (static_cast<uint64_t>(Hash(stageSeed, j, i)) << 32u));
}

/// A random point in the circle of radius 1
std::pair<float, float> InCircle(Random& random)
{
	float dx = 0.0f;
	float dz = 0.0f;
	do
	{
		dx = random.NextRange(-1.0f, 1.0f);
		dz = random.NextRange(-1.0f, 1.0f);
	} while ((dx * dx) + (dz * dz) > 1.0f || (dx == 0.0f && dz == 0.0f));
	return {dx, dz};
}

/// A random direction: the cosine and the sine of a random turn
std::pair<float, float> Direction(Random& random)
{
	const auto [dx, dz] = InCircle(random);
	const float length = std::sqrt((dx * dx) + (dz * dz));
	return {dx / length, dz / length};
}

float Distance(float ax, float az, float bx, float bz)
{
	const float dx = ax - bx;
	const float dz = az - bz;
	return std::sqrt((dx * dx) + (dz * dz));
}

/// The biomes of the cursed places. The places of the story are on heather, rock and the floor of a forest, never on
/// snow, sand or wet ground.
bool CursedIn(biomes::Biome biome)
{
	switch (biome)
	{
	case biomes::Biome::Grassland:
	case biomes::Biome::TemperateForest:
	case biomes::Biome::DenseJungle:
	case biomes::Biome::Desert:
	case biomes::Biome::DeadWoods:
	case biomes::Biome::Alpine:
		return true;
	default:
		return false;
	}
}

/// No part of a cursed place stands higher than this, in metres, so that no part stands on snow. The snow of each
/// country starts at 117 m or higher.
constexpr float k_PartHighest = 110.0f;

/// A place in metres can hold a part of a cursed place
bool FreeAt(const features::Ground& ground, float x, float z)
{
	const float cx = x / k_MetresPerCorner;
	const float cz = z / k_MetresPerCorner;
	if (!features::IsFree(ground, cx, cz))
	{
		return false;
	}
	if (features::SpotAt(ground, cx, cz).height > k_PartHighest)
	{
		return false;
	}
	const int side = ground.biomes.side;
	return CursedIn(ground.biomes.At(std::clamp(static_cast<int>(cx + 0.5f), 0, side - 1),
	                                 std::clamp(static_cast<int>(cz + 0.5f), 0, side - 1)));
}

/// The circles in metres that a place keeps clear: the stacks and the craters
struct Circle
{
	float x;
	float z;
	float radius;
};

bool InAny(std::span<const Circle> circles, float x, float z)
{
	return std::ranges::any_of(circles,
	                           [x, z](const Circle& circle) { return Distance(x, z, circle.x, circle.z) < circle.radius; });
}

/// Removes the features of the land in a circle in metres, apart from the big forests
void ClearCircle(std::vector<Feature>& placed, const Circle& circle)
{
	std::erase_if(placed, [&circle](const Feature& feature) {
		return feature.kind != Kind::BigForest && Distance(feature.x, feature.z, circle.x, circle.z) < circle.radius;
	});
}

/// A stack of dark mist with its boulders. Gives true when the stack fits.
bool PlaceStack(const features::Ground& ground, const Patch& patch, const Stamp& stamp, Random& random,
                std::vector<Feature>& placed, std::vector<Mist>& mists, std::vector<Circle>& circles)
{
	const float reach = ReachOf(stamp);
	// The first tries look for ground with the slope of the stack of the story. The later tries take any slope.
	for (uint32_t attempt = 0; attempt < k_Tries * 2; ++attempt)
	{
		const auto [ux, uz] = InCircle(random);
		const float x = patch.x + (ux * (patch.radius - reach));
		const float z = patch.z + (uz * (patch.radius - reach));
		if (!FreeAt(ground, x, z) || InAny(circles, x, z) || std::ranges::any_of(circles, [x, z](const Circle& circle) {
			    return Distance(x, z, circle.x, circle.z) < circle.radius + k_PartGap;
		    }))
		{
			continue;
		}
		const auto spot = features::SpotAt(ground, x / k_MetresPerCorner, z / k_MetresPerCorner);
		if (attempt < k_Tries && (spot.slope < stamp.flattest || spot.slope > stamp.steepest))
		{
			continue;
		}
		// Each boulder lies on ground that is not steeper than the ground of the boulders of the story, and near the
		// height of the stack, so that the clump does not hang on a cliff
		const bool fits = std::ranges::all_of(stamp.rocks, [&ground, &spot, x, z](const StampRock& rock) {
			if (!FreeAt(ground, x + rock.dx, z + rock.dz))
			{
				return false;
			}
			const auto at = features::SpotAt(ground, (x + rock.dx) / k_MetresPerCorner, (z + rock.dz) / k_MetresPerCorner);
			return at.slope <= k_BoulderSteepest && std::fabs(at.height - spot.height) <= k_StampRise;
		});
		if (!fits)
		{
			continue;
		}
		const Circle circle {.x = x, .z = z, .radius = reach + 2.0f};
		ClearCircle(placed, circle);
		circles.push_back(circle);
		for (const auto& puff : stamp.puffs)
		{
			mists.push_back({.x = x + puff.dx,
			                 .z = z + puff.dz,
			                 .altitude = 0.0f,
			                 .colour = puff.colour,
			                 .size = puff.size,
			                 .edgeShrink = 1.0f});
		}
		for (const auto& rock : stamp.rocks)
		{
			placed.push_back({.kind = Kind::Static,
			                  .type = rock.type,
			                  .x = x + rock.dx,
			                  .z = z + rock.dz,
			                  .angle = rock.yAngle,
			                  .size = rock.scale,
			                  .maxSize = rock.scale,
			                  .xAngle = rock.xAngle,
			                  .zAngle = rock.zAngle,
			                  .altitude = rock.altitude});
		}
		return true;
	}
	return false;
}

/// A crater with its puff of smoke. The second crater of a place stands at the distance of the craters of the story
/// from the first. Gives true when the crater fits.
bool PlaceCrater(const features::Ground& ground, const Patch& patch, const std::optional<Circle>& previous, Random& random,
                 std::vector<Feature>& placed, std::vector<Mist>& mists, std::vector<Circle>& circles)
{
	for (uint32_t attempt = 0; attempt < k_Tries; ++attempt)
	{
		float x = 0.0f;
		float z = 0.0f;
		if (previous)
		{
			const auto [cx, cz] = Direction(random);
			const float distance = random.NextRange(k_CraterNearest, k_CraterFarthest);
			x = previous->x + (cx * distance);
			z = previous->z + (cz * distance);
		}
		else
		{
			const auto [ux, uz] = InCircle(random);
			x = patch.x + (ux * (patch.radius - k_CraterClearing));
			z = patch.z + (uz * (patch.radius - k_CraterClearing));
		}
		const auto& crater = k_Craters[random.NextBelow(static_cast<uint32_t>(k_Craters.size()))];
		if (Distance(x, z, patch.x, patch.z) > patch.radius || !FreeAt(ground, x, z) ||
		    std::ranges::any_of(circles, [x, z](const Circle& circle) {
			    return Distance(x, z, circle.x, circle.z) < circle.radius + k_PartGap;
		    }))
		{
			continue;
		}
		if (features::SpotAt(ground, x / k_MetresPerCorner, z / k_MetresPerCorner).slope > k_CraterSteepest)
		{
			continue;
		}
		const Circle circle {.x = x, .z = z, .radius = k_CraterClearing};
		ClearCircle(placed, circle);
		circles.push_back(circle);
		placed.push_back({.kind = Kind::Feature,
		                  .type = k_CraterFeature,
		                  .x = x,
		                  .z = z,
		                  .angle = crater.angle,
		                  .size = crater.scale,
		                  .maxSize = crater.scale});
		mists.push_back(
		    {.x = x, .z = z, .altitude = 0.0f, .colour = k_SmokeColour, .size = k_SmokeSize, .edgeShrink = k_SmokeEdgeShrink});
		return true;
	}
	return false;
}

/// The groups of burnt trees of a place, outside its stacks and craters. Gives the number of trees.
uint32_t PlaceBurntTrees(const features::Ground& ground, const Patch& patch, uint32_t target, Random& random,
                         std::span<const Circle> circles, std::vector<Feature>& placed)
{
	uint32_t grown = 0;
	const auto groups = (target + static_cast<uint32_t>(k_BurntGroup.size()) - 1) / static_cast<uint32_t>(k_BurntGroup.size());
	for (uint32_t attempt = 0; attempt < groups * 3 && grown < target; ++attempt)
	{
		const auto [ux, uz] = InCircle(random);
		const auto [cosine, sine] = Direction(random);
		const float x = patch.x + (ux * patch.radius);
		const float z = patch.z + (uz * patch.radius);
		for (const auto& tree : k_BurntGroup)
		{
			const float angle = random.NextUnit() * k_FullTurn;
			if (grown >= target)
			{
				break;
			}
			// The group of the story turns as a whole
			const float tx = x + (tree.dx * cosine) - (tree.dz * sine);
			const float tz = z + (tree.dx * sine) + (tree.dz * cosine);
			if (Distance(tx, tz, patch.x, patch.z) > patch.radius || !FreeAt(ground, tx, tz) || InAny(circles, tx, tz))
			{
				continue;
			}
			if (features::SpotAt(ground, tx / k_MetresPerCorner, tz / k_MetresPerCorner).slope > k_BurntSteepest)
			{
				continue;
			}
			const bool crowded = std::ranges::any_of(
			    placed, [tx, tz](const Feature& feature) { return Distance(feature.x, feature.z, tx, tz) < k_BurntGap; });
			if (crowded)
			{
				continue;
			}
			placed.push_back({.kind = Kind::Tree,
			                  .type = static_cast<uint8_t>(features::Tree::Burnt),
			                  .x = tx,
			                  .z = tz,
			                  .angle = angle,
			                  .size = tree.scale,
			                  .maxSize = tree.scale});
			++grown;
		}
	}
	return grown;
}

/// A candidate middle of a place, with its random key
struct Candidate
{
	uint32_t key;
	int i;
	int j;
};
} // namespace

uint32_t cursed::PatchCount(float unit, const Settings& settings)
{
	if (settings.most <= settings.fewest)
	{
		return settings.most;
	}
	const auto span = static_cast<float>(settings.most - settings.fewest + 1);
	const float u = std::clamp(unit, 0.0f, 1.0f);
	const auto extra = static_cast<uint32_t>(std::floor(u * u * span));
	return std::min(settings.fewest + extra, settings.most);
}

Result cursed::Place(const features::Ground& ground, std::vector<Feature>& placed, uint64_t seed, const Settings& settings)
{
	Result result;
	const auto count = PatchCount(Random(StageSeed(seed, 50)).NextUnit(), settings);
	if (count == 0)
	{
		return result;
	}
	const auto siteSeed = StageSeed(seed, 51);
	const auto partSeed = StageSeed(seed, 52);
	const float last = static_cast<float>(ground.biomes.side - 1);
	const int points = static_cast<int>(last / settings.spacing);
	std::vector<Candidate> candidates;
	candidates.reserve(static_cast<size_t>(points) * static_cast<size_t>(points));
	for (int i = 0; i < points; ++i)
	{
		for (int j = 0; j < points; ++j)
		{
			candidates.push_back({.key = RandomAt(siteSeed, i, j).Next(), .i = i, .j = j});
		}
	}
	std::ranges::sort(candidates, [](const Candidate& a, const Candidate& b) {
		return a.key != b.key ? a.key < b.key : (a.i != b.i ? a.i < b.i : a.j < b.j);
	});

	std::vector<Circle> bigForests;
	for (const auto& feature : placed)
	{
		if (feature.kind == Kind::BigForest)
		{
			bigForests.push_back({.x = feature.x, .z = feature.z, .radius = k_BigForestClearing * feature.size});
		}
	}
	const float townX = ground.townX * k_MetresPerCorner;
	const float townZ = ground.townZ * k_MetresPerCorner;
	for (const auto& candidate : candidates)
	{
		if (result.patches.size() >= count)
		{
			break;
		}
		auto random = RandomAt(siteSeed, candidate.i, candidate.j);
		random.Next();
		const float x = (static_cast<float>(candidate.i) + random.NextUnit()) * settings.spacing;
		const float z = (static_cast<float>(candidate.j) + random.NextUnit()) * settings.spacing;
		const float radius = random.NextRange(settings.smallestRadius, settings.largestRadius);
		if (!FreeAt(ground, x * k_MetresPerCorner, z * k_MetresPerCorner))
		{
			continue;
		}
		const auto spot = features::SpotAt(ground, x, z);
		const float metresX = x * k_MetresPerCorner;
		const float metresZ = z * k_MetresPerCorner;
		if (spot.height < settings.lowest || spot.height > settings.highest || spot.sea < settings.nearestSea ||
		    Distance(metresX, metresZ, townX, townZ) < settings.nearestTown + radius ||
		    std::ranges::any_of(
		        ground.towns,
		        [&](const features::Clearing& town) {
			        return Distance(metresX, metresZ, town.x * k_MetresPerCorner, town.z * k_MetresPerCorner) <
			               settings.nearestTown + radius;
		        }))
		{
			continue;
		}
		const bool nearForest = std::ranges::any_of(bigForests, [metresX, metresZ, radius](const Circle& forest) {
			return Distance(metresX, metresZ, forest.x, forest.z) < forest.radius + radius;
		});
		const bool nearPatch = std::ranges::any_of(result.patches, [&](const Patch& other) {
			return Distance(metresX, metresZ, other.x, other.z) < other.radius + radius + settings.gap;
		});
		if (nearForest || nearPatch)
		{
			continue;
		}

		auto parts = RandomAt(partSeed, candidate.i, candidate.j);
		Patch patch {.x = metresX,
		             .z = metresZ,
		             .radius = radius,
		             .stack = parts.NextBelow(2) == 0 ? Stack::Black : Stack::Volcanic,
		             .burntTrees = 0,
		             .stacks = 0,
		             .craters = 0};
		const auto stacks = 1 + parts.NextBelow(k_MostStacks);
		const auto craters = parts.NextBelow(k_MostCraters + 1);
		const auto burnt = k_FewestBurnt + parts.NextBelow(k_MostBurnt - k_FewestBurnt + 1);
		std::vector<Circle> circles;
		for (uint32_t k = 0; k < stacks; ++k)
		{
			if (PlaceStack(ground, patch, StampOf(patch.stack), parts, placed, result.mists, circles))
			{
				++patch.stacks;
			}
		}
		std::optional<Circle> previous;
		for (uint32_t k = 0; k < craters; ++k)
		{
			if (PlaceCrater(ground, patch, previous, parts, placed, result.mists, circles))
			{
				++patch.craters;
				previous = circles.back();
			}
		}
		patch.burntTrees = PlaceBurntTrees(ground, patch, burnt, parts, circles, placed);
		result.patches.push_back(patch);
	}
	return result;
}
