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
#include <cstring>

#include <array>
#include <span>
#include <type_traits>
#include <utility>
#include <vector>

// The game writes each value of a saved game as its raw bytes, low byte first, with no length and no padding. These
// two types read and write values in that layout.
namespace openblack::gamesave
{

/// Adds values to the end of a block of bytes
class ByteWriter
{
public:
	template <typename T>
	void Put(const T& value)
	{
		static_assert(std::is_trivially_copyable_v<T>);
		const auto at = _bytes.size();
		_bytes.resize(at + sizeof(T));
		std::memcpy(_bytes.data() + at, &value, sizeof(T));
	}

	void PutBytes(std::span<const uint8_t> bytes) { _bytes.insert(_bytes.end(), bytes.begin(), bytes.end()); }

	[[nodiscard]] const std::vector<uint8_t>& Bytes() const { return _bytes; }
	[[nodiscard]] std::vector<uint8_t> Take() { return std::move(_bytes); }

private:
	std::vector<uint8_t> _bytes;
};

/// Reads values from a block of bytes, in order. A read past the end fails, and every read after it fails too.
class ByteReader
{
public:
	explicit ByteReader(std::span<const uint8_t> bytes)
	    : _bytes(bytes)
	{
	}

	template <typename T>
	bool Get(T& value)
	{
		static_assert(std::is_trivially_copyable_v<T>);
		if (_failed || _bytes.size() - _at < sizeof(T))
		{
			_failed = true;
			return false;
		}
		std::memcpy(&value, _bytes.data() + _at, sizeof(T));
		_at += sizeof(T);
		return true;
	}

	bool GetBytes(std::span<uint8_t> bytes)
	{
		if (_failed || _bytes.size() - _at < bytes.size())
		{
			_failed = true;
			return false;
		}
		std::memcpy(bytes.data(), _bytes.data() + _at, bytes.size());
		_at += bytes.size();
		return true;
	}

	[[nodiscard]] bool Failed() const { return _failed; }
	[[nodiscard]] size_t Position() const { return _at; }
	[[nodiscard]] size_t Left() const { return _failed ? 0 : _bytes.size() - _at; }

private:
	std::span<const uint8_t> _bytes;
	size_t _at {0};
	bool _failed {false};
};

} // namespace openblack::gamesave
