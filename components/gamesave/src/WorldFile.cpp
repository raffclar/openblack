/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "WorldFile.h"

using namespace openblack::gamesave;

namespace
{
/// The longest text that a read accepts
constexpr uint32_t k_LongestText = 4096;

void PutText(ByteWriter& writer, std::string_view text)
{
	writer.Put(static_cast<uint32_t>(text.size()));
	writer.PutBytes({reinterpret_cast<const uint8_t*>(text.data()), text.size()});
}

std::optional<std::string> GetText(ByteReader& reader)
{
	uint32_t length = 0;
	if (!reader.Get(length) || length > k_LongestText || length > reader.Left())
	{
		return std::nullopt;
	}
	std::string text(length, '\0');
	reader.GetBytes({reinterpret_cast<uint8_t*>(text.data()), text.size()});
	return text;
}
} // namespace

std::vector<uint8_t> openblack::gamesave::Write(const WorldFile& file)
{
	ByteWriter writer;
	writer.Put(k_WorldMagic);
	writer.Put(file.version);
	writer.Put(static_cast<uint32_t>(file.chunks.size()));
	for (const auto& chunk : file.chunks)
	{
		writer.Put(chunk.tag);
		writer.Put(static_cast<uint32_t>(chunk.bytes.size()));
		writer.PutBytes(chunk.bytes);
	}
	return writer.Take();
}

std::optional<WorldFile> openblack::gamesave::ReadWorldFile(std::span<const uint8_t> bytes)
{
	ByteReader reader(bytes);
	Tag magic {};
	WorldFile file;
	uint32_t count = 0;
	if (!reader.Get(magic) || magic != k_WorldMagic || !reader.Get(file.version) || !reader.Get(count))
	{
		return std::nullopt;
	}
	for (uint32_t i = 0; i < count; ++i)
	{
		Chunk chunk;
		uint32_t length = 0;
		if (!reader.Get(chunk.tag) || !reader.Get(length) || length > reader.Left())
		{
			return std::nullopt;
		}
		chunk.bytes.resize(length);
		reader.GetBytes(chunk.bytes);
		file.chunks.push_back(std::move(chunk));
	}
	return file;
}

Chunk openblack::gamesave::ToChunk(const GameChunk& game)
{
	ByteWriter writer;
	writer.Put(game.land);
	writer.Put(game.turn);
	PutText(writer, game.landscape);
	writer.Put(game.knownLeashes);
	writer.Put(game.creatureInTemple);
	return {.tag = k_GameTag, .bytes = writer.Take()};
}

std::optional<GameChunk> openblack::gamesave::ReadGameChunk(std::span<const uint8_t> bytes)
{
	ByteReader reader(bytes);
	GameChunk game;
	reader.Get(game.land);
	reader.Get(game.turn);
	auto landscape = GetText(reader);
	if (!landscape.has_value())
	{
		return std::nullopt;
	}
	game.landscape = std::move(*landscape);
	reader.Get(game.knownLeashes);
	reader.Get(game.creatureInTemple);
	if (reader.Failed())
	{
		return std::nullopt;
	}
	return game;
}

Chunk openblack::gamesave::ToChunk(const CreatureChunk& creature)
{
	ByteWriter writer;
	writer.Put(creature.self);
	writer.Put(creature.position);
	writer.Put(creature.heading);
	writer.Put(creature.life);
	Write(writer, creature.record);
	writer.Put(static_cast<uint32_t>(creature.leash.has_value() ? 1 : 0));
	if (creature.leash.has_value())
	{
		Write(writer, *creature.leash);
	}
	writer.Put(creature.agendaStep);
	return {.tag = k_CreatureTag, .bytes = writer.Take()};
}

std::optional<CreatureChunk> openblack::gamesave::ReadCreatureChunk(std::span<const uint8_t> bytes)
{
	ByteReader reader(bytes);
	CreatureChunk creature;
	reader.Get(creature.self);
	reader.Get(creature.position);
	reader.Get(creature.heading);
	reader.Get(creature.life);
	auto record = ReadCreatureRecord(reader);
	if (!record.has_value())
	{
		return std::nullopt;
	}
	creature.record = std::move(*record);
	uint32_t hasLeash = 0;
	reader.Get(hasLeash);
	if (hasLeash != 0)
	{
		creature.leash = ReadLeashRecord(reader);
		if (!creature.leash.has_value())
		{
			return std::nullopt;
		}
	}
	reader.Get(creature.agendaStep);
	if (reader.Failed())
	{
		return std::nullopt;
	}
	return creature;
}
