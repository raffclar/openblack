/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "EditorActions.h"

#include <optional>

#include <fmt/format.h>
#include <glm/vec2.hpp>

#include "ECS/Registry.h"
#include "ECS/Systems/EditorSystemInterface.h"
#include "EditorEntities.h"
#include "EditorNetworkEdits.h"

namespace openblack::editor
{

namespace
{
/// The distance across the land between a thing and its copy
constexpr glm::vec2 k_CopyOffset {8.0f, 0.0f};

std::string Describe(std::string_view done, const EditorHistory& history, bool atOnce,
                     const std::optional<HistoryResult>& result)
{
	if (!result.has_value() && history.Waiting())
	{
		return "The editor waits for the turn of its last change";
	}
	if (!result.has_value())
	{
		return fmt::format("Nothing to {}", done == "Undid" ? "undo" : "redo");
	}
	// In a network game the change comes at the next turn
	if (!atOnce)
	{
		return fmt::format("{} {}: it comes at the next turn", done == "Undid" ? "Sent the undo of" : "Sent the redo of",
		                   result->label);
	}
	if (result->skipped > 0)
	{
		return fmt::format("{} {} ({} gone, left as they are)", done, result->label, result->skipped);
	}
	return fmt::format("{} {}", done, result->label);
}
} // namespace

std::string StepLabel(std::string_view action, size_t count)
{
	return count == 1 ? std::string(action) : fmt::format("{} {} things", action, count);
}

void DeletePicked(const EditorActs& context)
{
	auto& selection = context.system.GetSelection();
	auto& history = context.system.GetHistory();
	const auto picked = selection.All();
	selection.Clear();
	HistoryStep step {.label = StepLabel("Delete", picked.size())};
	for (const auto entity : picked)
	{
		if (!context.registry.Valid(entity))
		{
			continue;
		}
		// A thing that the palette cannot make again goes with no step: undo cannot bring it back
		if (const auto recipe = RecipeOf(context.registry, entity))
		{
			step.changes.emplace_back(RemoveChange {.ref = history.Track(entity), .recipe = *recipe});
		}
		context.writes.Remove(entity);
	}
	// The recipe is from before the removal, so the step is the same when the removal comes at a later turn
	history.Push(std::move(step));
}

void DuplicatePicked(const EditorActs& context)
{
	auto& selection = context.system.GetSelection();
	auto& history = context.system.GetHistory();
	const auto picked = selection.All();
	std::vector<entt::entity> copies;
	HistoryStep step {.label = StepLabel("Duplicate", picked.size())};
	for (const auto entity : picked)
	{
		if (!context.registry.Valid(entity))
		{
			continue;
		}
		const auto sentBefore = context.writes.LastMakeRequest();
		const auto copy = context.writes.Duplicate(entity, k_CopyOffset);
		// In a network game the copy comes at a later turn, and the answer to its request gives it
		if (const auto sent = context.writes.LastMakeRequest(); !copy.has_value() && sent != sentBefore && sent != 0)
		{
			step.changes.emplace_back(MakeChange {.awaits = sent});
			continue;
		}
		if (!copy.has_value())
		{
			continue;
		}
		copies.push_back(*copy);
		if (const auto recipe = RecipeOf(context.registry, *copy))
		{
			step.changes.emplace_back(MakeChange {.ref = history.Track(*copy), .recipe = *recipe});
		}
	}
	if (!copies.empty())
	{
		selection.SelectAll(copies);
	}
	history.Push(std::move(step));
}

void RecordMade(const EditorActs& context, entt::entity entity, std::string label)
{
	if (!context.writes.ChangesAtOnce())
	{
		return;
	}
	auto& history = context.system.GetHistory();
	if (const auto recipe = RecipeOf(context.registry, entity))
	{
		history.Push({.label = std::move(label), .changes = {MakeChange {.ref = history.Track(entity), .recipe = *recipe}}});
	}
}

void RecordAllMade(const EditorActs& context, std::span<const entt::entity> entities, std::string_view action)
{
	if (!context.writes.ChangesAtOnce())
	{
		return;
	}
	auto& history = context.system.GetHistory();
	HistoryStep step {.label = StepLabel(action, entities.size())};
	for (const auto entity : entities)
	{
		if (!context.registry.Valid(entity))
		{
			continue;
		}
		// A thing that the palette cannot make again goes with no change: undo cannot bring it back
		if (const auto recipe = RecipeOf(context.registry, entity))
		{
			step.changes.emplace_back(MakeChange {.ref = history.Track(entity), .recipe = *recipe});
		}
	}
	history.Push(std::move(step));
}

void RecordAllSent(const EditorActs& context, std::span<const Request> requests, std::string_view action)
{
	HistoryStep step;
	for (const auto request : requests)
	{
		if (request != 0)
		{
			step.changes.emplace_back(MakeChange {.awaits = request});
		}
	}
	step.label = StepLabel(action, step.changes.size());
	context.system.GetHistory().Push(std::move(step));
}

std::string UndoStep(const EditorActs& context)
{
	WritesWorld world(context.registry, context.writes);
	auto& history = context.system.GetHistory();
	const auto result = history.Undo(world);
	if (result.has_value())
	{
		context.system.GetSelection().SelectAll(result->touched);
	}
	return Describe("Undid", history, context.writes.ChangesAtOnce(), result);
}

std::string RedoStep(const EditorActs& context)
{
	WritesWorld world(context.registry, context.writes);
	auto& history = context.system.GetHistory();
	const auto result = history.Redo(world);
	if (result.has_value())
	{
		context.system.GetSelection().SelectAll(result->touched);
	}
	return Describe("Redid", history, context.writes.ChangesAtOnce(), result);
}

bool WritesWorld::Exists(entt::entity entity) const
{
	return _registry.Valid(entity);
}

void WritesWorld::SetPose(entt::entity entity, const Pose& pose)
{
	_writes.SetPose(entity, pose);
}

MakeResult WritesWorld::Make(const Recipe& recipe)
{
	return _writes.Make(recipe);
}

void WritesWorld::Remove(entt::entity entity)
{
	_writes.Remove(entity);
}

void PoseEdit::Begin(const ecs::Registry& registry, std::span<const entt::entity> entities)
{
	_before.clear();
	for (const auto entity : entities)
	{
		if (const auto pose = PoseOf(registry, entity))
		{
			_before.emplace_back(entity, *pose);
		}
	}
}

void PoseEdit::Finish(const EditorActs& context, std::string_view action)
{
	// The editor sends the moves and the turns that wait, so that a drag ends with its last place
	context.writes.FlushNow();
	auto& history = context.system.GetHistory();
	HistoryStep step;
	for (const auto& [entity, before] : _before)
	{
		if (!context.registry.Valid(entity))
		{
			continue;
		}
		// In a network game the pose after the edit comes at the turn of the last command for the thing. A thing with
		// no command did not change.
		if (!context.writes.ChangesAtOnce())
		{
			const auto request = context.writes.TakeRequestOf(entity);
			if (request == 0)
			{
				continue;
			}
			if (history.IsOpen(request))
			{
				step.changes.emplace_back(
				    PoseChange {.ref = history.Track(entity), .before = before, .after = before, .awaits = request});
				continue;
			}
		}
		const auto after = PoseOf(context.registry, entity);
		if (after.has_value() && *after != before)
		{
			step.changes.emplace_back(PoseChange {.ref = history.Track(entity), .before = before, .after = *after});
		}
	}
	step.label = StepLabel(action, step.changes.size());
	_before.clear();
	history.Push(std::move(step));
}

} // namespace openblack::editor
