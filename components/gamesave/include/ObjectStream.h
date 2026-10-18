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

#include <optional>
#include <span>
#include <type_traits>
#include <unordered_map>
#include <utility>
#include <vector>

// The game writes its world file as one stream of values with no lengths. Each value goes in as its raw bytes, low
// byte first. A running sum adds the first byte and the size of each value. Each object writes this sum after its
// type, and a load reads it but does not compare it.
//
// A reference to an object is a number:
// - 0 is no object.
// - 1 to n is an object that the stream wrote before, counted in the order of its first write.
// - n + 1 is a new object. Its whole record follows the number at once.
namespace openblack::gamesave
{

/// The identity of an object in a stream. 0 is no object. A key below 2^32 is the object's number in a file of the
/// game. A key with bit 32 set is an entity of openblack.
using ObjectKey = uint64_t;

inline constexpr ObjectKey k_NoObject = 0;
inline constexpr ObjectKey k_EntityKeyBit = ObjectKey {1} << 32;

[[nodiscard]] constexpr ObjectKey NumberKey(uint32_t number)
{
	return number;
}

[[nodiscard]] constexpr ObjectKey EntityKey(uint32_t entity)
{
	return k_EntityKeyBit | entity;
}

[[nodiscard]] constexpr std::optional<uint32_t> NumberOf(ObjectKey key)
{
	if (key == k_NoObject || (key & k_EntityKeyBit) != 0)
	{
		return std::nullopt;
	}
	return static_cast<uint32_t>(key);
}

[[nodiscard]] constexpr std::optional<uint32_t> EntityOf(ObjectKey key)
{
	if ((key & k_EntityKeyBit) == 0)
	{
		return std::nullopt;
	}
	return static_cast<uint32_t>(key & 0xFFFFFFFFu);
}

/// Where a stream starts: the number of objects that the file has before it, and the running sum at that point
struct StreamStart
{
	uint32_t objects {0};
	uint32_t checksum {0};

	bool operator==(const StreamStart&) const = default;
};

/// A number that openblack gives to an object that it cannot write in the game's layout. No record follows the
/// number. Only openblack's own world file uses these.
struct ForeignNumber
{
	uint32_t number {0};
	ObjectKey key {k_NoObject};

	bool operator==(const ForeignNumber&) const = default;
};

/// Writes values and references as the game writes them
class StreamWriter
{
public:
	explicit StreamWriter(StreamStart start = {})
	    : _start(start)
	    , _objects(start.objects)
	    , _checksum(start.checksum)
	{
	}

	/// Adds one value of the game, with its first byte and its size added to the running sum
	template <typename T>
	void Put(const T& value)
	{
		static_assert(std::is_trivially_copyable_v<T>);
		const auto at = _bytes.size();
		_bytes.resize(at + sizeof(T));
		std::memcpy(_bytes.data() + at, &value, sizeof(T));
		_checksum += static_cast<uint32_t>(_bytes[at]) + static_cast<uint32_t>(sizeof(T));
	}

	/// Adds a value that the running sum does not count, as the first word of the file
	void PutUncounted(uint32_t value);

	/// Adds the running sum as the game writes it after the type of an object
	void PutChecksum() { Put(_checksum); }

	/// Writes the reference to the object. Returns true when the object is new to the stream: then the caller must
	/// write the object's whole record at once, or call MarkForeign.
	[[nodiscard]] bool PutReference(ObjectKey key);

	/// Keeps the last new number as an object that has no record in the stream. Only openblack's own file has these.
	void MarkForeign(ObjectKey key);

	/// Stops the writer. A writer that failed keeps the bytes that it has but they are not a complete stream.
	void Fail() { _failed = true; }

	[[nodiscard]] bool Failed() const { return _failed; }
	[[nodiscard]] uint32_t Checksum() const { return _checksum; }
	[[nodiscard]] uint32_t Objects() const { return _objects; }
	[[nodiscard]] std::optional<uint32_t> NumberOfKey(ObjectKey key) const;
	[[nodiscard]] const std::vector<ForeignNumber>& Foreign() const { return _foreign; }
	/// The keys of the numbers that this writer gave, in the order of the numbers
	[[nodiscard]] std::vector<ForeignNumber> Numbered() const;
	[[nodiscard]] const std::vector<uint8_t>& Bytes() const { return _bytes; }
	[[nodiscard]] std::vector<uint8_t> Take() { return std::move(_bytes); }

private:
	StreamStart _start;
	uint32_t _objects {0};
	uint32_t _checksum {0};
	std::unordered_map<ObjectKey, uint32_t> _numbers;
	std::vector<ForeignNumber> _foreign;
	std::vector<uint8_t> _bytes;
	bool _failed {false};
};

/// What a reference in a stream points at
enum class ReferenceKind : uint8_t
{
	None,
	/// An object that the stream has before
	Earlier,
	/// A new object whose whole record follows at once
	New,
	/// A number that openblack's own file gives to an object with no record
	Foreign,
	/// A number past the next new number: the stream is not valid
	Bad
};

/// Reads values and references as the game reads them
class StreamReader
{
public:
	explicit StreamReader(std::span<const uint8_t> bytes, StreamStart start = {}, size_t at = 0)
	    : _bytes(bytes)
	    , _at(at)
	    , _objects(start.objects)
	    , _checksum(start.checksum)
	{
	}

	/// Reads one value of the game and adds its first byte and its size to the running sum
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
		_checksum += static_cast<uint32_t>(_bytes[_at]) + static_cast<uint32_t>(sizeof(T));
		_at += sizeof(T);
		return true;
	}

	bool GetUncounted(uint32_t& value);

	/// Reads a reference. A new object gets the next number, and the caller must then read its record.
	ReferenceKind GetReference(ObjectKey& key);

	/// Gives the keys of the numbers that openblack's own file gave to objects, with or without a record
	void SetKeys(std::span<const ForeignNumber> keys, std::span<const uint32_t> foreign);

	void Fail() { _failed = true; }

	[[nodiscard]] bool Failed() const { return _failed; }
	[[nodiscard]] size_t Position() const { return _at; }
	[[nodiscard]] size_t Left() const { return _failed ? 0 : _bytes.size() - _at; }
	[[nodiscard]] uint32_t Checksum() const { return _checksum; }
	[[nodiscard]] uint32_t Objects() const { return _objects; }
	/// The type of the object whose record the reader could not read, as the game numbers its classes
	[[nodiscard]] std::optional<uint32_t> UnknownType() const { return _unknownType; }
	void SetUnknownType(uint32_t type)
	{
		_unknownType = type;
		_failed = true;
	}

private:
	std::span<const uint8_t> _bytes;
	size_t _at {0};
	uint32_t _objects {0};
	uint32_t _checksum {0};
	std::unordered_map<uint32_t, ObjectKey> _keys;
	std::unordered_map<uint32_t, bool> _foreign;
	std::optional<uint32_t> _unknownType;
	bool _failed {false};
};

} // namespace openblack::gamesave
