/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <cstddef>
#include <cstdint>

#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include <DanceFile.h>
#include <DanceShapeFile.h>
#include <GestureFile.h>
#include <ParticleFile.h>
#include <RawImage.h>
#include <SASFile.h>

#include "Gui/GameFont.h"
#include "InfoConstants.h"
#include "Serializer/GameThingSerializer.h"

/// The text that datatool prints for the game's smaller files. Each function turns a file that a component has read into
/// lines of text, so that the text can be tested without a game folder.
namespace openblack::datatool
{

/// The templates of a gesture file, one line each
[[nodiscard]] std::string DescribeGestures(const gestures::GestureFile& file);

/// The classes and the properties of a particle effect file. With `properties` every property line is listed.
[[nodiscard]] std::string DescribeParticleFile(const psys::ParticleFile& file, bool properties);
/// The text of a particle file as the game reads it: the loose text itself, or the text inflated from a compressed file
/// (a length, then a zlib stream). None when the bytes are neither.
[[nodiscard]] std::optional<std::string> ParticleText(std::span<const uint8_t> bytes, bool compressed);

/// What a raw image of a size holds: its size, and for each channel the lowest, the highest and the mean level
[[nodiscard]] std::string DescribeRgbImage(const rawimage::RgbImage& image);
[[nodiscard]] std::string DescribeGreyImage(const rawimage::GreyImage& image);
/// The size of a raw image that is square when the file has no size in it: the side whose square times the channels is
/// the number of bytes. None when no whole side fits.
[[nodiscard]] std::optional<uint32_t> SquareSide(size_t byteCount, uint32_t channels);
/// A size written as WIDTHxHEIGHT; none when it is not
[[nodiscard]] std::optional<std::pair<uint32_t, uint32_t>> ParseSize(std::string_view text);

/// The tables of the info block: each one's place in the block, its rows and the size of a row. The block is given as
/// its bytes, in the layout of version 1.0 or of the patch 1.20.
[[nodiscard]] std::string DescribeInfoBlock(std::span<const uint8_t> block);
/// Dumps the bytes of the rows of the table of that name, 16 bytes a line; none when there is no such table
[[nodiscard]] std::optional<std::string> DumpInfoTable(std::span<const uint8_t> block, std::string_view table, size_t firstRow,
                                                       size_t rowCount);

/// A font: its name, its height, its glyphs. With `glyphs` every glyph is listed with its metrics.
[[nodiscard]] std::string DescribeFont(const gui::GameFont& font, bool glyphs);
/// A glyph drawn as text, one character for each pixel of the font's full height atlas ('#' set, '.' clear). Empty when
/// the font has no such glyph.
[[nodiscard]] std::string DrawGlyph(const gui::GameFont& font, char16_t character);

/// The players' profiles file
[[nodiscard]] std::string DescribeProfiles(std::string_view text);
/// The small info file of a save slot
[[nodiscard]] std::string DescribeSaveInfo(std::span<const uint8_t> bytes);
/// A saved world: how it starts, and the creatures and leashes found in it
[[nodiscard]] std::string DescribeSavedWorld(std::span<const uint8_t> bytes);

/// The sound placements of the clips of a sound placement file (the .SAS files)
[[nodiscard]] std::string DescribeSounds(const sas::SASFile& file, bool frames);
/// The names of a header of enumerations (such as the sound actions) with their values, in the order of the values
[[nodiscard]] std::string DescribeEnumHeader(std::string_view text);

/// A dance file
[[nodiscard]] std::string DescribeDance(const dance::DanceFile& file);
/// The points of a dancers' shape
[[nodiscard]] std::string DescribeDanceShape(const dance::DanceShapeFile& file);

/// A land's footpath file: the links, then the paths, with the nodes
[[nodiscard]] std::string DescribeFootpaths(std::span<const serializer::GameThingSerializer::FootpathLinkSave> links,
                                            std::span<const serializer::GameThingSerializer::Footpath> paths);

} // namespace openblack::datatool
