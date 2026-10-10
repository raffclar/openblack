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

#include <array>
#include <span>

#include <BinkDecoder.h>

namespace openblack::bink::test
{

/// zlib's CRC-32 (reflected 0xEDB88320, starting and ending inverted)
class Crc32
{
public:
	Crc32& Add(std::span<const uint8_t> bytes) noexcept
	{
		for (const uint8_t byte : bytes)
		{
			_crc = k_Table[(_crc ^ byte) & 0xFF] ^ (_crc >> 8);
		}
		return *this;
	}
	Crc32& AddU32(uint32_t value) noexcept
	{
		const std::array<uint8_t, 4> bytes = {static_cast<uint8_t>(value), static_cast<uint8_t>(value >> 8),
		                                      static_cast<uint8_t>(value >> 16), static_cast<uint8_t>(value >> 24)};
		return Add(bytes);
	}
	[[nodiscard]] uint32_t Value() const noexcept { return ~_crc; }

private:
	static constexpr std::array<uint32_t, 256> MakeTable() noexcept
	{
		std::array<uint32_t, 256> table {};
		for (uint32_t i = 0; i < 256; ++i)
		{
			uint32_t c = i;
			for (int k = 0; k < 8; ++k)
			{
				c = (c & 1) != 0 ? 0xEDB88320u ^ (c >> 1) : c >> 1;
			}
			table[i] = c;
		}
		return table;
	}

	static const std::array<uint32_t, 256> k_Table;
	uint32_t _crc {0xFFFFFFFFu};
};

inline const std::array<uint32_t, 256> Crc32::k_Table = Crc32::MakeTable();

/// The CRC of a plane's visible pixels, row by row
[[nodiscard]] inline uint32_t PlaneCrc(const PlaneView& plane)
{
	Crc32 crc;
	for (uint32_t row = 0; row < plane.height; ++row)
	{
		crc.Add(plane.pixels.subspan(static_cast<size_t>(row) * plane.stride, plane.width));
	}
	return crc.Value();
}

} // namespace openblack::bink::test
