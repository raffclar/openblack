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
#include <functional>
#include <optional>
#include <span>
#include <vector>

#include <entt/entity/entity.hpp>

namespace openblack::editor
{

/// The things picked in the editor, which every panel shows and acts on. The last thing picked is the main pick: the
/// inspector shows it, and the camera looks at it. Each change of the picks has a count, so a panel can tell a new pick
/// from the one it last saw, even of the same thing again.
class EditorSelection
{
public:
	/// Picks only this thing
	void Select(entt::entity entity)
	{
		if (entity == entt::null)
		{
			Clear();
			return;
		}
		_entities.assign(1, entity);
		++_picks;
	}
	/// Picks this thing too, and makes it the main pick
	void Add(entt::entity entity)
	{
		if (entity == entt::null)
		{
			return;
		}
		std::erase(_entities, entity);
		_entities.push_back(entity);
		++_picks;
	}
	/// Lets go of this thing when it is picked, and picks it as the main pick when it is not
	void Toggle(entt::entity entity)
	{
		if (IsSelected(entity))
		{
			std::erase(_entities, entity);
			++_picks;
			return;
		}
		Add(entity);
	}
	/// Picks only these things. The last of them is the main pick.
	void SelectAll(std::span<const entt::entity> entities)
	{
		_entities.clear();
		for (const auto entity : entities)
		{
			if (entity != entt::null && !IsSelected(entity))
			{
				_entities.push_back(entity);
			}
		}
		++_picks;
	}
	void Clear()
	{
		if (!_entities.empty())
		{
			_entities.clear();
			++_picks;
		}
	}
	/// The main pick
	[[nodiscard]] std::optional<entt::entity> Get() const
	{
		return _entities.empty() ? std::nullopt : std::optional(_entities.back());
	}
	/// All the picked things, in the order of their picks
	[[nodiscard]] const std::vector<entt::entity>& All() const { return _entities; }
	[[nodiscard]] size_t Count() const { return _entities.size(); }
	[[nodiscard]] bool IsSelected(entt::entity entity) const { return std::ranges::find(_entities, entity) != _entities.end(); }
	[[nodiscard]] bool IsPrimary(entt::entity entity) const { return !_entities.empty() && _entities.back() == entity; }
	[[nodiscard]] bool Empty() const { return _entities.empty(); }
	/// How many times the selection has changed
	[[nodiscard]] uint32_t GetPicks() const { return _picks; }
	/// Lets go of the picked things that are gone, by the registry's say
	void Validate(const std::function<bool(entt::entity)>& exists)
	{
		if (std::erase_if(_entities, [&exists](entt::entity entity) { return !exists(entity); }) > 0)
		{
			++_picks;
		}
	}

private:
	std::vector<entt::entity> _entities;
	uint32_t _picks {0};
};

} // namespace openblack::editor
