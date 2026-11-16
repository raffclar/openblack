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
#include <type_traits>
#include <utility>
#include <vector>

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

void EditorHistory::Adopt(HistoryRef ref, entt::entity entity)
{
	if (const auto other = _refs.find(entity); other != _refs.end() && other->second != ref)
	{
		_entities.erase(other->second);
		std::erase(_made, other->second);
		_refs.erase(other);
	}
	Rebind(ref, entity);
}

void EditorHistory::Push(HistoryStep step)
{
	if (step.changes.empty())
	{
		return;
	}
	_redo.clear();
	for (const auto& change : step.changes)
	{
		// A thing that comes at a later turn gets its number from the answer
		if (const auto* made = std::get_if<MakeChange>(&change);
		    made != nullptr && made->ref != 0 && std::ranges::find(_made, made->ref) == _made.end())
		{
			_made.push_back(made->ref);
		}
	}
	_undo.push_back(std::move(step));
	while (_undo.size() > k_MaxSteps)
	{
		_undo.pop_front();
	}
}

std::optional<HistoryResult> EditorHistory::Undo(EditorWorld& world)
{
	if (!CanUndo())
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
	if (!CanRedo())
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
	_made.clear();
	_open.clear();
	for (const auto entity : std::exchange(_keepAtNextClear, {}))
	{
		_made.push_back(Track(entity));
	}
}

void EditorHistory::AddMade(entt::entity entity)
{
	if (entity == entt::null)
	{
		return;
	}
	const auto ref = Track(entity);
	if (std::ranges::find(_made, ref) == _made.end())
	{
		_made.push_back(ref);
	}
}

void EditorHistory::KeepMadeAtNextClear(std::vector<entt::entity> entities)
{
	_keepAtNextClear = std::move(entities);
}

std::optional<std::string> EditorHistory::UndoLabel() const
{
	return _undo.empty() ? std::nullopt : std::optional(_undo.back().label);
}

std::optional<std::string> EditorHistory::RedoLabel() const
{
	return _redo.empty() ? std::nullopt : std::optional(_redo.back().label);
}

Request EditorHistory::NewRequest(uint32_t turn)
{
	// The numbers go round. A number that is still open waits for its answer, so the next number goes to the request.
	do
	{
		++_lastRequest;
	} while (_lastRequest == 0 || _open.contains(_lastRequest));
	_open.insert_or_assign(_lastRequest, turn);
	return _lastRequest;
}

std::optional<Awaited> EditorHistory::AwaitedFrom(Request request) const
{
	if (request == 0 || !IsOpen(request))
	{
		return std::nullopt;
	}
	std::optional<Awaited> awaited;
	const auto look = [request, &awaited](const HistoryStep& step) {
		for (const auto& change : step.changes)
		{
			std::visit(
			    [request, &awaited](const auto& one) {
				    using Kind = std::decay_t<decltype(one)>;
				    if (one.awaits == request)
				    {
					    awaited = std::is_same_v<Kind, PoseChange> ? Awaited::Pose : Awaited::Thing;
				    }
			    },
			    change);
		}
	};
	std::ranges::for_each(_undo, look);
	std::ranges::for_each(_redo, look);
	return awaited;
}

bool EditorHistory::AnswerChange(HistoryChange& change, Request request, const RequestAnswer& answer)
{
	if (auto* pose = std::get_if<PoseChange>(&change); pose != nullptr && pose->awaits == request)
	{
		pose->awaits = 0;
		// A refused command changed nothing, and a change that gives the same pose is no change
		if (answer.entity == entt::null || !answer.pose.has_value() || *answer.pose == pose->before)
		{
			return false;
		}
		pose->after = *answer.pose;
		return true;
	}
	if (auto* made = std::get_if<MakeChange>(&change); made != nullptr && made->awaits == request)
	{
		made->awaits = 0;
		if (made->ref == 0)
		{
			// The first answer of a placement gives the thing and how to make it again. A thing that the palette cannot
			// make again goes with no change, as in a single player game.
			if (answer.entity == entt::null || !answer.recipe.has_value())
			{
				return false;
			}
			made->ref = Track(answer.entity);
			made->recipe = *answer.recipe;
			if (std::ranges::find(_made, made->ref) == _made.end())
			{
				_made.push_back(made->ref);
			}
			return true;
		}
		if (answer.entity != entt::null)
		{
			Adopt(made->ref, answer.entity);
		}
		return true;
	}
	if (auto* removed = std::get_if<RemoveChange>(&change); removed != nullptr && removed->awaits == request)
	{
		removed->awaits = 0;
		// A refused command made nothing: the thing stays gone, as when its thing is gone in a single player game
		if (answer.entity != entt::null)
		{
			Adopt(removed->ref, answer.entity);
		}
	}
	return true;
}

void EditorHistory::Answer(Request request, const RequestAnswer& answer)
{
	if (_open.erase(request) == 0)
	{
		return;
	}
	const auto answerStep = [this, request, &answer](HistoryStep& step) {
		std::erase_if(step.changes,
		              [this, request, &answer](HistoryChange& change) { return !AnswerChange(change, request, answer); });
	};
	std::ranges::for_each(_undo, answerStep);
	std::ranges::for_each(_redo, answerStep);
	std::erase_if(_undo, [](const HistoryStep& step) { return step.changes.empty(); });
	std::erase_if(_redo, [](const HistoryStep& step) { return step.changes.empty(); });
}

void EditorHistory::ForgetOldRequests(uint32_t turn)
{
	std::vector<Request> old;
	for (const auto& [request, sentAt] : _open)
	{
		if (turn >= sentAt && turn - sentAt > k_MostAnswerTurns)
		{
			old.push_back(request);
		}
	}
	for (const auto request : old)
	{
		Answer(request, {});
	}
}

HistoryResult EditorHistory::Apply(HistoryStep& step, Direction direction, EditorWorld& world)
{
	HistoryResult result {.label = step.label};
	const auto touch = [&result](entt::entity entity) {
		if (std::ranges::find(result.touched, entity) == result.touched.end())
		{
			result.touched.push_back(entity);
		}
	};
	const auto make = [&](HistoryRef ref, const Recipe& recipe, Request& awaits) {
		const auto made = world.Make(recipe);
		if (made.entity == entt::null && made.request != 0)
		{
			// The thing comes at a later turn, and the answer to the request gives it
			awaits = made.request;
			Rebind(ref, entt::null);
			return;
		}
		if (made.entity == entt::null)
		{
			++result.skipped;
			return;
		}
		Rebind(ref, made.entity);
		touch(made.entity);
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
	const auto apply = [&](HistoryChange& change) {
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
		else if (auto* made = std::get_if<MakeChange>(&change))
		{
			if (direction == Direction::Back)
			{
				remove(made->ref);
			}
			else
			{
				make(made->ref, made->recipe, made->awaits);
			}
		}
		else if (auto* removed = std::get_if<RemoveChange>(&change))
		{
			if (direction == Direction::Back)
			{
				make(removed->ref, removed->recipe, removed->awaits);
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
