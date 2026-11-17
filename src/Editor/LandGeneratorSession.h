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

#include <optional>
#include <string>
#include <string_view>

#include "3D/GeneratedLandChoices.h"

/// The state of the land generator of the editor (Editor/LandGenerator.h), which the editor system keeps
namespace openblack::editor::land_generator
{
/// The name of the save that the generator makes before it starts. The save game room shows it.
inline constexpr std::u16string_view k_SaveName = u"Before the land generator";

/// The state of the generator, which the editor keeps while lands load
struct Session
{
	/// The slot of the save from before the generator, while the generator is on
	std::optional<uint32_t> returnSlot;
	/// The choices of the next land that the panel generates
	generated_land::Choices choices;
	/// The result of the last act of the panel, for the player
	std::string status;
};
} // namespace openblack::editor::land_generator
