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
#include <map>
#include <span>
#include <string>

/// The language file of the creature upload tool of the game (creatureupload.cfg). It gives the texts of the dialog of
/// the tool, one text on each line, as "KEY= text".
///
/// The file is UTF-16 (little-endian) when it starts with the byte order mark, or 8-bit text if not. The rules of a
/// line are:
/// - The spaces at the start do not count.
/// - A line that starts with "#" is a comment, and an empty line has no text.
/// - The key stops at "=", a space or "#". Then the spaces, the tabs and the "=" signs do not count.
/// - The text continues to the end of the line. In UTF-16 text, the last character before the line break (the carriage
///   return) is not part of the text.
/// - A "#" after the key and before the text removes the line.
/// - In 8-bit text, a "#" in the text also removes the line, and the carriage return stays in the text.
/// - When two lines have the same key, the text of the last line is the text of the key.
///
/// A zero character also stops the text. Then the last character of the text stays. A last line with no line break
/// gives no text.
namespace openblack::creatureuploadcfg
{

/// The texts by their keys. The texts are UTF-8.
using Texts = std::map<std::string, std::string, std::less<>>;

/// The texts of the file. A file with no lines that the rules accept gives no texts.
[[nodiscard]] Texts Read(std::span<const uint8_t> bytes);

} // namespace openblack::creatureuploadcfg
