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

namespace openblack::editor
{

namespace
{
/// The distance across the land between a thing and its copy
constexpr glm::vec2 k_CopyOffset {8.0f, 0.0f};

std::string Describe(std::string_view done, const std::optional<HistoryResult>& result)
{
	if (!result.has_value())
	{
		return fmt::format("Nothing to {}", done == "Undid" ? "undo" : "redo");
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

void DeletePicked(EditorContext& context)
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
		Remove(entity);
	}
	history.Push(std::move(step));
}

void DuplicatePicked(EditorContext& context)
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
		const auto copy = Duplicate(entity, k_CopyOffset);
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

void RecordMade(EditorContext& context, entt::entity entity, std::string label)
{
	auto& history = context.system.GetHistory();
	if (const auto recipe = RecipeOf(context.registry, entity))
	{
		history.Push({.label = std::move(label), .changes = {MakeChange {.ref = history.Track(entity), .recipe = *recipe}}});
	}
}

std::string UndoStep(EditorContext& context)
{
	RegistryWorld world;
	const auto result = context.system.GetHistory().Undo(world);
	if (result.has_value())
	{
		context.system.GetSelection().SelectAll(result->touched);
	}
	return Describe("Undid", result);
}

std::string RedoStep(EditorContext& context)
{
	RegistryWorld world;
	const auto result = context.system.GetHistory().Redo(world);
	if (result.has_value())
	{
		context.system.GetSelection().SelectAll(result->touched);
	}
	return Describe("Redid", result);
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

void PoseEdit::Finish(EditorContext& context, std::string_view action)
{
	auto& history = context.system.GetHistory();
	HistoryStep step;
	for (const auto& [entity, before] : _before)
	{
		if (!context.registry.Valid(entity))
		{
			continue;
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
