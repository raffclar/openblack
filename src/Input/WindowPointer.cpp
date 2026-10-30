/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "WindowPointer.h"

#include "Common/Platform.h"
#include "GameActionMapInterface.h"
#include "InputDeviceInterface.h"
#include "Locator.h"

glm::ivec2 openblack::input::WindowPointer()
{
	if constexpr (k_Platform == Platform::Web)
	{
		if (Locator::gameActionSystem::has_value())
		{
			return Locator::gameActionSystem::value().GetPointerPosition();
		}
	}
	return Device().Mouse().position;
}

void openblack::input::PointWindowEvent(SDL_Event& event)
{
	if constexpr (k_Platform == Platform::Web)
	{
		if (Locator::gameActionSystem::has_value())
		{
			Locator::gameActionSystem::value().PointPlayerEvent(event);
		}
	}
	else
	{
		static_cast<void>(event);
	}
}
