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

#include <span>
#include <string_view>

namespace openblack::debug::gui
{

/// The format of one sample of a .sad bank, as the Audio banks window shows it: a wave file's format (its
/// WAVEFORMATEX tag, or what an AIFF's compression stands for), or raw MPEG for bytes that are not a wave file (the
/// music segments)
[[nodiscard]] std::string_view SampleFormatName(std::span<const uint8_t> sample) noexcept;

} // namespace openblack::debug::gui
