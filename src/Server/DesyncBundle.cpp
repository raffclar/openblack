/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "DesyncBundle.h"

#include <algorithm>
#include <chrono>
#include <fstream>
#include <ios>
#include <ranges>
#include <system_error>

#include <fmt/format.h>
#include <spdlog/spdlog.h>

namespace openblack::server::desync_bundle
{
namespace
{

/// The time at the start of a record's name, such as "20261114T034725Z"
constexpr size_t k_TimeLength = 16;

std::string Escaped(std::string_view text)
{
	std::string out;
	out.reserve(text.size() + 2);
	out.push_back('"');
	for (const char c : text)
	{
		switch (c)
		{
		case '"':
			out += "\\\"";
			break;
		case '\\':
			out += "\\\\";
			break;
		case '\n':
			out += "\\n";
			break;
		default:
			if (static_cast<unsigned char>(c) < 0x20)
			{
				out += fmt::format("\\u{:04x}", static_cast<unsigned char>(c));
			}
			else
			{
				out.push_back(c);
			}
		}
	}
	out.push_back('"');
	return out;
}

std::string FactsObject(const game_socket::SyncFacts& facts)
{
	std::string out = "{";
	bool first = true;
	for (const auto& [name, value] : online::desync_facts::Listed(facts))
	{
		out += fmt::format("{}{}: {}", first ? "" : ", ", Escaped(name), Escaped(value));
		first = false;
	}
	return out + "}";
}

std::string EntityObject(const std::optional<synced_state::EntityHash>& entity)
{
	if (!entity.has_value())
	{
		return "null";
	}
	const auto place = entity->position.has_value()
	                       ? fmt::format("[{}, {}, {}]", (*entity->position)[0], (*entity->position)[1], (*entity->position)[2])
	                       : std::string("null");
	return fmt::format(R"({{"hash": "{:016x}", "kind": {}, "position": {}}})", entity->hash, Escaped(entity->kind), place);
}

/// Only the characters of names, so that the name of a game cannot make another folder
std::string CleanName(std::string_view text)
{
	std::string clean;
	for (const char c : text.substr(0, 64))
	{
		const bool allowed = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '-' || c == '_';
		clean.push_back(allowed ? c : '-');
	}
	return clean.empty() ? std::string("game") : clean;
}

/// The time at the start of a record's name, none for another name
std::optional<std::chrono::system_clock::time_point> TimeOfName(std::string_view name)
{
	if (name.size() < k_TimeLength || name[8] != 'T' || name[15] != 'Z')
	{
		return std::nullopt;
	}
	const auto number = [name](size_t from, size_t count) -> std::optional<int> {
		int value = 0;
		for (const char c : name.substr(from, count))
		{
			if (c < '0' || c > '9')
			{
				return std::nullopt;
			}
			value = value * 10 + (c - '0');
		}
		return value;
	};
	const auto year = number(0, 4);
	const auto month = number(4, 2);
	const auto day = number(6, 2);
	const auto hour = number(9, 2);
	const auto minute = number(11, 2);
	const auto second = number(13, 2);
	if (!year || !month || !day || !hour || !minute || !second)
	{
		return std::nullopt;
	}
	const std::chrono::year_month_day date {std::chrono::year {*year}, std::chrono::month {static_cast<unsigned>(*month)},
	                                        std::chrono::day {static_cast<unsigned>(*day)}};
	if (!date.ok())
	{
		return std::nullopt;
	}
	return std::chrono::sys_days {date} + std::chrono::hours {*hour} + std::chrono::minutes {*minute} +
	       std::chrono::seconds {*second};
}

bool WriteFile(const std::filesystem::path& path, std::span<const char> bytes)
{
	std::ofstream out(path, std::ios::binary | std::ios::trunc);
	out.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
	return static_cast<bool>(out);
}

} // namespace

std::string NameOf(std::string_view game, uint8_t seat, uint32_t turn, std::chrono::system_clock::time_point at)
{
	const auto day = std::chrono::floor<std::chrono::days>(at);
	const std::chrono::year_month_day date {day};
	const std::chrono::hh_mm_ss time {std::chrono::floor<std::chrono::seconds>(at - day)};
	return fmt::format("{:04}{:02}{:02}T{:02}{:02}{:02}Z_{}_slot{}_turn{}", static_cast<int>(date.year()),
	                   static_cast<unsigned>(date.month()), static_cast<unsigned>(date.day()), time.hours().count(),
	                   time.minutes().count(), time.seconds().count(), CleanName(game), seat, turn);
}

std::string FactsJson(const CaptureRecord& record, std::string_view game, std::string_view name)
{
	std::string out = "{\n";
	out += fmt::format("  \"record\": {},\n  \"game\": {},\n", Escaped(name), Escaped(game));
	out += fmt::format("  \"slot\": {},\n  \"part_turn\": {},\n  \"capture_turn\": {},\n", record.seat, record.partTurn,
	                   record.captureTurn);
	out += fmt::format("  \"server\": {},\n", FactsObject(record.server));
	out += fmt::format("  \"player\": {},\n", record.player.has_value() ? FactsObject(*record.player) : "null");
	out += fmt::format("  \"first_fact\": {},\n",
	                   record.fact.has_value() ? fmt::format(R"({{"fact": {}, "server": {}, "player": {}, "words": {}}})",
	                                                         Escaped(record.fact->fact), Escaped(record.fact->server),
	                                                         Escaped(record.fact->player), Escaped(record.fact->words))
	                                           : std::string("null"));
	out += "  \"parts\": [";
	bool firstPart = true;
	for (const auto& part : record.parts)
	{
		out += fmt::format("{}\n    {{\"part\": {}, \"server_entities\": {}, \"player_entities\": {}, \"compared\": {}, "
		                   "\"entities\": [",
		                   firstPart ? "" : ",", Escaped(part.part), part.serverEntities,
		                   part.playerEntities.has_value() ? std::to_string(*part.playerEntities) : std::string("null"),
		                   part.compared ? "true" : "false");
		bool firstEntity = true;
		for (const auto& difference : part.differences)
		{
			out += fmt::format(R"({}{{"entity": {}, "server": {}, "player": {}}})", firstEntity ? "" : ", ", difference.entity,
			                   EntityObject(difference.server), EntityObject(difference.player));
			firstEntity = false;
		}
		out += "]}";
		firstPart = false;
	}
	out += "\n  ],\n  \"missing\": [";
	for (size_t i = 0; i < record.missing.size(); ++i)
	{
		out += fmt::format("{}{}", i == 0 ? "" : ", ", Escaped(record.missing[i]));
	}
	out += fmt::format("],\n  \"server_world_bytes\": {},\n  \"player_world_bytes\": {}\n}}\n", record.serverWorld.size(),
	                   record.playerWorld.size());
	return out;
}

std::vector<std::string> Lines(const CaptureRecord& record)
{
	std::vector<std::string> lines;
	lines.push_back(fmt::format(
	    "The capture of slot {} (parted at turn {}, captured at turn {}): {}", record.seat, record.partTurn, record.captureTurn,
	    record.fact.has_value() ? fmt::format("the first fact that differs is {}: {} (server \"{}\", player \"{}\")",
	                                          record.fact->fact, record.fact->words, record.fact->server, record.fact->player)
	                            : (record.player.has_value() ? std::string("no fact differs: the cause is in the logic")
	                                                         : std::string("the player sent no facts"))));
	for (const auto& part : record.parts)
	{
		if (!part.compared)
		{
			lines.push_back(fmt::format("The capture of slot {}: part {} was not compared ({} entities on the server)",
			                            record.seat, part.part, part.serverEntities));
			continue;
		}
		lines.push_back(fmt::format("The capture of slot {}: {} ({} entities on the server, {} on the player's computer)",
		                            record.seat, synced_state::Describe(part.part, part.differences), part.serverEntities,
		                            part.playerEntities.value_or(0)));
	}
	if (!record.missing.empty())
	{
		std::string missing;
		for (const auto& name : record.missing)
		{
			missing += missing.empty() ? name : ", " + name;
		}
		lines.push_back(fmt::format("The capture of slot {} did not get: {}", record.seat, missing));
	}
	return lines;
}

error_reports::DesyncDetail DetailOf(const CaptureRecord& record, std::string_view game, std::string_view name)
{
	error_reports::DesyncDetail detail;
	if (record.fact.has_value())
	{
		detail.fact = record.fact->fact;
		detail.extras.emplace_back("first_fact", fmt::format("{}: {}", record.fact->fact, record.fact->words));
		detail.extras.emplace_back("first_fact.server", record.fact->server);
		detail.extras.emplace_back("first_fact.player", record.fact->player);
	}
	else
	{
		detail.extras.emplace_back("first_fact", record.player.has_value() ? "none" : "no facts from the player");
	}
	for (const auto& [factName, value] : online::desync_facts::Listed(record.server))
	{
		detail.extras.emplace_back("server." + factName, value);
	}
	if (record.player.has_value())
	{
		for (const auto& [factName, value] : online::desync_facts::Listed(*record.player))
		{
			detail.extras.emplace_back("player." + factName, value);
		}
	}
	for (const auto& part : record.parts | std::views::take(error_reports::k_MostDesyncPartsInEvent))
	{
		detail.extras.emplace_back("entities." + part.part, part.compared ? synced_state::Describe(part.part, part.differences)
		                                                                  : std::string("not compared"));
	}
	detail.extras.emplace_back("capture_turn", std::to_string(record.captureTurn));
	detail.extras.emplace_back("record", std::string(name));
	detail.attachment = FactsJson(record, game, name);
	detail.attachmentName = std::string(k_FactsFile);
	return detail;
}

std::vector<std::filesystem::path>
OldRecords(std::span<const std::pair<std::filesystem::path, std::chrono::system_clock::time_point>> records,
           std::chrono::system_clock::time_point now, const Settings& settings)
{
	std::vector<std::pair<std::filesystem::path, std::chrono::system_clock::time_point>> kept;
	std::vector<std::filesystem::path> old;
	const auto oldest = now - std::chrono::days {settings.keepDays};
	for (const auto& record : records)
	{
		if (record.second < oldest)
		{
			old.push_back(record.first);
		}
		else
		{
			kept.push_back(record);
		}
	}
	std::ranges::sort(kept, {}, &std::pair<std::filesystem::path, std::chrono::system_clock::time_point>::second);
	if (kept.size() > settings.mostRecords)
	{
		for (size_t i = 0; i < kept.size() - settings.mostRecords; ++i)
		{
			old.push_back(kept[i].first);
		}
	}
	return old;
}

void Prune(const Settings& settings, std::chrono::system_clock::time_point now)
{
	std::error_code error;
	if (!std::filesystem::is_directory(settings.folder, error))
	{
		return;
	}
	std::vector<std::pair<std::filesystem::path, std::chrono::system_clock::time_point>> records;
	for (const auto& entry : std::filesystem::directory_iterator(settings.folder, error))
	{
		// Only a folder with the name of a record goes: the server never removes another file
		if (const auto time = TimeOfName(entry.path().filename().string()); time.has_value() && entry.is_directory(error))
		{
			records.emplace_back(entry.path(), *time);
		}
	}
	for (const auto& path : OldRecords(records, now, settings))
	{
		std::filesystem::remove_all(path, error);
		if (auto logger = spdlog::get("game"))
		{
			logger->info("The server removes the old desync record {}", path.filename().string());
		}
	}
}

std::optional<std::filesystem::path> Write(const Settings& settings, const CaptureRecord& record, std::string_view game,
                                           std::chrono::system_clock::time_point now)
{
	const auto name = NameOf(game, record.seat, record.captureTurn, now);
	const auto folder = settings.folder / name;
	std::error_code error;
	std::filesystem::create_directories(folder, error);
	if (error)
	{
		return std::nullopt;
	}
	const auto facts = FactsJson(record, game, name);
	bool written = WriteFile(folder / k_FactsFile, facts);
	if (!record.serverWorld.empty())
	{
		written = WriteFile(folder / k_ServerWorldFile,
		                    {reinterpret_cast<const char*>(record.serverWorld.data()), record.serverWorld.size()}) &&
		          written;
	}
	if (!record.playerWorld.empty())
	{
		written = WriteFile(folder / k_PlayerWorldFile,
		                    {reinterpret_cast<const char*>(record.playerWorld.data()), record.playerWorld.size()}) &&
		          written;
	}
	Prune(settings, now);
	if (!written)
	{
		return std::nullopt;
	}
	return folder;
}

} // namespace openblack::server::desync_bundle
