/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "HanoiRules.h"

#include "Common/GUtilsAngle.h"

namespace openblack::ecs::hanoi
{

namespace
{
/// The game turns radians into a game angle with two float products
constexpr float k_AngleCircle = 2048.0f;
constexpr float k_InverseTwoPi = 0.159154937f; ///< 0x3E22F983
/// A sine index past this reads past the table, and a cosine index reads a quarter of the circle further
constexpr uint32_t k_SineEntries = 2560;
constexpr uint32_t k_CosineOffset = 512;

/// A sum of 32-bit integers that wraps, as the game's sums do
[[nodiscard]] int32_t Wrap(int64_t value)
{
	return static_cast<int32_t>(static_cast<uint32_t>(static_cast<uint64_t>(value)));
}
} // namespace

float HeightOf(MobileObjectInfo kind)
{
	switch (kind)
	{
	case MobileObjectInfo::HanoiPuzzlePart1:
		return 4.0f;
	case MobileObjectInfo::HanoiPuzzlePart2:
		return 3.0f;
	case MobileObjectInfo::HanoiPuzzlePart3:
		return 2.0f;
	case MobileObjectInfo::HanoiPuzzlePart4:
		return 1.0f;
	default:
		return 0.0f;
	}
}

bool MayRestOn(MobileObjectInfo ring, MobileObjectInfo top)
{
	const auto ringKind = static_cast<int32_t>(ring);
	const auto topKind = static_cast<int32_t>(top);
	const auto base = static_cast<int32_t>(k_BaseKind);
	const auto smallest = static_cast<int32_t>(k_RingKinds.back());
	if (topKind == base)
	{
		return true;
	}
	if (topKind < base || topKind > smallest)
	{
		return false;
	}
	// A ring from the base kind up to the kind of the top piece is too large for it
	return ringKind < base || ringKind >= topKind;
}

int32_t GameAngleOf(float radians)
{
	const float turns = radians * k_AngleCircle;
	return map_coords::FtoL(turns * k_InverseTwoPi);
}

Steps StepsOf(uint16_t angle, float scale)
{
	const auto& table = gutils::SinTable();
	uint32_t index = angle;
	if (index + k_CosineOffset >= k_SineEntries)
	{
		index &= static_cast<uint32_t>(gutils::k_GameAngleMask);
	}
	const auto cosine = table.at(index + k_CosineOffset);
	const auto sine = table.at(index);
	return {.cosine = map_coords::FtoL(static_cast<float>(cosine) * scale),
	        .sine = map_coords::FtoL(static_cast<float>(sine) * scale)};
}

map_coords::MapCoords GridPlace(const map_coords::MapCoords& puzzle, Steps steps, int32_t across, int32_t along)
{
	const int64_t x = static_cast<int64_t>(steps.sine) * along + static_cast<int64_t>(steps.cosine) * across + puzzle.x;
	const int64_t z = static_cast<int64_t>(puzzle.z) +
	                  (static_cast<int64_t>(steps.cosine) * along - static_cast<int64_t>(steps.sine) * across);
	return {.x = Wrap(x), .z = Wrap(z), .altitude = puzzle.altitude};
}

Steps Spaced(Steps steps)
{
	return {.cosine = Wrap(static_cast<int64_t>(steps.cosine) * k_BaseSpacing),
	        .sine = Wrap(static_cast<int64_t>(steps.sine) * k_BaseSpacing)};
}

map_coords::MapCoords BasePlace(const map_coords::MapCoords& puzzle, Steps spaced, int32_t base)
{
	return GridPlace(puzzle, spaced, 0, base);
}

std::array<map_coords::MapCoords, k_Rings> StartRingPlaces(const map_coords::MapCoords& firstBase)
{
	std::array<map_coords::MapCoords, k_Rings> places {};
	auto place = firstBase;
	auto below = k_BaseKind;
	for (size_t ring = 0; ring < k_Rings; ++ring)
	{
		place.altitude = place.altitude + HeightOf(below);
		places.at(ring) = place;
		below = k_RingKinds.at(ring);
	}
	return places;
}

} // namespace openblack::ecs::hanoi
