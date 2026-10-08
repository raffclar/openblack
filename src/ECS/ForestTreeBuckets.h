/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <cstddef>
#include <cstdint>

#include <algorithm>
#include <optional>
#include <utility>
#include <vector>

#include <entt/entity/entity.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

#include "Common/GUtilsDistance.h"

namespace openblack::ecs
{
/// Each forest's trees, gathered in one pass over every tree. A forest sees its trees in the order the pass visited
/// them, the same order it would get by walking every tree itself and keeping its own.
class ForestTreeBuckets
{
public:
	/// A tree of the forest that has stopped growing: its entity and its x and z
	struct GrownTree
	{
		entt::entity entity;
		glm::vec2 position;
	};

	struct Bucket
	{
		/// Every tree of the forest
		size_t count {0};
		/// The trees that have stopped growing, in the order they were added
		std::vector<GrownTree> grown;
	};

	/// No trees; one empty bucket per id. `ids` must be ascending (the forest list's order)
	template <typename Ids>
	void Reset(const Ids& ids)
	{
		_ids.clear();
		_buckets.clear();
		for (const auto id : ids)
		{
			_ids.push_back(id);
		}
		_buckets.resize(_ids.size());
	}

	/// A tree of forest `forestId`; a tree whose forest has no bucket is not kept
	void Add(uint32_t forestId, entt::entity entity, bool grown, glm::vec2 position)
	{
		const auto index = IndexOf(forestId);
		if (!index)
		{
			return;
		}
		auto& bucket = _buckets[*index];
		++bucket.count;
		if (grown)
		{
			bucket.grown.push_back({entity, position});
		}
	}

	/// The forest's trees (an empty bucket for an id that was not given to Reset)
	[[nodiscard]] const Bucket& Of(uint32_t forestId) const
	{
		static const Bucket k_None {};
		const auto index = IndexOf(forestId);
		return index ? _buckets[*index] : k_None;
	}

private:
	[[nodiscard]] std::optional<size_t> IndexOf(uint32_t forestId) const
	{
		const auto it = std::ranges::lower_bound(_ids, forestId);
		if (it == _ids.end() || *it != forestId)
		{
			return std::nullopt;
		}
		return static_cast<size_t>(it - _ids.begin());
	}

	std::vector<uint32_t> _ids;
	std::vector<Bucket> _buckets;
};

/// The grown trees with their distance in metres (x and z) to `centre`, nearest first. Trees at the same distance keep
/// the order std::ranges::sort leaves them in, which depends only on the order they are given in.
[[nodiscard]] inline std::vector<std::pair<float, entt::entity>>
GrownByDistance(const std::vector<ForestTreeBuckets::GrownTree>& grown, glm::vec3 centre)
{
	std::vector<std::pair<float, entt::entity>> sorted;
	sorted.reserve(grown.size());
	for (const auto& tree : grown)
	{
		sorted.emplace_back(gutils::GetDistanceInMetres(tree.position, glm::vec2(centre.x, centre.z)), tree.entity);
	}
	std::ranges::sort(sorted, [](const auto& lhs, const auto& rhs) { return lhs.first < rhs.first; });
	return sorted;
}
} // namespace openblack::ecs
