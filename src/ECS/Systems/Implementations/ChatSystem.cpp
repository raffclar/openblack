/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#define LOCATOR_IMPLEMENTATIONS

#include "ChatSystem.h"

#include <algorithm>
#include <utility>

#include "Commands/TurnCodec.h"
#include "Gui/TextDatabase.h"

using namespace openblack::ecs::systems;
using namespace openblack::chat;
using openblack::PlayerNames;

ChatSystem::ChatSystem(Services services)
    : _services(std::move(services))
{
}

void ChatSystem::SetNames(std::map<PlayerNames, std::u16string> names)
{
	_names = std::move(names);
}

void ChatSystem::SendLine(std::u16string_view text, std::span<const PlayerNames> recipients)
{
	if (text.empty())
	{
		return;
	}
	_sent.emplace_back(std::nullopt);
	if (recipients.empty() || !_services.submit)
	{
		return;
	}
	auto shown = WithoutStaffCode(text);
	shown = shown.substr(0, std::min(shown.size(), turn_codec::k_MostChatCharacters));
	_services.submit(
	    player_commands::ChatSpeech {.recipients = {recipients.begin(), recipients.end()}, .text = std::u16string(shown)});
}

void ChatSystem::SendTaunt(uint32_t taunt, std::span<const PlayerNames> recipients)
{
	_sent.emplace_back(taunt);
	if (recipients.empty() || !_services.submit)
	{
		return;
	}
	_services.submit(player_commands::ChatTaunt {.recipients = {recipients.begin(), recipients.end()}, .taunt = taunt});
}

void ChatSystem::Receive(PlayerNames sender, const player_commands::ChatSpeech& speech)
{
	// The line ends at its first null character, as a line of the game does
	std::u16string_view text = speech.text;
	text = text.substr(0, text.find(u'\0'));
	Arrive(sender, speech.recipients, text, std::nullopt);
}

void ChatSystem::Receive(PlayerNames sender, const player_commands::ChatTaunt& taunt)
{
	const auto text = TauntText(taunt.taunt);
	if (!text.has_value())
	{
		return;
	}
	// The advisors of every computer say the taunt, before the test of who gets the line
	if (_services.advisorsSay)
	{
		_services.advisorsSay(*text);
	}
	Arrive(sender, taunt.recipients, {}, taunt.taunt);
}

void ChatSystem::Arrive(PlayerNames sender, std::span<const PlayerNames> recipients, std::u16string_view text,
                        std::optional<uint32_t> taunt)
{
	std::u16string tauntWords;
	if (taunt.has_value())
	{
		tauntWords = WordsOf(*TauntText(*taunt));
		text = tauntWords;
	}
	const auto name = NameOf(sender);
	const auto here = _services.localPlayer ? _services.localPlayer() : std::nullopt;
	const bool shown = ShowsHere(sender, here, recipients);
	if (shown && _services.incoming)
	{
		_services.incoming(NamedLine(name, text));
	}
	// The creature of the sender says the words alone, and each creature of the players that get the line says them
	// with the sender's name. Each says the line and adds to the chat log once for each time the list has its player.
	const auto say = [this, sender, &name, text](entt::entity creature, bool ofSender) {
		speech_bubble::Say(_bubbles[creature],
		                   {.text = ofSender ? std::u16string(text) : NamedLine(name, text), .sender = sender}, ofSender);
		if (_services.log)
		{
			_services.log(LogLine(name, text));
		}
	};
	if (_services.creatureOf)
	{
		if (const auto creature = _services.creatureOf(sender); creature.has_value())
		{
			say(*creature, true);
		}
		const auto players = _services.players ? _services.players() : std::vector<PlayerNames> {};
		for (const auto player : players)
		{
			const auto creature = _services.creatureOf(player);
			if (!creature.has_value() || !CreatureSays(player, sender, recipients))
			{
				continue;
			}
			const auto times = recipients.empty() ? 1 : std::ranges::count(recipients.begin(), recipients.end(), player);
			for (std::ptrdiff_t i = 0; i < times; ++i)
			{
				say(*creature, false);
			}
		}
	}
	_received.push_back({.sender = sender,
	                     .recipients = {recipients.begin(), recipients.end()},
	                     .text = std::u16string(text),
	                     .taunt = taunt,
	                     .shown = shown,
	                     .turn = _services.turn ? _services.turn() : 0});
	if (_received.size() > k_ReceivedKept)
	{
		_received.erase(_received.begin());
	}
}

void ChatSystem::ProcessTurn()
{
	// What the player sent makes the advisors of this computer speak at once: a taunt is said, and a typed line may
	// bring the one-off remark on the chat
	auto sent = std::exchange(_sent, {});
	for (const auto& taunt : sent)
	{
		if (taunt.has_value())
		{
			if (const auto text = TauntText(*taunt); text.has_value() && _services.advisorsSay)
			{
				_services.advisorsSay(*text);
			}
		}
		else if (_services.yackingRemark)
		{
			_services.yackingRemark();
		}
	}
}

void ChatSystem::UpdateFrame(float gameMilliseconds)
{
	for (auto& [creature, bubble] : _bubbles)
	{
		speech_bubble::Update(bubble, gameMilliseconds);
	}
}

speech_bubble::Bubble* ChatSystem::GetBubble(entt::entity creature)
{
	const auto found = _bubbles.find(creature);
	return found != _bubbles.end() ? &found->second : nullptr;
}

void ChatSystem::Reset()
{
	_bubbles.clear();
	_sent.clear();
}

std::u16string ChatSystem::WordsOf(uint32_t text) const
{
	if (_services.tauntText)
	{
		return _services.tauntText(text);
	}
	return _texts != nullptr ? std::u16string(_texts->GetHelpText(text).text) : std::u16string {};
}

std::map<PlayerNames, std::u16string> ChatSystem::GetNames() const
{
	if (!_names.empty() || !_services.names)
	{
		return _names;
	}
	return _services.names();
}

std::u16string ChatSystem::NameOf(PlayerNames god) const
{
	const auto names = GetNames();
	const auto found = names.find(god);
	return found != names.end() ? found->second : std::u16string {};
}
