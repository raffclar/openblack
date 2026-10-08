/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

namespace openblack::worship::trace
{
/// OPENBLACK_WORSHIP_TRACE=1: the per-turn chant accounting of every worship site, the icons' charging and the
/// villagers' worship states in the log
[[nodiscard]] bool Enabled();
} // namespace openblack::worship::trace
