/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <cstddef>
#include <cstdint>

#include <chrono>
#include <filesystem>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "Common/DesyncReports.h"
#include "Server/DesyncCapture.h"

/// The record of a desync on the server's disk: a folder with the server's world and the player's world of the turn of
/// the capture, and the facts of the capture (facts.json). tools/sim/desync_bundle.py compares the two worlds entity by
/// entity and component by component. The server keeps the records for some days.
namespace openblack::server::desync_bundle
{

/// The folder of the records, relative to the working folder of the server: its data folder
inline constexpr std::string_view k_DefaultFolder = "desyncs";
/// The days that the server keeps a record
inline constexpr uint32_t k_DefaultKeepDays = 14;
/// The most records that the folder keeps. The oldest go first.
inline constexpr size_t k_MostRecords = 20;
/// The names of the files of a record
inline constexpr std::string_view k_FactsFile = "facts.json";
inline constexpr std::string_view k_ServerWorldFile = "server.obsav";
inline constexpr std::string_view k_PlayerWorldFile = "player.obsav";

struct Settings
{
	std::filesystem::path folder {k_DefaultFolder};
	uint32_t keepDays {k_DefaultKeepDays};
	size_t mostRecords {k_MostRecords};
};

/// The name of the folder of a record: the time in UTC, the game, the god slot and the turn of the capture, such as
/// "20261114T034725Z_game_slot1_turn52". The game has only the characters of names, so the name is a safe folder name.
[[nodiscard]] std::string NameOf(std::string_view game, uint8_t seat, uint32_t turn, std::chrono::system_clock::time_point at);

/// The facts of a capture as JSON, for facts.json and for the file of the event
[[nodiscard]] std::string FactsJson(const CaptureRecord& record, std::string_view game, std::string_view name);

/// The capture in lines of words, for the log
[[nodiscard]] std::vector<std::string> Lines(const CaptureRecord& record);

/// What the capture gives the event of its desync: the first fact that differs, the first entities that differ in each
/// part, the name of the record and its facts as a file
[[nodiscard]] error_reports::DesyncDetail DetailOf(const CaptureRecord& record, std::string_view game, std::string_view name);

/// The records to remove: each one older than the days to keep, then the oldest past the most records. Each record is
/// its folder and the time that it was written.
[[nodiscard]] std::vector<std::filesystem::path>
OldRecords(std::span<const std::pair<std::filesystem::path, std::chrono::system_clock::time_point>> records,
           std::chrono::system_clock::time_point now, const Settings& settings);

/// Writes the record of a capture into a new folder in the folder of the settings, and removes the old records. Gives
/// the folder, or none when it could not write.
std::optional<std::filesystem::path> Write(const Settings& settings, const CaptureRecord& record, std::string_view game,
                                           std::chrono::system_clock::time_point now);

/// Removes the old records of the folder of the settings
void Prune(const Settings& settings, std::chrono::system_clock::time_point now);

} // namespace openblack::server::desync_bundle
