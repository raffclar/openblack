/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "CreatureArena.h"

#include <cmath>

#include <algorithm>
#include <limits>

#include "3D/MapCoords.h"
#include "Common/GUtilsDistance.h"

using namespace openblack;
using namespace openblack::creature_arena;

namespace
{
/// A creature's running pace: its running share of its top speed, within 0 and 1, times its size's pace, with a tenth
/// to spare
constexpr float k_PaceSizeShare = 0.5f * 1.75f;
constexpr float k_PaceBase = 0.25f;
constexpr float k_PacePerSize = 20.0f;
constexpr float k_PaceMargin = 1.1f;

constexpr float k_CellSize = map_coords::k_CellSize;

/// The 16.16 position of a cell's corner moved on by a distance in metres, as the search turns a square into a place
int32_t CornerPlus(int32_t cell, float metres)
{
	// The cell's 16.16 value, wrapped as the game's 16-bit cells wrap
	const auto fixed = static_cast<int32_t>(static_cast<uint32_t>(static_cast<uint16_t>(cell)) << 16u);
	const double inMetres = static_cast<double>(fixed) * static_cast<double>(k_CellSize) / 65536.0;
	const double back = (inMetres + static_cast<double>(metres)) * 65536.0 / static_cast<double>(k_CellSize);
	return static_cast<int32_t>(back);
}
} // namespace

float creature_arena::ReuseDistance(float runShare, float size)
{
	const float pacePerShare = ((size * k_PaceSizeShare) + k_PaceBase) * k_PacePerSize;
	const float pace = std::clamp(runShare, 0.0f, 1.0f) * pacePerShare * k_PaceMargin;
	return pace * k_ReusePerPace;
}

std::optional<size_t> creature_arena::Nearest(std::span<const glm::ivec2> arenas, glm::ivec2 point, float within)
{
	std::optional<size_t> nearest;
	for (size_t i = 0; i < arenas.size(); ++i)
	{
		const float distance = gutils::GetDistanceInMetres(point, arenas[i]);
		if (distance < within)
		{
			nearest = i;
			within = distance;
		}
	}
	return nearest;
}

std::optional<glm::ivec2> creature_arena::FindClearPlace(glm::ivec2 start, float diameter, float range,
                                                         const UsableCell& usable)
{
	// A square of cells round the start's cell, each marked usable or not
	const auto side = static_cast<uint32_t>(map_coords::FtoL((range / k_CellSize) + 1.0f));
	const auto half = side / 2;
	const int32_t firstX = static_cast<int32_t>(map_coords::CellOf(start.x)) - static_cast<int32_t>(half);
	const int32_t firstZ = static_cast<int32_t>(map_coords::CellOf(start.y)) - static_cast<int32_t>(half);
	std::vector<bool> clear(static_cast<size_t>(side) * side, false);
	for (uint32_t i = 0; i < side; ++i)
	{
		for (uint32_t j = 0; j < side; ++j)
		{
			clear[(static_cast<size_t>(i) * side) + j] =
			    usable({firstX + static_cast<int32_t>(i), firstZ + static_cast<int32_t>(j)});
		}
	}

	// The circle covers a square of this many cells a side
	auto cells = static_cast<uint32_t>(map_coords::FtoL(diameter / k_CellSize));
	if (map_coords::FtoL(diameter) % map_coords::FtoL(k_CellSize) != 0)
	{
		++cells;
	}
	if (cells > side)
	{
		return std::nullopt;
	}
	const double halfDiameter = static_cast<double>(diameter) * 0.5;
	const double reach = static_cast<double>(diameter) * static_cast<double>(diameter) * 0.25;
	// Whether a cell of the square, by its place in it, lies under the circle
	const auto underCircle = [&](uint32_t p, uint32_t q) {
		const double across = halfDiameter - ((static_cast<double>(p) * k_CellSize) + (k_CellSize * 0.5));
		const double along = halfDiameter - ((static_cast<double>(q) * k_CellSize) + (k_CellSize * 0.5));
		return (along * along) + (across * across) <= reach;
	};

	std::optional<glm::ivec2> best;
	float bestDistance = std::numeric_limits<float>::max();
	const uint32_t places = side - cells;
	for (uint32_t a = 0; a < places; ++a)
	{
		for (uint32_t b = 0; b < places; ++b)
		{
			bool fits = true;
			for (uint32_t p = 0; p < cells && fits; ++p)
			{
				for (uint32_t q = 0; q < cells; ++q)
				{
					if (underCircle(p, q) && !clear[(static_cast<size_t>(a + p) * side) + b + q])
					{
						fits = false;
						break;
					}
				}
			}
			if (!fits)
			{
				continue;
			}
			const glm::ivec2 place {CornerPlus(firstX + static_cast<int32_t>(a), static_cast<float>(halfDiameter)),
			                        CornerPlus(firstZ + static_cast<int32_t>(b), static_cast<float>(halfDiameter))};
			const float distance = gutils::GetDistanceInMetres(start, place);
			if (distance < bestDistance)
			{
				best = place;
				bestDistance = distance;
			}
		}
	}
	return best;
}

std::optional<Placed> creature_arena::Place(glm::ivec2 start, float wantedRadius, const UsableCell& usable)
{
	for (float scale = 1.0f; scale > k_SmallestScale; scale -= k_ScaleStep)
	{
		const float radius = scale * wantedRadius;
		if (const auto place = FindClearPlace(start, radius + radius, k_SearchRange, usable))
		{
			return Placed {.centre = *place, .radius = radius, .scale = scale};
		}
	}
	return std::nullopt;
}

std::vector<glm::vec3> creature_arena::RingPoints(glm::vec2 centre, float radius, const GroundHeight& ground)
{
	constexpr auto k_FullTurn = static_cast<double>(2.0f * std::numbers::pi_v<float>);
	std::vector<glm::vec3> points;
	points.reserve(k_RingPoints);
	for (uint32_t i = 0; i < k_RingPoints; ++i)
	{
		const double angle = static_cast<double>(i) * k_FullTurn / (static_cast<double>(k_RingPoints) - 1.0);
		const glm::vec2 at {static_cast<float>((std::cos(angle) * radius) + centre.x),
		                    static_cast<float>((std::sin(angle) * radius) + centre.y)};
		points.emplace_back(at.x, ground(at), at.y);
	}
	return points;
}
