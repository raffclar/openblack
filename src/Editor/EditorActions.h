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

#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <entt/entity/fwd.hpp>

#include "EditorContext.h"
#include "EditorHistory.h"

namespace openblack::ecs
{
class Registry;
}

/// The editor's actions on the picked things that its tool bar, keys and panels share. Each action is one step of the
/// editor's history.
namespace openblack::editor
{

/// The label of a step for one thing or for a number of things, such as "Move" or "Move 3 things"
[[nodiscard]] std::string StepLabel(std::string_view action, size_t count);

/// Removes all the picked things
void DeletePicked(EditorContext& context);
/// Makes a copy of each picked thing beside it, and picks the copies
void DuplicatePicked(EditorContext& context);
/// Keeps in the history a thing that the editor made, such as from the palette
void RecordMade(EditorContext& context, entt::entity entity, std::string label);
/// Undoes the newest step and picks the things that it changed. Gives a line that tells what it did.
[[nodiscard]] std::string UndoStep(EditorContext& context);
/// Does again the step that the last undo undid and picks the things that it changed. Gives a line that tells what it
/// did.
[[nodiscard]] std::string RedoStep(EditorContext& context);

/// The poses of some things before an edit, so that the edit becomes one step of the history when it stops
class PoseEdit
{
public:
	/// Keeps the poses of these things now
	void Begin(const ecs::Registry& registry, std::span<const entt::entity> entities);
	[[nodiscard]] bool Active() const { return !_before.empty(); }
	/// Adds a step for the things whose poses changed since Begin
	void Finish(EditorContext& context, std::string_view action);
	/// Forgets the poses with no step
	void Cancel() { _before.clear(); }

private:
	std::vector<std::pair<entt::entity, Pose>> _before;
};

} // namespace openblack::editor
