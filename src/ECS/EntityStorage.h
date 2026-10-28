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
#include <utility>

#include <entt/entity/entity.hpp>
#include <entt/entity/storage.hpp>

namespace openblack::ecs::entity_storage
{

/// Removes from a storage outside the registry the row of an older version of an entity, and gives its value.
///
/// The registry gives the number of a destroyed entity again, with a new version. A storage keeps one row for each
/// number, so it cannot keep the new entity while it keeps the row of the destroyed one. For the new entity, contains()
/// gives false, but emplace() fails because the row of the older version uses the slot. Call this before contains() and
/// emplace() with an entity that the registry can give again.
///
/// The function gives no value when the storage holds no row for the number, or holds the row of the same version.
template <typename Type>
[[nodiscard]] std::optional<Type> TakeOlder(entt::storage<Type>& storage, entt::entity entity)
{
	using Traits = entt::entt_traits<entt::entity>;
	constexpr auto k_NoVersion = Traits::to_version(entt::tombstone);
	const auto heldVersion = storage.current(entity);
	if (heldVersion == k_NoVersion || heldVersion == Traits::to_version(entity))
	{
		return std::nullopt;
	}
	const auto held = Traits::construct(Traits::to_entity(entity), heldVersion);
	std::optional<Type> value {std::move(storage.get(held))};
	storage.erase(held);
	return value;
}

} // namespace openblack::ecs::entity_storage
