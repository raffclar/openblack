/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "ForestDebugHooks.h"

#include <cstdlib>

#include <optional>
#include <sstream>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include <spdlog/spdlog.h>

#include "ECS/Systems/DebugHooksInterface.h"
#include "ECS/Systems/ScreenshotRequestSystemInterface.h"
#include "Game.h"
#include "Locator.h"

using namespace openblack;
using namespace openblack::magic;

namespace
{
struct Shot
{
	unsigned int turns;
	std::string path;
	bool done {false};
};

/// What these hooks keep between calls, in the debug hooks' store (Locator::debugHooks)
struct ForestDebugHooksState
{
	// Each spell's landing turn. A map here, not a component on the spell: test-only state that stays out of the
	// game's components
	std::unordered_map<entt::entity, unsigned int> landed {};
	std::optional<std::vector<Shot>> shots; // OPENBLACK_TEST_FOREST_SHOT, parsed on first use
};

ForestDebugHooksState& ForestDebugHooksData()
{
	return openblack::Locator::debugHooks::value().Get<ForestDebugHooksState>();
}

std::vector<Shot>& Shots()
{
	auto& shots = ForestDebugHooksData().shots;
	if (shots.has_value())
	{
		return *shots;
	}
	shots = [] {
		std::vector<Shot> list;
		const char* value = std::getenv("OPENBLACK_TEST_FOREST_SHOT");
		if (value == nullptr)
		{
			return list;
		}
		std::stringstream stream(value);
		std::string item;
		while (std::getline(stream, item, ';'))
		{
			const auto comma = item.find(',');
			if (comma == std::string::npos)
			{
				continue;
			}
			list.push_back({static_cast<unsigned int>(std::atoi(item.substr(0, comma).c_str())), item.substr(comma + 1)});
		}
		return list;
	}();
	return *shots;
}

} // namespace

void forest_debug::OnLanded(entt::entity spell, unsigned int turn)
{
	if (!Shots().empty())
	{
		ForestDebugHooksData().landed[spell] = turn;
	}
}

void forest_debug::OnTurn(entt::entity spell, unsigned int turn)
{
	const auto landed = ForestDebugHooksData().landed.find(spell);
	if (landed == ForestDebugHooksData().landed.end() || !Locator::screenshotRequest::has_value())
	{
		return;
	}
	for (auto& shot : Shots())
	{
		if (!shot.done && turn >= landed->second + shot.turns)
		{
			shot.done = true;
			Locator::screenshotRequest::value().Request(shot.path);
			SPDLOG_LOGGER_INFO(spdlog::get("game"), "Forest test: screenshot {} turns after the landing (turn {}) -> {}",
			                   shot.turns, turn, shot.path);
			return; // one request per frame
		}
	}
}
