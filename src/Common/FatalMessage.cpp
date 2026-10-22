/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "FatalMessage.h"

#if defined(__EMSCRIPTEN__)
#include "Web/WebPlatform.h"
#else
#include <SDL_messagebox.h>
#endif

void openblack::ShowFatalMessage(const std::string& title, const std::string& text)
{
#if defined(__EMSCRIPTEN__)
	web::ShowPageMessage(title, text);
#else
	SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, title.c_str(), text.c_str(), nullptr);
#endif
}
