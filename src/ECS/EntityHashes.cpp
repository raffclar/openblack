/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "EntityHashes.h"

#include <algorithm>

#include <fmt/format.h>

namespace openblack::synced_state
{
namespace
{
constexpr uint64_t k_HashPrime = 1099511628211ull;

uint64_t MixNumber(uint64_t hash, uint64_t value) noexcept
{
	for (int shift = 0; shift < 64; shift += 8)
	{
		hash ^= (value >> shift) & 0xffu;
		hash *= k_HashPrime;
	}
	return hash;
}

std::string Side(std::string_view name, const std::optional<EntityHash>& entity)
{
	if (!entity.has_value())
	{
		return fmt::format("{} has no entity", name);
	}
	const auto place = entity->position.has_value() ? fmt::format(" at {:.3f} {:.3f} {:.3f}", (*entity->position)[0],
	                                                              (*entity->position)[1], (*entity->position)[2])
	                                                : std::string();
	return fmt::format("{} {}{} hash {:016x}", name, entity->kind.empty() ? std::string("Entity") : entity->kind, place,
	                   entity->hash);
}
} // namespace

uint64_t MixHash(uint64_t hash, std::span<const uint8_t> bytes) noexcept
{
	for (const auto byte : bytes)
	{
		hash ^= byte;
		hash *= k_HashPrime;
	}
	return hash;
}

std::vector<uint64_t> BlocksOf(std::span<const EntityHash> entities)
{
	std::vector<uint64_t> blocks;
	for (size_t first = 0; first < entities.size() && blocks.size() < k_MostBlocks; first += k_EntitiesInBlock)
	{
		const auto count = std::min(k_EntitiesInBlock, entities.size() - first);
		auto hash = k_EntityHashStart;
		for (const auto& entity : entities.subspan(first, count))
		{
			hash = MixNumber(hash, entity.entity);
			hash = MixNumber(hash, entity.hash);
		}
		blocks.push_back(hash);
	}
	return blocks;
}

std::vector<uint32_t> DifferentBlocks(std::span<const uint64_t> own, std::span<const uint64_t> other)
{
	std::vector<uint32_t> different;
	const auto count = std::max(own.size(), other.size());
	for (size_t index = 0; index < count && different.size() < k_MostAskedBlocks; ++index)
	{
		if (index >= own.size() || index >= other.size() || own[index] != other[index])
		{
			different.push_back(static_cast<uint32_t>(index));
		}
	}
	return different;
}

std::vector<EntityHash> EntitiesOfBlocks(std::span<const EntityHash> entities, std::span<const uint32_t> blocks)
{
	std::vector<EntityHash> result;
	for (const auto block : blocks)
	{
		const auto first = static_cast<size_t>(block) * k_EntitiesInBlock;
		if (first >= entities.size())
		{
			continue;
		}
		const auto count = std::min(k_EntitiesInBlock, entities.size() - first);
		const auto part = entities.subspan(first, count);
		result.insert(result.end(), part.begin(), part.end());
	}
	std::ranges::sort(result, {}, &EntityHash::entity);
	const auto [last, end] = std::ranges::unique(result, {}, &EntityHash::entity);
	result.erase(last, end);
	return result;
}

std::vector<EntityDifference> DifferentEntities(std::span<const EntityHash> server, std::span<const EntityHash> player)
{
	std::vector<EntityDifference> different;
	size_t s = 0;
	size_t p = 0;
	while ((s < server.size() || p < player.size()) && different.size() < k_MostDifferentEntities)
	{
		if (p >= player.size() || (s < server.size() && server[s].entity < player[p].entity))
		{
			different.push_back({.entity = server[s].entity, .server = server[s], .player = std::nullopt});
			++s;
		}
		else if (s >= server.size() || player[p].entity < server[s].entity)
		{
			different.push_back({.entity = player[p].entity, .server = std::nullopt, .player = player[p]});
			++p;
		}
		else
		{
			if (server[s].hash != player[p].hash)
			{
				different.push_back({.entity = server[s].entity, .server = server[s], .player = player[p]});
			}
			++s;
			++p;
		}
	}
	return different;
}

std::string Describe(std::string_view part, std::span<const EntityDifference> differences)
{
	if (differences.empty())
	{
		return fmt::format("{}: no entity differs", part);
	}
	std::string text = fmt::format("{}:", part);
	for (const auto& difference : differences)
	{
		text += fmt::format(" entity {} ({}; {})", difference.entity, Side("server", difference.server),
		                    Side("player", difference.player));
	}
	return text;
}

std::string KindOf(std::span<const std::string_view> components)
{
	std::string kind;
	size_t names = 0;
	for (const auto name : components)
	{
		if (name == "Transform" || names >= k_MostKindNames)
		{
			continue;
		}
		if (!kind.empty())
		{
			kind += '+';
		}
		kind += name;
		++names;
	}
	return kind;
}

} // namespace openblack::synced_state
