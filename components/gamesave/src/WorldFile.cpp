/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "WorldFile.h"

#include <algorithm>
#include <map>
#include <set>
#include <variant>

#include "SaveBytes.h"

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
	writer.Put(game.lastAutosaveTurn);
	writer.Put(game.lastQuickSaveTurn);
	PutText(writer, game.landscapeFile);
	return {.tag = k_GameTag, .bytes = writer.Take()};
}

std::optional<std::span<const uint8_t>> openblack::gamesave::FindChunk(const WorldFile& file, const Tag& tag)
{
	const auto found = std::ranges::find(file.chunks, tag, &Chunk::tag);
	if (found == file.chunks.end())
	{
		return std::nullopt;
	}
	return std::span<const uint8_t>(found->bytes);
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
	// A block from before the turns of the last autosave and quick save keeps them at zero
	if (reader.Left() >= 2 * sizeof(uint32_t))
	{
		reader.Get(game.lastAutosaveTurn);
		reader.Get(game.lastQuickSaveTurn);
	}
	// A block from before the whole world has no landscape file
	if (reader.Left() > 0)
	{
		auto landscapeFile = GetText(reader);
		if (!landscapeFile.has_value())
		{
			return std::nullopt;
		}
		game.landscapeFile = std::move(*landscapeFile);
	}
	return game;
}

namespace
{
/// The record with the records of the other saved creatures in its references, as the game writes a creature in full at
/// its first reference. A creature that the write holds open already has its number, so its reference stays a number.
std::shared_ptr<SavedObject> Attached(CreatureObject creature, std::span<const SavedCreature> creatures,
                                      std::set<ObjectKey>& open)
{
	ForEachReference(creature, [&creatures, &open](ObjectRef& ref) {
		if (ref.record != nullptr || ref.Empty() || open.contains(ref.key))
		{
			return;
		}
		const auto other = std::ranges::find(creatures, ref.key, &SavedCreature::key);
		if (other == creatures.end())
		{
			return;
		}
		open.insert(ref.key);
		ref.record = Attached(other->creature, creatures, open);
		open.erase(ref.key);
	});
	return std::make_shared<SavedObject>(SavedObject {.value = std::move(creature)});
}
} // namespace

std::vector<Chunk> openblack::gamesave::ToChunks(std::span<const SavedCreature> creatures)
{
	// The game object is the first object of a file of the game. The stream counts it, so the first creature has the
	// number 2 as in a file of the game.
	StreamWriter writer({.objects = 1, .checksum = 0});
	writer.Put(static_cast<uint32_t>(creatures.size()));
	for (const auto& saved : creatures)
	{
		std::set<ObjectKey> open {saved.key};
		Write(writer, ObjectRef {.key = saved.key, .record = Attached(saved.creature, creatures, open)});
		if (saved.leash.has_value())
		{
			Write(writer, ObjectRef {.key = LeashKey(saved.key),
			                         .record = std::make_shared<SavedObject>(SavedObject {.value = *saved.leash})});
		}
		else
		{
			Write(writer, ObjectRef {});
		}
		writer.Put(saved.agendaStep);
		writer.Put(saved.heading);
		writer.Put(saved.vanillaNumber);
	}
	ByteWriter keys;
	const auto numbered = writer.Numbered();
	keys.Put(static_cast<uint32_t>(numbered.size()));
	for (const auto& entry : numbered)
	{
		keys.Put(entry.number);
		keys.Put(entry.key);
	}
	keys.Put(static_cast<uint32_t>(writer.Foreign().size()));
	for (const auto& entry : writer.Foreign())
	{
		keys.Put(entry.number);
	}
	return {{.tag = k_ObjectsTag, .bytes = writer.Take()}, {.tag = k_KeysTag, .bytes = keys.Take()}};
}

namespace
{
/// Keeps every creature record that the reference holds, and the records inside it
void Collect(const ObjectRef& ref, std::map<ObjectKey, CreatureObject>& creatures,
             std::map<ObjectKey, LeashStatusObject>& leashes)
{
	if (ref.record == nullptr)
	{
		return;
	}
	if (const auto* creature = std::get_if<CreatureObject>(&ref.record->value))
	{
		creatures.emplace(ref.key, *creature);
		auto inner = *creature;
		ForEachReference(inner, [&creatures, &leashes](ObjectRef& each) { Collect(each, creatures, leashes); });
	}
	else if (const auto* leash = std::get_if<LeashStatusObject>(&ref.record->value))
	{
		leashes.emplace(ref.key, *leash);
		Collect(leash->leash.tiedTo, creatures, leashes);
		Collect(leash->leash.holder, creatures, leashes);
	}
}
} // namespace

std::optional<std::vector<SavedCreature>> openblack::gamesave::ReadCreatures(const WorldFile& file)
{
	const auto objects = std::ranges::find(file.chunks, k_ObjectsTag, &Chunk::tag);
	const auto keysChunk = std::ranges::find(file.chunks, k_KeysTag, &Chunk::tag);
	if (objects == file.chunks.end() || keysChunk == file.chunks.end())
	{
		return std::nullopt;
	}
	ByteReader keyReader(keysChunk->bytes);
	uint32_t count = 0;
	std::vector<ForeignNumber> keys;
	std::vector<uint32_t> foreign;
	if (!keyReader.Get(count) || count > keyReader.Left())
	{
		return std::nullopt;
	}
	keys.resize(count);
	for (auto& entry : keys)
	{
		keyReader.Get(entry.number);
		keyReader.Get(entry.key);
	}
	if (!keyReader.Get(count) || count > keyReader.Left())
	{
		return std::nullopt;
	}
	foreign.resize(count);
	for (auto& number : foreign)
	{
		keyReader.Get(number);
	}
	if (keyReader.Failed())
	{
		return std::nullopt;
	}
	StreamReader reader(objects->bytes, {.objects = 1, .checksum = 0});
	reader.SetKeys(keys, foreign);
	if (!reader.Get(count) || count > reader.Left())
	{
		return std::nullopt;
	}
	std::map<ObjectKey, CreatureObject> creatures;
	std::map<ObjectKey, LeashStatusObject> leashes;
	std::vector<SavedCreature> saved(count);
	for (auto& entry : saved)
	{
		ObjectRef creature;
		ObjectRef leash;
		if (!Read(reader, creature) || !Read(reader, leash) || !reader.Get(entry.agendaStep) || !reader.Get(entry.heading) ||
		    !reader.Get(entry.vanillaNumber))
		{
			return std::nullopt;
		}
		Collect(creature, creatures, leashes);
		Collect(leash, creatures, leashes);
		entry.key = creature.key;
		const auto found = creatures.find(creature.key);
		if (found == creatures.end())
		{
			return std::nullopt;
		}
		entry.creature = found->second;
		if (const auto ownLeash = leashes.find(leash.key); !leash.Empty() && ownLeash != leashes.end())
		{
			entry.leash = ownLeash->second;
		}
	}
	return saved;
}
