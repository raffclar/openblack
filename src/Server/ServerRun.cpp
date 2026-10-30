/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The dedicated server with the devices that use no hardware. This file is in the library of these devices
// (openblack_headless), because the simulation does not know the renderer and the audio.

#include "DedicatedServer.h"
#include "Locator.h"

namespace openblack::server
{

int Run(const ServerSettings& settings, const std::atomic<bool>& stop,
        const std::function<void(uint32_t turn, uint64_t checksum)>& onTurn)
{
	return RunWithDevices(settings, stop, onTurn, [] { return InitializeHeadlessEngine(); });
}

} // namespace openblack::server
