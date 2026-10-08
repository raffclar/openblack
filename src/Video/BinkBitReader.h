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

#include <span>

namespace openblack::video::bink
{

/// Reads the bits of a Bink video packet: bytes in order, each byte from its lowest bit. Past the end of the packet it
/// reads zeros and remembers that it overran, so a damaged packet never reads outside its buffer.
class BitReader
{
public:
	explicit BitReader(std::span<const uint8_t> data) noexcept
	    : _data(data)
	    , _size(data.size() * 8)
	{
	}

	/// The next `count` bits (0 to 32) without moving: the first bit is bit 0 of the result
	[[nodiscard]] uint32_t Peek(uint32_t count) const noexcept
	{
		if (count == 0)
		{
			return 0;
		}
		// 32 bits starting anywhere inside a byte span at most 5 bytes
		const size_t first = _position >> 3;
		uint64_t window = 0;
		for (size_t i = 0; i < 5; ++i)
		{
			if (first + i < _data.size())
			{
				window |= static_cast<uint64_t>(_data[first + i]) << (8 * i);
			}
		}
		const uint64_t mask = (uint64_t {1} << count) - 1;
		return static_cast<uint32_t>((window >> (_position & 7)) & mask);
	}

	/// The next `count` bits (0 to 32), as Peek, and moves past them
	[[nodiscard]] uint32_t Read(uint32_t count) noexcept
	{
		const uint32_t value = Peek(count);
		_position += count;
		return value;
	}

	[[nodiscard]] bool ReadBit() noexcept { return Read(1) != 0; }

	void Skip(size_t count) noexcept { _position += count; }

	/// Moves to the next multiple of 32 bits (nothing if it is on one)
	void Align32() noexcept { _position = (_position + 31) & ~size_t {31}; }

	/// The bits read so far
	[[nodiscard]] size_t Position() const noexcept { return _position; }
	/// The packet's size in bits
	[[nodiscard]] size_t Size() const noexcept { return _size; }
	/// The bits not read yet; negative once it has overrun
	[[nodiscard]] int64_t BitsLeft() const noexcept { return static_cast<int64_t>(_size) - static_cast<int64_t>(_position); }
	/// It has read past the end of the packet
	[[nodiscard]] bool Overrun() const noexcept { return _position > _size; }

private:
	std::span<const uint8_t> _data;
	size_t _size;
	size_t _position {0};
};

} // namespace openblack::video::bink
