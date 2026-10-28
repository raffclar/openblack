/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#define LOCATOR_IMPLEMENTATIONS

#include "IncomingMessageSystem.h"

#include <utility>

#include "Gui/TextDatabase.h"

namespace openblack::ecs::systems
{

IncomingMessageSystem::IncomingMessageSystem(Clock clock)
    : _clock(std::move(clock))
{
}

void IncomingMessageSystem::Add(incoming_messages::Kind kind, std::u16string_view text, uint8_t picture)
{
	_queue.Add(kind, text, picture, _clock());
}

void IncomingMessageSystem::AddWeather(std::u16string_view place, uint8_t picture)
{
	// A text table that is too short gives its first text
	const std::u16string_view heading =
	    _texts != nullptr ? std::u16string_view(_texts->GetHelpText(incoming_messages::k_WeatherHeadingText).text)
	                      : std::u16string_view {};
	Add(incoming_messages::Kind::Weather, incoming_messages::WeatherText(heading, place), picture);
}

void IncomingMessageSystem::Update(bool buttonShows)
{
	if (buttonShows)
	{
		_queue.Expire(_clock());
	}
}

void IncomingMessageSystem::Dismiss()
{
	_queue.Dismiss(_clock());
}

} // namespace openblack::ecs::systems
