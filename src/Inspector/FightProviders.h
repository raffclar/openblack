/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <memory>

#include <InspectorProvider.h>

namespace openblack::inspector
{

/// The fight transcript: the fights recorded, the records of one fight from an index, a clear, and a folder for files
[[nodiscard]] std::unique_ptr<ProviderInterface> MakeFightTranscriptProvider();
/// The fight autopilot of the running inspector: start it with a policy, stop it, and read its state and its log
[[nodiscard]] std::unique_ptr<ProviderInterface> MakeFightAutopilotProvider();

} // namespace openblack::inspector
