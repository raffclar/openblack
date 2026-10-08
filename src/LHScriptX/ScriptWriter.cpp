/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "ScriptWriter.h"

#include <cmath>
#include <cstdio>

#include <algorithm>
#include <array>

using namespace openblack;
using namespace openblack::lhscriptx;

namespace
{
/// The original's sprintf for %0.Nf: the digit string rounded half up (0.625 -> "0.63", 2^-7 -> "0.007813"; the
/// UCRT / glibc round half to even there). The exact decimal expansion of the double (a double below 2^31 has at most
/// 52 binary fraction digits, so %.60f is exact), then rounded half away from zero at `decimals`
std::string FixedHalfUp(double value, int decimals)
{
	std::array<char, 128> buffer;
	std::snprintf(buffer.data(), buffer.size(), "%.60f", std::fabs(value));
	std::string s(buffer.data());
	const auto dot = s.find('.');
	const auto keep = dot + 1 + static_cast<size_t>(decimals);
	const bool up = keep < s.size() && s[keep] >= '5';
	s.resize(decimals > 0 ? keep : dot);
	if (up)
	{
		auto i = s.size();
		while (i > 0)
		{
			--i;
			if (s[i] == '.')
			{
				continue;
			}
			if (s[i] != '9')
			{
				++s[i];
				break;
			}
			s[i] = '0';
			if (i == 0)
			{
				s.insert(s.begin(), '1');
			}
		}
	}
	return value < 0.0 ? "-" + s : s;
}
} // namespace

// clang-format off
const std::array<CommandFormat, 105> lhscriptx::k_CommandFormats = {{
	{"CREATE_MIST", "AFNFF"}, // 0
	{"CREATE_PATH", "NNNN"}, // 1
	{"CREATE_TOWN", "NALNL"}, // 2
	{"SET_TOWN_BELIEF", "NLF"}, // 3
	{"SET_TOWN_BELIEF_CAP", "NLF"}, // 4
	{"SET_TOWN_UNINHABITABLE", "N"}, // 5
	{"SET_TOWN_CONGREGATION_POS", "NA"}, // 6
	{"CREATE_ABODE", "NALNNNN"}, // 7
	{"CREATE_PLANNED_ABODE", "NALNNNN"}, // 8
	{"CREATE_TOWN_CENTRE", "NALNNN"}, // 9
	{"CREATE_TOWN_SPELL", "NL"}, // 10
	{"CREATE_NEW_TOWN_SPELL", "NL"}, // 11
	{"CREATE_TOWN_CENTRE_SPELL_ICON", "NL"}, // 12
	{"CREATE_SPELL_ICON", "ALNNN"}, // 13
	{"CREATE_PLANNED_SPELL_ICON", "NALNNN"}, // 14
	{"CREATE_VILLAGER", "AAL"}, // 15
	{"CREATE_TOWN_VILLAGER", "NAAN"}, // 16
	{"CREATE_SPECIAL_TOWN_VILLAGER", "NANN"}, // 17
	{"CREATE_VILLAGER_POS", "AALN"}, // 18
	{"CREATE_CITADEL", "ANLNN"}, // 19
	{"CREATE_PLANNED_CITADEL", "NANLNN"}, // 20
	{"CREATE_CREATURE_PEN", "ANNNNN"}, // 21
	{"CREATE_WORSHIP_SITE", "ANLLNN"}, // 22
	{"CREATE_PLANNED_WORSHIP_SITE", "ANLLNN"}, // 23
	{"CREATE_ANIMAL", "ANNN"}, // 24
	{"CREATE_NEW_ANIMAL", "ANNNN"}, // 25
	{"CREATE_FOREST", "NA"}, // 26
	{"CREATE_TREE", "NANNN"}, // 27
	{"CREATE_NEW_TREE", "NANNFFF"}, // 28
	{"CREATE_FIELD", "AN"}, // 29
	{"CREATE_TOWN_FIELD", "NAN"}, // 30
	{"CREATE_FISH_FARM", "AN"}, // 31
	{"CREATE_TOWN_FISH_FARM", "NAN"}, // 32
	{"CREATE_FEATURE", "ANNNN"}, // 33
	{"CREATE_FLOWERS", "ANFF"}, // 34
	{"CREATE_WALL_SECTION", "ANNNN"}, // 35
	{"CREATE_PLANNED_WALL_SECTION", "ANNNN"}, // 36
	{"CREATE_PITCH", "ANNNNN"}, // 37
	{"CREATE_POT", "ANNN"}, // 38
	{"CREATE_TOWN_TEMPORARY_POTS", "NNN"}, // 39
	{"CREATE_MOBILEOBJECT", "ANNN"}, // 40
	{"CREATE_MOBILESTATIC", "ANFF"}, // 41
	{"CREATE_MOBILE_STATIC", "ANFFFFF"}, // 42
	{"CREATE_DEAD_TREE", "ALNFFFF"}, // 43
	{"CREATE_SCAFFOLD", "NANNN"}, // 44
	{"COUNTRY_CHANGE", "AN"}, // 45
	{"HEIGHT_CHANGE", "AN"}, // 46
	{"CREATE_CREATURE", "ANN"}, // 47
	{"CREATE_CREATURE_FROM_FILE", "LNAA"}, // 48
	{"CREATE_FLOCK", "NAANNN"}, // 49
	{"LOAD_LANDSCAPE", "L"}, // 50
	{"VERSION", "F"}, // 51
	{"CREATE_AREA", "AF"}, // 52
	{"START_CAMERA_POS", "A"}, // 53
	{"FLY_BY_FILE", "L"}, // 54
	{"TOWN_NEEDS_POS", "NA"}, // 55
	{"CREATE_FURNITURE", "ANF"}, // 56
	{"CREATE_BIG_FOREST", "ANFF"}, // 57
	{"CREATE_NEW_BIG_FOREST", "ANNFF"}, // 58
	{"CREATE_INFLUENCE_RING", "ANFN"}, // 59
	{"CREATE_WEATHER_CLIMATE", "NNAFF"}, // 60
	{"CREATE_WEATHER_CLIMATE_RAIN", "NFNNN"}, // 61
	{"CREATE_WEATHER_CLIMATE_TEMP", "NFF"}, // 62
	{"CREATE_WEATHER_CLIMATE_WIND", "NFFF"}, // 63
	{"CREATE_WEATHER_STORM", "NAFNAAAFA"}, // 64
	{"BRUSH_SIZE", "FF"}, // 65
	{"CREATE_STREAM", "N"}, // 66
	{"CREATE_STREAM_POINT", "NA"}, // 67
	{"CREATE_WATERFALL", "A"}, // 68
	{"CREATE_ARENA", "AF"}, // 69
	{"CREATE_FOOTPATH", "N"}, // 70
	{"CREATE_FOOTPATH_NODE", "NA"}, // 71
	{"LINK_FOOTPATH", "N"}, // 72
	{"CREATE_BONFIRE", "AFFF"}, // 73
	{"CREATE_BASE", "AN"}, // 74
	{"CREATE_NEW_FEATURE", "ALNNN"}, // 75
	{"SET_INTERACT_DESIRE", "F"}, // 76
	{"TOGGLE_COMPUTER_PLAYER", "LN"}, // 77
	{"SET_COMPUTER_PLAYER_CREATURE_LIKE", "LL"}, // 78
	{"MULTIPLAYER_DEBUG", "NN"}, // 79
	{"CREATE_STREET_LANTERN", "AN"}, // 80
	{"CREATE_STREET_LIGHT", "A"}, // 81
	{"SET_LAND_NUMBER", "N"}, // 82
	{"CREATE_ONE_SHOT_SPELL", "AL"}, // 83
	{"CREATE_ONE_SHOT_SPELL_PU", "AL"}, // 84
	{"CREATE_FIRE_FLY", "A"}, // 85
	{"TOWN_DESIRE_BOOST", "NLF"}, // 86
	{"CREATE_ANIMATED_STATIC", "ALNN"}, // 87
	{"FIRE_FLY_SPELL_REWARD_PROB", "AF"}, // 88
	{"CREATE_NEW_TOWN_FIELD", "NANF"}, // 89
	{"CREATE_SPELL_DISPENSER", "NALLFFF"}, // 90
	{"LOAD_COMPUTER_PLAYER_PERSONALLTY", "NA"}, // 91
	{"SET_COMPUTER_PLAYER_PERSONALLTY", "LAF"}, // 92
	{"SET_GLOBAL_LAND_BALANCE", "NF"}, // 93
	{"SET_LAND_BALANCE", "LNF"}, // 94
	{"CREATE_DRINK_WAYPOINT", "A"}, // 95
	{"SET_TOWN_INFLUENCE_MULTIPLIER", "F"}, // 96
	{"SET_PLAYER_INFLUENCE_MULTIPLIER", "F"}, // 97
	{"SET_TOWN_BALANCE_BELIEF_SCALE", "NF"}, // 98
	{"START_GAME_MESSAGE", "AN"}, // 99
	{"ADD_GAME_MESSAGE_LINE", "AN"}, // 100
	{"EDIT_LEVEL", ""}, // 101
	{"SET_NIGHTTIME", "FFF"}, // 102
	{"MAKE_LAST_OBJECT_ARTIFACT", "NLF"}, // 103
	{"SET_LOST_TOWN_SCALE", "F"}, // 104
}};
// clang-format on

const CommandFormat* lhscriptx::FindCommandFormat(std::string_view name)
{
	const auto it = std::find_if(k_CommandFormats.begin(), k_CommandFormats.end(),
	                             [name](const CommandFormat& c) { return c.name == name; });
	return it != k_CommandFormats.end() ? &*it : nullptr;
}

std::string lhscriptx::CommandAsText(const CommandFormat& command)
{
	// the name, "(", remember the length
	std::string text(command.name);
	text += "(";
	const auto start = text.size();
	// the 12 letters
	for (const char letter : command.types)
	{
		switch (letter)
		{
		case 'A':
			text += "%s, ";
			break;
		case 'L':
			text += "\"%s\", ";
			break;
		case 'N':
			text += "%d, ";
			break;
		case 'F':
			text += "%f, ";
			break;
		default:
			break; // padding
		}
	}
	// something was added -> the last two characters go
	if (text.size() != start)
	{
		text.resize(text.size() - 2);
	}
	text += ")\n";
	return text;
}

std::string lhscriptx::PositionText(const map_coords::MapCoords& coords)
{
	// the int (exact) x 10.0f rounded to a float (the double product of an int32 and 10 is exact, so one rounding),
	// x 2^-16 exact, stored as a double
	const auto metres = [](int32_t fixed) {
		return static_cast<double>(static_cast<float>(static_cast<double>(fixed) * 10.0) * (1.0f / 65536.0f));
	};
	return "\"" + FixedHalfUp(metres(coords.x), 2) + "," + FixedHalfUp(metres(coords.z), 2) + "\"";
}

std::optional<std::string> lhscriptx::WriteCommand(std::string_view name, std::initializer_list<CommandValue> values)
{
	const auto* command = FindCommandFormat(name);
	if (command == nullptr)
	{
		return std::nullopt;
	}
	std::string line(command->name);
	line += "(";
	const auto* value = values.begin();
	bool first = true;
	std::array<char, 512> buffer;
	for (const char letter : command->types)
	{
		if (letter != 'A' && letter != 'L' && letter != 'N' && letter != 'F')
		{
			continue;
		}
		if (value == values.end())
		{
			return std::nullopt;
		}
		if (!first)
		{
			line += ", ";
		}
		first = false;
		switch (letter)
		{
		case 'A':
			if (const auto* coords = std::get_if<map_coords::MapCoords>(value))
			{
				line += PositionText(*coords);
			}
			else if (const auto* text = std::get_if<std::string>(value))
			{
				line += *text;
			}
			else
			{
				return std::nullopt;
			}
			break;
		case 'L':
			if (const auto* text = std::get_if<std::string>(value))
			{
				line += "\"" + *text + "\""; // no escaping in the original
			}
			else
			{
				return std::nullopt;
			}
			break;
		case 'N':
			if (const auto* number = std::get_if<int32_t>(value))
			{
				std::snprintf(buffer.data(), buffer.size(), "%d", *number);
				line += buffer.data();
			}
			else
			{
				return std::nullopt;
			}
			break;
		default: // 'F': a float promoted to double, printf's 6 decimals
			if (const auto* real = std::get_if<float>(value))
			{
				line += FixedHalfUp(static_cast<double>(*real), 6);
			}
			else
			{
				return std::nullopt;
			}
			break;
		}
		++value;
	}
	if (value != values.end())
	{
		return std::nullopt;
	}
	line += ")\n";
	return line;
}
