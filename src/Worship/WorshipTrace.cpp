/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "WorshipTrace.h"

#include <cstdlib>

bool openblack::worship::trace::Enabled()
{
	static const bool enabled = std::getenv("OPENBLACK_WORSHIP_TRACE") != nullptr;
	return enabled;
}
