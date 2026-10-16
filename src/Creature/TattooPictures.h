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

#include <functional>
#include <optional>
#include <span>
#include <string>
#include <vector>

#include "Creature/CreatureTattoo.h"

/// The player's own pictures for their symbol and their creature's tattoos. The player puts images with the names
/// Symbol1.bmp to Symbol16.bmp in the folder of their profile. Each image takes the place of the design with the same
/// number.
namespace openblack::tattoo_pictures
{
/// A file smaller than this many bytes is not a picture
constexpr size_t k_SmallestFile = 4096;

/// The name of the file of a design's picture, Symbol1.bmp for the first design
[[nodiscard]] std::string FileName(uint32_t design);

/// The picture in the bytes of a bitmap file, if the file is large enough and the game can read it
[[nodiscard]] std::optional<creature_tattoo::Picture> FromBitmap(std::span<const uint8_t> bytes);

/// Reads the picture of each design from its file. A design without a file, or with a file that is not a picture, has no
/// picture. The game keeps the picture of the last profile there (vanilla-bugs 380), and openblack fixes this.
void Read(creature_tattoo::Pictures& pictures,
          const std::function<std::optional<std::vector<uint8_t>>(const std::string& file)>& reader);
} // namespace openblack::tattoo_pictures
