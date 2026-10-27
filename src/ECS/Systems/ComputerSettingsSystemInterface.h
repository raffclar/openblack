/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <ComputerSettings.h>

namespace openblack::ecs::systems
{

/// The settings of this computer, which all the players' profiles share (see computer_settings). They are read from
/// their file once, as the game starts, and the values that the player changes in the options are written back.
class ComputerSettingsSystemInterface
{
public:
	virtual ~ComputerSettingsSystemInterface() = default;

	/// The values as the file gave them at the start, with the changes kept since
	[[nodiscard]] virtual const computer_settings::Settings& GetSettings() const = 0;
	/// Keeps the values that hold one (the others stay as they are). The file is written only when a value is new.
	virtual void Keep(const computer_settings::Settings& changed) = 0;
};

} // namespace openblack::ecs::systems
