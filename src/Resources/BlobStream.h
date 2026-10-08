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
#include <span>
#include <spanstream>
#include <vector>

namespace openblack::resources
{
/// A read-only stream over cached bytes (the byte cache, ResourcesInterface::GetBlobs), for parsers that read streams.
/// No copy: the bytes must outlive the stream
[[nodiscard]] inline std::unique_ptr<std::istream> BlobStream(const std::vector<uint8_t>& bytes)
{
	return std::make_unique<std::ispanstream>(std::span<const char>(reinterpret_cast<const char*>(bytes.data()), bytes.size()));
}
} // namespace openblack::resources
