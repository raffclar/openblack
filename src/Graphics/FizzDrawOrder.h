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

#include <algorithm>
#include <functional>
#include <optional>
#include <span>
#include <unordered_map>
#include <vector>

#include <entt/entity/entity.hpp>

#include "ZSort.h"

/// The game draws the things of the land's draw list one by one, in the order of the list. A creature that fizzes in
/// or out of sight first writes its depth where its static is strong. Then it blends its body over that depth. A solid
/// thing that comes later in the list and stands behind the creature does not show where the creature wrote its depth.
/// A solid thing that came before it shows through the blended body.
///
/// The renderer draws the solid things together. Each fizzing creature divides the list into segments: segment 0 is
/// the things before the first fizzing creature, and segment n is the things after the creature n. The renderer draws
/// segment 0 with the solid things, and each later segment after the passes of the creature before it. A thing that
/// fades out in the distance comes after the solid things of its segment, at its place in the list, and before the
/// passes of the next creature.
namespace openblack::graphics::fizz_draw_order
{

/// The sort depths of a view sorted by depth, the greatest first, where the fizzing creatures and the segments go. The
/// top sixteen depths stay free for the draws that come straight after the land. A distance's bits never reach the top
/// bit, so all these depths come before the blended things.
inline constexpr uint32_t k_FirstSegmentDepth = 0xFFFFFFEFu;
/// Each fizzing creature has sixteen depths: its depth pass, its body pass, then the passes of its eyes, and the solid
/// things of the segment after it at the lowest depth
inline constexpr uint32_t k_DepthsPerCreature = 16;
/// Each segment that comes before a fizzing creature has a depth for each place of the list, for the things that fade
/// out in it. Then the passes of the creature after the segment follow.
inline constexpr uint32_t k_DepthsPerSegment = zsort::k_ListPlaces + k_DepthsPerCreature;
/// The most fizzing creatures that get a place of their own in the list order. This keeps the depths above the things
/// that fade out after the last fizzing creature.
inline constexpr uint32_t k_MaxRanked = (k_FirstSegmentDepth - zsort::AtOnce(0)) / k_DepthsPerSegment - 1;

/// The sort depth of a thing that fades out at this place of the list, in a segment before a fizzing creature
[[nodiscard]] constexpr uint32_t FadingDepth(uint32_t segment, uint32_t listPlace) noexcept
{
	return k_FirstSegmentDepth - (segment * k_DepthsPerSegment) - std::min(listPlace, zsort::k_ListPlaces - 1);
}

/// The sort depth of the depth pass of the fizzing creature at this rank, 0 for the first in the list
[[nodiscard]] constexpr uint32_t CreatureDepth(uint32_t rank) noexcept
{
	return k_FirstSegmentDepth - (rank * k_DepthsPerSegment) - zsort::k_ListPlaces;
}

/// The sort depth of the solid things of a segment after the first, below every pass of the creature before it
[[nodiscard]] constexpr uint32_t SegmentDepth(uint32_t segment) noexcept
{
	return CreatureDepth(segment - 1) - (k_DepthsPerCreature - 1);
}

static_assert(SegmentDepth(1) == FadingDepth(1, 0) + 1);
static_assert(FadingDepth(k_MaxRanked, zsort::k_ListPlaces) > zsort::AtOnce(0));

/// The fizzing creatures of the land's draw list in their order, and the segment of each listed thing after the first
/// of them
class Order
{
public:
	/// Find the fizzing creatures in the list, and the segment of each thing that comes after one of them
	[[nodiscard]] static Order Of(std::span<const entt::entity> list, const std::function<bool(entt::entity)>& fizzing);

	/// The segment that the thing is drawn in: 0 for a thing before every fizzing creature, and for a thing that is not
	/// in the list
	[[nodiscard]] uint32_t SegmentOf(entt::entity entity) const;
	/// The place of a fizzing creature among the fizzing creatures of the list, 0 for the first. None for a thing that
	/// is not a listed fizzing creature.
	[[nodiscard]] std::optional<uint32_t> RankOf(entt::entity entity) const;
	/// No listed creature fizzes, so all things are in segment 0
	[[nodiscard]] bool Empty() const noexcept { return _fizzing.empty(); }
	/// The sort depth of a thing that fades out at this place of the list (see zsort::AtOnce). A thing before a
	/// fizzing creature comes before the passes of the creature. A thing after the last fizzing creature, or not in the
	/// list, comes after all of them.
	[[nodiscard]] uint32_t AtOnceDepth(uint32_t listPlace) const;

private:
	std::vector<entt::entity> _fizzing;
	/// The places of the fizzing creatures in the list, the first place first
	std::vector<uint32_t> _places;
	std::unordered_map<entt::entity, uint32_t> _segments;
};

/// Record the segment of the next instance of a mesh. The instances of a mesh come in the order of their segments.
/// `starts[n - 1]` is the first instance of segment n, counted from the first instance of the mesh. `filled` is the
/// number of instances of the mesh before this one.
void NoteSegment(std::vector<uint32_t>& starts, uint32_t filled, uint32_t segment);

/// Call `draw(first, count, segment)` for each segment that has instances, from `first` for `count` instances. The
/// segments start as `NoteSegment` recorded them.
template <typename Draw>
void ForEachSegment(uint32_t first, uint32_t count, std::span<const uint32_t> starts, Draw&& draw)
{
	uint32_t begin = 0;
	for (uint32_t segment = 0; segment <= starts.size(); ++segment)
	{
		const uint32_t end = segment < starts.size() ? std::min(starts[segment], count) : count;
		if (end > begin)
		{
			draw(first + begin, end - begin, segment);
		}
		begin = std::max(begin, end);
	}
}

} // namespace openblack::graphics::fizz_draw_order
