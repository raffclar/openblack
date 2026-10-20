/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "TownAreaRules.h"

#include "Common/X87.h"

using namespace openblack;
using namespace openblack::ecs;

namespace
{
/// A map unit is ten metres over 65536
constexpr float k_TenMetres = 10.0f;
constexpr float k_UnitShare = 1.0f / 65536.0f;
constexpr float k_UnitsPerShare = 65536.0f;

/// Map units to metres: times ten, rounded to a float, then over 65536
float Metres(int32_t units)
{
	return x87::Mul(static_cast<double>(units), k_TenMetres) * k_UnitShare;
}

/// Metres to map units: times 65536, over ten, rounded to a float, then cut to a whole number
int32_t Units(float metres)
{
	return map_coords::FtoL(x87::Div(static_cast<double>(metres * k_UnitsPerShare), k_TenMetres));
}

/// The size of a whole number, as the game takes it: the least whole number stays as it is
int32_t Size(int32_t value)
{
	const auto bits = static_cast<uint32_t>(value);
	return static_cast<int32_t>(value < 0 ? 0u - bits : bits);
}

/// Widen one axis of the box: the least corner moves out to the place less the radius, the greatest to the place
/// plus the radius
void WidenAxis(int32_t& least, int32_t& most, int32_t at, float radius)
{
	const float place = Metres(at);
	const float low = place - radius;
	if (low < Metres(least))
	{
		least = Units(low);
	}
	const float high = radius + place;
	if (high > Metres(most))
	{
		most = Units(high);
	}
}
} // namespace

void town_area::Widen(components::TownArea& area, const map_coords::MapCoords& at, float radius)
{
	WidenAxis(area.min.x, area.max.x, at.x, radius);
	WidenAxis(area.min.y, area.max.y, at.z, radius);
}

map_coords::MapCoords town_area::Centre(const components::TownArea& area)
{
	constexpr float k_Half = 0.5f;
	const float z = (Metres(area.max.y) + Metres(area.min.y)) * k_Half;
	const float x = (Metres(area.max.x) + Metres(area.min.x)) * k_Half;
	return {.x = Units(x), .z = Units(z), .altitude = 0.0f};
}

float town_area::Radius(const components::TownArea& area)
{
	constexpr float k_Half = 0.5f;
	const int32_t across = Size(map_coords::FtoL(Metres(area.max.x) - Metres(area.min.x)));
	const int32_t down = Size(map_coords::FtoL(Metres(area.max.y) - Metres(area.min.y)));
	return static_cast<float>(across > down ? across : down) * k_Half;
}
