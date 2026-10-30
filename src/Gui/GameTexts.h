/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include "TextDatabase.h"

namespace openblack::gui
{

/// Reads the game's texts from its text scripts. The first script holds the help texts, which the game's scripts refer
/// to by number. A script that is not there gives no texts.
[[nodiscard]] TextDatabase LoadGameTexts();

} // namespace openblack::gui
