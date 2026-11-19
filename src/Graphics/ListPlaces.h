/*******************************************************************************
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
#include <optional>
#include <span>
#include <utility>
#include <vector>

#include <entt/entity/entity.hpp>

/// The place of each thing in the land's draw list. The renderer finds it again for each frame. This keeps the places by
/// the number of the entity. Thus a frame makes them again with no hash and no new memory.
namespace openblack::graphics::list_places
{

class Places
{
public:
	/// Keep the place of each thing in `list`. A thing that is in the list more than one time keeps its first place.
	void Make(std::span<const entt::entity> list)
	{
		for (const auto entity : _made)
		{
			_byIndex[IndexOf(entity)] = {};
		}
		_made.clear();
		_others.clear();
		for (uint32_t place = 0; place < static_cast<uint32_t>(list.size()); ++place)
		{
			const auto entity = list[place];
			if (entity == entt::null)
			{
				KeepOther(entity, place);
				continue;
			}
			const auto index = IndexOf(entity);
			if (index >= _byIndex.size())
			{
				_byIndex.resize(index + 1);
			}
			auto& entry = _byIndex[index];
			if (entry.entity == entity)
			{
				continue;
			}
			if (entry.entity == entt::null)
			{
				entry = {.entity = entity, .place = place};
				_made.push_back(entity);
				continue;
			}
			// Another entity with the same number, of an older or newer version, keeps its place in a short list
			KeepOther(entity, place);
		}
	}

	/// The place of a thing in the list, none when the list does not have it
	[[nodiscard]] std::optional<uint32_t> Of(entt::entity entity) const
	{
		const auto index = IndexOf(entity);
		if (entity != entt::null && index < _byIndex.size() && _byIndex[index].entity == entity)
		{
			return _byIndex[index].place;
		}
		const auto other = std::ranges::find(_others, entity, &std::pair<entt::entity, uint32_t>::first);
		return other != _others.end() ? std::optional(other->second) : std::nullopt;
	}

private:
	struct Entry
	{
		entt::entity entity {entt::null};
		uint32_t place {0};
	};

	void KeepOther(entt::entity entity, uint32_t place)
	{
		if (std::ranges::find(_others, entity, &std::pair<entt::entity, uint32_t>::first) == _others.end())
		{
			_others.emplace_back(entity, place);
		}
	}

	[[nodiscard]] static size_t IndexOf(entt::entity entity) { return static_cast<size_t>(entt::to_entity(entity)); }

	std::vector<Entry> _byIndex;
	/// The entities that have an entry in `_byIndex`
	std::vector<entt::entity> _made;
	std::vector<std::pair<entt::entity, uint32_t>> _others;
};

} // namespace openblack::graphics::list_places
