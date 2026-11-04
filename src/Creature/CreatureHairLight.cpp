/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "CreatureHairLight.h"

#include <algorithm>
#include <numeric>

namespace openblack::creature_hair_light
{

float SortKey(glm::vec3 place, glm::vec3 eye) noexcept
{
	const float x = place.x - eye.x;
	const float y = place.y - eye.y;
	const float z = place.z - eye.z;
	const float xy = x * x + y * y;
	return xy + z * z;
}

std::vector<size_t> SortedOrder(std::span<const float> keys)
{
	std::vector<size_t> order(keys.size());
	std::iota(order.begin(), order.end(), size_t {0});
	std::ranges::stable_sort(order, [keys](size_t a, size_t b) { return keys[a] > keys[b]; });
	return order;
}

Plan PlanFrame(std::span<const BodyDraw> draws)
{
	Plan plan;
	plan.hair.reserve(draws.size());
	for (size_t i = 0; i < draws.size(); ++i)
	{
		const auto& draw = draws[i];
		// The draw with the environment map does not keep colours: it only reads the last colours
		if (draw.environmentMapped)
		{
			plan.hair.emplace_back(plan.last.has_value() ? Source {FromDraw {*plan.last}} : Source {FromFrameBefore {}});
			continue;
		}
		plan.hair.emplace_back(std::nullopt);
		if (draw.keepsHairLight)
		{
			plan.last = i;
		}
	}
	return plan;
}

Look LookOf(const Source& source, std::span<const std::optional<Colours>> known, const Carried& before)
{
	if (const auto* draw = std::get_if<FromDraw>(&source); draw != nullptr)
	{
		if (draw->draw < known.size() && known[draw->draw].has_value())
		{
			return *known[draw->draw];
		}
		return AtPlace {draw->draw};
	}
	if (const auto* colours = std::get_if<Colours>(&before); colours != nullptr)
	{
		return *colours;
	}
	return Kept {};
}

Carried Carry(const Plan& plan, std::span<const std::optional<Colours>> known, const Carried& before)
{
	if (!plan.last.has_value())
	{
		return before;
	}
	if (*plan.last < known.size() && known[*plan.last].has_value())
	{
		return *known[*plan.last];
	}
	return Kept {};
}

} // namespace openblack::creature_hair_light
