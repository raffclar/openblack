/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "WorldStateAtTurn.h"

#include <charconv>

#include <algorithm>
#include <fstream>
#include <system_error>

#include <fmt/format.h>
#include <spdlog/spdlog.h>

#include "Common/GameRandom.h"
#include "ECS/Systems/ParticleRuleSystemInterface.h"
#include "ECS/Systems/SaveGameSystemInterface.h"
#include "ECS/WorldSave.h"
#include "Locator.h"

namespace openblack::world_state_at_turn
{

namespace
{

/// A text as a JSON string, with the quotes and the backslashes escaped
std::string Quoted(std::string_view text)
{
	std::string out = "\"";
	for (const auto letter : text)
	{
		if (letter == '"' || letter == '\\')
		{
			out += '\\';
		}
		if (static_cast<unsigned char>(letter) < 0x20)
		{
			out += fmt::format("\\u{:04x}", static_cast<unsigned>(letter));
			continue;
		}
		out += letter;
	}
	return out + "\"";
}

bool WriteFile(const std::filesystem::path& path, std::span<const char> bytes)
{
	std::ofstream file(path, std::ios::binary | std::ios::trunc);
	if (!file.is_open())
	{
		return false;
	}
	file.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
	return file.good();
}

} // namespace

std::optional<std::vector<uint32_t>> ParseTurns(std::string_view text)
{
	std::vector<uint32_t> turns;
	while (true)
	{
		const auto comma = text.find(',');
		const auto item = text.substr(0, comma);
		uint32_t turn = 0;
		const auto* end = item.data() + item.size();
		const auto [last, error] = std::from_chars(item.data(), end, turn);
		if (item.empty() || error != std::errc() || last != end)
		{
			return std::nullopt;
		}
		turns.push_back(turn);
		if (comma == std::string_view::npos)
		{
			return turns;
		}
		text.remove_prefix(comma + 1);
	}
}

std::filesystem::path StateFile(const std::filesystem::path& folder, uint32_t turn)
{
	return folder / fmt::format("world-{}.obsav", turn);
}

std::filesystem::path MapFile(const std::filesystem::path& folder, uint32_t turn)
{
	return folder / fmt::format("world-{}.json", turn);
}

std::string MapText(const Map& map)
{
	std::string text = fmt::format(R"({{"turn": {}, "syncedSeed": {}, "syncedDraws": {}, "waitingSounds": {}, )", map.turn,
	                               map.syncedSeed, map.syncedDraws, map.waitingSounds);
	text += "\"presentationParts\": [";
	for (size_t index = 0; index < map.parts.size(); ++index)
	{
		const auto& part = map.parts[index];
		text += fmt::format(R"({}{{"component": {}, "field": {}, "marker": {}}})", index == 0 ? "" : ", ",
		                    Quoted(part.component), Quoted(part.field), Quoted(part.marker));
	}
	text += "], \"sections\": [";
	for (size_t index = 0; index < map.sections.size(); ++index)
	{
		const auto& section = map.sections[index];
		text += fmt::format(R"({}{{"name": {}, "size": {}, "presentation": [)", index == 0 ? "" : ", ", Quoted(section.name),
		                    section.size);
		for (size_t range = 0; range < section.presentation.size(); ++range)
		{
			const auto& [first, last] = section.presentation[range];
			text += fmt::format("{}[{}, {}]", range == 0 ? "" : ", ", first, last);
		}
		text += "]}";
	}
	return text + "]}\n";
}

bool WriteIfDue(const Settings& settings, uint32_t turn, const entt::meta_ctx& reflection)
{
	if (std::ranges::find(settings.turns, turn) == settings.turns.end() || !Locator::saveGameSystem::has_value())
	{
		return true;
	}
	Map map {.turn = turn, .parts = synced_state::PresentationParts()};
	for (auto& section : ecs::world_save::SavedSystemSections(reflection))
	{
		map.sections.push_back(
		    {.name = std::move(section.name), .size = section.bytes.size(), .presentation = std::move(section.presentation)});
	}
	if (Locator::gameRandom::has_value())
	{
		const auto& random = Locator::gameRandom::value();
		map.syncedSeed = random.GetSeeds().synced;
		map.syncedDraws = random.GetSyncedDraws();
	}
	if (Locator::particleRuleSystem::has_value())
	{
		map.waitingSounds = Locator::particleRuleSystem::value().GetWaitingSoundCount();
	}
	const auto state = Locator::saveGameSystem::value().WorldState();
	const auto text = MapText(map);
	std::error_code error;
	std::filesystem::create_directories(settings.folder, error);
	const bool written =
	    WriteFile(StateFile(settings.folder, turn), std::span(reinterpret_cast<const char*>(state.data()), state.size())) &&
	    WriteFile(MapFile(settings.folder, turn), text);
	if (const auto logger = spdlog::get("game"); logger != nullptr)
	{
		if (written)
		{
			logger->info("The world state of turn {} ({} bytes, {} sounds wait) is in {}", turn, state.size(),
			             map.waitingSounds, StateFile(settings.folder, turn).string());
		}
		else
		{
			logger->error("The world state of turn {} does not write to {}", turn, settings.folder.string());
		}
	}
	return written;
}

} // namespace openblack::world_state_at_turn
