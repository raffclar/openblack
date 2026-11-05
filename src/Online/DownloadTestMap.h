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
#include <vector>

/// The land of the test of the map download: a land of a network game that no game folder has, so that each player
/// must get it from the lobby. The tool makes it from the land for two gods of the game folder and writes it to a new
/// file. The game folder does not change. The land keeps the landscape and the things of its template. Only its name
/// and the first line of its script are different, so it loads as fast as its template.
namespace openblack::download_test_map
{

/// The file name of the land, with no folder and no extension
inline constexpr std::string_view k_File = "obt_dl_2p_1";
/// The name of the land in the list of maps
inline constexpr std::string_view k_Name = "Download test";
/// The gods that the land takes
inline constexpr int32_t k_Players = 2;
/// The land of the game folder that the tool makes the land from
inline constexpr std::string_view k_TemplateMap = "Online Maps/mpm_2p_1.map";
/// The first line of the script, which makes the bytes of the land different from the bytes of its template
inline constexpr std::string_view k_FirstLine = "rem openblack: the land of the map download test";

/// The file name of the land with its extension, for example "obt_dl_2p_1.map"
[[nodiscard]] std::string FileName();

/// The bytes of the land, from the bytes of the template. The script gets the first line, and its landscape line names
/// the landscape of the land. Nothing when the template is not a land of a network game or its script loads no
/// landscape.
[[nodiscard]] std::optional<std::vector<uint8_t>> Make(std::span<const uint8_t> templateMap);

/// The script with the landscape line of a land of a network game with this file name, and the first line. Nothing when
/// the script has no landscape line.
[[nodiscard]] std::optional<std::string> ScriptFor(std::string_view script, std::string_view file);

} // namespace openblack::download_test_map
