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
#include <utility>
#include <vector>

#include <entt/entity/entity.hpp>

namespace openblack::ecs::reflection
{

/// The rows of some components that can have a change since the last hash of the world. Each write to a row of these
/// components marks the row. The hash takes again the bytes of the last hash for a row with no mark, and does not save
/// it. The marks keep the index of the entity, so a mark is also on each other version of the entity.
class RowMarks
{
public:
	/// The marks of one component
	class Marks
	{
	public:
		void Mark(entt::entity entity)
		{
			const auto index = static_cast<size_t>(entt::to_entity(entity));
			if (index >= _byIndex.size())
			{
				_byIndex.resize(index + 1, 0);
			}
			if (_byIndex[index] == 0)
			{
				_byIndex[index] = 1;
				_marked.push_back(static_cast<uint32_t>(index));
			}
		}
		[[nodiscard]] bool Has(entt::entity entity) const
		{
			const auto index = static_cast<size_t>(entt::to_entity(entity));
			return index < _byIndex.size() && _byIndex[index] != 0;
		}
		/// Removes each mark, and keeps the memory
		void Clear()
		{
			for (const auto index : _marked)
			{
				_byIndex[index] = 0;
			}
			_marked.clear();
		}
		[[nodiscard]] size_t Count() const { return _marked.size(); }

	private:
		/// A mark for each index of an entity. An index after the end has no mark.
		std::vector<uint8_t> _byIndex;
		std::vector<uint32_t> _marked;
	};

	/// Marks the row of the entity in the component with this storage id. The function does nothing for a component
	/// that the marks do not follow.
	void Mark(entt::id_type component, entt::entity entity)
	{
		if (auto* marks = Find(component); marks != nullptr)
		{
			marks->Mark(entity);
		}
	}
	/// The marks follow the rows of the component with this storage id
	void Follow(entt::id_type component)
	{
		if (Find(component) == nullptr)
		{
			_components.emplace_back(component, Marks {});
		}
	}
	/// A write that the marks do not see can change any row: the next marks are not complete
	void Forget() noexcept { _complete = false; }

	/// Gives the marks since the last call to the marks of a hash memo, and starts again with no marks. The marks of the
	/// memo are complete only when this object gave the last marks to the same memo, and a hash used those marks. The
	/// source is the registry of the rows.
	void GiveTo(RowMarks& taker, const void* source)
	{
		const bool sameChain = _lastTaker == &taker && taker._source == source && taker._used;
		const bool complete = _complete && sameChain;
		std::swap(_components, taker._components);
		// This object keeps the components that it follows, with no marks
		for (const auto& [id, marks] : taker._components)
		{
			Follow(id);
		}
		std::ranges::for_each(_components, [](auto& component) { component.second.Clear(); });
		taker._complete = complete;
		taker._source = source;
		taker._used = false;
		_lastTaker = &taker;
		_complete = true;
	}

	/// The hash of the rows of this source can use the marks: they are complete, and no hash used them before
	[[nodiscard]] bool Usable(const void* source) const noexcept { return _complete && !_used && _source == source; }
	/// A hash saved each row with no mark again or took its bytes from the last hash. The next marks can be complete.
	void Used() noexcept { _used = true; }

	/// The marks of the component with this storage id, or null when the marks do not follow it
	[[nodiscard]] const Marks* Of(entt::id_type component) const
	{
		const auto found =
		    std::ranges::find_if(_components, [component](const auto& entry) { return entry.first == component; });
		return found != _components.end() ? &found->second : nullptr;
	}

private:
	[[nodiscard]] Marks* Find(entt::id_type component)
	{
		const auto found =
		    std::ranges::find_if(_components, [component](const auto& entry) { return entry.first == component; });
		return found != _components.end() ? &found->second : nullptr;
	}

	std::vector<std::pair<entt::id_type, Marks>> _components;
	bool _complete {false};
	bool _used {false};
	const void* _source {nullptr};
	const RowMarks* _lastTaker {nullptr};
};

} // namespace openblack::ecs::reflection
