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

#include <string>
#include <string_view>

/// The build id of a network game: the version of the build and the version of the format of the turns, such as
/// "0.1.0+0123456789ab/codec34". The client sends it in its join, and a game server of another build refuses the client
/// (deviation 248). The lobby lists the build id of each game.
namespace openblack::online::build_id
{

/// The text between the version and the number of the format of the turns
inline constexpr std::string_view k_CodecWord = "/codec";

/// The build id of a version text and a number of the format of the turns
[[nodiscard]] std::string Compose(std::string_view version, uint8_t codec);

/// The build id of this build
[[nodiscard]] std::string Own();

} // namespace openblack::online::build_id
