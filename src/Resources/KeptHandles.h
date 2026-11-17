/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <optional>

#include <entt/container/dense_map.hpp>
#include <entt/core/fwd.hpp>

namespace openblack::resources
{

/// The answers of a resource cache for one walk of many things, so that each id costs one search of the cache in the
/// walk. The first question for an id goes to the cache, and the next questions for the same id get the same answer.
/// The cache gets the questions in the order of the walk, so a resource that loads when it is first asked for loads at
/// the same point as before. Clear the answers before each walk, because the cache can change between walks.
template <typename Resource>
class KeptHandles
{
public:
	/// Forgets all the answers and keeps the memory for the next walk
	void Clear() { _kept.clear(); }

	/// Whether the cache has the resource, loaded or registered
	template <typename Cache>
	[[nodiscard]] bool Contains(const Cache& cache, entt::id_type id)
	{
		auto& kept = _kept[id];
		if (!kept.contains.has_value())
		{
			kept.contains = cache.Contains(id);
		}
		return *kept.contains;
	}

	/// The resource, loaded now if it is not loaded yet, or null when the cache has none of the id
	template <typename Cache>
	[[nodiscard]] const Resource* Find(const Cache& cache, entt::id_type id)
	{
		auto& kept = _kept[id];
		if (!kept.found)
		{
			kept.resource = cache.Find(id);
			kept.found = true;
		}
		return kept.resource;
	}

	/// The resource only when it is loaded already, else null. This question does not load a resource.
	template <typename Cache>
	[[nodiscard]] const Resource* FindLoaded(const Cache& cache, entt::id_type id)
	{
		auto& kept = _kept[id];
		if (!kept.found)
		{
			kept.resource = cache.FindLoaded(id);
			// A resource that is not loaded can load later in the walk, so only a loaded resource is kept
			kept.found = kept.resource != nullptr;
		}
		return kept.resource;
	}

private:
	struct Kept
	{
		std::optional<bool> contains;
		const Resource* resource {nullptr};
		bool found {false};
	};

	entt::dense_map<entt::id_type, Kept> _kept;
};

} // namespace openblack::resources
