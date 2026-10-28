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

#include <filesystem>
#include <span>
#include <vector>

namespace openblack::lnd
{
struct LNDCell;
}

/// The rules that let a load of a world state keep the land that is open, in place of a land that it builds again
namespace openblack::land_reuse
{

/// The land that is open and the land of the world that loads
struct Lands
{
	/// The load brings the world state of a network game, which a client loads when it comes back in step
	bool worldState {false};
	/// A land is open now
	bool landLoaded {false};
	std::filesystem::path openLand;
	/// The landscape file that the open land read
	std::filesystem::path openLandscape;
	std::filesystem::path savedLand;
	std::filesystem::path savedLandscape;
	/// The game builds the saved land itself (the testbed, the map for 32 gods), so no landscape file names it
	bool builtByGame {false};
};

/// Whether the load can keep the open land. A world state of the land that is open has the same landscape: only the
/// altitudes that the world changed go back, and the load makes again only the blocks that change.
[[nodiscard]] bool KeepsLand(const Lands& lands);

/// The cells of a block, [x * 17 + z], whose altitude is not the altitude of the land as the game built it
[[nodiscard]] std::vector<uint16_t> ChangedCells(std::span<const lnd::LNDCell> cells, std::span<const uint8_t> built);

} // namespace openblack::land_reuse
