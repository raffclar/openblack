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

#include <glm/geometric.hpp>

#include "3D/MapCoords.h"
#include "Common/GUtilsDistance.h"
#include "Common/X87.h"
#include "Common/X87Vector.h"

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
constexpr float k_FixedPerUnit = 65536.0f;
constexpr float k_PerFixed = 1.0f / 65536.0f;

/// The 16.16 position of a cell's corner moved on by a distance in metres, as the search turns a square into a place
int32_t CornerPlus(int32_t cell, float metres)
{
	// The cell's 16.16 value, wrapped as the game's 16-bit cells wrap
	const auto fixed = static_cast<int32_t>(static_cast<uint32_t>(static_cast<uint16_t>(cell)) << 16u);
	const float inMetres = static_cast<float>(fixed) * k_CellSize * k_PerFixed;
	const float back = (inMetres + metres) * k_FixedPerUnit / k_CellSize;
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
	const float halfDiameter = diameter * 0.5f;
	const float reach = diameter * diameter * 0.25f;
	// Whether a cell of the square, by its place in it, lies under the circle
	const auto underCircle = [&](uint32_t p, uint32_t q) {
		const float across = halfDiameter - ((static_cast<float>(p) * k_CellSize) + (k_CellSize * 0.5f));
		const float along = halfDiameter - ((static_cast<float>(q) * k_CellSize) + (k_CellSize * 0.5f));
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
			const glm::ivec2 place {CornerPlus(firstX + static_cast<int32_t>(a), halfDiameter),
			                        CornerPlus(firstZ + static_cast<int32_t>(b), halfDiameter)};
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

bool creature_arena::KeepsArenaOff(const ThingInCell& thing, float creatureSize, glm::ivec2 cell)
{
	if (thing.kind == ThingKind::Temple)
	{
		return true;
	}
	if (thing.living || !thing.hasModel)
	{
		return false;
	}
	const bool tall = creatureSize * k_TallPerSize < thing.height;
	const bool ignored = !tall && (thing.kind == ThingKind::Building || thing.kind == ThingKind::Movable);
	if (ignored)
	{
		return false;
	}
	// The cell's middle at a height of zero. The game adds the squares from z to x.
	const glm::vec3 middle {static_cast<float>(cell.x * 10 + 5), 0.0f, static_cast<float>(cell.y * 10 + 5)};
	return std::ranges::any_of(thing.circles, [&middle](const GroundCircle& circle) {
		const glm::vec3 centre {circle.centre.x, circle.height, circle.centre.y};
		return x87::Distance<x87::SumOrder::Zyx>(middle, centre) - circle.radius < k_ThingClearance;
	});
}

std::vector<glm::vec3> creature_arena::RingPoints(glm::vec2 centre, float radius, const GroundHeight& ground)
{
	constexpr float k_FullTurn = 2.0f * std::numbers::pi_v<float>;
	std::vector<glm::vec3> points;
	points.reserve(k_RingPoints);
	for (uint32_t i = 0; i < k_RingPoints; ++i)
	{
		const float angle = (static_cast<float>(i) * k_FullTurn) / (static_cast<float>(k_RingPoints) - 1.0f);
		// The cosine is kept as a float before the multiply; the sine goes wide into it
		const float cosine = x87::RoundToFloat(x87::Cos(angle));
		const glm::vec2 at {(cosine * radius) + centre.x, x87::Mul(x87::Sin(angle), radius) + centre.y};
		points.emplace_back(at.x, ground(at), at.y);
	}
	return points;
}
