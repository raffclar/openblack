/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#if !defined(LOCATOR_IMPLEMENTATIONS)
#error "Locator interface implementations should only be included in Locator.cpp, use interface instead."
#endif

#include <functional>

#include "Commands/PlayerCommands.h"
#include "ECS/Systems/ChatSystemInterface.h"

namespace openblack::ecs::systems
{

class ChatSystem final: public ChatSystemInterface
{
public:
	/// What the chat asks of the rest of the game. A part with no function does nothing.
	struct Services
	{
		/// Sends a command of the player to the turns
		std::function<void(player_commands::Action)> submit;
		/// The god slot of the player of this computer, none for an observer
		std::function<std::optional<PlayerNames>()> localPlayer;
		/// The primary creature of a god, none when it has none
		std::function<std::optional<entt::entity>(PlayerNames)> creatureOf;
		/// The players of the land that can have a creature, for a line to all
		std::function<std::vector<PlayerNames>()> players;
		/// A line comes into the incoming messages of this computer
		std::function<void(std::u16string_view)> incoming;
		/// The words of a text of the help. With no function, the system takes the words from its texts.
		std::function<std::u16string(uint32_t text)> tauntText;
		/// The advisors say a text, as the advisors' remarks do
		std::function<void(uint32_t text)> advisorsSay;
		/// The advisors may make their one-off remark on the chat
		std::function<void()> yackingRemark;
		/// Adds a line to the chat log of the profile
		std::function<void(std::string_view line)> log;
		/// The names of the human gods of the network game, by god slot
		std::function<std::map<PlayerNames, std::u16string>()> names;
		/// The turn now, for the inspector
		std::function<uint32_t()> turn;
	};

	explicit ChatSystem(Services services);

	void SetTexts(const gui::TextDatabase* texts) override { _texts = texts; }
	void SetNames(std::map<PlayerNames, std::u16string> names) override;
	[[nodiscard]] std::map<PlayerNames, std::u16string> GetNames() const override;

	void SendLine(std::u16string_view text, std::span<const PlayerNames> recipients) override;
	void SendTaunt(uint32_t taunt, std::span<const PlayerNames> recipients) override;

	void Receive(PlayerNames sender, const player_commands::ChatSpeech& speech) override;
	void Receive(PlayerNames sender, const player_commands::ChatTaunt& taunt) override;
	void ProcessTurn() override;
	void UpdateFrame(float gameMilliseconds) override;

	[[nodiscard]] chat::speech_bubble::Bubble* GetBubble(entt::entity creature) override;
	[[nodiscard]] const std::map<entt::entity, chat::speech_bubble::Bubble>& GetBubbles() const override { return _bubbles; }

	void ShowBubble(entt::entity creature) override;
	[[nodiscard]] std::span<const Received> GetReceived() const override { return _received; }

	void Reset() override;

	/// The most lines that the inspector keeps
	static constexpr size_t k_ReceivedKept = 64;

private:
	/// A line comes in: the incoming messages, the bubbles and the log
	void Arrive(PlayerNames sender, std::span<const PlayerNames> recipients, std::u16string_view text,
	            std::optional<uint32_t> taunt);
	[[nodiscard]] std::u16string NameOf(PlayerNames god) const;
	[[nodiscard]] std::u16string WordsOf(uint32_t text) const;

	Services _services;
	const gui::TextDatabase* _texts {nullptr};
	std::map<PlayerNames, std::u16string> _names;
	std::map<entt::entity, chat::speech_bubble::Bubble> _bubbles;
	std::vector<Received> _received;
	/// What the player sent since the last turn, for the advisors at the next turn: a taunt's number, or none for a
	/// typed line
	std::vector<std::optional<uint32_t>> _sent;
};

} // namespace openblack::ecs::systems
