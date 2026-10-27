/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include "DeveloperTools.h"

/// The gate of the developer tools in the running game. It reads the network game of this computer from the Locator.
namespace openblack::developer_tools
{

/// True from the join of a game server until the network session ends
[[nodiscard]] bool NetworkSessionLive();

/// Whether the developer action can run now (see RefusalOf). The log tells each refusal, thus call it only for an
/// event such as a key press or a request, not in each frame.
[[nodiscard]] bool Allowed(Action action);

} // namespace openblack::developer_tools
