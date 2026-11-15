/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#if !defined(__EMSCRIPTEN__)
#error "The web platform code is only for the web build"
#endif

namespace openblack::web
{

/// Tell the page that the game wrote files of the profiles, such as a saved game or a creature file. The page then
/// sends the new saved games and creatures to the site in the background. Any thread can call it, and the call does
/// not wait.
void NoticeProfileWrite();

} // namespace openblack::web
