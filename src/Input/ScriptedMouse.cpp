/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "ScriptedMouse.h"

#include <algorithm>
#include <vector>

#include <SDL_events.h>
#include <SDL_mouse.h>
#include <glm/common.hpp>

#include "InjectedInput.h"

using namespace openblack::input;

namespace
{

/// The events waiting in the window's queue, left there
std::vector<SDL_Event> PeekQueue()
{
	const auto count = SDL_PeepEvents(nullptr, 0, SDL_PEEKEVENT, SDL_FIRSTEVENT, SDL_LASTEVENT);
	if (count <= 0)
	{
		return {};
	}
	std::vector<SDL_Event> events(static_cast<size_t>(count));
	const auto peeked = SDL_PeepEvents(events.data(), count, SDL_PEEKEVENT, SDL_FIRSTEVENT, SDL_LASTEVENT);
	events.resize(static_cast<size_t>(std::max(peeked, 0)));
	return events;
}

} // namespace

glm::ivec2 openblack::input::RestingPointer(glm::ivec2 windowSize)
{
	return glm::max(windowSize, glm::ivec2(1)) / 2;
}

SDL_Event openblack::input::PumpedMotion(glm::ivec2 by, uint32_t windowId)
{
	SDL_Event motion {};
	motion.type = SDL_MOUSEMOTION;
	motion.motion.windowID = windowId;
	motion.motion.xrel = by.x;
	motion.motion.yrel = by.y;
	MarkPumped(motion);
	return motion;
}

SDL_Event openblack::input::PumpedButton(uint8_t button, bool down, uint8_t clicks, uint32_t windowId)
{
	SDL_Event event {};
	event.type = down ? SDL_MOUSEBUTTONDOWN : SDL_MOUSEBUTTONUP;
	event.button.windowID = windowId;
	event.button.button = button;
	event.button.state = down ? SDL_PRESSED : SDL_RELEASED;
	event.button.clicks = clicks;
	MarkPumped(event);
	return event;
}

ScriptedPointer openblack::input::Pump(ScriptedPointer pointer, SDL_Event& event, glm::ivec2 windowSize)
{
	if (!IsPumped(event))
	{
		return pointer;
	}
	if (event.type == SDL_MOUSEMOTION)
	{
		// The pointer moves on from where it is now, and stays in the window, as SDL keeps a real one
		pointer.position = glm::clamp(pointer.position + glm::ivec2(event.motion.xrel, event.motion.yrel), glm::ivec2(0),
		                              glm::max(windowSize - 1, glm::ivec2(0)));
		event.motion.x = pointer.position.x;
		event.motion.y = pointer.position.y;
		event.motion.state = pointer.buttons;
		return pointer;
	}
	const auto mask = SDL_BUTTON(event.button.button);
	pointer.buttons = event.type == SDL_MOUSEBUTTONDOWN ? (pointer.buttons | mask) : (pointer.buttons & ~mask);
	event.button.x = pointer.position.x;
	event.button.y = pointer.position.y;
	return pointer;
}

void openblack::input::PumpAll(std::optional<ScriptedPointer>& pointer, std::span<SDL_Event> events, glm::ivec2 windowSize)
{
	if (!pointer.has_value())
	{
		return;
	}
	for (auto& event : events)
	{
		pointer = Pump(*pointer, event, windowSize);
	}
}

void openblack::input::PumpQueued(std::optional<ScriptedPointer>& pointer, glm::ivec2 windowSize)
{
	if (!pointer.has_value())
	{
		return;
	}
	auto queued = PeekQueue();
	if (std::ranges::none_of(queued, [](const SDL_Event& event) { return IsPumped(event); }))
	{
		return;
	}
	// Every event is taken out and put back in its place, the scripted mouse's written as the window reports them now
	const auto taken =
	    SDL_PeepEvents(queued.data(), static_cast<int>(queued.size()), SDL_GETEVENT, SDL_FIRSTEVENT, SDL_LASTEVENT);
	queued.resize(static_cast<size_t>(std::max(taken, 0)));
	PumpAll(pointer, queued, windowSize);
	SDL_PeepEvents(queued.data(), static_cast<int>(queued.size()), SDL_ADDEVENT, SDL_FIRSTEVENT, SDL_LASTEVENT);
}

ScriptedPointer openblack::input::QueuedAhead(ScriptedPointer pointer, glm::ivec2 windowSize)
{
	for (auto& event : PeekQueue())
	{
		pointer = Pump(pointer, event, windowSize);
	}
	return pointer;
}
