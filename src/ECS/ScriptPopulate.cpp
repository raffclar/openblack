/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "ScriptPopulate.h"

#include <cmath>

namespace openblack::ecs::script_populate
{

namespace
{
constexpr int32_t k_FirstType = 1;
constexpr int32_t k_LastType = 41;
constexpr double k_SpreadPerThing = 0.1;
constexpr double k_SpreadBase = 4.0;
/// The game's conversion of a quantity to a whole number gives up past 2 to the 63
constexpr float k_WholeLimit = 9.223372e18f;
} // namespace

bool IsValidType(int32_t type)
{
	return type >= k_FirstType && type <= k_LastType;
}

uint32_t CountOf(float quantity)
{
	if (!(std::fabs(quantity) < k_WholeLimit))
	{
		return 0;
	}
	// The low 32 bits of the whole number, its fraction dropped towards zero
	return static_cast<uint32_t>(static_cast<uint64_t>(static_cast<int64_t>(quantity)));
}

float SpreadOf(uint32_t count)
{
	return static_cast<float>(static_cast<double>(count) * k_SpreadPerThing + k_SpreadBase);
}

glm::vec3 PlaceOf(glm::vec3 centre, float spread, const std::function<float(float)>& floatRand)
{
	const float range = spread + spread;
	const float across = floatRand(range) - spread;
	const float up = floatRand(range) - spread;
	return {centre.x + across, centre.y + up, centre.z};
}

} // namespace openblack::ecs::script_populate
