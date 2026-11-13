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
#include <iterator>
#include <limits>
#include <map>
#include <ranges>

#include <fmt/format.h>
#include <glm/geometric.hpp>
#include <glm/vec2.hpp>

using namespace openblack;
using namespace openblack::gods_ring_map;

namespace
{
// The map's places and angles use double precision, so that each computer that makes the map writes the same numbers
constexpr double k_FullTurn = 2.0 * std::numbers::pi;
/// The scripts give most angles in thousandths of a radian
constexpr double k_MilliradiansPerRadian = 1000.0;

/// An angle in the range from 0 to a full turn
double Wrapped(double radians)
{
	const double wrapped = std::fmod(radians, k_FullTurn);
	return wrapped < 0.0 ? wrapped + k_FullTurn : wrapped;
}

/// An angle in the range from minus a half turn to a half turn
double Signed(double radians)
{
	const double wrapped = Wrapped(radians);
	return wrapped > std::numbers::pi ? wrapped - k_FullTurn : wrapped;
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

std::string NameOf(PlayerNames player)
{
	return player_slots::ScriptNameOf(player);
}

/// The commands of a town that give the town's number first
constexpr std::array<std::string_view, 10> k_TownCommands {
    "CREATE_TOWN",        "CREATE_NEW_TOWN_SPELL", "SET_TOWN_BELIEF",      "SET_TOWN_BELIEF_CAP",   "CREATE_NEW_TOWN_FIELD",
    "CREATE_TOWN_CENTRE", "CREATE_ABODE",          "CREATE_PLANNED_ABODE", "CREATE_TOWN_FISH_FARM", "CREATE_PLANNED_CITADEL"};

/// The commands of the things that stand on the land of a town: the copy of a town in a gap stands between the same
/// things of the two villages beside it
constexpr std::array<std::string_view, 7> k_BuildingCommands {
    "CREATE_NEW_TOWN_FIELD", "CREATE_TOWN_CENTRE", "CREATE_ABODE",          "CREATE_PLANNED_ABODE",
    "CREATE_TOWN_FISH_FARM", "CREATE_CITADEL",     "CREATE_PLANNED_CITADEL"};

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
constexpr std::array<AngleArgument, 8> k_AngleArguments {{
    {.command = "CREATE_TOWN_CENTRE", .argument = 3, .unit = AngleUnit::Milliradians},
    {.command = "CREATE_ABODE", .argument = 3, .unit = AngleUnit::Milliradians},
    {.command = "CREATE_PLANNED_ABODE", .argument = 3, .unit = AngleUnit::Milliradians},
    {.command = "CREATE_CITADEL", .argument = 3, .unit = AngleUnit::Milliradians},
    {.command = "CREATE_PLANNED_CITADEL", .argument = 4, .unit = AngleUnit::Milliradians},
    {.command = "CREATE_NEW_TOWN_FIELD", .argument = 3, .unit = AngleUnit::Radians},
    {.command = "CREATE_NEW_TREE", .argument = 4, .unit = AngleUnit::Radians},
    {.command = "CREATE_NEW_BIG_FOREST", .argument = 3, .unit = AngleUnit::Radians},
}};

/// The arguments of the things of a town that give a number of a flock, of a forest or of a town
struct NumberArgument
{
	std::string_view command;
	size_t argument;
};
constexpr std::array<NumberArgument, 2> k_FlockArguments {{
    {.command = "CREATE_FLOCK", .argument = 0},
    {.command = "CREATE_NEW_ANIMAL", .argument = 2},
}};
constexpr std::array<NumberArgument, 2> k_ForestArguments {{
    {.command = "CREATE_FOREST", .argument = 0},
    {.command = "CREATE_NEW_TREE", .argument = 0},
}};
constexpr std::array<NumberArgument, 2> k_ThingTownArguments {{
    {.command = "CREATE_FLOCK", .argument = 5},
    {.command = "CREATE_NEW_ANIMAL", .argument = 3},
}};

bool IsPlayerName(std::string_view text)
{
	return player_slots::PlayerOfScriptName(text).has_value();
}

/// The place of the first argument of the line that is a place
std::optional<glm::vec2> FirstPlace(const Line& line)
{
	for (const auto& argument : line.arguments)
	{
		if (const auto place = PlaceOf(argument); place.has_value())
		{
			return place;
		}
	}
	return std::nullopt;
}

/// The direction of a place from a middle, in radians from the x axis toward the z axis
double DirectionOf(glm::dvec2 place, glm::dvec2 middle)
{
	const auto offset = place - middle;
	return std::atan2(offset.y, offset.x);
}

/// How a group of lines moves onto the map: each place turns about a centre, then moves from the centre to a new centre
struct Placement
{
	glm::dvec2 from {0.0};
	glm::dvec2 to {0.0};
	double turn {0.0};
	/// The number of the town in the template, and its number on the map
	std::string_view templateTown;
	size_t town {0};
	/// The owner of the town on the map, and the god of the template whose cap each god of the map gets
	std::string owner;
	std::string_view templateGod;

	[[nodiscard]] glm::dvec2 Moved(glm::dvec2 place) const
	{
		const glm::dvec2 offset = place - from;
		const double c = std::cos(turn);
		const double s = std::sin(turn);
		return to + glm::dvec2((offset.x * c) - (offset.y * s), (offset.x * s) + (offset.y * c));
	}
};

/// The new number of a flock or a forest of the town, from its place in the town's list. The number stays when it is
/// not in the list, as for a group of trees in no forest.
void Renumber(Line::Argument& argument, std::span<const std::string> numbers, size_t town)
{
	const auto found = std::ranges::find(numbers, argument.text);
	if (found != numbers.end())
	{
		const auto index = static_cast<int32_t>(std::distance(numbers.begin(), found));
		argument.text = fmt::format("{}", k_FirstThingNumber + (static_cast<int32_t>(town) * k_ThingsPerTown) + index);
	}
}

/// The lines put on the map
void AddPlaced(std::vector<Line>& lines, std::span<const Line> from, const TownThings& things, const Placement& placement,
               std::span<const PlayerNames> gods)
{
	const std::string town = fmt::format("{}", placement.town);
	const bool neutral = placement.owner == NameOf(PlayerNames::NEUTRAL);
	for (const auto& line : from)
	{
		// The cap of the template's god is the cap of each god of the map; the caps of the other gods go
		if (line.command == "SET_TOWN_BELIEF_CAP")
		{
			if (line.arguments.size() > 1 && line.arguments.at(1).text == placement.templateGod)
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
		// A town with no god has no belief in a god
		if (line.command == "SET_TOWN_BELIEF" && neutral)
		{
			continue;
		}

		auto moved = line;
		auto& arguments = moved.arguments;
		if (std::ranges::find(k_TownCommands, line.command) != k_TownCommands.end() && !arguments.empty() &&
		    arguments.front().text == placement.templateTown)
		{
			arguments.front().text = town;
		}
		for (const auto& number : k_ThingTownArguments)
		{
			if (number.command == line.command && number.argument < arguments.size() &&
			    arguments.at(number.argument).text == placement.templateTown)
			{
				arguments.at(number.argument).text = town;
			}
		}
		for (const auto& number : k_FlockArguments)
		{
			if (number.command == line.command && number.argument < arguments.size())
			{
				Renumber(arguments.at(number.argument), things.flocks, placement.town);
			}
		}
		for (const auto& number : k_ForestArguments)
		{
			if (number.command == line.command && number.argument < arguments.size())
			{
				Renumber(arguments.at(number.argument), things.forests, placement.town);
			}
		}
		for (auto& argument : arguments)
		{
			if (const auto place = PlaceOf(argument); place.has_value())
			{
				const auto at = placement.Moved(glm::dvec2(*place));
				argument.text = fmt::format("{:.2f},{:.2f}", at.x, at.y);
			}
			else if (argument.quoted && IsPlayerName(argument.text) && argument.text != NameOf(PlayerNames::NEUTRAL))
			{
				// The town of the template has no thing of another god
				argument.text = placement.owner;
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
				const double radians = Wrapped((static_cast<double>(*value) / k_MilliradiansPerRadian) + placement.turn);
				argument.text = fmt::format("{}", static_cast<int32_t>(std::lround(radians * k_MilliradiansPerRadian)));
			}
			else
			{
				argument.text = fmt::format("{:.6f}", Wrapped(static_cast<double>(*value) + placement.turn));
			}
		}
		lines.push_back(std::move(moved));
	}
}

/// What the reading of the whole script knows of a town
struct LandTown
{
	TownTemplate copy;
	std::string owner;
	/// The place that the distance to its forests starts at: its storage pit, else its centre
	glm::vec2 store {0.0f};
	/// The places that a flock of no town is nearest to: the town and the god's citadel
	std::vector<glm::vec2> sites;
};

/// The towns of the script with their lines, and the things of each town: a flock of a town is the town's, a flock of
/// no town is the town's nearest to it, and a forest or a group of trees in no forest is the town's with the nearest
/// storage pit, when it is near enough
std::vector<LandTown> ReadTowns(std::span<const Line> lines)
{
	std::vector<LandTown> towns;
	std::optional<size_t> current;
	for (const auto& line : lines)
	{
		const auto& arguments = line.arguments;
		if (line.command == "CREATE_TOWN" && arguments.size() > 2)
		{
			const auto centre = PlaceOf(arguments.at(1)).value_or(glm::vec2(0.0f));
			towns.push_back({.copy = {.town = {}, .things = {}, .number = arguments.front().text, .centre = centre},
			                 .owner = arguments.at(2).text,
			                 .store = centre,
			                 .sites = {centre}});
			current = towns.size() - 1;
		}
		if (!current.has_value())
		{
			continue;
		}
		auto& town = towns.at(*current);
		const bool ofTown = std::ranges::find(k_TownCommands, line.command) != k_TownCommands.end() && !arguments.empty() &&
		                    arguments.front().text == town.copy.number;
		if (!ofTown && line.command != "CREATE_VILLAGER_POS" && line.command != "CREATE_CITADEL")
		{
			current.reset();
			continue;
		}
		town.copy.town.push_back(line);
		if (line.command == "CREATE_ABODE" && arguments.size() > 2 && arguments.at(2).text.ends_with("_STORAGE_PIT"))
		{
			town.store = PlaceOf(arguments.at(1)).value_or(town.store);
		}
		if (line.command == "CREATE_CITADEL")
		{
			if (const auto place = FirstPlace(line); place.has_value())
			{
				town.sites.push_back(*place);
			}
			current.reset();
		}
	}

	const auto townByNumber = [&towns](std::string_view number) -> std::optional<size_t> {
		const auto found = std::ranges::find(towns, number, [](const LandTown& town) { return town.copy.number; });
		return found == towns.end() ? std::nullopt : std::optional(static_cast<size_t>(found - towns.begin()));
	};
	const auto nearestSite = [&towns](glm::vec2 place) {
		size_t nearest = 0;
		float best = std::numeric_limits<float>::max();
		for (size_t i = 0; i < towns.size(); ++i)
		{
			for (const auto& site : towns.at(i).sites)
			{
				if (const float distance = glm::distance(site, place); distance < best)
				{
					best = distance;
					nearest = i;
				}
			}
		}
		return nearest;
	};
	const auto nearestStore = [&towns](glm::vec2 place) -> std::optional<size_t> {
		std::optional<size_t> nearest;
		float best = k_TownForestReach;
		for (size_t i = 0; i < towns.size(); ++i)
		{
			if (const float distance = glm::distance(towns.at(i).store, place); distance < best)
			{
				best = distance;
				nearest = i;
			}
		}
		return nearest;
	};

	// The forests, and the middle of each group of trees with a number that no forest has
	std::map<std::string, glm::vec2> forests;
	std::map<std::string, std::pair<glm::dvec2, size_t>> loneTrees;
	for (const auto& line : lines)
	{
		if (line.arguments.size() < 2)
		{
			continue;
		}
		const auto& number = line.arguments.front().text;
		const auto place = PlaceOf(line.arguments.at(1));
		if (line.command == "CREATE_FOREST" && place.has_value())
		{
			forests.try_emplace(number, *place);
		}
		else if (line.command == "CREATE_NEW_TREE" && place.has_value() && !forests.contains(number))
		{
			auto& [sum, count] = loneTrees[number];
			sum += glm::dvec2(*place);
			++count;
		}
	}
	// A tree joins the first forest with its number, and an animal the latest flock with its number
	std::map<std::string, std::optional<size_t>> forestOwners;
	for (const auto& [number, place] : forests)
	{
		forestOwners[number] = nearestStore(place);
	}
	std::map<std::string, std::optional<size_t>> loneOwners;
	for (const auto& [number, group] : loneTrees)
	{
		const auto& [sum, count] = group;
		loneOwners[number] = nearestStore(glm::vec2(sum / static_cast<double>(count)));
	}
	std::map<std::string, size_t> flockOwners;
	for (const auto& line : lines)
	{
		const auto& arguments = line.arguments;
		std::optional<size_t> owner;
		if (line.command == "CREATE_FLOCK" && arguments.size() > 5)
		{
			const auto place = PlaceOf(arguments.at(1));
			owner = townByNumber(arguments.at(5).text);
			if (!owner.has_value() && place.has_value())
			{
				owner = nearestSite(*place);
			}
			if (owner.has_value())
			{
				flockOwners[arguments.front().text] = *owner;
				towns.at(*owner).copy.things.flocks.push_back(arguments.front().text);
			}
		}
		else if (line.command == "CREATE_NEW_ANIMAL" && arguments.size() > 2)
		{
			if (const auto found = flockOwners.find(arguments.at(2).text); found != flockOwners.end())
			{
				owner = found->second;
			}
		}
		else if (line.command == "CREATE_FOREST" && !arguments.empty())
		{
			if (const auto found = forestOwners.find(arguments.front().text); found != forestOwners.end())
			{
				owner = found->second;
				// A second forest with the same number has no trees, so it is not part of the copy
				forestOwners.erase(found);
				if (owner.has_value())
				{
					towns.at(*owner).copy.things.forests.push_back(arguments.front().text);
				}
			}
		}
		else if (line.command == "CREATE_NEW_TREE" && !arguments.empty())
		{
			const auto& number = arguments.front().text;
			if (forests.contains(number))
			{
				const auto forest = std::ranges::find(towns, true, [&number](const LandTown& town) {
					return std::ranges::find(town.copy.things.forests, number) != town.copy.things.forests.end();
				});
				owner = forest == towns.end() ? std::nullopt : std::optional(static_cast<size_t>(forest - towns.begin()));
			}
			else if (const auto found = loneOwners.find(number); found != loneOwners.end())
			{
				owner = found->second;
			}
		}
		if (owner.has_value())
		{
			towns.at(*owner).copy.things.lines.push_back(line);
		}
	}
	return towns;
}
} // namespace

std::optional<PlayerNames> gods_ring_map::GodOfSlot(size_t slot)
{
	// Each slot has its god. The script of the map names the gods after the seventh PLAYER_8 to PLAYER_32.
	return slot < k_Slots ? player_slots::PlayerOfSlot(slot) : std::nullopt;
}

std::array<Slot, k_Slots> gods_ring_map::LayOutSlots(glm::vec2 middle)
{
	std::array<Slot, k_Slots> slots {};
	for (size_t i = 0; i < slots.size(); ++i)
	{
		const float outward =
		    static_cast<float>(Wrapped(static_cast<double>(k_FirstSlotAngle + (static_cast<float>(i) * k_SlotStep))));
		const double radians = outward;
		slots.at(i) = Slot {
		    .index = i,
		    .citadel = glm::vec2(glm::dvec2(middle) +
		                         (static_cast<double>(k_RingRadius) * glm::dvec2(std::cos(radians), std::sin(radians)))),
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
	std::vector<Line> lines;
	for (const auto lineText : script | std::views::split('\n'))
	{
		if (auto line = Line::Parse(std::string_view(lineText.begin(), lineText.end())); line.has_value())
		{
			lines.push_back(std::move(*line));
		}
	}

	const auto godName = NameOf(god);
	VillageTemplate village;
	village.god = godName;
	village.middle = middle;
	bool townsStarted = false;
	bool inVillage = false;
	bool villageDone = false;
	for (const auto& line : lines)
	{
		const auto& arguments = line.arguments;
		const bool namesGod = std::ranges::any_of(arguments, [&godName](const auto& a) { return a.text == godName; });
		if (line.command == "CREATE_TOWN")
		{
			townsStarted = true;
			if (!villageDone && namesGod && !arguments.empty())
			{
				inVillage = true;
				village.town = arguments.front().text;
			}
		}
		if (line.command.starts_with("CREATE_WEATHER"))
		{
			village.weather.push_back(line);
			continue;
		}
		// The chances of the spells that the fireflies give are part of the land, wherever the script gives them
		if (line.command == "FIRE_FLY_SPELL_REWARD_PROB")
		{
			village.settings.push_back(line);
			continue;
		}
		// The map has its own landscape, and its camera starts at the citadel of the player
		if (!townsStarted && line.command != "LOAD_LANDSCAPE" && line.command != "START_CAMERA_POS")
		{
			village.settings.push_back(line);
			continue;
		}
		if (!inVillage)
		{
			continue;
		}
		village.village.push_back(line);
		if (line.command == "CREATE_CITADEL" && namesGod && !arguments.empty())
		{
			if (const auto place = PlaceOf(arguments.front()); place.has_value())
			{
				village.citadel = *place;
				village.outward = static_cast<float>(Wrapped(DirectionOf(glm::dvec2(*place), glm::dvec2(middle))));
				villageDone = true;
			}
			inVillage = false;
		}
	}
	if (!villageDone)
	{
		return std::nullopt;
	}

	// The things of the land near the god's village, the neutral towns between the god and the neighbour god, and the
	// neutral town in the middle (deviation 256)
	auto towns = ReadTowns(lines);
	const auto neutral = NameOf(PlayerNames::NEUTRAL);
	if (const auto own = std::ranges::find(towns, village.town, [](const LandTown& town) { return town.copy.number; });
	    own != towns.end())
	{
		village.things = own->copy.things;
	}
	const auto neighbourName = NameOf(k_TemplateNeighbour);
	const auto neighbour = std::ranges::find(towns, neighbourName, &LandTown::owner);
	std::optional<glm::vec2> neighbourCitadel;
	if (neighbour != towns.end() && neighbour->sites.size() > 1)
	{
		neighbourCitadel = neighbour->sites.back();
	}
	std::optional<size_t> middleTown;
	float nearestMiddle = k_MiddleTownReach;
	for (size_t i = 0; i < towns.size(); ++i)
	{
		const auto& town = towns.at(i);
		if (const float distance = glm::distance(town.copy.centre, middle); town.owner == neutral && distance < nearestMiddle)
		{
			nearestMiddle = distance;
			middleTown = i;
		}
	}
	if (middleTown.has_value())
	{
		village.middleTown = towns.at(*middleTown).copy;
	}
	if (neighbourCitadel.has_value())
	{
		village.godsRadius = (glm::distance(village.citadel, middle) + glm::distance(*neighbourCitadel, middle)) * 0.5f;
		const double from = DirectionOf(glm::dvec2(village.citadel), glm::dvec2(middle));
		const double gap = Signed(DirectionOf(glm::dvec2(*neighbourCitadel), glm::dvec2(middle)) - from);
		const auto between = [&middle, from, gap](glm::vec2 place) {
			const double at = Signed(DirectionOf(glm::dvec2(place), glm::dvec2(middle)) - from);
			return (at > 0.0) == (gap > 0.0) && std::abs(at) < std::abs(gap);
		};
		for (size_t i = 0; i < towns.size(); ++i)
		{
			const auto& town = towns.at(i);
			if (town.owner == neutral && between(town.copy.centre) && middleTown != i)
			{
				village.gapTowns.push_back(town.copy);
			}
		}
		for (const auto& line : lines)
		{
			const auto place = line.command == "CREATE_NEW_BIG_FOREST" ? FirstPlace(line) : std::nullopt;
			if (place.has_value() && between(*place))
			{
				village.gapBigForests.push_back(line);
			}
		}
	}
	return village;
}

std::vector<Line> gods_ring_map::VillageInSlot(const VillageTemplate& village, const Slot& slot,
                                               std::span<const PlayerNames> gods)
{
	const Placement placement {
	    .from = glm::dvec2(village.citadel),
	    .to = glm::dvec2(slot.citadel),
	    .turn = static_cast<double>(slot.outward) - static_cast<double>(village.outward),
	    .templateTown = village.town,
	    .town = slot.index,
	    .owner = slot.god.has_value() ? NameOf(*slot.god) : NameOf(PlayerNames::NEUTRAL),
	    .templateGod = village.god,
	};
	std::vector<Line> lines;
	AddPlaced(lines, village.village, village.things, placement, gods);
	AddPlaced(lines, village.things.lines, village.things, placement, gods);
	return lines;
}

size_t gods_ring_map::GapTownNumber(const VillageTemplate& village, size_t slot, size_t gapTown)
{
	return k_Slots + (slot * village.gapTowns.size()) + gapTown;
}

size_t gods_ring_map::MiddleTownNumber(const VillageTemplate& village)
{
	return k_Slots + (k_Slots * village.gapTowns.size());
}

namespace
{
/// The copy of a gap town in the gap after a slot, turned by an angle about the middle of the map
Placement GapPlacement(const VillageTemplate& village, const TownTemplate& town, double slotOutward, double turn, size_t number)
{
	const glm::dvec2 middle(village.middle);
	const glm::dvec2 centre(town.centre);
	const double radius =
	    static_cast<double>(k_RingRadius) - (static_cast<double>(village.godsRadius) - glm::distance(centre, middle));
	const double direction = slotOutward + (static_cast<double>(k_SlotStep) * 0.5) + turn;
	return Placement {
	    .from = centre,
	    .to = middle + (radius * glm::dvec2(std::cos(direction), std::sin(direction))),
	    .turn = direction - DirectionOf(centre, middle),
	    .templateTown = town.number,
	    .town = number,
	    .owner = NameOf(PlayerNames::NEUTRAL),
	    .templateGod = village.god,
	};
}

/// The directions from the middle of the map, and the distances, of the buildings of the lines
struct Spread
{
	double first {std::numeric_limits<double>::max()};
	double last {std::numeric_limits<double>::lowest()};
	double nearest {std::numeric_limits<double>::max()};
	double farthest {std::numeric_limits<double>::lowest()};
	[[nodiscard]] bool Empty() const { return first > last; }
};

Spread SpreadOf(std::span<const Line> lines, glm::dvec2 middle, std::optional<std::pair<double, double>> distances)
{
	Spread spread;
	for (const auto& line : lines)
	{
		if (std::ranges::find(k_BuildingCommands, line.command) == k_BuildingCommands.end())
		{
			continue;
		}
		const auto place = FirstPlace(line);
		if (!place.has_value())
		{
			continue;
		}
		const double distance = glm::distance(glm::dvec2(*place), middle);
		if (distances.has_value() && (distance < distances->first || distance > distances->second))
		{
			continue;
		}
		const double direction = DirectionOf(glm::dvec2(*place), middle);
		spread.first = std::min(spread.first, direction);
		spread.last = std::max(spread.last, direction);
		spread.nearest = std::min(spread.nearest, distance);
		spread.farthest = std::max(spread.farthest, distance);
	}
	return spread;
}

/// The angle that turns the copy of a gap town about the middle of the map, so that its buildings are half-way between
/// the buildings of the two villages beside it, at the same distances from the middle. The angle is the same for each
/// gap, so the slots here are the two slots each side of the direction of the x axis.
double GapTurn(const VillageTemplate& village, const TownTemplate& town)
{
	const glm::dvec2 middle(village.middle);
	const auto slotAt = [&middle](size_t index, double outward) {
		return Slot {
		    .index = index,
		    .citadel =
		        glm::vec2(middle + (static_cast<double>(k_RingRadius) * glm::dvec2(std::cos(outward), std::sin(outward)))),
		    .outward = static_cast<float>(outward),
		    .god = std::nullopt,
		};
	};
	const double half = static_cast<double>(k_SlotStep) * 0.5;
	const auto before = slotAt(0, -half);
	const auto after = slotAt(1, half);
	std::vector<Line> copy;
	AddPlaced(copy, town.town, town.things, GapPlacement(village, town, -half, 0.0, 0), {});
	const auto own = SpreadOf(copy, middle, std::nullopt);
	if (own.Empty())
	{
		return 0.0;
	}
	const std::pair distances {own.nearest, own.farthest};
	const auto left = SpreadOf(VillageInSlot(village, before, {}), middle, distances);
	const auto right = SpreadOf(VillageInSlot(village, after, {}), middle, distances);
	if (left.Empty() || right.Empty())
	{
		return 0.0;
	}
	return ((left.last + right.first) - (own.first + own.last)) * 0.5;
}
} // namespace

std::vector<Line> gods_ring_map::GapTowns(const VillageTemplate& village, const Slot& slot, std::span<const PlayerNames> gods)
{
	const glm::dvec2 middle(village.middle);
	std::vector<Line> lines;
	// The nearest and the farthest distance from the middle of the map of the places of each copy of a town
	std::vector<std::pair<double, double>> spans;
	for (size_t i = 0; i < village.gapTowns.size(); ++i)
	{
		const auto& town = village.gapTowns.at(i);
		const auto placement = GapPlacement(village, town, static_cast<double>(slot.outward), GapTurn(village, town),
		                                    GapTownNumber(village, slot.index, i));
		const auto first = lines.size();
		AddPlaced(lines, town.town, town.things, placement, gods);
		AddPlaced(lines, town.things.lines, town.things, placement, gods);
		std::pair span {std::numeric_limits<double>::max(), std::numeric_limits<double>::lowest()};
		for (const auto& line : std::span(lines).subspan(first))
		{
			for (const auto& argument : line.arguments)
			{
				if (const auto place = PlaceOf(argument); place.has_value())
				{
					const double distance = glm::distance(glm::dvec2(*place), middle);
					span = {std::min(span.first, distance), std::max(span.second, distance)};
				}
			}
		}
		if (span.first <= span.second)
		{
			spans.push_back(span);
		}
	}
	if (village.gapBigForests.empty())
	{
		return lines;
	}

	// The big forests stand in a row across the gap, in the order of the script, at equal distances. The row is half-way
	// across the widest space between the copies of the towns. With fewer than two copies, each big forest keeps its
	// distance in from the gods' circle.
	std::optional<double> rowRadius;
	std::ranges::sort(spans);
	double widest = 0.0;
	for (size_t i = 1; i < spans.size(); ++i)
	{
		if (const double space = spans.at(i).first - spans.at(i - 1).second; space > widest)
		{
			widest = space;
			rowRadius = (spans.at(i).first + spans.at(i - 1).second) * 0.5;
		}
	}
	const auto count = static_cast<double>(village.gapBigForests.size());
	for (size_t i = 0; i < village.gapBigForests.size(); ++i)
	{
		const auto& forest = village.gapBigForests.at(i);
		const glm::dvec2 from(FirstPlace(forest).value_or(village.middle));
		const double radius = rowRadius.value_or(static_cast<double>(k_RingRadius) -
		                                         (static_cast<double>(village.godsRadius) - glm::distance(from, middle)));
		const double direction =
		    static_cast<double>(slot.outward) + (static_cast<double>(k_SlotStep) * (static_cast<double>(i) + 0.5) / count);
		const Placement placement {
		    .from = from,
		    .to = middle + (radius * glm::dvec2(std::cos(direction), std::sin(direction))),
		    .turn = direction - DirectionOf(from, middle),
		    .templateTown = {},
		    .town = 0,
		    .owner = NameOf(PlayerNames::NEUTRAL),
		    .templateGod = village.god,
		};
		AddPlaced(lines, std::span(&forest, 1), {}, placement, gods);
	}
	return lines;
}

std::vector<Line> gods_ring_map::MiddleTown(const VillageTemplate& village, std::span<const PlayerNames> gods)
{
	std::vector<Line> lines;
	if (!village.middleTown.has_value())
	{
		return lines;
	}
	const auto& town = *village.middleTown;
	const Placement placement {
	    .from = glm::dvec2(town.centre),
	    .to = glm::dvec2(town.centre),
	    .turn = 0.0,
	    .templateTown = town.number,
	    .town = MiddleTownNumber(village),
	    .owner = NameOf(PlayerNames::NEUTRAL),
	    .templateGod = village.god,
	};
	AddPlaced(lines, town.town, town.things, placement, gods);
	AddPlaced(lines, town.things.lines, town.things, placement, gods);
	return lines;
}

std::string gods_ring_map::MapScript(const VillageTemplate& village, std::span<const Slot> slots)
{
	// The frame of the same slots has a slot for each mark line, so the fill always gives a script
	return FillFrame(ScriptFrame(slots), village, slots).value_or(std::string {});
}

std::string gods_ring_map::ScriptFrame(std::span<const Slot> slots)
{
	std::string frame = std::string(k_SettingsMark) + "\n";
	for (const auto& slot : slots)
	{
		frame += fmt::format("\nrem ---------------- Slot: {} ----------------\n", slot.index);
		frame += fmt::format("{}{}\n", k_VillageMark, slot.index);
	}
	for (const auto& slot : slots)
	{
		frame += fmt::format("\nrem ---------------- Gap after slot: {} ----------------\n", slot.index);
		frame += fmt::format("{}{}\n", k_GapMark, slot.index);
	}
	frame += "\nrem ---------------- Middle ----------------\n";
	frame += std::string(k_MiddleMark) + "\n";
	frame += "\n" + std::string(k_WeatherMark) + "\n";
	return frame;
}

std::optional<std::string> gods_ring_map::FillFrame(std::string_view frame, const VillageTemplate& village,
                                                    std::span<const Slot> slots)
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
	const auto addOriginals = [&script](std::span<const Line> lines) {
		for (const auto& line : lines)
		{
			script += line.original + "\n";
		}
	};
	// The frame ends with a new line, so the text after the last new line is not a line
	size_t start = 0;
	while (start < frame.size())
	{
		const auto end = std::min(frame.find('\n', start), frame.size());
		const auto line = frame.substr(start, end - start);
		start = end + 1;
		if (line == k_SettingsMark)
		{
			addOriginals(village.settings);
		}
		else if (line == k_WeatherMark)
		{
			addOriginals(village.weather);
		}
		else if (line == k_MiddleMark)
		{
			for (const auto& townLine : MiddleTown(village, gods))
			{
				script += townLine.Text() + "\n";
			}
		}
		else if (line.starts_with(k_VillageMark) || line.starts_with(k_GapMark))
		{
			const bool isVillage = line.starts_with(k_VillageMark);
			const auto number = line.substr(isVillage ? k_VillageMark.size() : k_GapMark.size());
			size_t index = 0;
			const auto [last, error] = std::from_chars(number.data(), number.data() + number.size(), index);
			const auto slot = std::ranges::find(slots, index, &Slot::index);
			if (error != std::errc() || last != number.data() + number.size() || slot == slots.end())
			{
				return std::nullopt;
			}
			for (const auto& placed : isVillage ? VillageInSlot(village, *slot, gods) : GapTowns(village, *slot, gods))
			{
				script += placed.Text() + "\n";
			}
		}
		else
		{
			script += std::string(line) + "\n";
		}
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
