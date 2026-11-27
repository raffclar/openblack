/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <cstdint>

#include <chrono>
#include <filesystem>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include <LobbyApi.h>

#include "Enums.h"
#include "Server/LockstepHost.h"

/// The report of the end of an internet game to the lobby: each god's position, the gods that it defeated and the time
/// that it played. The lobby keeps the result and gives the ranking points from it.
namespace openblack::server::lobby_results
{

/// The text before the body in the bytes that the server signs
constexpr std::string_view k_SigningPrefix = "bwgame-result-v1\n";
/// The text before the game, the account and the mind file in the bytes that the server signs for the creature of a god
constexpr std::string_view k_CreatureSigningPrefix = "bwgame-creature-v1\n";
/// The header that holds the signature (base64url with no padding)
constexpr std::string_view k_SignatureHeader = "Bwgame-Signature";
/// The time before the server sends the report again after the lobby did not answer
constexpr auto k_SendAgainAfter = std::chrono::seconds(30);
/// The most times that the server sends one report
constexpr uint32_t k_MostSends = 10;
/// The longest id of a game that the lobby keeps
constexpr size_t k_MostGameIdCharacters = 64;
/// The name of the file in the folder of the snapshots that keeps the id of the game, so that a server that continues
/// the game from a snapshot reports it with the same id
constexpr std::string_view k_GameIdFile = "game-id.txt";

/// What the world gives for one god when the game is decided
struct GodFigures
{
	PlayerNames god {PlayerNames::PLAYER_ONE};
	/// The finishing place of the god: 1 is the first god out. None for a god that has no place.
	std::optional<int32_t> place;
	/// The gods that this god defeated
	uint32_t godsDefeated {0};
	/// The mind file of the god's creature when the game is decided. Empty when the god has no creature.
	std::vector<uint8_t> creature;
};

/// The creature of a god of the report, which goes to the lobby after the lobby took the report
struct CreatureReport
{
	/// The account of the god
	int64_t account {0};
	/// The mind file of the god's creature
	std::vector<uint8_t> mind;
};

/// The game that the report is about
struct Game
{
	/// The server's own id for the game
	std::string id;
	/// The land of the game
	std::string land;
	/// The turn at which the game was decided
	uint32_t turn {0};
};

/// The lobby's position of a god from its finishing place: the last place is the winner, position 1. A god with no
/// place shares the position after the gods with a place.
[[nodiscard]] int32_t PositionOf(std::optional<int32_t> place, int32_t lastPlace) noexcept;

/// The report of a decided game. Each god slot whose seat had a player with an account is a god of the report. The
/// clans come from the lobby's answer to the status report. None when fewer than 2 gods have an account, as the lobby
/// ranks only games of 2 gods or more.
[[nodiscard]] std::optional<lobby_api::ResultReport> MakeReport(const Game& game, std::span<const GodFigures> gods,
                                                                std::span<const LockstepHost::Seat> seats,
                                                                std::span<const lobby_api::ServerStatusClan> clans);

/// The creatures of the gods of the report: each god of the report that has a creature, in the order of the report
[[nodiscard]] std::vector<CreatureReport> MakeCreatureReports(std::span<const GodFigures> gods,
                                                              std::span<const LockstepHost::Seat> seats);

/// The bytes that the server signs for the creature of a god: the creature signing prefix, the id of the game and a new
/// line, the account and a new line, then the exact mind file
[[nodiscard]] std::vector<uint8_t> CreatureSignedBytes(std::string_view game, int64_t account, std::span<const uint8_t> mind);

/// The address of the creature of a god after the address of the results: <results>/<game>/creatures/<account>
[[nodiscard]] std::string CreatureAddress(std::string_view resultsAddress, std::string_view game, int64_t account);

/// The bytes that the server signs: the signing prefix, then the exact body
[[nodiscard]] std::vector<uint8_t> SignedBytes(std::string_view body);

/// The address of the report: <api>/servers/<id>/results
[[nodiscard]] std::string ResultsAddress(std::string_view api, std::string_view serverId);

/// What the server does after an answer of the lobby
enum class Next : uint8_t
{
	/// The lobby took the report (or had it already)
	Done,
	/// The lobby refused the report: the server does not send it again
	Stop,
	/// The lobby did not answer or had a fault: the server sends the report again later
	SendAgain,
};
[[nodiscard]] Next NextAfter(int32_t status) noexcept;

/// The id of the game from 16 random bytes
[[nodiscard]] std::string GameIdOf(std::span<const uint8_t, 16> random);
/// The id of the game in the folder of the snapshots. None when the file is not there or does not hold an id.
[[nodiscard]] std::optional<std::string> ReadGameId(const std::filesystem::path& folder);
/// Writes the id of the game in the folder of the snapshots. False when the file does not write.
bool WriteGameId(const std::filesystem::path& folder, std::string_view id);

} // namespace openblack::server::lobby_results
