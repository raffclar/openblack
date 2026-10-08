/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <cstdint>

namespace openblack::ecs::fire
{
/// OPENBLACK_TEST_FIRE (FireDebugHooks.cpp), once the land exists; called every turn from the magic loop
void RunDebugHooks(uint32_t turn);
/// A land is loaded
void ResetDebugHooks();
} // namespace openblack::ecs::fire
