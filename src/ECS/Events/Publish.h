/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include "Common/EventManager.h"
#include "Locator.h"

namespace openblack::ecs::events
{
/// Sends an event to its handlers right away. Without an event manager nothing is sent
template <typename Event>
void Publish(const Event& event)
{
	if (Locator::events::has_value())
	{
		Locator::events::value().Create<Event>(event);
	}
}
} // namespace openblack::ecs::events
