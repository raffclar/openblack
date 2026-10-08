/*******************************************************************************
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

#include <vector>

namespace openblack::test
{

/// Writes bits the way a Bink video packet holds them, for the decoder tests: bytes in order, each from its lowest bit
class BinkBitWriter
{
public:
	/// The low `count` bits of `value`, its bit 0 first
	BinkBitWriter& Put(uint32_t value, uint32_t count)
	{
		for (uint32_t i = 0; i < count; ++i)
		{
			if ((_bits & 7) == 0)
			{
				_bytes.push_back(0);
			}
			if (((value >> i) & 1) != 0)
			{
				_bytes.back() = static_cast<uint8_t>(_bytes.back() | 1u << (_bits & 7));
			}
			++_bits;
		}
		return *this;
	}

	BinkBitWriter& Bit(bool bit) { return Put(bit ? 1 : 0, 1); }

	/// Zero bits up to the next multiple of 32
	BinkBitWriter& Align32()
	{
		while ((_bits & 31) != 0)
		{
			Put(0, 1);
		}
		return *this;
	}

	[[nodiscard]] size_t Bits() const { return _bits; }
	[[nodiscard]] const std::vector<uint8_t>& Bytes() const { return _bytes; }

private:
	std::vector<uint8_t> _bytes;
	size_t _bits {0};
};

} // namespace openblack::test
