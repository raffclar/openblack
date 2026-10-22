/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "OpenPage.h"

#include <string>

#if defined(__EMSCRIPTEN__)
#include <emscripten/emscripten.h>
#else
#include <SDL_error.h>
#include <SDL_misc.h>
#endif

#include <spdlog/spdlog.h>

void openblack::online::OpenPage(std::string_view url)
{
	const std::string address(url);
#if defined(__EMSCRIPTEN__)
	// The game runs in a worker, which has no window: the page's own thread opens the tab
	MAIN_THREAD_EM_ASM({ window.open(UTF8ToString($0), "_blank", "noopener"); }, address.c_str());
#else
	if (SDL_OpenURL(address.c_str()) != 0)
	{
		SPDLOG_LOGGER_WARN(spdlog::get("game"), "The browser did not open {}: {}", address, SDL_GetError());
	}
#endif
}
