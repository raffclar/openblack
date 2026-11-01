/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "EditorHistory.h"

#include <algorithm>
#include <ranges>
#include <utility>

namespace openblack::editor
{

HistoryRef EditorHistory::Track(entt::entity entity)
{
	if (const auto found = _refs.find(entity); found != _refs.end())
	{
		return found->second;
	}
	const auto ref = _nextRef++;
	_refs.emplace(entity, ref);
	_entities.emplace(ref, entity);
	return ref;
}

entt::entity EditorHistory::Resolve(HistoryRef ref) const
{
	const auto found = _entities.find(ref);
	return found != _entities.end() ? found->second : entt::null;
}

void EditorHistory::Rebind(HistoryRef ref, entt::entity entity)
{
	if (const auto old = _entities.find(ref); old != _entities.end())
	{
		_refs.erase(old->second);
	}
	_entities.insert_or_assign(ref, entity);
	if (entity != entt::null)
	{
		_refs.insert_or_assign(entity, ref);
	}
}

void EditorHistory::Push(HistoryStep step)
{
	if (step.changes.empty())
	{
		return;
	}
	_redo.clear();
	_undo.push_back(std::move(step));
	while (_undo.size() > k_MaxSteps)
	{
		_undo.pop_front();
	}
}

std::optional<HistoryResult> EditorHistory::Undo(EditorWorld& world)
{
	if (_undo.empty())
	{
		return std::nullopt;
	}
	auto step = std::move(_undo.back());
	_undo.pop_back();
	auto result = Apply(step, Direction::Back, world);
	_redo.push_back(std::move(step));
	return result;
}

std::optional<HistoryResult> EditorHistory::Redo(EditorWorld& world)
{
	if (_redo.empty())
	{
		return std::nullopt;
	}
	auto step = std::move(_redo.back());
	_redo.pop_back();
	auto result = Apply(step, Direction::Forward, world);
	_undo.push_back(std::move(step));
	return result;
}

void EditorHistory::Clear()
{
	_undo.clear();
	_redo.clear();
	_entities.clear();
	_refs.clear();
}

std::optional<std::string> EditorHistory::UndoLabel() const
{
	return _undo.empty() ? std::nullopt : std::optional(_undo.back().label);
}

std::optional<std::string> EditorHistory::RedoLabel() const
{
	return _redo.empty() ? std::nullopt : std::optional(_redo.back().label);
}

HistoryResult EditorHistory::Apply(const HistoryStep& step, Direction direction, EditorWorld& world)
{
	HistoryResult result {.label = step.label};
	const auto touch = [&result](entt::entity entity) {
		if (std::ranges::find(result.touched, entity) == result.touched.end())
		{
			result.touched.push_back(entity);
		}
	};
	const auto make = [&](HistoryRef ref, const Recipe& recipe) {
		const auto made = world.Make(recipe);
		if (made == entt::null)
		{
			++result.skipped;
			return;
		}
		Rebind(ref, made);
		touch(made);
	};
	const auto remove = [&](HistoryRef ref) {
		// A thing that is already gone needs nothing more
		const auto entity = Resolve(ref);
		if (entity != entt::null && world.Exists(entity))
		{
			world.Remove(entity);
		}
		std::erase(result.touched, entity);
		Rebind(ref, entt::null);
	};
	const auto apply = [&](const HistoryChange& change) {
		if (const auto* pose = std::get_if<PoseChange>(&change))
		{
			const auto entity = Resolve(pose->ref);
			if (entity == entt::null || !world.Exists(entity))
			{
				++result.skipped;
				return;
			}
			world.SetPose(entity, direction == Direction::Back ? pose->before : pose->after);
			touch(entity);
		}
		else if (const auto* made = std::get_if<MakeChange>(&change))
		{
			if (direction == Direction::Back)
			{
				remove(made->ref);
			}
			else
			{
				make(made->ref, made->recipe);
			}
		}
		else if (const auto* removed = std::get_if<RemoveChange>(&change))
		{
			if (direction == Direction::Back)
			{
				make(removed->ref, removed->recipe);
			}
			else
			{
				remove(removed->ref);
			}
		}
	};
	// Undo goes back through the changes of the step from the last, and redo goes through them from the first
	if (direction == Direction::Back)
	{
		std::ranges::for_each(step.changes | std::views::reverse, apply);
		// The things touched keep the order of the step, so the last of them is the main pick again
		std::ranges::reverse(result.touched);
	}
	else
	{
		std::ranges::for_each(step.changes, apply);
	}
	return result;
}

} // namespace openblack::editor
