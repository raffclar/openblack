/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <string_view>

#include "Temple/TempleHelp.h"

namespace openblack::temple_help
{

/// The game's scripts, as the temple's help starts and stops them. Only a single player game has them start.
class GameScripts final: public Scripts
{
public:
	void Start(std::string_view name) override;
	void StopHelp() override;
	void Stop(std::string_view name) override;
};

/// Whether the help system is on, which a room's help needs to start
[[nodiscard]] bool HelpSystemOn();

} // namespace openblack::temple_help
