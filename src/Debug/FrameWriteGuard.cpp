/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "FrameWriteGuard.h"

#include <algorithm>
#include <span>
#include <string>
#include <unordered_map>
#include <vector>

#include <entt/meta/context.hpp>
#include <fmt/ranges.h>
#include <spdlog/fmt/fmt.h>
#include <spdlog/spdlog.h>

#include "Common/GameRandom.h"
#include "ECS/Reflection/ComponentReflection.h"
#include "ECS/Registry.h"
#include "ECS/WorldSave.h"
#include "Locator.h"

namespace openblack::frame_write_guard
{

namespace
{

constexpr uint64_t k_FnvOffset = 14695981039346656037ull;
constexpr uint64_t k_FnvPrime = 1099511628211ull;

/// The FNV-1a hash of the bytes, from a start value
[[nodiscard]] uint64_t Fnv(std::span<const uint8_t> bytes, uint64_t hash)
{
	for (const auto byte : bytes)
	{
		hash = (hash ^ byte) * k_FnvPrime;
	}
	return hash;
}

} // namespace

std::vector<std::string> ChangedParts(const Sample& before, const Sample& after)
{
	std::unordered_map<std::string_view, uint64_t> earlier;
	for (const auto& [name, hash] : before.hash.parts)
	{
		earlier.emplace(name, hash);
	}
	std::vector<std::string> changed;
	for (const auto& [name, hash] : after.hash.parts)
	{
		const auto found = earlier.find(name);
		if (found == earlier.end() || found->second != hash)
		{
			changed.push_back(name);
		}
		if (found != earlier.end())
		{
			earlier.erase(found);
		}
	}
	// A part that the step removed also changed
	for (const auto& [name, hash] : before.hash.parts)
	{
		if (earlier.contains(name))
		{
			changed.push_back(name);
		}
	}
	if (before.syncedDraws != after.syncedDraws)
	{
		changed.emplace_back(k_SyncedDrawsPart);
	}
	return changed;
}

std::optional<uint32_t> ChangedEntity(const Sample& before, const Sample& after, const std::string& part)
{
	const auto earlier = before.rows.find(part);
	const auto later = after.rows.find(part);
	if (earlier == before.rows.end() || later == after.rows.end())
	{
		return std::nullopt;
	}
	for (const auto& [entity, hash] : later->second)
	{
		const auto found = earlier->second.find(entity);
		if (found == earlier->second.end() || found->second != hash)
		{
			return entity;
		}
	}
	return std::nullopt;
}

std::vector<size_t> ChangedValues(const Sample& before, const Sample& after, const std::string& part)
{
	const auto earlier = before.systemValues.find(part);
	const auto later = after.systemValues.find(part);
	if (earlier == before.systemValues.end() || later == after.systemValues.end())
	{
		return {};
	}
	std::vector<size_t> changed;
	const auto count = std::max(earlier->second.size(), later->second.size());
	for (size_t i = 0; i < count; ++i)
	{
		if (i >= earlier->second.size() || i >= later->second.size() || earlier->second[i] != later->second[i])
		{
			changed.push_back(i);
		}
	}
	return changed;
}

Guard::Guard(bool byField, bool systems)
    : _context(std::make_unique<entt::meta_ctx>())
    , _byField(byField)
    , _systems(systems)
{
	ecs::reflection::RegisterComponents(*_context);
}

Guard::~Guard() = default;

Sample Guard::Take() const
{
	Sample sample;
	if (Locator::entitiesRegistry::has_value())
	{
		ecs::reflection::RowVisitor visit;
		if (_byField)
		{
			visit = [&sample](std::string_view component, entt::entity entity, std::string_view field,
			                  std::span<const uint8_t> bytes) {
				auto& rows = sample.rows[std::string(component) + "." + std::string(field)];
				rows[entt::to_integral(entity)] = Fnv(bytes, k_FnvOffset);
			};
		}
		sample.hash = synced_state::HashOf(Locator::entitiesRegistry::value().Underlying(), *_context,
		                                   synced_state::PresentationParts(), _byField, visit);
	}
	if (Locator::gameRandom::has_value())
	{
		sample.syncedDraws = Locator::gameRandom::value().GetSyncedDraws();
	}
	if (_systems)
	{
		// Each system's saved state is one more part, and it goes into the hash of the whole sample
		for (const auto& section : ecs::world_save::SavedSystemSections(*_context, _byField))
		{
			const auto hash = Fnv(section.bytes, k_FnvOffset);
			auto name = std::string(k_SystemPart) + section.name;
			if (_byField)
			{
				// The hash of each value, from the end of the value before it
				auto& values = sample.systemValues[name];
				size_t start = 0;
				for (const auto end : section.valueEnds)
				{
					values.push_back(Fnv(std::span(section.bytes).subspan(start, end - start), k_FnvOffset));
					start = end;
				}
			}
			sample.hash.parts.emplace_back(std::move(name), hash);
			sample.hash.hash = Fnv(std::span(reinterpret_cast<const uint8_t*>(&hash), sizeof(hash)), sample.hash.hash);
		}
	}
	return sample;
}

void Guard::Before()
{
	_before = _last.has_value() ? std::move(*_last) : Take();
	_last.reset();
}

void Guard::After(std::string_view step, uint32_t turn)
{
	_last = Take();
	const auto& after = *_last;
	// The hash of the whole state is the quick test: most steps change nothing
	if (after.hash.hash == _before.hash.hash && after.syncedDraws == _before.syncedDraws)
	{
		return;
	}
	for (auto& part : ChangedParts(_before, after))
	{
		auto key = std::make_pair(std::string(step), std::move(part));
		if (!_found.contains(key))
		{
			const auto entity = ChangedEntity(_before, after, key.second);
			const auto values = ChangedValues(_before, after, key.second);
			SPDLOG_LOGGER_WARN(spdlog::get("game"),
			                   "Frame write guard: the frame step {} changed the synced part {} at turn {}{}{}", key.first,
			                   key.second, turn, entity.has_value() ? fmt::format(" (entity {})", *entity) : "",
			                   values.empty() ? "" : fmt::format(" (values {})", fmt::join(values, ",")));
			_found.insert(std::move(key));
		}
	}
}

} // namespace openblack::frame_write_guard
