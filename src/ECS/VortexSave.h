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
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include <entt/entity/entity.hpp>

#include "3D/MapCoords.h"

/// The land change of the vortexes (docs/bw1-notes/vortex.md). An In writes what it swallows to "vortex.txt" in the
/// save folder as land-script lines (each class saves itself); the Out of the next land reads them back one per even
/// turn and runs each through the land-script interpreter, which creates the object.
namespace openblack::ecs::vortex_save
{
/// The reader (read mode): the file's lines and how many were read
struct Reader
{
	std::vector<std::string> lines;
	size_t next {0};
	uint32_t reads {0};
};

/// The save folder of the current save game. (pending) openblack has no save games:
/// none is set, so no file is read: the Out's reader is empty, its first even turn makes nothing, then come its 30 villagers
void SetSaveFolder(std::optional<std::filesystem::path> folder);
[[nodiscard]] std::optional<std::filesystem::path> SaveFolder();

/// Opens "vortex.txt" in the save folder for reading. Always a reader: without a folder or a file it is empty, its
/// first ReadNext fails and the Out deletes it, as in the original
[[nodiscard]] std::shared_ptr<Reader> OpenReader();

/// Runs the next line as a land-script command at `at` (the Out's emission point): counts the read, offsets every
/// script position by `at`, no tribe override; returns the object the line created (see the .cpp for openblack's
/// clear before the line), or nullopt at the end of the file.
[[nodiscard]] std::optional<entt::entity> ReadNext(Reader& reader, const map_coords::MapCoords& at);
} // namespace openblack::ecs::vortex_save
