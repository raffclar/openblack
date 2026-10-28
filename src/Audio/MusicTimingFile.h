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
#include <vector>

#include "ScriptMusic.h"

namespace openblack::pack
{
struct AudioBankSampleHeader;
} // namespace openblack::pack

namespace openblack::audio
{

/// The times of a music bank from its sample headers and the sound data of its chunks: the cues in the order that the
/// game checks them, the frames of sound in each chunk, the first chunk's sample rate and the loop count of the first
/// header where its flags set one. None when the bank has no chunks or a chunk does not count.
[[nodiscard]] std::optional<MusicTiming> MakeMusicTiming(std::span<const pack::AudioBankSampleHeader> headers,
                                                         std::span<const std::vector<uint8_t>> chunks);

/// The times of a music bank from the bytes of its file. None when the file does not read.
[[nodiscard]] std::optional<MusicTiming> ReadMusicTiming(const std::vector<uint8_t>& bankFile);

} // namespace openblack::audio
