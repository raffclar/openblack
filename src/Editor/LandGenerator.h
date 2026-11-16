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
#include <span>
#include <string>
#include <string_view>

#include "Editor/LandGeneratorSession.h"

/// The land generator of the editor. When the player starts it, the game saves into a free slot of the player's own
/// saves, clears the world and loads a land from a random seed. The player then makes more lands with other seeds and
/// choices. Exit Editor in the menu, or the exit button of the editor, loads the save, so the player is back at the
/// place where the generator started.
namespace openblack::editor::land_generator
{
/// The first slot of the player's own saves that has no save in it, or nothing when each slot has a save
[[nodiscard]] std::optional<uint32_t> FreeSlot(std::span<const uint32_t> usedSlots);

/// Whether the generator is on: it has a save to go back to, and the land that is open is a generated land. Another
/// land that the player loads stops the generator.
[[nodiscard]] bool IsOn(const Session& session, std::string_view landPath);

/// A seed for a new land, from the random device of the computer. Only the editor uses it, before a land loads.
[[nodiscard]] uint64_t RandomSeed();

/// Starts the generator: saves the game into a free slot, then loads a land from the seed with the first choices.
/// Gives false, and changes nothing, when the game cannot save. The status tells the player why.
bool Start(Session& session, uint64_t seed);

/// Loads a land from the choices of the session. Gives false when the land does not load.
bool Generate(Session& session);

/// Stops the generator and loads the save from before it. Gives false when the save does not load.
bool Exit(Session& session);
} // namespace openblack::editor::land_generator
