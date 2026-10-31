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

bool SystemSaysWheel()
{
#if defined(_WIN32)
	return GetSystemMetrics(SM_MOUSEWHEELPRESENT) != 0;
#else
	return false;
#endif
}

} // namespace openblack::platform_mouse
