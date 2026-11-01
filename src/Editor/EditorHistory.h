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

#include <deque>
#include <optional>
#include <string>
#include <unordered_map>
#include <variant>
#include <vector>

#include <entt/entity/entity.hpp>
#include <glm/mat3x3.hpp>
#include <glm/vec3.hpp>

#include "EditorPalette.h"
#include "Enums.h"

/// The editor's undo and redo: the changes that the editor made to the things on the land, step by step
namespace openblack::editor
{

/// Where a thing is, how it turns and its size
struct Pose
{
	glm::vec3 position {0.0f};
	glm::mat3 rotation {1.0f};
	/// The angles that the thing keeps with its turn, if it keeps them
	std::optional<glm::vec3> keptAngles;
	glm::vec3 scale {1.0f};

	friend bool operator==(const Pose&, const Pose&) = default;
};

/// The body of a creature that the editor makes again
struct CreatureCopy
{
	PlayerNames owner {PlayerNames::PLAYER_ONE};
	entt::id_type mind {0};
	float size {1.0f};
	float alignment {0.0f};
	float fatness {0.5f};
	float strength {0.5f};

	friend bool operator==(const CreatureCopy&, const CreatureCopy&) = default;
};

/// All that the editor needs to make a thing again: the palette item, its pose and, for a creature, its body
struct Recipe
{
	PlaceItem item;
	Pose pose;
	std::optional<CreatureCopy> creature;

	friend bool operator==(const Recipe&, const Recipe&) = default;
};

/// How the history changes the things on the land. The game gives one that works on the registry; the tests give a
/// fake.
class EditorWorld
{
public:
	virtual ~EditorWorld() = default;
	[[nodiscard]] virtual bool Exists(entt::entity entity) const = 0;
	virtual void SetPose(entt::entity entity, const Pose& pose) = 0;
	/// Makes a thing from a recipe, or gives entt::null when it cannot
	virtual entt::entity Make(const Recipe& recipe) = 0;
	virtual void Remove(entt::entity entity) = 0;
};

/// A number that stays with a thing in the history while undo and redo remove it and make it again with a new entity
using HistoryRef = uint32_t;

/// A thing moved, turned or changed its size
struct PoseChange
{
	HistoryRef ref {0};
	Pose before;
	Pose after;
};

/// The editor made a thing. Undo removes it and redo makes it again.
struct MakeChange
{
	HistoryRef ref {0};
	Recipe recipe;
};

/// The editor removed a thing. Undo makes it again and redo removes it.
struct RemoveChange
{
	HistoryRef ref {0};
	Recipe recipe;
};

using HistoryChange = std::variant<PoseChange, MakeChange, RemoveChange>;

/// One step of the history: all the changes of one action of the editor, such as a move of all the picked things
struct HistoryStep
{
	std::string label;
	std::vector<HistoryChange> changes;
};

/// What an undo or a redo did
struct HistoryResult
{
	std::string label;
	/// The things that the step changed and that are on the land after it, to pick
	std::vector<entt::entity> touched;
	/// The changes that the step could not do, because their thing is gone
	size_t skipped {0};
};

/// The steps that the editor can undo and redo. A new step removes the steps that the editor can redo. The history
/// keeps the newest k_MaxSteps steps.
class EditorHistory
{
public:
	static constexpr size_t k_MaxSteps = 100;

	/// The number that the history uses for a thing. The same thing always gets the same number.
	[[nodiscard]] HistoryRef Track(entt::entity entity);
	/// The thing that has a number now, or entt::null when no thing has it
	[[nodiscard]] entt::entity Resolve(HistoryRef ref) const;

	/// Adds a step. A step with no changes does nothing.
	void Push(HistoryStep step);
	/// Undoes the newest step, if there is one
	std::optional<HistoryResult> Undo(EditorWorld& world);
	/// Does again the step that the last undo undid, if there is one
	std::optional<HistoryResult> Redo(EditorWorld& world);
	/// Forgets all the steps and the numbers of the things, as when the land loads again
	void Clear();

	[[nodiscard]] bool CanUndo() const { return !_undo.empty(); }
	[[nodiscard]] bool CanRedo() const { return !_redo.empty(); }
	[[nodiscard]] size_t UndoCount() const { return _undo.size(); }
	[[nodiscard]] size_t RedoCount() const { return _redo.size(); }
	/// The label of the step that the next undo undoes
	[[nodiscard]] std::optional<std::string> UndoLabel() const;
	/// The label of the step that the next redo does
	[[nodiscard]] std::optional<std::string> RedoLabel() const;

private:
	enum class Direction : uint8_t
	{
		Back,
		Forward,
	};

	HistoryResult Apply(const HistoryStep& step, Direction direction, EditorWorld& world);
	/// Gives a number to a new entity, after the history made its thing again
	void Rebind(HistoryRef ref, entt::entity entity);

	std::deque<HistoryStep> _undo;
	std::vector<HistoryStep> _redo;
	std::unordered_map<HistoryRef, entt::entity> _entities;
	std::unordered_map<entt::entity, HistoryRef> _refs;
	HistoryRef _nextRef {1};
};

} // namespace openblack::editor
