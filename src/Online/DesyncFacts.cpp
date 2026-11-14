/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "DesyncFacts.h"

#include <cstdint>

#include <string_view>

#include <fmt/format.h>

namespace openblack::online::desync_facts
{
namespace
{
std::string Hex(uint64_t value)
{
	return value == 0 ? std::string() : fmt::format("{:016x}", value);
}
} // namespace

std::vector<std::pair<std::string, std::string>> Listed(const game_socket::SyncFacts& facts)
{
	return {
	    {"turn", std::to_string(facts.turn)},
	    {"build", facts.build},
	    {"data", Hex(facts.data)},
	    {"land", facts.land},
	    {"land_sha256", facts.landSha256},
	    {"map", facts.map},
	    {"seed", std::to_string(facts.seed)},
	    {"patch", facts.patch},
	    {"language", facts.language},
	};
}

std::optional<FactDifference> FirstDifferent(const game_socket::SyncFacts& server, const game_socket::SyncFacts& player)
{
	struct Check
	{
		std::string_view fact;
		std::string server;
		std::string player;
		/// Only when both sides know the fact
		bool needsBoth {false};
		std::string_view words;
	};
	const std::vector<Check> checks {
	    {"build", server.build, player.build, false, "the player has another build"},
	    {"data", Hex(server.data), Hex(player.data), true, "the player has other game data"},
	    {"land", server.land, player.land, false, "the player loaded another land"},
	    {"land_sha256", server.landSha256, player.landSha256, true, "the player loaded another file of the land"},
	    {"map", server.map, player.map, true, "the player got another map from the lobby"},
	    {"seed", std::to_string(server.seed), std::to_string(player.seed), false, "the player has another seed"},
	};
	for (const auto& check : checks)
	{
		if (check.needsBoth && (check.server.empty() || check.player.empty()))
		{
			continue;
		}
		if (check.server != check.player)
		{
			return FactDifference {.fact = std::string(check.fact),
			                       .server = check.server,
			                       .player = check.player,
			                       .words = std::string(check.words)};
		}
	}
	return std::nullopt;
}

} // namespace openblack::online::desync_facts
