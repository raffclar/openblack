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
#include <unordered_map>

#include <entt/meta/context.hpp>
#include <spdlog/spdlog.h>

#include "Common/GameRandom.h"
#include "ECS/Reflection/ComponentReflection.h"
#include "ECS/Registry.h"
#include "Locator.h"

namespace openblack::frame_write_guard
{

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

Guard::Guard(bool byField)
    : _context(std::make_unique<entt::meta_ctx>())
    , _byField(byField)
{
	ecs::reflection::RegisterComponents(*_context);
}

Guard::~Guard() = default;

Sample Guard::Take() const
{
	Sample sample;
	if (Locator::entitiesRegistry::has_value())
	{
		sample.hash = synced_state::HashOf(Locator::entitiesRegistry::value().Underlying(), *_context,
		                                   synced_state::PresentationParts(), _byField);
	}
	if (Locator::gameRandom::has_value())
	{
		sample.syncedDraws = Locator::gameRandom::value().GetSyncedDraws();
	}
	return sample;
}

void Guard::Before()
{
	_before = Take();
}

void Guard::After(std::string_view step, uint32_t turn)
{
	const auto after = Take();
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
			SPDLOG_LOGGER_WARN(spdlog::get("game"),
			                   "Frame write guard: the frame step {} changed the synced part {} at turn {}", key.first,
			                   key.second, turn);
			_found.insert(std::move(key));
		}
	}
}

} // namespace openblack::frame_write_guard
