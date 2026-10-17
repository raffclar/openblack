/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "VanillaSaveFile.h"

#include <cstring>

#include <algorithm>
#include <variant>

using namespace openblack::gamesave;

namespace
{
/// The longest text that a read of the start of the file accepts
constexpr uint32_t k_MostText = 260;
/// The most objects that a file of the game has, so that a scan does not take a random word for a reference
constexpr uint32_t k_MostObjects = 0x1000000;

uint32_t WordAt(std::span<const uint8_t> bytes, size_t at)
{
	uint32_t word = 0;
	std::memcpy(&word, bytes.data() + at, sizeof(word));
	return word;
}

bool ReadText(StreamReader& reader, std::string& text)
{
	uint32_t length = 0;
	if (!reader.Get(length) || length == 0 || length > k_MostText)
	{
		return false;
	}
	text.clear();
	for (uint32_t i = 0; i < length; ++i)
	{
		char letter = 0;
		reader.Get(letter);
		if (letter != 0)
		{
			text.push_back(letter);
		}
	}
	return !reader.Failed();
}

void WriteText(StreamWriter& writer, std::string_view text)
{
	writer.Put(static_cast<uint32_t>(text.size() + 1));
	for (const char letter : text)
	{
		writer.Put(letter);
	}
	writer.Put('\0');
}

/// Reads the object at the reference and gives it when it is new and of the class
template <typename Object>
std::optional<FoundRecord<Object>> ReadFoundAt(std::span<const uint8_t> bytes, size_t at)
{
	const auto start = StartOfRecordAt(bytes, at);
	if (!start.has_value())
	{
		return std::nullopt;
	}
	StreamReader reader(bytes, *start, at);
	ObjectRef ref;
	if (!Read(reader, ref) || ref.record == nullptr)
	{
		return std::nullopt;
	}
	const auto* object = std::get_if<Object>(&ref.record->value);
	if (object == nullptr)
	{
		return std::nullopt;
	}
	return FoundRecord<Object> {.at = at,
	                            .end = reader.Position(),
	                            .number = start->objects + 1,
	                            .start = *start,
	                            .object = *object,
	                            .objectsAfter = reader.Objects()};
}

template <typename Object>
std::vector<FoundRecord<Object>> Find(std::span<const uint8_t> bytes, uint32_t type, auto&& accept)
{
	std::vector<FoundRecord<Object>> found;
	if (bytes.size() < 16)
	{
		return found;
	}
	for (size_t at = 0; at + 16 <= bytes.size(); ++at)
	{
		if (WordAt(bytes, at + 4) != type)
		{
			continue;
		}
		const auto number = WordAt(bytes, at);
		if (number == 0 || number > k_MostObjects)
		{
			continue;
		}
		if (auto record = ReadFoundAt<Object>(bytes, at); record.has_value() && accept(record->object))
		{
			found.push_back(std::move(*record));
		}
	}
	return found;
}
} // namespace

std::optional<VanillaFraming> openblack::gamesave::ReadVanillaFraming(std::span<const uint8_t> bytes)
{
	VanillaFraming framing;
	StreamReader reader(bytes);
	if (!reader.GetUncounted(framing.attempts) || !ReadText(reader, framing.revision) || !ReadText(reader, framing.landscape))
	{
		return std::nullopt;
	}
	framing.body = reader.Position();
	framing.checksum = reader.Checksum();
	return framing;
}

std::vector<uint8_t> openblack::gamesave::WriteVanillaFraming(std::string_view landscape)
{
	StreamWriter writer;
	writer.PutUncounted(0);
	WriteText(writer, k_VanillaRevision);
	WriteText(writer, landscape);
	return writer.Take();
}

std::optional<StreamStart> openblack::gamesave::StartOfRecordAt(std::span<const uint8_t> bytes, size_t at)
{
	if (at + 16 > bytes.size())
	{
		return std::nullopt;
	}
	const auto number = WordAt(bytes, at);
	if (number == 0)
	{
		return std::nullopt;
	}
	// The record keeps the running sum after its type and its extra word. Take away those two words and the reference.
	const auto stored = WordAt(bytes, at + 12);
	const auto counted = static_cast<uint32_t>(bytes[at]) + static_cast<uint32_t>(bytes[at + 4]) +
	                     static_cast<uint32_t>(bytes[at + 8]) + 3u * static_cast<uint32_t>(sizeof(uint32_t));
	return StreamStart {.objects = number - 1, .checksum = stored - counted};
}

std::vector<FoundCreature> openblack::gamesave::FindCreatures(std::span<const uint8_t> bytes)
{
	return Find<CreatureObject>(bytes, k_CreatureType, [](const CreatureObject& creature) {
		const auto& record = creature.creature;
		if (record.version == 0 || record.version > k_CreatureRecordVersion)
		{
			return false;
		}
		for (uint32_t i = 0; i < record.spells.size(); ++i)
		{
			if (record.spells.at(i).spell != i)
			{
				return false;
			}
		}
		return true;
	});
}

std::vector<FoundLeash> openblack::gamesave::FindLeashes(std::span<const uint8_t> bytes)
{
	return Find<LeashStatusObject>(bytes, k_LeashStatusType, [](const LeashStatusObject& leash) {
		return leash.leash.on <= 1 && leash.leash.works <= 1 && leash.leash.type >= 1 && leash.leash.type <= 3;
	});
}

std::optional<std::vector<uint8_t>> openblack::gamesave::WriteRecordAt(StreamStart start, const ObjectRef& ref)
{
	StreamWriter writer(start);
	Write(writer, ref);
	if (writer.Failed() || !writer.Foreign().empty())
	{
		return std::nullopt;
	}
	return writer.Take();
}

std::optional<std::vector<uint8_t>> openblack::gamesave::Replace(std::span<const uint8_t> file, size_t at, size_t end,
                                                                 StreamStart start, uint32_t objectsAfter, const ObjectRef& ref)
{
	if (at > end || end > file.size())
	{
		return std::nullopt;
	}
	StreamWriter writer(start);
	Write(writer, ref);
	if (writer.Failed() || !writer.Foreign().empty() || writer.Objects() != objectsAfter)
	{
		return std::nullopt;
	}
	std::vector<uint8_t> bytes;
	bytes.reserve(file.size() - (end - at) + writer.Bytes().size());
	bytes.insert(bytes.end(), file.begin(), file.begin() + static_cast<std::ptrdiff_t>(at));
	bytes.insert(bytes.end(), writer.Bytes().begin(), writer.Bytes().end());
	bytes.insert(bytes.end(), file.begin() + static_cast<std::ptrdiff_t>(end), file.end());
	return bytes;
}
