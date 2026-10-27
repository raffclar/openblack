/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "DescriptorSetTally.h"

#include <algorithm>
#include <array>
#include <limits>
#include <optional>

namespace openblack::graphics
{

namespace
{
/// No texture bound, as bgfx marks a free stage
constexpr uint16_t k_Unbound = std::numeric_limits<uint16_t>::max();

[[nodiscard]] uint32_t HashBindings(uint16_t program, const std::array<uint16_t, DescriptorSetTally::k_Stages>& textures)
{
	// FNV-1a over the program and every stage, as bgfx hashes a draw's program layout and all its bindings
	constexpr uint32_t k_Prime = 16777619u;
	uint32_t hash = 2166136261u;
	const auto add = [&hash](uint16_t value) {
		hash = (hash ^ (value & 0xFFu)) * k_Prime;
		hash = (hash ^ (value >> 8u)) * k_Prime;
	};
	add(program);
	std::ranges::for_each(textures, add);
	return hash;
}
} // namespace

void DescriptorSetTally::SetTexture(uint8_t stage, uint16_t texture) noexcept
{
	if (stage < _textures.size())
	{
		_textures.at(stage) = texture;
	}
}

void DescriptorSetTally::DiscardBindings() noexcept
{
	_textures.fill(k_Unbound);
}

void DescriptorSetTally::SetViewOrder(uint16_t view, ViewOrder order) noexcept
{
	if (view < _orders.size())
	{
		_orders.at(view) = order;
	}
}

void DescriptorSetTally::Submit(uint16_t view, uint16_t program, uint32_t depth, bool dropsBindings)
{
	_draws.push_back({
	    .view = view,
	    .program = program,
	    .depth = depth,
	    .sequence = static_cast<uint32_t>(_draws.size()),
	    .bindings = HashBindings(program, _textures),
	});
	if (dropsBindings)
	{
		DiscardBindings();
	}
}

void DescriptorSetTally::Append(DescriptorSetTally& other)
{
	_draws.reserve(_draws.size() + other._draws.size());
	for (auto draw : other._draws)
	{
		draw.sequence = static_cast<uint32_t>(_draws.size());
		_draws.push_back(draw);
	}
	other._draws.clear();
	other.DiscardBindings();
}

FrameDescriptorSets DescriptorSetTally::EndFrame()
{
	// What each view would take in the order its draws came, before they are sorted
	std::array<uint32_t, k_Views> setsInOrder {};
	std::array<std::optional<uint32_t>, k_Views> boundInOrder {};
	for (const auto& draw : _draws)
	{
		if (draw.view < k_Views && boundInOrder.at(draw.view) != draw.bindings)
		{
			boundInOrder.at(draw.view) = draw.bindings;
			++setsInOrder.at(draw.view);
		}
	}
	// bgfx's sort key for each view order: the view first, then what the order sorts by, ties in the order they came.
	// Each key is made once, in two words, so that the sort compares only numbers.
	_sorted.clear();
	_sorted.reserve(_draws.size());
	for (const auto& draw : _draws)
	{
		const auto order = draw.view < _orders.size() ? _orders.at(draw.view) : ViewOrder::ByProgram;
		uint32_t first = 0;
		uint32_t second = 0;
		switch (order)
		{
		case ViewOrder::ByProgram:
			first = draw.program;
			second = draw.depth;
			break;
		case ViewOrder::Sequential:
			break;
		case ViewOrder::DepthAscending:
			first = draw.depth;
			second = draw.program;
			break;
		case ViewOrder::DepthDescending:
			first = std::numeric_limits<uint32_t>::max() - draw.depth;
			second = draw.program;
			break;
		}
		_sorted.push_back({
		    .high = (static_cast<uint64_t>(draw.view) << 32U) | first,
		    .low = (static_cast<uint64_t>(second) << 32U) | draw.sequence,
		    .bindings = draw.bindings,
		});
	}
	std::ranges::sort(
	    _sorted, [](const SortedDraw& a, const SortedDraw& b) { return a.high != b.high ? a.high < b.high : a.low < b.low; });

	FrameDescriptorSets frame;
	frame.draws = static_cast<uint32_t>(_draws.size());
	// The set in use carries over from one view to the next
	std::optional<uint32_t> bound;
	for (const auto& draw : _sorted)
	{
		const auto view = static_cast<uint16_t>(draw.high >> 32U);
		if (frame.views.empty() || frame.views.back().view != view)
		{
			frame.views.push_back({
			    .view = view,
			    .draws = 0,
			    .sets = 0,
			    .setsInOrder = view < k_Views ? setsInOrder.at(view) : 0,
			});
		}
		auto& counted = frame.views.back();
		++counted.draws;
		if (bound != draw.bindings)
		{
			bound = draw.bindings;
			++counted.sets;
			++frame.sets;
		}
	}

	_draws.clear();
	DiscardBindings();
	return frame;
}

DescriptorSetRelief PlanDescriptorSetRelief(const FrameDescriptorSets& frame, std::span<const uint16_t> reorderable)
{
	DescriptorSetRelief relief;
	uint32_t left = frame.sets;
	for (const auto& view : frame.views)
	{
		if (std::ranges::find(reorderable, view.view) != reorderable.end() && view.setsInOrder * 2 <= view.sets)
		{
			relief.inOrder.push_back(view.view);
			left -= view.sets - view.setsInOrder;
		}
	}
	relief.skipOptional = left >= k_DescriptorSetsNear;
	return relief;
}

} // namespace openblack::graphics
