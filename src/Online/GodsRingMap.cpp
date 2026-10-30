/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "GodsRingMap.h"

#include <cctype>
#include <charconv>
#include <cmath>

#include <algorithm>
#include <ranges>

#include <fmt/format.h>

using namespace openblack;
using namespace openblack::gods_ring_map;

namespace
{
constexpr float k_FullTurn = 2.0f * std::numbers::pi_v<float>;
/// The scripts give most angles in thousandths of a radian
constexpr float k_MilliradiansPerRadian = 1000.0f;

/// An angle in the range from 0 to a full turn
float Wrapped(float radians)
{
	const float wrapped = std::fmod(radians, k_FullTurn);
	return wrapped < 0.0f ? wrapped + k_FullTurn : wrapped;
}

std::string_view Trimmed(std::string_view text)
{
	const auto isSpace = [](char c) { return std::isspace(static_cast<unsigned char>(c)) != 0; };
	while (!text.empty() && isSpace(text.front()))
	{
		text.remove_prefix(1);
	}
	while (!text.empty() && isSpace(text.back()))
	{
		text.remove_suffix(1);
	}
	return text;
}

std::optional<float> NumberOf(std::string_view text)
{
	text = Trimmed(text);
	float value = 0.0f;
	const auto [end, error] = std::from_chars(text.data(), text.data() + text.size(), value);
	if (error != std::errc() || end != text.data() + text.size())
	{
		return std::nullopt;
	}
	return value;
}

std::string_view NameOf(PlayerNames player)
{
	return k_PlayerNamesStrs.at(static_cast<size_t>(player));
}

/// The commands of a town that give the town's number first
constexpr std::array<std::string_view, 10> k_TownCommands {
    "CREATE_TOWN",        "CREATE_NEW_TOWN_SPELL", "SET_TOWN_BELIEF",      "SET_TOWN_BELIEF_CAP",   "CREATE_NEW_TOWN_FIELD",
    "CREATE_TOWN_CENTRE", "CREATE_ABODE",          "CREATE_PLANNED_ABODE", "CREATE_TOWN_FISH_FARM", "CREATE_PLANNED_CITADEL"};

/// How a command gives the angle of the thing that it makes
enum class AngleUnit : uint8_t
{
	Milliradians,
	Radians,
};
struct AngleArgument
{
	std::string_view command;
	size_t argument;
	AngleUnit unit;
};
constexpr std::array<AngleArgument, 6> k_AngleArguments {{
    {.command = "CREATE_TOWN_CENTRE", .argument = 3, .unit = AngleUnit::Milliradians},
    {.command = "CREATE_ABODE", .argument = 3, .unit = AngleUnit::Milliradians},
    {.command = "CREATE_PLANNED_ABODE", .argument = 3, .unit = AngleUnit::Milliradians},
    {.command = "CREATE_CITADEL", .argument = 3, .unit = AngleUnit::Milliradians},
    {.command = "CREATE_PLANNED_CITADEL", .argument = 4, .unit = AngleUnit::Milliradians},
    {.command = "CREATE_NEW_TOWN_FIELD", .argument = 3, .unit = AngleUnit::Radians},
}};

bool IsPlayerName(std::string_view text)
{
	return std::ranges::find(k_PlayerNamesStrs, text) != k_PlayerNamesStrs.end();
}

/// The place turned by an angle about a centre, then moved from the centre to a new centre
glm::vec2 Moved(glm::vec2 place, glm::vec2 from, glm::vec2 to, float turn)
{
	const glm::vec2 offset = place - from;
	const float c = std::cos(turn);
	const float s = std::sin(turn);
	return to + glm::vec2((offset.x * c) - (offset.y * s), (offset.x * s) + (offset.y * c));
}
} // namespace

std::optional<PlayerNames> gods_ring_map::GodOfSlot(size_t slot)
{
	// The script of the map names its gods as the scripts of the game do, and these have names for the gods of a land
	// only. A later slot has no god until the scripts can name it.
	if (slot >= player_slots::k_LandGods)
	{
		return std::nullopt;
	}
	return player_slots::PlayerOfSlot(slot);
}

std::array<Slot, k_Slots> gods_ring_map::LayOutSlots(glm::vec2 middle)
{
	std::array<Slot, k_Slots> slots {};
	for (size_t i = 0; i < slots.size(); ++i)
	{
		const float outward = Wrapped(k_FirstSlotAngle + (static_cast<float>(i) * k_SlotStep));
		slots.at(i) = Slot {
		    .index = i,
		    .citadel = middle + (k_RingRadius * glm::vec2(std::cos(outward), std::sin(outward))),
		    .outward = outward,
		    .god = GodOfSlot(i),
		};
	}
	return slots;
}

std::optional<Line> Line::Parse(std::string_view text)
{
	text = Trimmed(text);
	const auto open = text.find('(');
	const auto close = text.rfind(')');
	if (text.empty() || text.starts_with("rem") || open == std::string_view::npos || close == std::string_view::npos ||
	    close < open)
	{
		return std::nullopt;
	}
	Line line {.command = std::string(Trimmed(text.substr(0, open))), .arguments = {}, .original = std::string(text)};
	const auto inside = text.substr(open + 1, close - open - 1);
	if (Trimmed(inside).empty())
	{
		return line;
	}
	// A comma inside quotes is part of a place, not the end of an argument
	bool quoted = false;
	size_t start = 0;
	for (size_t i = 0; i <= inside.size(); ++i)
	{
		if (i < inside.size() && inside[i] == '"')
		{
			quoted = !quoted;
		}
		if (i == inside.size() || (inside[i] == ',' && !quoted))
		{
			auto argument = Trimmed(inside.substr(start, i - start));
			const bool isQuoted = argument.size() >= 2 && argument.front() == '"' && argument.back() == '"';
			if (isQuoted)
			{
				argument = argument.substr(1, argument.size() - 2);
			}
			line.arguments.push_back({.text = std::string(argument), .quoted = isQuoted});
			start = i + 1;
		}
	}
	return line;
}

std::string Line::Text() const
{
	std::string text = command + "(";
	for (size_t i = 0; i < arguments.size(); ++i)
	{
		if (i != 0)
		{
			text += ", ";
		}
		const auto& argument = arguments.at(i);
		text += argument.quoted ? fmt::format("\"{}\"", argument.text) : argument.text;
	}
	return text + ")";
}

std::optional<glm::vec2> gods_ring_map::PlaceOf(const Line::Argument& argument)
{
	if (!argument.quoted)
	{
		return std::nullopt;
	}
	const std::string_view text = argument.text;
	const auto comma = text.find(',');
	if (comma == std::string_view::npos)
	{
		return std::nullopt;
	}
	const auto x = NumberOf(text.substr(0, comma));
	const auto z = NumberOf(text.substr(comma + 1));
	if (!x.has_value() || !z.has_value())
	{
		return std::nullopt;
	}
	return glm::vec2(*x, *z);
}

std::optional<VillageTemplate> gods_ring_map::ReadTemplate(std::string_view script, PlayerNames god, glm::vec2 middle)
{
	const auto godName = NameOf(god);
	VillageTemplate village;
	bool townsStarted = false;
	bool inVillage = false;
	bool villageDone = false;
	for (const auto lineText : script | std::views::split('\n'))
	{
		const auto line = Line::Parse(std::string_view(lineText.begin(), lineText.end()));
		if (!line.has_value())
		{
			continue;
		}
		const auto& arguments = line->arguments;
		const bool namesGod = std::ranges::any_of(arguments, [godName](const auto& a) { return a.text == godName; });
		if (line->command == "CREATE_TOWN")
		{
			townsStarted = true;
			if (!villageDone && namesGod && !arguments.empty())
			{
				inVillage = true;
				village.town = arguments.front().text;
			}
		}
		if (line->command.starts_with("CREATE_WEATHER"))
		{
			village.weather.push_back(*line);
			continue;
		}
		// The map has its own landscape, and its camera starts at the citadel of the player
		if (!townsStarted && line->command != "LOAD_LANDSCAPE" && line->command != "START_CAMERA_POS")
		{
			village.settings.push_back(*line);
			continue;
		}
		if (!inVillage)
		{
			continue;
		}
		village.village.push_back(*line);
		if (line->command == "CREATE_CITADEL" && namesGod && !arguments.empty())
		{
			if (const auto place = PlaceOf(arguments.front()); place.has_value())
			{
				village.citadel = *place;
				const auto outward = *place - middle;
				village.outward = Wrapped(std::atan2(outward.y, outward.x));
				villageDone = true;
			}
			inVillage = false;
		}
	}
	if (!villageDone)
	{
		return std::nullopt;
	}
	return village;
}

std::vector<Line> gods_ring_map::VillageInSlot(const VillageTemplate& village, const Slot& slot,
                                               std::span<const PlayerNames> gods)
{
	const auto templateGod =
	    std::ranges::find_if(village.village, [](const Line& line) { return line.command == "CREATE_TOWN"; });
	// The god of the template is the owner of its town, its third argument
	const std::string godName =
	    templateGod != village.village.end() && templateGod->arguments.size() > 2 ? templateGod->arguments.at(2).text : "";
	const std::string owner(slot.god.has_value() ? NameOf(*slot.god) : NameOf(PlayerNames::NEUTRAL));
	const float turn = slot.outward - village.outward;
	const std::string town = fmt::format("{}", slot.index);

	std::vector<Line> lines;
	for (const auto& line : village.village)
	{
		// The cap of the template's god is the cap of each god of the map; the caps of the other gods go
		if (line.command == "SET_TOWN_BELIEF_CAP")
		{
			if (line.arguments.size() > 1 && line.arguments.at(1).text == godName)
			{
				for (const auto god : gods)
				{
					auto cap = line;
					cap.arguments.front().text = town;
					cap.arguments.at(1).text = NameOf(god);
					lines.push_back(std::move(cap));
				}
			}
			continue;
		}
		// A village with no god has no belief in a god
		if (line.command == "SET_TOWN_BELIEF" && !slot.god.has_value())
		{
			continue;
		}

		auto moved = line;
		auto& arguments = moved.arguments;
		if (std::ranges::find(k_TownCommands, line.command) != k_TownCommands.end() && !arguments.empty() &&
		    arguments.front().text == village.town)
		{
			arguments.front().text = town;
		}
		for (auto& argument : arguments)
		{
			if (const auto place = PlaceOf(argument); place.has_value())
			{
				const auto at = Moved(*place, village.citadel, slot.citadel, turn);
				argument.text = fmt::format("{:.2f},{:.2f}", at.x, at.y);
			}
			else if (argument.quoted && argument.text == godName)
			{
				argument.text = owner;
			}
			else if (argument.quoted && IsPlayerName(argument.text) && argument.text != NameOf(PlayerNames::NEUTRAL))
			{
				// The village of the template has no thing of another god
				argument.text = owner;
			}
		}
		for (const auto& angle : k_AngleArguments)
		{
			if (angle.command != line.command || angle.argument >= arguments.size())
			{
				continue;
			}
			auto& argument = arguments.at(angle.argument);
			const auto value = NumberOf(argument.text);
			if (!value.has_value())
			{
				continue;
			}
			if (angle.unit == AngleUnit::Milliradians)
			{
				const float radians = Wrapped((*value / k_MilliradiansPerRadian) + turn);
				argument.text = fmt::format("{}", static_cast<int32_t>(std::lround(radians * k_MilliradiansPerRadian)));
			}
			else
			{
				argument.text = fmt::format("{:.6f}", Wrapped(*value + turn));
			}
		}
		lines.push_back(std::move(moved));
	}
	return lines;
}

std::string gods_ring_map::MapScript(const VillageTemplate& village, std::span<const Slot> slots)
{
	std::vector<PlayerNames> gods;
	for (const auto& slot : slots)
	{
		if (slot.god.has_value())
		{
			gods.push_back(*slot.god);
		}
	}

	std::string script;
	for (const auto& line : village.settings)
	{
		script += line.original + "\n";
	}
	for (const auto& slot : slots)
	{
		script += fmt::format("\nrem ---------------- Slot: {} ----------------\n", slot.index);
		for (const auto& line : VillageInSlot(village, slot, gods))
		{
			script += line.Text() + "\n";
		}
	}
	script += "\n";
	for (const auto& line : village.weather)
	{
		script += line.original + "\n";
	}
	return script;
}

std::string gods_ring_map::AddToLocalMaps(std::string_view localMaps)
{
	const std::string_view newLine = localMaps.find("\r\n") != std::string_view::npos ? "\r\n" : "\n";
	int32_t last = 0;
	for (const auto lineText : localMaps | std::views::split('\n'))
	{
		const auto line = Trimmed(std::string_view(lineText.begin(), lineText.end()));
		const auto first = line.find(':');
		if (first == std::string_view::npos)
		{
			continue;
		}
		const auto second = line.find(':', first + 1);
		const auto third = second == std::string_view::npos ? second : line.find(':', second + 1);
		if (third != std::string_view::npos && IsThisMap(line.substr(second + 1, third - second - 1)))
		{
			return std::string(localMaps);
		}
		int32_t number = 0;
		const auto [end, error] = std::from_chars(line.data(), line.data() + first, number);
		if (error == std::errc() && end == line.data() + first)
		{
			last = std::max(last, number);
		}
	}
	std::string text(localMaps);
	if (!text.empty() && !text.ends_with('\n'))
	{
		text += newLine;
	}
	return text + fmt::format("{}:{}:{}:{}:{}", last + 1, k_Name, k_File, k_Slots, newLine);
}

bool gods_ring_map::IsThisMap(std::string_view file)
{
	return std::ranges::equal(file, k_File, [](char a, char b) {
		return std::tolower(static_cast<unsigned char>(a)) == std::tolower(static_cast<unsigned char>(b));
	});
}
