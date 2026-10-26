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

#include <map>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include <entt/entity/entity.hpp>

#include "ECS/ChatRules.h"
#include "Enums.h"

namespace openblack::player_commands
{
struct ChatSpeech;
struct ChatTaunt;
} // namespace openblack::player_commands

namespace openblack::gui
{
class TextDatabase;
} // namespace openblack::gui

namespace openblack::ecs::systems
{

/// The chat of a network game on this computer (see chat): it sends the lines and the taunts of the player as commands,
/// and does the lines that come in at their turn. It keeps the speech bubbles of the creatures and the names of the
/// gods. Nothing here is a part of the synced world: the turn's hash does not include it, and a save does not keep it.
class ChatSystemInterface
{
public:
	/// A line that came in, for the inspector
	struct Received
	{
		PlayerNames sender {PlayerNames::PLAYER_ONE};
		std::vector<PlayerNames> recipients;
		std::u16string text;
		std::optional<uint32_t> taunt;
		/// The line came into the incoming messages of this computer
		bool shown {false};
		uint32_t turn {0};
	};

	virtual ~ChatSystemInterface() = default;

	/// The texts of the game, for the words of the taunts. None while the interface has no texts.
	virtual void SetTexts(const gui::TextDatabase* texts) = 0;

	/// The names of the human gods of the game, by god slot. Names that the testbed gives replace the names of the
	/// network game.
	virtual void SetNames(std::map<PlayerNames, std::u16string> names) = 0;
	[[nodiscard]] virtual std::map<PlayerNames, std::u16string> GetNames() const = 0;

	/// The player sends a typed line to these players. An empty line does not go, and a line with no player does not go.
	virtual void SendLine(std::u16string_view text, std::span<const PlayerNames> recipients) = 0;
	/// The player sends a taunt of the quick chat to these players
	virtual void SendTaunt(uint32_t taunt, std::span<const PlayerNames> recipients) = 0;

	/// At the turn of the command: the line or the taunt of a sender comes in on this computer
	virtual void Receive(PlayerNames sender, const player_commands::ChatSpeech& speech) = 0;
	virtual void Receive(PlayerNames sender, const player_commands::ChatTaunt& taunt) = 0;
	/// Once a game turn: what the player sent since the last turn makes the advisors of this computer speak
	virtual void ProcessTurn() = 0;
	/// Each frame, with the frame's game milliseconds: the time to show of each bubble runs down
	virtual void UpdateFrame(float gameMilliseconds) = 0;

	/// The bubble of a creature, none while it has said no line
	[[nodiscard]] virtual chat::speech_bubble::Bubble* GetBubble(entt::entity creature) = 0;
	[[nodiscard]] virtual const std::map<entt::entity, chat::speech_bubble::Bubble>& GetBubbles() const = 0;

	/// The bubble of the creature shows for a time again, as the chat comes on the screen
	virtual void ShowBubble(entt::entity creature) = 0;

	[[nodiscard]] virtual std::span<const Received> GetReceived() const = 0;

	/// As a new land opens: the bubbles go
	virtual void Reset() = 0;
};

} // namespace openblack::ecs::systems
