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

#include <string_view>

#include "ECS/IncomingMessageRules.h"

namespace openblack::gui
{
class TextDatabase;
} // namespace openblack::gui

namespace openblack::ecs::systems
{

/// The list of incoming messages that show at the bottom left of the screen (see incoming_messages). Other systems add
/// messages. The interface shows the top message, steps the list each frame and removes the top message when the
/// player clicks its button.
class IncomingMessageSystemInterface
{
public:
	virtual ~IncomingMessageSystemInterface() = default;

	/// The text table that the weather message takes its heading from, none while there is none
	virtual void SetTexts(const gui::TextDatabase* texts) = 0;
	/// Adds a message at the end of the list
	virtual void Add(incoming_messages::Kind kind, std::u16string_view text, uint8_t picture) = 0;
	/// Adds the message of the real weather: the heading, a space, then the place, with one of the weather pictures
	virtual void AddWeather(std::u16string_view place, uint8_t picture) = 0;
	/// Each frame: removes the top message when it showed long enough. The list removes a message only while its button
	/// shows, but the time of the message also counts while the button does not show.
	virtual void Update(bool buttonShows) = 0;
	/// Removes the top message, as a click on its button does
	virtual void Dismiss() = 0;
	[[nodiscard]] virtual const incoming_messages::Queue& GetQueue() const = 0;
	/// The real time in milliseconds that the list measures by
	[[nodiscard]] virtual uint32_t GetMilliseconds() const = 0;
};

} // namespace openblack::ecs::systems
