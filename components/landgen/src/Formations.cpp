/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "landgen/Formations.h"

#include <cmath>

#include <algorithm>
#include <array>
#include <optional>
#include <span>
#include <utility>
#include <vector>

#include "landgen/Random.h"
#include "landgen/Stages.h"
#include "landgen/Terrain.h"

using namespace openblack::landgen;
using namespace openblack::landgen::formations;
using features::Feature;
using features::Kind;

namespace
{
constexpr float k_MetresPerCorner = 10.0f;
constexpr float k_SquareMetresPerKm2 = 1000000.0f;

/// A loose object or a rock of a fairy ring of the story, at a distance in metres from the middle of the ring. The
/// turn, the scale and the pose are the values of the land script.
struct StampThing
{
	float dx;
	float dz;
	Kind kind;
	uint8_t type;
	float angle;
	float scale;
	float altitude {0.0f};
	float xAngle {0.0f};
	float zAngle {0.0f};
};

constexpr auto k_Mushroom = k_MagicMushroomObject;
constexpr auto k_Champi = k_ChampiObject;
constexpr auto k_Rock = features::k_PlainRockStatic;

// The fairy ring of the first land of the story: ten magic mushrooms and two Champi mushrooms 5 to 9 m from the middle,
// round five small rocks that lean into the ground. The ring stands in a wood of conifers on flat ground at 27 m.
constexpr std::array<StampThing, 17> k_FirstRing {{
    {-6.58f, -3.27f, Kind::Mobile, k_Mushroom, 4.471f, 0.879f},
    {-3.14f, -7.78f, Kind::Mobile, k_Mushroom, 3.782f, 1.179f},
    {-5.51f, -6.39f, Kind::Mobile, k_Mushroom, 0.898f, 1.179f},
    {-2.51f, 6.88f, Kind::Mobile, k_Champi, 0.054f, 1.0f},
    {-7.31f, 2.87f, Kind::Mobile, k_Mushroom, 3.671f, 1.179f},
    {-1.62f, 7.86f, Kind::Mobile, k_Mushroom, 5.017f, 1.179f},
    {5.17f, -2.83f, Kind::Mobile, k_Champi, 2.118f, 1.0f},
    {5.36f, -0.76f, Kind::Mobile, k_Mushroom, 2.157f, 0.879f},
    {5.79f, -1.68f, Kind::Mobile, k_Mushroom, 2.388f, 1.179f},
    {2.54f, -8.31f, Kind::Mobile, k_Mushroom, 2.273f, 1.179f},
    {2.96f, 7.27f, Kind::Mobile, k_Mushroom, 0.804f, 0.879f},
    {4.80f, 6.15f, Kind::Mobile, k_Mushroom, 0.851f, 1.179f},
    {0.20f, 0.21f, Kind::Static, k_Rock, 1.006636f, 0.339f, 0.348221f, -0.616918f, 0.088435f},
    {1.53f, -2.16f, Kind::Static, k_Rock, -0.224255f, 0.640f, 0.640937f, -0.460966f, 0.307587f},
    {-1.44f, 1.47f, Kind::Static, k_Rock, -1.305233f, 0.279f, 0.606002f, 0.760450f, 2.074401f},
    {-0.83f, -2.59f, Kind::Static, k_Rock, -2.823640f, 0.279f, 0.305509f, -0.251430f, 0.361004f},
    {0.69f, 1.52f, Kind::Static, k_Rock, -0.746897f, 0.159f, 0.097632f, -0.349535f, 0.097065f},
}};
// The fairy ring of the third land of the story: six large magic mushrooms 2 to 4 m from the middle, on a heather slope
// at 101 m
constexpr std::array<StampThing, 6> k_ThirdRing {{
    {-2.50f, -2.41f, Kind::Mobile, k_Mushroom, 0.582f, 2.200f},
    {-1.02f, -1.53f, Kind::Mobile, k_Mushroom, 0.906f, 2.647f},
    {-2.87f, 2.76f, Kind::Mobile, k_Mushroom, 4.591f, 1.559f},
    {1.87f, -2.50f, Kind::Mobile, k_Mushroom, 1.338f, 1.542f},
    {1.82f, 2.40f, Kind::Mobile, k_Mushroom, 3.325f, 1.034f},
    {2.70f, 1.27f, Kind::Mobile, k_Mushroom, 4.671f, 1.014f},
}};

std::span<const StampThing> StampOf(Ring ring)
{
	return ring == Ring::First ? std::span<const StampThing>(k_FirstRing) : std::span<const StampThing>(k_ThirdRing);
}

/// The ground of the fairy rings of the story. The middle of each ring and each of its things is on ground of this
/// slope or less (15 degrees, as the rise for each metre), between the heights in metres and this far from the sea.
constexpr float k_RingSteepest = 0.2754f;
constexpr float k_RingLowest = 27.0f;
constexpr float k_RingHighest = 101.0f;
constexpr float k_RingNearestSea = 54.0f;
/// The two groups of magic mushrooms on the hill of the third land of the story are 116 m apart. No two formations
/// stand nearer than this to a fairy ring.
constexpr float k_RingGap = 116.0f;
/// A ring removes the trees and rocks of the land in its circle. The circle is this distance in metres past the
/// farthest thing of the ring.
constexpr float k_RingClearing = 1.0f;

/// A pillar of a group of lone pillars of the story, at a distance in metres from the first pillar of the group
struct StampPillar
{
	float dx;
	float dz;
	uint8_t type;
	float angle;
	float scale;
};
struct PillarGroup
{
	std::span<const StampPillar> pillars;
};

// The groups of lone pillars of limestone of the first, second and third lands of the story
constexpr std::array<StampPillar, 1> k_FirstLandPillar2 {{{0.0f, 0.0f, k_Pillar2Lime, 0.0f, 1.0f}}};
constexpr std::array<StampPillar, 1> k_FirstLandSmallFat {{{0.0f, 0.0f, k_FatPillarLime, 0.0f, 0.059f}}};
constexpr std::array<StampPillar, 1> k_FirstLandPillar3 {{{0.0f, 0.0f, k_Pillar3Lime, 1.047f, 0.474f}}};
constexpr std::array<StampPillar, 2> k_FirstLandPair {{
    {0.0f, 0.0f, k_FatPillarLime, 0.0f, 0.578f},
    {0.21f, 11.43f, k_Pillar3Lime, 1.184f, 1.0f},
}};
constexpr std::array<StampPillar, 1> k_FirstLandFat {{{0.0f, 0.0f, k_FatPillarLime, 1.325f, 0.216f}}};
constexpr std::array<StampPillar, 1> k_FirstLandHighPillar2 {{{0.0f, 0.0f, k_Pillar2Lime, 0.981f, 0.577f}}};
constexpr std::array<StampPillar, 3> k_SecondLandTrio {{
    {0.0f, 0.0f, k_Pillar2Lime, 0.0f, 0.339f},
    {-0.96f, 5.08f, k_Pillar3Lime, -0.767f, 0.399f},
    {-3.98f, 2.94f, k_Pillar2Lime, -4.047f, 0.159f},
}};
constexpr std::array<StampPillar, 1> k_SecondLandPillar2 {{{0.0f, 0.0f, k_Pillar2Lime, 0.0f, 0.458f}}};
constexpr std::array<StampPillar, 2> k_ThirdLandPair {{
    {0.0f, 0.0f, k_Pillar2Lime, 0.0f, 0.158f},
    {12.68f, 0.04f, k_Pillar2Lime, 0.0f, 0.158f},
}};
constexpr std::array<PillarGroup, 9> k_PillarGroups {{
    {k_FirstLandPillar2},
    {k_FirstLandSmallFat},
    {k_FirstLandPillar3},
    {k_FirstLandPair},
    {k_FirstLandFat},
    {k_FirstLandHighPillar2},
    {k_SecondLandTrio},
    {k_SecondLandPillar2},
    {k_ThirdLandPair},
}};

/// The ground of the lone pillars of the story: each pillar is on ground of this slope or less (55 degrees, as the rise
/// for each metre), between the heights in metres and this far from the sea
constexpr float k_PillarSteepest = 1.40f;
constexpr float k_PillarLowest = 3.0f;
constexpr float k_PillarHighest = 74.3f;
constexpr float k_PillarNearestSea = 36.0f;
/// The nearest two groups of lone pillars of the story are 217 m apart
constexpr float k_PillarGap = 217.0f;
/// A pillar removes the trees in a circle of this radius in metres round it. The nearest tree to a lone pillar of the
/// story is 2 m from it.
constexpr float k_PillarTreeClearing = 2.0f;

/// A mushroom or a toadstool of a clump of the story, at a distance in metres from the middle of the clump. The turn
/// and the scale are the values of the land script.
struct StampMushroom
{
	float dx;
	float dz;
	uint8_t type;
	float angle;
	float scale;
};

constexpr auto k_Toadstool = k_ToadstoolObject;

// The clumps of mushrooms and toadstools of the first land of the story, apart from its fairy ring
constexpr std::array<StampMushroom, 5> k_FirstLandFive {{
    {1.31f, -2.14f, k_Mushroom, 5.421f, 0.577f},
    {0.42f, 1.05f, k_Mushroom, 0.523f, 1.508f},
    {-0.14f, -0.02f, k_Mushroom, 1.220f, 1.095f},
    {-3.51f, 2.09f, k_Mushroom, 0.870f, 1.095f},
    {1.90f, -0.99f, k_Mushroom, 5.326f, 1.508f},
}};
constexpr std::array<StampMushroom, 1> k_FirstLandOne {{{0.0f, 0.0f, k_Mushroom, 0.0f, 1.0f}}};
constexpr std::array<StampMushroom, 2> k_FirstLandToadstools {{
    {-0.22f, -0.27f, k_Toadstool, 3.177f, 0.165f},
    {0.21f, 0.27f, k_Toadstool, 3.815f, 0.165f},
}};
constexpr std::array<StampMushroom, 2> k_FirstLandToadstoolPair {{
    {0.51f, -0.22f, k_Toadstool, 0.574f, 0.372f},
    {-0.51f, 0.22f, k_Toadstool, 1.690f, 0.163f},
}};
// Twenty small magic mushrooms in three tight groups, and five toadstools, round a field of rocks
constexpr std::array<StampMushroom, 25> k_FirstLandTwentyFive {{
    {-14.96f, 9.62f, k_Mushroom, 4.486f, 0.460f},  {-14.46f, 8.84f, k_Mushroom, 0.014f, 0.488f},
    {-14.63f, 9.60f, k_Mushroom, 6.190f, 0.697f},  {-13.91f, 8.40f, k_Mushroom, 2.615f, 0.662f},
    {-14.11f, 9.91f, k_Mushroom, 5.170f, 0.685f},  {-9.45f, 3.46f, k_Toadstool, 1.922f, 0.386f},
    {-10.27f, 4.14f, k_Toadstool, 3.111f, 0.261f}, {-13.22f, 9.05f, k_Mushroom, 5.960f, 0.458f},
    {-13.57f, 10.07f, k_Mushroom, 2.940f, 0.662f}, {4.98f, 8.20f, k_Mushroom, 5.092f, 0.712f},
    {4.25f, 7.09f, k_Mushroom, 5.585f, 0.652f},    {3.78f, 7.54f, k_Mushroom, 3.552f, 0.648f},
    {4.37f, 7.70f, k_Mushroom, 3.661f, 0.680f},    {4.62f, 6.92f, k_Mushroom, 1.245f, 0.351f},
    {3.99f, 8.50f, k_Mushroom, 0.398f, 0.356f},    {4.57f, 8.38f, k_Mushroom, 1.419f, 0.374f},
    {7.69f, -18.10f, k_Mushroom, 2.680f, 0.381f},  {6.60f, -17.91f, k_Mushroom, 3.101f, 0.564f},
    {7.37f, -17.49f, k_Mushroom, 5.875f, 0.600f},  {6.72f, -18.41f, k_Mushroom, 1.567f, 0.586f},
    {6.27f, -17.08f, k_Mushroom, 0.888f, 0.358f},  {6.19f, -18.37f, k_Mushroom, 4.664f, 0.351f},
    {14.00f, -0.67f, k_Toadstool, 0.638f, 0.266f}, {16.29f, -10.48f, k_Toadstool, 3.095f, 0.268f},
    {16.77f, -9.02f, k_Toadstool, 2.884f, 0.388f},
}};
// The clumps of the third land of the story, apart from its fairy ring
constexpr std::array<StampMushroom, 2> k_ThirdLandChampiPair {{
    {-0.27f, -0.63f, k_Champi, 0.349f, 0.939f},
    {0.27f, 0.63f, k_Champi, 0.0f, 1.179f},
}};
constexpr std::array<StampMushroom, 3> k_ThirdLandToadstools {{
    {0.50f, 1.32f, k_Toadstool, -0.628f, 0.518f},
    {-1.67f, 0.29f, k_Toadstool, -0.628f, 0.279f},
    {1.18f, -1.62f, k_Toadstool, 0.0f, 1.0f},
}};
constexpr std::array<StampMushroom, 7> k_ThirdLandSeven {{
    {1.39f, -0.53f, k_Mushroom, 0.143f, 1.074f},
    {2.45f, -0.57f, k_Mushroom, 5.585f, 1.054f},
    {3.24f, -0.89f, k_Mushroom, 4.526f, 1.952f},
    {-1.86f, -5.66f, k_Mushroom, 0.488f, 2.019f},
    {-2.66f, 1.99f, k_Mushroom, 5.460f, 2.615f},
    {-1.68f, 3.57f, k_Mushroom, 3.213f, 1.993f},
    {-0.91f, 2.08f, k_Mushroom, 0.312f, 1.483f},
}};
constexpr std::array<StampMushroom, 5> k_ThirdLandChampiFive {{
    {-1.13f, -0.20f, k_Champi, 0.0f, 1.761f},
    {1.55f, -3.13f, k_Champi, 0.0f, 2.401f},
    {-7.33f, 9.77f, k_Champi, 0.0f, 1.697f},
    {3.79f, -1.68f, k_Champi, 0.0f, 1.817f},
    {3.14f, -4.75f, k_Champi, 0.0f, 1.595f},
}};
constexpr std::array<StampMushroom, 3> k_ThirdLandLargeToadstools {{
    {-10.88f, -1.86f, k_Toadstool, 0.0f, 1.039f},
    {4.15f, -1.21f, k_Toadstool, 0.0f, 0.842f},
    {6.72f, 3.07f, k_Toadstool, 0.0f, 0.912f},
}};
// The clumps of the fifth land of the story
constexpr std::array<StampMushroom, 12> k_FifthLandTwelve {{
    {1.16f, -1.52f, k_Mushroom, 4.322f, 0.648f},
    {3.63f, 3.47f, k_Mushroom, 3.947f, 0.458f},
    {2.25f, 2.61f, k_Mushroom, 4.859f, 0.465f},
    {1.19f, 3.41f, k_Mushroom, 3.217f, 0.476f},
    {0.88f, 2.88f, k_Mushroom, 0.476f, 0.490f},
    {-0.82f, -1.00f, k_Mushroom, 5.407f, 0.966f},
    {0.58f, -2.56f, k_Mushroom, 4.091f, 0.986f},
    {-0.08f, -1.97f, k_Mushroom, 5.602f, 0.967f},
    {-1.18f, -1.84f, k_Mushroom, 5.171f, 0.998f},
    {-1.26f, -2.51f, k_Mushroom, 5.664f, 1.019f},
    {-2.90f, -0.58f, k_Mushroom, 0.512f, 0.990f},
    {-3.47f, -0.35f, k_Mushroom, 5.763f, 0.998f},
}};
constexpr std::array<StampMushroom, 5> k_FifthLandFive {{
    {-2.77f, 3.16f, k_Mushroom, -1.534f, 0.398f},
    {-2.58f, 2.47f, k_Mushroom, -1.674f, 0.578f},
    {-2.95f, 2.78f, k_Mushroom, 0.418f, 0.759f},
    {3.66f, -4.18f, k_Mushroom, 0.418f, 0.398f},
    {4.62f, -4.24f, k_Mushroom, 0.0f, 0.458f},
}};
constexpr std::array<std::span<const StampMushroom>, 12> k_Clumps {{
    k_FirstLandFive,
    k_FirstLandOne,
    k_FirstLandToadstools,
    k_FirstLandToadstoolPair,
    k_FirstLandTwentyFive,
    k_ThirdLandChampiPair,
    k_ThirdLandToadstools,
    k_ThirdLandSeven,
    k_ThirdLandChampiFive,
    k_ThirdLandLargeToadstools,
    k_FifthLandTwelve,
    k_FifthLandFive,
}};

/// The ground of the clumps of the story. The middle of each clump is between the heights in metres and this far from
/// the sea, and each of its things is on ground of this slope or less (40 degrees, as the rise for each metre).
constexpr float k_ClumpSteepest = 0.8391f;
constexpr float k_ClumpLowest = 2.0f;
constexpr float k_ClumpHighest = 117.2f;
constexpr float k_ClumpNearestSea = 22.0f;
/// The nearest two clumps of the story are 60 m apart. A clump and a fairy ring or a group of pillars are 47 m or more
/// apart.
constexpr float k_ClumpGap = 60.0f;
constexpr float k_ClumpFormationGap = 47.0f;
/// A mushroom of the story is 0.4 m or more from the nearest tree or rock. A clump removes the trees and rocks nearer
/// than this to its things.
constexpr float k_ClumpClearing = 0.4f;

/// A big forest keeps a circle of this radius clear at scale 1, in metres
constexpr float k_BigForestClearing = 33.0f;

/// A land of the story: its fairy rings, its groups of lone pillars and its clumps for each square kilometre, its ring,
/// the first of its groups of k_PillarGroups and their number, and the first of its clumps of k_Clumps and their number
struct StoryLand
{
	float rings;
	float pillarGroups;
	Ring ring;
	size_t firstGroup;
	size_t groups;
	float clumpDensity;
	size_t firstClump;
	size_t clumps;
};

// The five lands of the story, then the lands of two, three and four gods. The first land has one ring, six groups of
// pillars and five clumps on 2.06 km2, the second land two groups on 2.65 km2, the third land one ring, one group and
// five clumps on 1.71 km2, and the fifth land two clumps on 3.90 km2. A land with no ring of its own takes the ring of
// the first land when the settings give it rings. A land with no clumps of its own takes the clumps of the first land
// when the settings give it clumps.
constexpr std::array<StoryLand, 8> k_StoryLands {{
    {1.0f / 2.0616f, 6.0f / 2.0616f, Ring::First, 0, 6, 5.0f / 2.0616f, 0, 5},
    {0.0f, 2.0f / 2.6517f, Ring::First, 6, 2, 0.0f, 0, 5},
    {1.0f / 1.7108f, 1.0f / 1.7108f, Ring::Third, 8, 1, 5.0f / 1.7108f, 5, 5},
    {0.0f, 0.0f, Ring::First, 0, 0, 0.0f, 0, 5},
    {0.0f, 0.0f, Ring::First, 0, 0, 2.0f / 3.8989f, 10, 2},
    {0.0f, 0.0f, Ring::First, 0, 0, 0.0f, 0, 5},
    {0.0f, 0.0f, Ring::First, 0, 0, 0.0f, 0, 5},
    {0.0f, 0.0f, Ring::First, 0, 0, 0.0f, 0, 5},
}};

/// The random numbers of a point of a grid, from the seed of the stage and the place of the point only
Random RandomAt(uint64_t stageSeed, int i, int j)
{
	return Random(static_cast<uint64_t>(Hash(stageSeed, i, j)) | (static_cast<uint64_t>(Hash(stageSeed, j, i)) << 32u));
}

/// A random direction: the cosine and the sine of a random turn
std::pair<float, float> Direction(Random& random)
{
	float dx = 0.0f;
	float dz = 0.0f;
	do
	{
		dx = random.NextRange(-1.0f, 1.0f);
		dz = random.NextRange(-1.0f, 1.0f);
	} while ((dx * dx) + (dz * dz) > 1.0f || (dx == 0.0f && dz == 0.0f));
	const float length = std::sqrt((dx * dx) + (dz * dz));
	return {dx / length, dz / length};
}

float Distance(float ax, float az, float bx, float bz)
{
	const float dx = ax - bx;
	const float dz = az - bz;
	return std::sqrt((dx * dx) + (dz * dz));
}

bool RingIn(biomes::Biome biome)
{
	return biome == biomes::Biome::Grassland || biome == biomes::Biome::TemperateForest || biome == biomes::Biome::Alpine;
}

/// The lone pillars of the story stand on grass, rock and the floor of woods, and never on sand, wet ground or the dry
/// ground of the deserts and the dead woods, where the stone of the rocks is not limestone
bool PillarIn(biomes::Biome biome)
{
	switch (biome)
	{
	case biomes::Biome::Grassland:
	case biomes::Biome::TemperateForest:
	case biomes::Biome::DenseJungle:
	case biomes::Biome::NordicWastes:
	case biomes::Biome::Alpine:
		return true;
	default:
		return false;
	}
}

/// A candidate start of a formation, with its random key
struct Candidate
{
	uint32_t key;
	int i;
	int j;
};

std::vector<Candidate> CandidatesOf(int side, float spacing, uint64_t stageSeed)
{
	const int points = static_cast<int>(static_cast<float>(side - 1) / spacing);
	std::vector<Candidate> candidates;
	candidates.reserve(static_cast<size_t>(points) * static_cast<size_t>(points));
	for (int i = 0; i < points; ++i)
	{
		for (int j = 0; j < points; ++j)
		{
			candidates.push_back({.key = RandomAt(stageSeed, i, j).Next(), .i = i, .j = j});
		}
	}
	std::ranges::sort(candidates, [](const Candidate& a, const Candidate& b) {
		return a.key != b.key ? a.key < b.key : (a.i != b.i ? a.i < b.i : a.j < b.j);
	});
	return candidates;
}

/// The ground and the circles that the formations keep out of
class Site
{
public:
	Site(const features::Ground& ground, std::span<const Feature> placed, std::span<const Circle> avoid)
	    : _ground(ground)
	{
		_keepOut.assign(avoid.begin(), avoid.end());
		for (const auto& feature : placed)
		{
			if (feature.kind == Kind::BigForest)
			{
				_keepOut.push_back({.x = feature.x, .z = feature.z, .radius = k_BigForestClearing * feature.size});
			}
		}
	}

	/// The ground at a place in metres, if a thing of a formation can stand there in a biome of the test
	template <typename BiomeTest>
	[[nodiscard]] std::optional<features::Spot> SpotAt(float x, float z, BiomeTest biomeTest) const
	{
		const float cx = x / k_MetresPerCorner;
		const float cz = z / k_MetresPerCorner;
		if (!features::IsFree(_ground, cx, cz))
		{
			return std::nullopt;
		}
		const int side = _ground.biomes.side;
		// NOLINTBEGIN(bugprone-incorrect-roundings): A change gives a different land.
		if (!biomeTest(_ground.biomes.At(std::clamp(static_cast<int>(cx + 0.5f), 0, side - 1),
		                                 std::clamp(static_cast<int>(cz + 0.5f), 0, side - 1))))
		// NOLINTEND(bugprone-incorrect-roundings)
		{
			return std::nullopt;
		}
		// The height and the slope are in the metres of the land that the game draws: the curve of the ground gives them
		return features::SpotAt(_ground, cx, cz);
	}

	/// Whether a circle in metres is clear of the circles to keep out of
	[[nodiscard]] bool Clear(float x, float z, float radius) const
	{
		return std::ranges::none_of(_keepOut, [x, z, radius](const Circle& circle) {
			return Distance(x, z, circle.x, circle.z) < circle.radius + radius;
		});
	}

private:
	const features::Ground& _ground;
	std::vector<Circle> _keepOut;
};

/// The farthest thing of a fairy ring from its middle, in metres
float ReachOf(std::span<const StampThing> stamp)
{
	float reach = 0.0f;
	for (const auto& thing : stamp)
	{
		reach = std::max(reach, std::sqrt((thing.dx * thing.dx) + (thing.dz * thing.dz)));
	}
	return reach;
}

bool NearAny(std::span<const Placed> formations, float x, float z, float gap)
{
	return std::ranges::any_of(formations, [x, z, gap](const Placed& other) { return Distance(x, z, other.x, other.z) < gap; });
}

/// Tries to put a fairy ring at a place in metres. Gives true when the ring fits.
bool PlaceRing(const Site& site, Ring ring, float x, float z, float cosine, float sine, std::vector<Feature>& placed)
{
	const auto stamp = StampOf(ring);
	const float reach = ReachOf(stamp);
	const auto middle = site.SpotAt(x, z, RingIn);
	if (!middle || middle->height < k_RingLowest || middle->height > k_RingHighest || middle->sea < k_RingNearestSea ||
	    middle->slope > k_RingSteepest || !site.Clear(x, z, reach))
	{
		return false;
	}
	const auto at = [x, z, cosine, sine](const StampThing& thing) {
		return std::pair {x + (thing.dx * cosine) - (thing.dz * sine), z + (thing.dx * sine) + (thing.dz * cosine)};
	};
	const bool fits = std::ranges::all_of(stamp, [&site, &at](const StampThing& thing) {
		const auto [tx, tz] = at(thing);
		const auto spot = site.SpotAt(tx, tz, RingIn);
		return spot && spot->slope <= k_RingSteepest;
	});
	if (!fits)
	{
		return false;
	}
	const float clearing = reach + k_RingClearing;
	std::erase_if(placed, [x, z, clearing](const Feature& feature) {
		return feature.kind != Kind::BigForest && Distance(feature.x, feature.z, x, z) < clearing;
	});
	for (const auto& thing : stamp)
	{
		const auto [tx, tz] = at(thing);
		placed.push_back({.kind = thing.kind,
		                  .type = thing.type,
		                  .x = tx,
		                  .z = tz,
		                  .angle = thing.angle,
		                  .size = thing.scale,
		                  .maxSize = thing.scale,
		                  .xAngle = thing.xAngle,
		                  .zAngle = thing.zAngle,
		                  .altitude = thing.altitude});
	}
	return true;
}

/// Tries to put a group of lone pillars with its first pillar at a place in metres. Gives true when the group fits.
bool PlacePillars(const Site& site, const PillarGroup& group, float x, float z, float cosine, float sine,
                  std::vector<Feature>& placed)
{
	std::vector<std::pair<float, float>> places;
	for (const auto& pillar : group.pillars)
	{
		const float px = x + (pillar.dx * cosine) - (pillar.dz * sine);
		const float pz = z + (pillar.dx * sine) + (pillar.dz * cosine);
		const auto spot = site.SpotAt(px, pz, PillarIn);
		if (!spot || spot->height < k_PillarLowest || spot->height > k_PillarHighest || spot->sea < k_PillarNearestSea ||
		    spot->slope > k_PillarSteepest || !site.Clear(px, pz, k_PillarTreeClearing))
		{
			return false;
		}
		places.emplace_back(px, pz);
	}
	std::erase_if(placed, [&places](const Feature& feature) {
		return feature.kind == Kind::Tree && std::ranges::any_of(places, [&feature](const std::pair<float, float>& place) {
			       return Distance(feature.x, feature.z, place.first, place.second) < k_PillarTreeClearing;
		       });
	});
	for (size_t k = 0; k < places.size(); ++k)
	{
		const auto& pillar = group.pillars[k];
		placed.push_back({.kind = Kind::Feature,
		                  .type = pillar.type,
		                  .x = places[k].first,
		                  .z = places[k].second,
		                  .angle = pillar.angle,
		                  .size = pillar.scale,
		                  .maxSize = pillar.scale});
	}
	return true;
}
/// Tries to put a clump of mushrooms with its middle at a place in metres. Gives true when the clump fits.
bool PlaceClump(const Site& site, std::span<const StampMushroom> clump, float x, float z, float cosine, float sine,
                std::vector<Feature>& placed)
{
	const auto middle = site.SpotAt(x, z, RingIn);
	if (!middle || middle->height < k_ClumpLowest || middle->height > k_ClumpHighest || middle->sea < k_ClumpNearestSea)
	{
		return false;
	}
	std::vector<std::pair<float, float>> places;
	for (const auto& thing : clump)
	{
		const float tx = x + (thing.dx * cosine) - (thing.dz * sine);
		const float tz = z + (thing.dx * sine) + (thing.dz * cosine);
		const auto spot = site.SpotAt(tx, tz, RingIn);
		if (!spot || spot->slope > k_ClumpSteepest || !site.Clear(tx, tz, k_ClumpClearing))
		{
			return false;
		}
		places.emplace_back(tx, tz);
	}
	std::erase_if(placed, [&places](const Feature& feature) {
		return feature.kind != Kind::BigForest && std::ranges::any_of(places, [&feature](const std::pair<float, float>& place) {
			       return Distance(feature.x, feature.z, place.first, place.second) < k_ClumpClearing;
		       });
	});
	for (size_t k = 0; k < places.size(); ++k)
	{
		const auto& thing = clump[k];
		placed.push_back({.kind = Kind::Mobile,
		                  .type = thing.type,
		                  .x = places[k].first,
		                  .z = places[k].second,
		                  .angle = thing.angle,
		                  .size = thing.scale,
		                  .maxSize = thing.scale});
	}
	return true;
}
} // namespace

Density formations::DensityOf(float unit)
{
	const auto count = static_cast<float>(k_StoryLands.size());
	const auto index = static_cast<uint8_t>(std::clamp(std::floor(unit * count), 0.0f, count - 1.0f));
	return {.land = index,
	        .rings = k_StoryLands[index].rings,
	        .pillarGroups = k_StoryLands[index].pillarGroups,
	        .clumps = k_StoryLands[index].clumpDensity};
}

uint32_t formations::CountFor(float perKm2, float km2, float unit)
{
	const float mean = std::max(perKm2 * km2, 0.0f);
	return static_cast<uint32_t>(std::floor(mean + std::clamp(unit, 0.0f, 0.99999994f)));
}

float formations::LandKm2(const features::Ground& ground)
{
	const auto land =
	    std::ranges::count_if(ground.heights.values, [](float height) { return height > terrain::k_SeaLevel + 1.0f; });
	return static_cast<float>(land) * k_MetresPerCorner * k_MetresPerCorner / k_SquareMetresPerKm2;
}

Result formations::Place(const features::Ground& ground, std::vector<Feature>& placed, std::span<const Circle> avoid,
                         uint64_t seed, const Settings& settings)
{
	Result result;
	const float km2 = LandKm2(ground);
	auto counts = Random(StageSeed(seed, Stage::FormationCounts));
	const float pick = counts.NextUnit();
	const auto& story = k_StoryLands[std::min<size_t>(settings.land.value_or(DensityOf(pick).land), k_StoryLands.size() - 1)];
	const auto rings = CountFor(settings.ringsPerKm2.value_or(story.rings), km2, counts.NextUnit());
	// A land of the story with no lone pillars has no groups to copy
	const auto groups =
	    story.groups == 0 ? 0u : CountFor(settings.pillarGroupsPerKm2.value_or(story.pillarGroups), km2, counts.NextUnit());
	// The count of the clumps comes after the other counts, so that the rings and the pillars of a seed stay the same
	const auto clumps = CountFor(settings.clumpsPerKm2.value_or(story.clumpDensity), km2, counts.NextUnit());
	const Site site(ground, placed, avoid);
	const int side = ground.biomes.side;

	const auto ringSeed = StageSeed(seed, Stage::FormationRings);
	for (const auto& candidate : CandidatesOf(side, settings.spacing, ringSeed))
	{
		if (result.rings.size() >= rings)
		{
			break;
		}
		auto random = RandomAt(ringSeed, candidate.i, candidate.j);
		random.Next();
		const float x = (static_cast<float>(candidate.i) + random.NextUnit()) * settings.spacing * k_MetresPerCorner;
		const float z = (static_cast<float>(candidate.j) + random.NextUnit()) * settings.spacing * k_MetresPerCorner;
		const auto ring = story.ring;
		const auto [cosine, sine] = Direction(random);
		if (NearAny(result.rings, x, z, k_RingGap) || !PlaceRing(site, ring, x, z, cosine, sine, placed))
		{
			continue;
		}
		result.rings.push_back({.x = x, .z = z, .stamp = static_cast<uint8_t>(ring)});
	}

	const auto pillarSeed = StageSeed(seed, Stage::FormationPillars);
	for (const auto& candidate : CandidatesOf(side, settings.spacing, pillarSeed))
	{
		if (result.pillarGroups.size() >= groups)
		{
			break;
		}
		auto random = RandomAt(pillarSeed, candidate.i, candidate.j);
		random.Next();
		const float x = (static_cast<float>(candidate.i) + random.NextUnit()) * settings.spacing * k_MetresPerCorner;
		const float z = (static_cast<float>(candidate.j) + random.NextUnit()) * settings.spacing * k_MetresPerCorner;
		const auto stamp = story.firstGroup + random.NextBelow(static_cast<uint32_t>(story.groups));
		const auto [cosine, sine] = Direction(random);
		if (NearAny(result.rings, x, z, k_RingGap) || NearAny(result.pillarGroups, x, z, k_PillarGap) ||
		    !PlacePillars(site, k_PillarGroups[stamp], x, z, cosine, sine, placed))
		{
			continue;
		}
		result.pillarGroups.push_back({.x = x, .z = z, .stamp = static_cast<uint8_t>(stamp)});
	}

	const auto clumpSeed = StageSeed(seed, Stage::FormationClumps);
	for (const auto& candidate : CandidatesOf(side, settings.spacing, clumpSeed))
	{
		if (result.clumps.size() >= clumps)
		{
			break;
		}
		auto random = RandomAt(clumpSeed, candidate.i, candidate.j);
		random.Next();
		const float x = (static_cast<float>(candidate.i) + random.NextUnit()) * settings.spacing * k_MetresPerCorner;
		const float z = (static_cast<float>(candidate.j) + random.NextUnit()) * settings.spacing * k_MetresPerCorner;
		const auto stamp = story.firstClump + random.NextBelow(static_cast<uint32_t>(story.clumps));
		const auto [cosine, sine] = Direction(random);
		if (NearAny(result.rings, x, z, k_ClumpFormationGap) || NearAny(result.pillarGroups, x, z, k_ClumpFormationGap) ||
		    NearAny(result.clumps, x, z, k_ClumpGap) || !PlaceClump(site, k_Clumps[stamp], x, z, cosine, sine, placed))
		{
			continue;
		}
		result.clumps.push_back({.x = x, .z = z, .stamp = static_cast<uint8_t>(stamp)});
	}
	return result;
}
