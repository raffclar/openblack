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

#include <optional>
#include <span>
#include <vector>

#include "landgen/Features.h"

/// The fairy rings of magic mushrooms, the clumps of mushrooms and toadstools, and the groups of lone pillars of rock, as
/// the lands of the story have them
namespace openblack::landgen::formations
{
/// The game's numbers of the loose objects of a fairy ring
constexpr uint8_t k_ChampiObject = 17;
constexpr uint8_t k_MagicMushroomObject = 18;
/// The game's number of the loose object of a toadstool, a poisoned mushroom
constexpr uint8_t k_ToadstoolObject = 19;
/// The game's numbers of the pillars of limestone of the features
constexpr uint8_t k_FatPillarLime = 24;
constexpr uint8_t k_Pillar2Lime = 30;
constexpr uint8_t k_Pillar3Lime = 34;

/// The two fairy rings of the story
enum class Ring : uint8_t
{
	/// The ring of the first land: ten magic mushrooms and two champignons round five small rocks
	First,
	/// The ring of the third land: six large magic mushrooms on a hill
	Third,
};

/// A circle of the map in metres that the formations keep out of, such as a cursed place
struct Circle
{
	float x;
	float z;
	float radius;
};

/// A formation on the land: the middle of a fairy ring or a clump, or the first pillar of a group, in metres
struct Placed
{
	float x;
	float z;
	/// For a fairy ring, its ring. For a clump or a group of pillars, the number of its clump or group of the story.
	uint8_t stamp;
};

/// The formations of a land of the story: the number of the land (0 to 7: the five lands of the story, then the lands
/// of two, three and four gods), and its fairy rings, groups of lone pillars and clumps for each square kilometre
struct Density
{
	uint8_t land;
	float rings;
	float pillarGroups;
	float clumps;
};

struct Settings
{
	/// The land of the story (0 to 7) whose ring and groups of pillars the land gets. Without a value, the seed picks one
	/// of the eight lands of the story.
	std::optional<uint8_t> land;
	/// The fairy rings and the groups of lone pillars for each square kilometre of land. Without a value, the land gets
	/// the density of its land of the story.
	std::optional<float> ringsPerKm2;
	std::optional<float> pillarGroupsPerKm2;
	/// The clumps of mushrooms and toadstools for each square kilometre of land. Without a value, the land gets the
	/// density of its land of the story.
	std::optional<float> clumpsPerKm2;
	/// The distance between the points where a formation can start, in corners
	float spacing {3.0f};
};

struct Result
{
	std::vector<Placed> rings;
	std::vector<Placed> pillarGroups;
	std::vector<Placed> clumps;
};

/// The number of formations on a land of an area in square kilometres, for a density for each square kilometre and a
/// random number from 0 to below 1. The fraction of the mean count becomes one more formation as often as its size.
[[nodiscard]] uint32_t CountFor(float perKm2, float km2, float unit);

/// The density of the formations of one of the eight lands of the story, from a random number from 0 to below 1. The
/// first land has one fairy ring and six groups of lone pillars on 2.1 km2, the second land two groups on 2.7 km2 and
/// the third land one ring and one group on 1.7 km2. The other five lands have none. The four pillars on the summit of
/// the third land are a shrine, not lone pillars. The first and the third lands have five clumps of mushrooms and
/// toadstools each, and the fifth land two clumps on 3.9 km2.
[[nodiscard]] Density DensityOf(float unit);

/// The area of the land in square kilometres: the corners that are not in the sea
[[nodiscard]] float LandKm2(const features::Ground& ground);

/// Puts the fairy rings and the groups of lone pillars on the land:
/// - The land is like one land of the story. It has the fairy ring and the groups of lone pillars of that land, with the
///   same number for each square kilometre. Five lands of the story have no ring and three have no lone pillars.
/// - Each fairy ring is a copy of the ring of its land of the story: the same mushrooms, Champi mushrooms and rocks at
///   the same places round the middle, with the same turns and scales. The whole ring turns by a random angle.
/// - A fairy ring stands on gentle grass, forest or high ground, at the heights and distances from the sea of the rings
///   of the story.
/// - Each group of lone pillars is a copy of a group of one, two or three pillars of limestone of its land. The group
///   stands on rough or flat ground between the heights of the pillars of the story, away from the sea.
/// - Each clump is a copy of a clump of magic mushrooms, Champi mushrooms or toadstools of its land: the first, the
///   third and the fifth lands have clumps. A clump stands on grass, forest or high ground, at the heights and
///   distances from the sea of the clumps of the story. The whole clump turns by a random angle.
///
/// A formation keeps away from the other formations, the big forests and the circles to avoid. A fairy ring removes the
/// trees and rocks under it, and a pillar removes the trees next to it. A clump removes the trees and rocks that touch
/// its mushrooms. The same seed gives the same formations.
/// The heights and slopes of the rules are in the metres that the game draws: the curve of the ground gives the metres of
/// each altitude.
[[nodiscard]] Result Place(const features::Ground& ground, std::vector<features::Feature>& placed,
                           std::span<const Circle> avoid, uint64_t seed, const Settings& settings);
} // namespace openblack::landgen::formations
