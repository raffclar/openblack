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
#include "EditorPalette.h"

namespace openblack::ecs
{
class Registry;
}

/// The edits of the editor as commands of the turns. In a network game the editor does not change the world itself: it
/// sends each edit to the game server, and each computer does the edit at the same turn.
namespace openblack::editor
{

/// How the editor changes the things on the land. The game gives one that changes the world at once (a single player
/// game) and one that sends commands (a network game). The tests give a fake.
class EditorWrites
{
public:
	virtual ~EditorWrites() = default;
	/// True when the world changes at once, so that the editor keeps the history of its changes. In a network game the
	/// editor keeps no history: an undo would change the world on this computer only.
	[[nodiscard]] virtual bool ChangesAtOnce() const = 0;
	/// Puts an item of the palette on the land. Gives the thing, or none when the thing comes at a later turn.
	virtual std::optional<entt::entity> Place(const PlaceItem& item, glm::vec3 position, float yawRadians) = 0;
	/// Moves a thing to a point
	virtual void MoveTo(entt::entity entity, glm::vec3 position) = 0;
	/// Turns a thing about the up axis by an angle in radians
	virtual void Turn(entt::entity entity, float radians) = 0;
	virtual void Remove(entt::entity entity) = 0;
	/// Puts a copy of a thing beside it. Gives the copy, or none when the copy comes at a later turn or cannot be made.
	virtual std::optional<entt::entity> Duplicate(entt::entity entity, glm::vec2 offset) = 0;
	/// Once a frame: sends the edits that wait
	virtual void Flush() {}
	/// Sends the edits that wait now, such as at the end of a drag
	virtual void FlushNow() {}
};

/// The editor changes the world of this computer at once
class LocalWrites final: public EditorWrites
{
public:
	[[nodiscard]] bool ChangesAtOnce() const override { return true; }
	std::optional<entt::entity> Place(const PlaceItem& item, glm::vec3 position, float yawRadians) override;
	void MoveTo(entt::entity entity, glm::vec3 position) override;
	void Turn(entt::entity entity, float radians) override;
	void Remove(entt::entity entity) override;
	std::optional<entt::entity> Duplicate(entt::entity entity, glm::vec2 offset) override;
};

/// The editor sends its edits as commands of the turns. A drag moves and turns the things in each frame, so the moves
/// and the turns of a thing wait and go at most once for each turn: the last move, and the sum of the turns. The
/// placements and the removals go at once, after the moves that wait.
class CommandWrites final: public EditorWrites
{
public:
	/// Sends a command to the game server
	using Submit = std::function<void(player_commands::Action action)>;
	/// The turn that this computer did last
	using TurnNow = std::function<uint32_t()>;
	/// Gives the copy of a thing as an item of the palette, at the position of the copy, or none when the palette does not
	/// make its kind
	using CopyOf = std::function<std::optional<player_commands::EditorEdit>(entt::entity entity, glm::vec2 offset)>;

	CommandWrites(Submit submit, TurnNow turnNow, CopyOf copyOf);

	[[nodiscard]] bool ChangesAtOnce() const override { return false; }
	std::optional<entt::entity> Place(const PlaceItem& item, glm::vec3 position, float yawRadians) override;
	void MoveTo(entt::entity entity, glm::vec3 position) override;
	void Turn(entt::entity entity, float radians) override;
	void Remove(entt::entity entity) override;
	std::optional<entt::entity> Duplicate(entt::entity entity, glm::vec2 offset) override;
	/// Sends the moves and the turns that wait, if the turn changed since the last send
	void Flush() override;
	void FlushNow() override;

	/// The number of things with a move or a turn that waits
	[[nodiscard]] size_t Waiting() const { return _waiting.size(); }

private:
	struct Pending
	{
		std::optional<glm::vec3> position;
		float radians {0.0f};
	};

	Submit _submit;
	TurnNow _turnNow;
	CopyOf _copyOf;
	std::map<entt::entity, Pending> _waiting;
	std::optional<uint32_t> _sentAt;
};

/// Why the world does not do an edit, or none when it does it. The rules keep no state.
[[nodiscard]] std::optional<std::string_view> EditRefusal(const player_commands::EditorEdit& edit) noexcept;

/// Whether the editor can change a thing in a network game: a thing that the editor picks, with a place on the land and a
/// model or a marker, and not a hand of a player
[[nodiscard]] bool EditableInNetworkGame(const ecs::Registry& registry, entt::entity entity);

/// Does an edit at the turn, as each computer of a network game does it. False when the world refused the edit.
bool ApplyEdit(const player_commands::EditorEdit& edit);

/// The placement of a copy of a thing beside it, as the palette makes the thing, or none when the palette does not make
/// its kind or the thing is a creature
[[nodiscard]] std::optional<player_commands::EditorEdit> CopyEditOf(const ecs::Registry& registry, entt::entity entity,
                                                                    glm::vec2 offset);

/// The writes of the editor in a network game: the commands go to the game server as commands of the player of this
/// computer
[[nodiscard]] std::unique_ptr<EditorWrites> MakeNetworkWrites();

/// Sends a debug cheat of the player of this computer as a command of the turns. The cheat goes on at the next turn,
/// also in a single player game.
void SendDebugCheat(debug_cheats::Cheat cheat, bool on);

/// The cheats of the player of this computer that are on now, from the synced world
[[nodiscard]] uint32_t OwnDebugCheats();

} // namespace openblack::editor
