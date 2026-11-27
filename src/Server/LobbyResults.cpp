/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "LobbyResults.h"

#include <algorithm>
#include <fstream>
#include <iterator>
#include <ranges>
#include <string>
#include <system_error>
#include <utility>

#include <JoinTicket.h>

#include "ECS/StatisticsRules.h"
#include "PlayerSlots.h"
#include "Server/LobbyStatus.h"

namespace openblack::server::lobby_results
{

namespace
{

/// A god of the report: its account and its figures
struct GodWithAccount
{
	int64_t account {0};
	const GodFigures* figures {nullptr};
};

/// Each god slot whose seat had a player with an account and whose god has figures, in the order of the slots
std::vector<GodWithAccount> GodsWithAccounts(std::span<const GodFigures> gods, std::span<const LockstepHost::Seat> seats)
{
	std::vector<GodWithAccount> found;
	for (size_t slot = 0; slot < seats.size(); ++slot)
	{
		const auto& seat = seats[slot];
		const auto account = seat.state == LockstepHost::SeatState::Open ? std::nullopt : lobby_status::AccountId(seat.account);
		const auto player = player_slots::PlayerOfSlot(slot);
		if (!account.has_value() || !player.has_value())
		{
			continue;
		}
		const auto figures = std::ranges::find(gods, *player, &GodFigures::god);
		if (figures != gods.end())
		{
			found.push_back({.account = *account, .figures = &*figures});
		}
	}
	return found;
}

} // namespace

int32_t PositionOf(std::optional<int32_t> place, int32_t lastPlace) noexcept
{
	if (!place.has_value())
	{
		return lastPlace + 1;
	}
	return std::max(lastPlace - *place + 1, 1);
}

std::optional<lobby_api::ResultReport> MakeReport(const Game& game, std::span<const GodFigures> gods,
                                                  std::span<const LockstepHost::Seat> seats,
                                                  std::span<const lobby_api::ServerStatusClan> clans)
{
	// The last place is the place of the winner. The places count only the gods that a human played.
	int32_t lastPlace = 0;
	for (const auto& god : gods)
	{
		lastPlace = std::max(lastPlace, god.place.value_or(0));
	}
	lobby_api::ResultReport report {
	    .game = game.id.substr(0, std::min(game.id.size(), k_MostGameIdCharacters)),
	    .land = game.land,
	    .gods = {},
	};
	const auto timePlayed = static_cast<int64_t>(statistics::TimePlayed(game.turn));
	for (const auto& [account, figures] : GodsWithAccounts(gods, seats))
	{
		const auto clan = std::ranges::find(clans, account, &lobby_api::ServerStatusClan::account);
		report.gods.push_back({
		    .account = account,
		    .clan = clan != clans.end() ? std::optional(clan->clan) : std::nullopt,
		    .position = PositionOf(figures->place, lastPlace),
		    .godsDefeated = static_cast<int32_t>(figures->godsDefeated),
		    .timePlayed = timePlayed,
		});
	}
	if (report.gods.size() < 2)
	{
		return std::nullopt;
	}
	return report;
}

std::vector<uint8_t> SignedBytes(std::string_view body)
{
	std::vector<uint8_t> bytes(k_SigningPrefix.begin(), k_SigningPrefix.end());
	bytes.insert(bytes.end(), body.begin(), body.end());
	return bytes;
}

std::string ResultsAddress(std::string_view api, std::string_view serverId)
{
	while (api.ends_with('/'))
	{
		api.remove_suffix(1);
	}
	return std::string(api) + "/servers/" + std::string(serverId) + "/results";
}

std::vector<CreatureReport> MakeCreatureReports(std::span<const GodFigures> gods, std::span<const LockstepHost::Seat> seats)
{
	std::vector<CreatureReport> creatures;
	for (const auto& [account, figures] : GodsWithAccounts(gods, seats))
	{
		if (!figures->creature.empty())
		{
			creatures.push_back({.account = account, .mind = figures->creature});
		}
	}
	return creatures;
}

std::vector<uint8_t> CreatureSignedBytes(std::string_view game, int64_t account, std::span<const uint8_t> mind)
{
	const auto head = std::string(k_CreatureSigningPrefix) + std::string(game) + '\n' + std::to_string(account) + '\n';
	std::vector<uint8_t> bytes(head.begin(), head.end());
	bytes.insert(bytes.end(), mind.begin(), mind.end());
	return bytes;
}

std::string CreatureAddress(std::string_view resultsAddress, std::string_view game, int64_t account)
{
	return std::string(resultsAddress) + "/" + std::string(game) + "/creatures/" + std::to_string(account);
}

Next NextAfter(int32_t status) noexcept
{
	if (status == 200 || status == 201)
	{
		return Next::Done;
	}
	// No answer, too many calls, or a fault of the lobby
	if (status <= 0 || status == 429 || status >= 500)
	{
		return Next::SendAgain;
	}
	return Next::Stop;
}

std::string GameIdOf(std::span<const uint8_t, 16> random)
{
	return join_ticket::EncodeBase64Url(random);
}

std::optional<std::string> ReadGameId(const std::filesystem::path& folder)
{
	std::ifstream file(folder / k_GameIdFile, std::ios::binary);
	if (!file)
	{
		return std::nullopt;
	}
	std::string id((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
	while (!id.empty() && (id.back() == '\n' || id.back() == '\r'))
	{
		id.pop_back();
	}
	const bool valid = !id.empty() && id.size() <= k_MostGameIdCharacters && std::ranges::all_of(id, [](char c) {
		return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '-' || c == '_';
	});
	return valid ? std::optional(std::move(id)) : std::nullopt;
}

bool WriteGameId(const std::filesystem::path& folder, std::string_view id)
{
	std::error_code error;
	std::filesystem::create_directories(folder, error);
	std::ofstream file(folder / k_GameIdFile, std::ios::binary | std::ios::trunc);
	file << id << '\n';
	return static_cast<bool>(file);
}

} // namespace openblack::server::lobby_results
