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
namespace openblack::ecs::systems
{
class EditorSystemInterface;
}

/// The editor's actions on the picked things that its tool bar, keys and panels share. Each action is one step of the
/// editor's history.
namespace openblack::editor
{

/// The label of a step for one thing or for a number of things, such as "Move" or "Move 3 things"
[[nodiscard]] std::string StepLabel(std::string_view action, size_t count);

/// What the actions on the picked things use: the state of the editor, the world and the writes of the editor. The
/// panels give it from their context, and a tool can give it with no panels.
struct EditorActs
{
	ecs::systems::EditorSystemInterface& system;
	ecs::Registry& registry;
	EditorWrites& writes;
};

/// The part of the context of the panels that the actions use
[[nodiscard]] inline EditorActs ActsOf(EditorContext& context)
{
	return {.system = context.system, .registry = context.registry, .writes = context.writes};
}

/// Removes all the picked things
void DeletePicked(const EditorActs& context);
inline void DeletePicked(EditorContext& context)
{
	DeletePicked(ActsOf(context));
}
/// Makes a copy of each picked thing beside it, and picks the copies
void DuplicatePicked(const EditorActs& context);
inline void DuplicatePicked(EditorContext& context)
{
	DuplicatePicked(ActsOf(context));
}
/// Keeps in the history a thing that the editor made, such as from the palette
void RecordMade(const EditorActs& context, entt::entity entity, std::string label);
inline void RecordMade(EditorContext& context, entt::entity entity, std::string label)
{
	RecordMade(ActsOf(context), entity, std::move(label));
}
/// Keeps in the history, as one step, all the things that the editor made at one time, such as a layout of the palette
void RecordAllMade(const EditorActs& context, std::span<const entt::entity> entities, std::string_view action);
inline void RecordAllMade(EditorContext& context, std::span<const entt::entity> entities, std::string_view action)
{
	RecordAllMade(ActsOf(context), entities, action);
}
/// Keeps in the history, as one step, the things that the commands of these requests make at a later turn, in a network
/// game. The answers to the requests give the things. A request of 0 makes nothing.
void RecordAllSent(const EditorActs& context, std::span<const Request> requests, std::string_view action);
inline void RecordAllSent(EditorContext& context, std::span<const Request> requests, std::string_view action)
{
	RecordAllSent(ActsOf(context), requests, action);
}
/// Undoes the newest step and picks the things that it changed. Gives a line that tells what it did.
[[nodiscard]] std::string UndoStep(const EditorActs& context);
[[nodiscard]] inline std::string UndoStep(EditorContext& context)
{
	return UndoStep(ActsOf(context));
}
/// Does again the step that the last undo undid and picks the things that it changed. Gives a line that tells what it
/// did.
[[nodiscard]] std::string RedoStep(const EditorActs& context);
[[nodiscard]] inline std::string RedoStep(EditorContext& context)
{
	return RedoStep(ActsOf(context));
}

/// The history of the editor, through the writes of the editor: at once, or with the commands of the turns
class WritesWorld final: public EditorWorld
{
public:
	WritesWorld(const ecs::Registry& registry, EditorWrites& writes)
	    : _registry(registry)
	    , _writes(writes)
	{
	}

	[[nodiscard]] bool Exists(entt::entity entity) const override;
	void SetPose(entt::entity entity, const Pose& pose) override;
	MakeResult Make(const Recipe& recipe) override;
	void Remove(entt::entity entity) override;

private:
	const ecs::Registry& _registry;
	EditorWrites& _writes;
};

/// The poses of some things before an edit, so that the edit becomes one step of the history when it stops
class PoseEdit
{
public:
	/// Keeps the poses of these things now
	void Begin(const ecs::Registry& registry, std::span<const entt::entity> entities);
	[[nodiscard]] bool Active() const { return !_before.empty(); }
	/// Adds a step for the things whose poses changed since Begin
	void Finish(const EditorActs& context, std::string_view action);
	void Finish(EditorContext& context, std::string_view action) { Finish(ActsOf(context), action); }
	/// Forgets the poses with no step
	void Cancel() { _before.clear(); }

private:
	std::vector<std::pair<entt::entity, Pose>> _before;
};

} // namespace openblack::editor
