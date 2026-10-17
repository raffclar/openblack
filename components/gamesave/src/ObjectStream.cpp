/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "ObjectStream.h"

#include <algorithm>

using namespace openblack::gamesave;

void StreamWriter::PutUncounted(uint32_t value)
{
	const auto at = _bytes.size();
	_bytes.resize(at + sizeof(value));
	std::memcpy(_bytes.data() + at, &value, sizeof(value));
}

bool StreamWriter::PutReference(ObjectKey key)
{
	if (key == k_NoObject)
	{
		Put(uint32_t {0});
		return false;
	}
	if (const auto known = NumberOfKey(key); known.has_value())
	{
		Put(*known);
		return false;
	}
	++_objects;
	_numbers.emplace(key, _objects);
	Put(_objects);
	return true;
}

void StreamWriter::MarkForeign(ObjectKey key)
{
	_foreign.push_back({.number = _objects, .key = key});
}

std::optional<uint32_t> StreamWriter::NumberOfKey(ObjectKey key) const
{
	if (const auto found = _numbers.find(key); found != _numbers.end())
	{
		return found->second;
	}
	// The objects before the start of the stream keep the numbers that the file gave them
	if (const auto number = NumberOf(key); number.has_value() && *number <= _start.objects)
	{
		return number;
	}
	return std::nullopt;
}

bool StreamReader::GetUncounted(uint32_t& value)
{
	if (_failed || _bytes.size() - _at < sizeof(value))
	{
		_failed = true;
		return false;
	}
	std::memcpy(&value, _bytes.data() + _at, sizeof(value));
	_at += sizeof(value);
	return true;
}

ReferenceKind StreamReader::GetReference(ObjectKey& key)
{
	uint32_t number = 0;
	key = k_NoObject;
	if (!Get(number))
	{
		return ReferenceKind::Bad;
	}
	if (number == 0)
	{
		return ReferenceKind::None;
	}
	const auto keyOf = [this](uint32_t of) {
		const auto found = _keys.find(of);
		return found != _keys.end() ? found->second : NumberKey(of);
	};
	if (number <= _objects)
	{
		key = keyOf(number);
		return ReferenceKind::Earlier;
	}
	if (number != _objects + 1)
	{
		_failed = true;
		return ReferenceKind::Bad;
	}
	++_objects;
	key = keyOf(number);
	if (const auto foreign = _foreign.find(number); foreign != _foreign.end() && foreign->second)
	{
		return ReferenceKind::Foreign;
	}
	return ReferenceKind::New;
}

void StreamReader::SetKeys(std::span<const ForeignNumber> keys, std::span<const uint32_t> foreign)
{
	for (const auto& entry : keys)
	{
		_keys[entry.number] = entry.key;
	}
	for (const auto number : foreign)
	{
		_foreign[number] = true;
	}
}

std::vector<ForeignNumber> StreamWriter::Numbered() const
{
	std::vector<ForeignNumber> numbered;
	numbered.reserve(_numbers.size());
	for (const auto& [key, number] : _numbers)
	{
		numbered.push_back({.number = number, .key = key});
	}
	std::ranges::sort(numbered, {}, &ForeignNumber::number);
	return numbered;
}
