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

/// A big forest keeps a circle of this radius clear at scale 1, in metres
constexpr float k_BigForestClearing = 33.0f;

/// A land of the story: its fairy rings and its groups of lone pillars for each square kilometre, its ring, and the
/// first of its groups of k_PillarGroups and their number
struct StoryLand
{
	float rings;
	float pillarGroups;
	Ring ring;
	size_t firstGroup;
	size_t groups;
};

// The five lands of the story, then the lands of two, three and four gods. The first land has one ring and six groups
// of pillars on 2.06 km2, the second land two groups on 2.65 km2, and the third land one ring and one group on 1.71 km2.
// A land with no ring of its own takes the ring of the first land when the settings give it rings.
constexpr std::array<StoryLand, 8> k_StoryLands {{
    {1.0f / 2.0616f, 6.0f / 2.0616f, Ring::First, 0, 6},
    {0.0f, 2.0f / 2.6517f, Ring::First, 6, 2},
    {1.0f / 1.7108f, 1.0f / 1.7108f, Ring::Third, 8, 1},
    {0.0f, 0.0f, Ring::First, 0, 0},
    {0.0f, 0.0f, Ring::First, 0, 0},
    {0.0f, 0.0f, Ring::First, 0, 0},
    {0.0f, 0.0f, Ring::First, 0, 0},
    {0.0f, 0.0f, Ring::First, 0, 0},
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
		if (!biomeTest(_ground.biomes.At(std::clamp(static_cast<int>(cx + 0.5f), 0, side - 1),
		                                 std::clamp(static_cast<int>(cz + 0.5f), 0, side - 1))))
		{
			return std::nullopt;
		}
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
} // namespace

Density formations::DensityOf(float unit)
{
	const auto count = static_cast<float>(k_StoryLands.size());
	const auto index = static_cast<uint8_t>(std::clamp(std::floor(unit * count), 0.0f, count - 1.0f));
	return {.land = index, .rings = k_StoryLands[index].rings, .pillarGroups = k_StoryLands[index].pillarGroups};
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
	auto counts = Random(StageSeed(seed, 90));
	const float pick = counts.NextUnit();
	const auto& story = k_StoryLands[std::min<size_t>(settings.land.value_or(DensityOf(pick).land), k_StoryLands.size() - 1)];
	const auto rings = CountFor(settings.ringsPerKm2.value_or(story.rings), km2, counts.NextUnit());
	// A land of the story with no lone pillars has no groups to copy
	const auto groups =
	    story.groups == 0 ? 0u : CountFor(settings.pillarGroupsPerKm2.value_or(story.pillarGroups), km2, counts.NextUnit());
	const Site site(ground, placed, avoid);
	const int side = ground.biomes.side;

	const auto ringSeed = StageSeed(seed, 91);
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

	const auto pillarSeed = StageSeed(seed, 92);
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
	return result;
}
