/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "FizzDrawOrder.h"

#include <algorithm>

namespace openblack::graphics::fizz_draw_order
{

Order Order::Of(std::span<const entt::entity> list, const std::function<bool(entt::entity)>& fizzing)
{
	Order order;
	for (const auto entity : list)
	{
		if (entity == entt::null)
		{
			continue;
		}
		// The creature itself stays in the segment before it: its own passes divide the list
		const auto segment = static_cast<uint32_t>(order._fizzing.size());
		if (segment > 0)
		{
			order._segments.insert_or_assign(entity, segment);
		}
		if (segment < k_MaxRanked && fizzing(entity))
		{
			order._fizzing.push_back(entity);
		}
	}
	return order;
}

uint32_t Order::SegmentOf(entt::entity entity) const
{
	const auto found = _segments.find(entity);
	return found != _segments.end() ? found->second : 0;
}

std::optional<uint32_t> Order::RankOf(entt::entity entity) const
{
	const auto found = std::ranges::find(_fizzing, entity);
	if (found == _fizzing.end())
	{
		return std::nullopt;
	}
	return static_cast<uint32_t>(std::distance(_fizzing.begin(), found));
}

void NoteSegment(std::vector<uint32_t>& starts, uint32_t filled, uint32_t segment)
{
	while (starts.size() < segment)
	{
		starts.push_back(filled);
	}
}

} // namespace openblack::graphics::fizz_draw_order
