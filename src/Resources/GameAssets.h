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

#include <memory>
#include <optional>

#include "Common/BuildVersion.h"
#include "Common/GameLanguage.h"

namespace openblack
{
class HandAnimation;
}

namespace openblack::filesystem
{
class FileSystemInterface;
}

/// The files of the game's data that only the game reads, not the dedicated server: the build version, the language of
/// the install, the hand's animations and looks, and the sheet of the players' symbols
namespace openblack::game_assets
{

/// The openblack commit of this build. A build without the version file has an unknown version.
[[nodiscard]] build_version::BuildVersion ReadBuildVersion(filesystem::FileSystemInterface& fileSystem);

/// The language of the install from Country.txt in the game folder, or UK English when the game can not read the file
[[nodiscard]] GameLanguage ReadGameLanguage(filesystem::FileSystemInterface& fileSystem);

/// Loads the hand animations of Data/CTR/hh.hbn for the hand mesh, and the meshes and skins of the hand's looks. The
/// result is empty when a file is missing or when the animations do not fit the hand mesh.
[[nodiscard]] std::unique_ptr<HandAnimation> LoadHandAnimation();

/// Makes the sheet of the players' symbols that the world shows, with the symbol of the player's profile. Each
/// computer player shows the picture of its god. A testbed scenario can give another symbol in place of the
/// profile's.
void MakePlayersSymbols(std::optional<uint32_t> symbolInPlace);

} // namespace openblack::game_assets
