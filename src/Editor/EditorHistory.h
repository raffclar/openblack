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
#include <map>
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

/// A number that the editor of this computer gives to a command of a network game, so that the turn of the command tells
/// the editor what the command made or changed. 0 is no request.
using Request = uint16_t;

/// What a make of a thing gives
struct MakeResult
{
	/// The new thing, or entt::null when the world cannot make it or the thing comes at a later turn
	entt::entity entity {entt::null};
	/// Not 0 when the thing comes at a later turn: the answer to this request gives the thing
	Request request {0};
};

/// How the history changes the things on the land. The game gives one that changes the world at once and one that sends
/// commands of the turns; the tests give a fake.
class EditorWorld
{
public:
	virtual ~EditorWorld() = default;
	[[nodiscard]] virtual bool Exists(entt::entity entity) const = 0;
	virtual void SetPose(entt::entity entity, const Pose& pose) = 0;
	/// Makes a thing from a recipe
	virtual MakeResult Make(const Recipe& recipe) = 0;
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
	/// Not 0 while the change waits for the answer to this request, which gives the pose after the change
	Request awaits {0};
};

/// The editor made a thing. Undo removes it and redo makes it again.
struct MakeChange
{
	/// 0 while the change waits for the first answer, which gives the thing and its recipe
	HistoryRef ref {0};
	Recipe recipe;
	/// Not 0 while the change waits for the answer to this request, which gives the thing
	Request awaits {0};
};

/// The editor removed a thing. Undo makes it again and redo removes it.
struct RemoveChange
{
	HistoryRef ref {0};
	Recipe recipe;
	/// Not 0 while the change waits for the answer to this request, which gives the thing that undo made again
	Request awaits {0};
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

/// What the turn of a request gives to the history: the thing that the command made or changed, and what the editor sees of
/// it after the command
struct RequestAnswer
{
	/// The thing, or entt::null when the world refused the command
	entt::entity entity {entt::null};
	/// How the editor makes the thing again, if the palette makes its kind
	std::optional<Recipe> recipe;
	/// The pose of the thing after the command
	std::optional<Pose> pose;
};

/// What a change of the history waits for from the answer to a request
enum class Awaited : uint8_t
{
	/// The thing that the command made
	Thing,
	/// The pose of the thing after the command
	Pose,
};

/// The steps that the editor can undo and redo. A new step removes the steps that the editor can redo. The history
/// keeps the newest k_MaxSteps steps.
///
/// In a network game a change of the world comes at a later turn. Each command of the editor then has a request number,
/// and the turn of the command gives the answer: the thing that the command made, or the pose after it. The changes that
/// wait for an answer get it then. Undo and redo wait until each request has its answer.
class EditorHistory
{
public:
	static constexpr size_t k_MaxSteps = 100;
	/// A request with no answer after this number of turns counts as a command that the world refused
	static constexpr uint32_t k_MostAnswerTurns = 300;

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
	/// Forgets all the steps and the numbers of the things, as when the land loads again. The things that the history
	/// must keep at this clear then get new numbers and become the things that the editor made.
	void Clear();
	/// Keeps things as the things that the editor made, in this order, through the next clear. A world that the editor
	/// loads gives back the things that the editor made before it saved the world. The history has no step for them.
	void KeepMadeAtNextClear(std::vector<entt::entity> entities);
	/// Keeps a thing as a thing that the editor made, with no step. In a network game the history has no steps, and the
	/// things that the editors place at the turns come here, so that a save of the land files writes them.
	void AddMade(entt::entity entity);

	/// Gives a new request number for a command that this computer sends at a turn. The number is not 0 and no open
	/// request has it.
	[[nodiscard]] Request NewRequest(uint32_t turn);
	/// Whether a request has no answer yet
	[[nodiscard]] bool IsOpen(Request request) const { return _open.contains(request); }
	/// What a change waits for from the answer to an open request, or none when no change waits for it
	[[nodiscard]] std::optional<Awaited> AwaitedFrom(Request request) const;
	/// Gives the answer to a request to the changes that wait for it, and closes the request. A change that the answer
	/// leaves with nothing to do goes out of its step, and a step with no changes goes out of the history.
	void Answer(Request request, const RequestAnswer& answer);
	/// Closes the requests that have no answer after k_MostAnswerTurns turns, as commands that the world refused
	void ForgetOldRequests(uint32_t turn);
	/// True while a request has no answer. Undo and redo then wait.
	[[nodiscard]] bool Waiting() const { return !_open.empty(); }

	[[nodiscard]] bool CanUndo() const { return !_undo.empty() && !Waiting(); }
	[[nodiscard]] bool CanRedo() const { return !_redo.empty() && !Waiting(); }
	[[nodiscard]] size_t UndoCount() const { return _undo.size(); }
	[[nodiscard]] size_t RedoCount() const { return _redo.size(); }
	/// The label of the step that the next undo undoes
	[[nodiscard]] std::optional<std::string> UndoLabel() const;
	/// The label of the step that the next redo does
	[[nodiscard]] std::optional<std::string> RedoLabel() const;
	/// The numbers of the things that the editor made, in the order that it made them. A number stays after its step
	/// leaves the history, until the history is cleared. Resolve gives the thing that has the number now.
	[[nodiscard]] const std::vector<HistoryRef>& MadeRefs() const { return _made; }

private:
	enum class Direction : uint8_t
	{
		Back,
		Forward,
	};

	HistoryResult Apply(HistoryStep& step, Direction direction, EditorWorld& world);
	/// Gives a number to a new entity, after the history made its thing again
	void Rebind(HistoryRef ref, entt::entity entity);
	/// Gives a number to the thing of an answer. If the thing already has another number, such as from AddMade, that
	/// number goes.
	void Adopt(HistoryRef ref, entt::entity entity);
	/// Gives an answer to one change. False when the change has nothing more to do and goes out of its step.
	bool AnswerChange(HistoryChange& change, Request request, const RequestAnswer& answer);

	std::deque<HistoryStep> _undo;
	std::vector<HistoryStep> _redo;
	std::unordered_map<HistoryRef, entt::entity> _entities;
	std::unordered_map<entt::entity, HistoryRef> _refs;
	HistoryRef _nextRef {1};
	std::vector<HistoryRef> _made;
	std::vector<entt::entity> _keepAtNextClear;
	/// The requests with no answer, and the turn that each one went at
	std::map<Request, uint32_t> _open;
	Request _lastRequest {0};
};

} // namespace openblack::editor
