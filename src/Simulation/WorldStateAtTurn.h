/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#if !defined(OPENBLACK_DEVELOPER_TOOLS)
#error "The world states at a turn are a developer tool: a build without the developer tools does not include them"
#endif

#include <cstddef>
#include <cstdint>

#include <filesystem>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <LHVMFile.h>
#include <entt/meta/fwd.hpp>

#include "ECS/SyncedState.h"

/// Tools: the game server and each client of a network game write the world state of the same turns, with a map of the
/// bytes that only show the world on that computer. tools/sim/save_diff.py --synced then compares the synced bytes of
/// two computers. The computers write the state at the end of the turn, where they make the checksum of the turn.
namespace openblack::world_state_at_turn
{

/// The turns at which a computer writes its world state, and the folder of the files
struct Settings
{
	std::vector<uint32_t> turns;
	std::filesystem::path folder;
};

/// The turns of a list such as "250" or "100,250,400", in their order. None when the text is not such a list.
[[nodiscard]] std::optional<std::vector<uint32_t>> ParseTurns(std::string_view text);

/// The file of the world state of a turn in the folder
[[nodiscard]] std::filesystem::path StateFile(const std::filesystem::path& folder, uint32_t turn);
/// The file of the map of the world state of a turn in the folder
[[nodiscard]] std::filesystem::path MapFile(const std::filesystem::path& folder, uint32_t turn);

/// The bytes of one system's section, and the bytes in it that only show the world on this computer
struct SectionMap
{
	std::string name;
	size_t size {0};
	std::vector<std::pair<size_t, size_t>> presentation;
};

/// What a tool must know to compare the synced part of two world states
struct Map
{
	uint32_t turn {0};
	/// The parts of the entities that only show the world on one computer
	std::span<const synced_state::PresentationPart> parts;
	/// The sections of the systems, in the order of the file
	std::vector<SectionMap> sections;
	/// The presentation values in the block of the registry's context
	std::vector<std::pair<size_t, size_t>> contextPresentation;
	/// The bytes of the scripts' block that only one computer has (see ScriptPresentation)
	std::vector<std::pair<size_t, size_t>> scriptPresentation;
	/// The synced seed and the count of its draws, which the checksum of the turn has
	uint32_t syncedSeed {0};
	uint64_t syncedDraws {0};
	/// The particle sounds that travel to the listener of this computer and do not play yet
	size_t waitingSounds {0};
};

/// The bytes of the scripts' block that only one computer has, from the parts of the block's running state. In a network
/// game only the help of each computer starts scripts, and after the first turn no other script runs. Each computer
/// starts its help at its own turns, so the main stack, the tasks and the counts of the script machine are of this
/// computer only. No script of the help writes a global variable, so the values of the global variables stay synced. In
/// a game for one player the world's scripts run in the same machine, so no byte is local.
[[nodiscard]] std::vector<std::pair<size_t, size_t>> ScriptPresentation(const lhvm::LHVMFile::RunningStateLayout& layout,
                                                                        bool networkGame);

/// The map as JSON text
[[nodiscard]] std::string MapText(const Map& map);

/// Writes the world state of this computer and its map for the turn, when the turn is one of the settings. Returns
/// false when a file does not write.
bool WriteIfDue(const Settings& settings, uint32_t turn, const entt::meta_ctx& reflection);

} // namespace openblack::world_state_at_turn
