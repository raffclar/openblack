/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

namespace openblack
{
/// On Windows: an unhandled exception (access violation...) writes the call stack, with function names and lines when
/// the PDB is next to the exe, to stderr and to openblack_crash.txt in the working directory. No-op elsewhere.
void InstallCrashHandler();
} // namespace openblack
