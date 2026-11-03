/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "MouseWheel.h"

#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

namespace openblack::platform_mouse
{

uint32_t SystemMouseButtons()
{
#if defined(_WIN32)
	return SystemMouseButtonsFor(System::Windows, GetSystemMetrics(SM_CMOUSEBUTTONS));
#else
	return SystemMouseButtonsFor(System::Other, 0);
#endif
}

bool SystemSaysWheel()
{
#if defined(_WIN32)
	return SystemSaysWheelFor(System::Windows, GetSystemMetrics(SM_MOUSEWHEELPRESENT) != 0);
#else
	return SystemSaysWheelFor(System::Other, false);
#endif
}

} // namespace openblack::platform_mouse
