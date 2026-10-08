/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

// OPENBLACK_TEST_GESTURE (documented in docs/bw1-notes/openblack-internals.md): a stroke played as mouse messages.

namespace openblack::magic::gestures
{
/// Every frame (seconds of game time): once the land exists and the start time has passed, the stroke starts
void RunDebugHooks(float seconds);
/// A land is loaded: the stroke plays again
void ResetDebugHooks();
} // namespace openblack::magic::gestures
