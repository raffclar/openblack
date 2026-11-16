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

#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <string_view>

#include <entt/entity/entity.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

#include "Commands/PlayerCommands.h"
#include "Common/DebugCheats.h"
#include "EditorHistory.h"
#include "EditorPalette.h"
#include "Story/StoryLandSkip.h"

namespace openblack::ecs
{
class Registry;
}

namespace openblack::ecs::systems
{
class LivingActionSystemInterface;
}

/// The edits of the editor as commands of the turns. In a network game the editor does not change the world itself: it
/// sends each edit to the game server, and each computer does the edit at the same turn.
namespace openblack::editor
{

/// The smallest scale that the editor gives to the first axis of a thing
inline constexpr float k_SmallestEditorScale = 0.05f;
/// The largest scale that the editor gives to the first axis of a thing
inline constexpr float k_LargestEditorScale = 50.0f;

/// How the editor changes the things on the land. The game gives one that changes the world at once (a single player
/// game) and one that sends commands (a network game). The tests give a fake.
class EditorWrites
{
public:
	virtual ~EditorWrites() = default;
	/// True when the world changes at once. In a network game the world changes at a later turn, and the history of the
	/// editor learns each change from the answer to its request.
	[[nodiscard]] virtual bool ChangesAtOnce() const = 0;
	/// Puts an item of the palette on the land. Gives the thing, or none when the thing comes at a later turn.
	virtual std::optional<entt::entity> Place(const PlaceItem& item, glm::vec3 position, float yawRadians) = 0;
	/// Moves a thing to a point
	virtual void MoveTo(entt::entity entity, glm::vec3 position) = 0;
	/// Turns a thing about the up axis by an angle in radians
	virtual void Turn(entt::entity entity, float radians) = 0;
	/// Changes the size of a thing to a new scale of its first axis. The other axes keep their ratio to the first axis.
	virtual void ScaleTo(entt::entity entity, float firstAxis) = 0;
	virtual void Remove(entt::entity entity) = 0;
	/// Puts a copy of a thing beside it. Gives the copy, or none when the copy comes at a later turn or cannot be made.
	virtual std::optional<entt::entity> Duplicate(entt::entity entity, glm::vec2 offset) = 0;
	/// Puts a thing to a full pose, as an undo or a redo does
	virtual void SetPose(entt::entity entity, const Pose& pose) = 0;
	/// Makes a thing again from a recipe, as an undo or a redo does
	virtual MakeResult Make(const Recipe& recipe) = 0;
	/// Once a frame: sends the edits that wait
	virtual void Flush() {}
	/// Sends the edits that wait now, such as at the end of a drag
	virtual void FlushNow() {}
	/// The request of the newest command for a thing since the last call for the thing, or 0 when no command went
	[[nodiscard]] virtual Request TakeRequestOf([[maybe_unused]] entt::entity entity) { return 0; }
	/// The request of the newest command that makes a thing: a placement, a copy or a make again. 0 when no such command
	/// went.
	[[nodiscard]] virtual Request LastMakeRequest() const { return 0; }
};

/// The editor changes the world of this computer at once
class LocalWrites final: public EditorWrites
{
public:
	[[nodiscard]] bool ChangesAtOnce() const override { return true; }
	std::optional<entt::entity> Place(const PlaceItem& item, glm::vec3 position, float yawRadians) override;
	void MoveTo(entt::entity entity, glm::vec3 position) override;
	void Turn(entt::entity entity, float radians) override;
	void ScaleTo(entt::entity entity, float firstAxis) override;
	void Remove(entt::entity entity) override;
	std::optional<entt::entity> Duplicate(entt::entity entity, glm::vec2 offset) override;
	void SetPose(entt::entity entity, const Pose& pose) override;
	MakeResult Make(const Recipe& recipe) override;
};

/// The editor sends its edits as commands of the turns. A drag moves and turns the things in each frame, so the moves
/// and the turns of a thing wait and go at most once for each turn: the last move, and the sum of the turns. The
/// placements, the changes of size and the removals go at once, after the moves that wait.
class CommandWrites final: public EditorWrites
{
public:
	/// Sends a command to the game server
	using Submit = std::function<void(player_commands::Action action)>;
	/// The turn that this computer did last
	using TurnNow = std::function<uint32_t()>;
	/// Gives the command that makes a copy of a thing at the position of the copy, or none when the palette does not make
	/// its kind
	using CopyOf = std::function<std::optional<player_commands::Action>(entt::entity entity, glm::vec2 offset)>;
	/// Gives a new request number for a command, or 0 when the editor needs no answer
	using NewRequest = std::function<Request()>;

	CommandWrites(Submit submit, TurnNow turnNow, CopyOf copyOf, NewRequest newRequest = {});

	[[nodiscard]] bool ChangesAtOnce() const override { return false; }
	std::optional<entt::entity> Place(const PlaceItem& item, glm::vec3 position, float yawRadians) override;
	void MoveTo(entt::entity entity, glm::vec3 position) override;
	void Turn(entt::entity entity, float radians) override;
	void ScaleTo(entt::entity entity, float firstAxis) override;
	void Remove(entt::entity entity) override;
	std::optional<entt::entity> Duplicate(entt::entity entity, glm::vec2 offset) override;
	void SetPose(entt::entity entity, const Pose& pose) override;
	MakeResult Make(const Recipe& recipe) override;
	/// Sends the moves and the turns that wait, if the turn changed since the last send
	void Flush() override;
	void FlushNow() override;
	[[nodiscard]] Request TakeRequestOf(entt::entity entity) override;
	[[nodiscard]] Request LastMakeRequest() const override { return _lastMakeRequest; }

	/// The number of things with a move or a turn that waits
	[[nodiscard]] size_t Waiting() const { return _waiting.size(); }

private:
	struct Pending
	{
		std::optional<glm::vec3> position;
		float radians {0.0f};
	};

	/// Sends an edit of a thing with a new request
	void SubmitEdit(player_commands::EditorEdit edit);
	/// Sends a command that makes a thing with a new request
	void SubmitMake(player_commands::Action action);

	Submit _submit;
	TurnNow _turnNow;
	CopyOf _copyOf;
	NewRequest _newRequest;
	std::map<entt::entity, Pending> _waiting;
	std::optional<uint32_t> _sentAt;
	/// The request of the newest command for each thing, until TakeRequestOf takes it
	std::map<entt::entity, Request> _requestOf;
	Request _lastMakeRequest {0};
};

/// Why the world does not do an edit, or none when it does it. The rules keep no state.
[[nodiscard]] std::optional<std::string_view> EditRefusal(const player_commands::EditorEdit& edit) noexcept;

/// Whether the editor can change a thing in a network game: a thing that the editor picks, with a place on the land and a
/// model or a marker, and not a hand of a player
[[nodiscard]] bool EditableInNetworkGame(const ecs::Registry& registry, entt::entity entity);

/// Does an edit at the turn, as each computer of a network game does it. Gives the thing that the edit made or changed,
/// or none when the world refused the edit.
std::optional<entt::entity> ApplyEdit(const player_commands::EditorEdit& edit);

/// The edit that puts a thing to a full pose
[[nodiscard]] player_commands::EditorEdit PoseEditOf(entt::entity entity, const Pose& pose) noexcept;
/// The full pose of an edit of the kind Pose
[[nodiscard]] Pose PoseOf(const player_commands::EditorEdit& edit) noexcept;
/// The command that makes a thing again from a recipe
[[nodiscard]] player_commands::EditorRemake RemakeOf(const Recipe& recipe) noexcept;
/// The recipe of a command that makes a thing again
[[nodiscard]] Recipe RecipeOf(const player_commands::EditorRemake& remake) noexcept;

/// Why the world does not make a thing again, or none when it makes it. The rules keep no state.
[[nodiscard]] std::optional<std::string_view> RemakeRefusal(const player_commands::EditorRemake& remake) noexcept;
/// Makes a thing again at the turn, as each computer of a network game does it, and as the history of a single player
/// game makes it. Gives entt::null when the world refused it.
entt::entity ApplyRemake(const player_commands::EditorRemake& remake);

/// The placement of a copy of a thing beside it, as the palette makes the thing, or none when the palette does not make
/// its kind or the thing is a creature
[[nodiscard]] std::optional<player_commands::EditorEdit> CopyEditOf(const ecs::Registry& registry, entt::entity entity,
                                                                    glm::vec2 offset);
/// The command that makes a copy of a thing beside it: a placement, or for a creature a make again with the body of the
/// creature, as a single player game copies it. None when the palette does not make its kind.
[[nodiscard]] std::optional<player_commands::Action> CopyCommandOf(const ecs::Registry& registry, entt::entity entity,
                                                                   glm::vec2 offset);

/// The answer to question ED-2: in a network game, the history of each editor keeps only the steps of that editor. When
/// this is false, the history must also learn the steps of the other editors from the turns.
inline constexpr bool k_UndoesOnlyOwnSteps = true;

/// Whether the history of this computer takes the answer to a request: always for a command of this computer, and for a
/// command of another computer only when an editor also undoes the steps of the other editors
[[nodiscard]] constexpr bool HistoryTakesAnswer(bool sentByThisComputer) noexcept
{
	return sentByThisComputer || !k_UndoesOnlyOwnSteps;
}

/// Whether this computer sent a command of a god, or of an observer of the god of the observers
[[nodiscard]] bool SentByThisComputer(PlayerNames player, uint8_t observer);

/// A new request number from the history of the editor of this computer, or 0 with no editor
[[nodiscard]] Request NewEditorRequest();

/// What the editor sees of a thing after a command, for the answer to a request
[[nodiscard]] RequestAnswer AnswerOf(const ecs::Registry& registry, entt::entity entity, Awaited awaited);

/// The writes of the editor in a network game: the commands go to the game server as commands of the player of this
/// computer
[[nodiscard]] std::unique_ptr<EditorWrites> MakeNetworkWrites();

/// Why the world does not do an edit of a villager, or none when it does it. The rules keep no state.
[[nodiscard]] std::optional<std::string_view> VillagerEditRefusal(const player_commands::EditorVillagerEdit& edit) noexcept;

/// Does an edit of a villager at the turn, as each computer of a network game does it. The states of the villager change
/// through the actions, and with no actions a state does not change. False when the world refused the edit.
bool ApplyVillagerEdit(ecs::Registry& registry, const ecs::systems::LivingActionSystemInterface* actions,
                       const player_commands::EditorVillagerEdit& edit);

/// Sends an edit of a villager as a command of the turns. The edit comes at the next turn.
void SendVillagerEdit(const player_commands::EditorVillagerEdit& edit);

/// Sends a debug cheat of the player of this computer as a command of the turns. The cheat goes on at the next turn,
/// also in a single player game.
void SendDebugCheat(debug_cheats::Cheat cheat, bool on);

/// The cheats of the player of this computer that are on now, from the synced world
[[nodiscard]] uint32_t OwnDebugCheats();

/// The game that the player of this computer plays now, as the skip to a land of the story sees it
[[nodiscard]] story_land_skip::GameFacts OwnStoryGame();

/// Sends a skip to a land of the story after the first as a command of the turns (story_land_skip). The land changes at
/// the next turn. Gives false when the game does not show the skip, or the land does not go as a command.
bool SendStoryLandSkip(int32_t land);

} // namespace openblack::editor
