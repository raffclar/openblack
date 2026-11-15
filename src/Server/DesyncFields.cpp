/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "DesyncFields.h"

#include <cstring>

#include <algorithm>
#include <map>
#include <ranges>

#include <fmt/format.h>

#include "ECS/Reflection/WorldArchive.h"
#include "WorldFile.h"

namespace openblack::server::desync_fields
{
namespace
{

/// Reads the numbers and the names of the entities block of a saved world, little end first. A read past the end
/// gives none and stops all later reads.
class Reader
{
public:
	explicit Reader(std::span<const uint8_t> bytes)
	    : _bytes(bytes)
	{
	}

	[[nodiscard]] std::optional<uint32_t> Number()
	{
		const auto bytes = Take(sizeof(uint32_t));
		if (!bytes.has_value())
		{
			return std::nullopt;
		}
		uint32_t value = 0;
		for (size_t i = 0; i < sizeof(uint32_t); ++i)
		{
			value |= static_cast<uint32_t>((*bytes)[i]) << (8 * i);
		}
		return value;
	}

	[[nodiscard]] std::optional<std::span<const uint8_t>> Take(size_t count)
	{
		if (_failed || _bytes.size() - _at < count)
		{
			_failed = true;
			return std::nullopt;
		}
		const auto taken = _bytes.subspan(_at, count);
		_at += count;
		return taken;
	}

	/// A name: its length, then its letters
	[[nodiscard]] std::optional<std::string_view> Name()
	{
		const auto length = Number();
		const auto letters = length.has_value() ? Take(*length) : std::nullopt;
		if (!letters.has_value())
		{
			return std::nullopt;
		}
		return std::string_view(reinterpret_cast<const char*>(letters->data()), letters->size());
	}

private:
	std::span<const uint8_t> _bytes;
	size_t _at {0};
	bool _failed {false};
};

/// The rows of one component of a saved world: the names of its fields, and the bytes of each field of each wanted entity
struct ComponentRows
{
	std::vector<std::string> fields;
	std::map<uint32_t, std::vector<std::span<const uint8_t>>> rows;
};

/// The rows of the component for the wanted entities, an empty set when the world has no such component, or none when
/// the world does not read. The rows point into the file.
std::optional<ComponentRows> RowsOf(const gamesave::WorldFile& file, std::string_view component,
                                    std::span<const uint32_t> wanted)
{
	const auto block = gamesave::FindChunk(file, gamesave::k_EntitiesTag);
	if (!block.has_value())
	{
		return std::nullopt;
	}
	Reader in(*block);
	const auto magic = in.Take(ecs::reflection::k_WorldArchiveMagic.size());
	if (!magic.has_value() || std::memcmp(magic->data(), ecs::reflection::k_WorldArchiveMagic.data(),
	                                      ecs::reflection::k_WorldArchiveMagic.size()) != 0)
	{
		return std::nullopt;
	}
	const auto version = in.Number();
	const auto count = in.Number();
	const auto inUse = in.Number();
	if (!version.has_value() || !count.has_value() || !inUse.has_value() ||
	    !in.Take(static_cast<size_t>(*count) * sizeof(uint32_t)).has_value())
	{
		return std::nullopt;
	}
	const auto components = in.Number();
	if (!components.has_value())
	{
		return std::nullopt;
	}
	ComponentRows result;
	for (uint32_t c = 0; c < *components; ++c)
	{
		const auto name = in.Name();
		const auto length = in.Number();
		const auto bytes = length.has_value() ? in.Take(*length) : std::nullopt;
		if (!name.has_value() || !bytes.has_value())
		{
			return std::nullopt;
		}
		if (*name != component)
		{
			continue;
		}
		Reader rows(*bytes);
		const auto fieldCount = rows.Number();
		if (!fieldCount.has_value())
		{
			return std::nullopt;
		}
		for (uint32_t f = 0; f < *fieldCount; ++f)
		{
			const auto field = rows.Name();
			if (!field.has_value())
			{
				return std::nullopt;
			}
			result.fields.emplace_back(*field);
		}
		const auto rowCount = rows.Number();
		if (!rowCount.has_value())
		{
			return std::nullopt;
		}
		for (uint32_t r = 0; r < *rowCount; ++r)
		{
			const auto entity = rows.Number();
			if (!entity.has_value())
			{
				return std::nullopt;
			}
			std::vector<std::span<const uint8_t>> values;
			values.reserve(result.fields.size());
			for (size_t f = 0; f < result.fields.size(); ++f)
			{
				const auto size = rows.Number();
				const auto value = size.has_value() ? rows.Take(*size) : std::nullopt;
				if (!value.has_value())
				{
					return std::nullopt;
				}
				values.push_back(*value);
			}
			if (std::ranges::find(wanted, *entity) != wanted.end())
			{
				result.rows.emplace(*entity, std::move(values));
			}
		}
		return result;
	}
	return result;
}

} // namespace

std::optional<std::vector<EntityFields>> DifferentFields(std::span<const uint8_t> serverWorld,
                                                         std::span<const uint8_t> playerWorld, std::string_view component,
                                                         std::span<const uint32_t> entities,
                                                         std::span<const synced_state::PresentationPart> presentation)
{
	const auto serverFile = gamesave::ReadWorldFile(serverWorld);
	const auto playerFile = gamesave::ReadWorldFile(playerWorld);
	if (!serverFile.has_value() || !playerFile.has_value())
	{
		return std::nullopt;
	}
	const auto server = RowsOf(*serverFile, component, entities);
	const auto player = RowsOf(*playerFile, component, entities);
	if (!server.has_value() || !player.has_value())
	{
		return std::nullopt;
	}
	std::vector<EntityFields> result;
	for (const auto entity : entities)
	{
		const auto own = server->rows.find(entity);
		const auto theirs = player->rows.find(entity);
		if (own == server->rows.end() || theirs == player->rows.end())
		{
			continue;
		}
		EntityFields differs {.entity = entity, .fields = {}};
		for (size_t f = 0; f < server->fields.size(); ++f)
		{
			const auto& field = server->fields[f];
			// A field of the server's build that the player's build saves at another place is compared by its name
			const auto other = std::ranges::find(player->fields, field);
			if (other == player->fields.end() || synced_state::IsPresentationOnly(presentation, component, field))
			{
				continue;
			}
			const auto a = own->second[f];
			const auto b = theirs->second[static_cast<size_t>(other - player->fields.begin())];
			if (!std::ranges::equal(a, b))
			{
				differs.fields.push_back(field);
			}
		}
		if (!differs.fields.empty())
		{
			result.push_back(std::move(differs));
		}
	}
	return result;
}

std::string Describe(std::span<const EntityFields> fields)
{
	std::string words;
	for (const auto& entity : fields)
	{
		std::string names;
		for (const auto& field : entity.fields | std::views::take(k_MostFieldsOfEntity))
		{
			names += names.empty() ? field : ", " + field;
		}
		if (entity.fields.size() > k_MostFieldsOfEntity)
		{
			names += fmt::format(" and {} more", entity.fields.size() - k_MostFieldsOfEntity);
		}
		words += fmt::format("{}entity {}: {}", words.empty() ? "" : "; ", entity.entity, names);
	}
	return words.empty() ? std::string("no field differs") : words;
}

std::optional<std::string> FirstField(std::string_view component, std::span<const EntityFields> fields)
{
	for (const auto& entity : fields)
	{
		if (!entity.fields.empty())
		{
			return fmt::format("{}.{}", component, entity.fields.front());
		}
	}
	return std::nullopt;
}

} // namespace openblack::server::desync_fields
