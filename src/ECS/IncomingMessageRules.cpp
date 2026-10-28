/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "IncomingMessageRules.h"

#include <algorithm>

namespace openblack::incoming_messages
{

ButtonLook LookAt(const Message& message, uint32_t milliseconds) noexcept
{
	// One step in four shows the picture without the arrow
	const bool still = ((milliseconds / k_FlashStepMilliseconds) & 3U) == 0;
	switch (message.kind)
	{
	case Kind::Weather:
		return {.look = Look::Weather, .picture = message.picture};
	case Kind::Mail:
		return {.look = still ? Look::Envelope : Look::EnvelopeArrow};
	case Kind::Chat:
		break;
	}
	return {.look = still ? Look::Speech : Look::SpeechArrow};
}

void Queue::Add(Kind kind, std::u16string_view text, uint8_t picture, uint32_t now)
{
	_messages.push_back({.kind = kind,
	                     .text = std::u16string(text.substr(0, std::min(text.size(), k_KeptLength))),
	                     .picture = picture,
	                     .since = now});
}

bool Queue::Expire(uint32_t now)
{
	// The time goes round as the machine's count does
	if (_messages.empty() || now - _messages.front().since <= k_ShowMilliseconds)
	{
		return false;
	}
	Dismiss(now);
	return true;
}

void Queue::Dismiss(uint32_t now)
{
	if (_messages.empty())
	{
		return;
	}
	_messages.pop_front();
	if (!_messages.empty())
	{
		_messages.front().since = now;
	}
}

std::u16string_view Queue::ShownText() const noexcept
{
	if (_messages.empty())
	{
		return {};
	}
	const std::u16string_view text = _messages.front().text;
	return text.substr(0, std::min(text.size(), k_ShownLength));
}

bool FollowsCreature(const ClickFacts& facts) noexcept
{
	return facts.kind != Kind::Mail && !facts.rightButton && facts.playersCamera && facts.cameraMoves && facts.cameraKeys &&
	       facts.placeKeys && facts.hasCreature;
}

std::u16string WeatherText(std::u16string_view heading, std::u16string_view place)
{
	std::u16string text(heading);
	text += u' ';
	text += place;
	return text;
}

} // namespace openblack::incoming_messages
