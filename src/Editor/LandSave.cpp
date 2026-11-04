/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "LandSave.h"

#include <charconv>

#include <algorithm>
#include <array>
#include <system_error>
#include <type_traits>
#include <utility>
#include <vector>

#include <fmt/format.h>

#include "Common/X87.h"
#include "LHScriptX/FeatureScriptWriter.h"
#include "LHScriptX/Script.h"

namespace openblack::editor::land_save
{

namespace writer = lhscriptx::writer;

size_t CopyCells(LandData& land, std::span<const lnd::LNDBlock> blocks)
{
	size_t copied = 0;
	for (const auto& block : blocks)
	{
		if (block.blockX >= LandData::k_BlocksPerSide || block.blockZ >= LandData::k_BlocksPerSide)
		{
			continue;
		}
		const auto index = land.blockIndexLookup.at(block.blockX * LandData::k_BlocksPerSide + block.blockZ);
		if (index == 0 || index > land.blocks.size())
		{
			continue;
		}
		land.blocks.at(index - 1u).cells = block.cells;
		++copied;
	}
	return copied;
}

std::string TreeText(const TreeLine& tree)
{
	const std::array<writer::Argument, 7> arguments {
	    tree.forest,   writer::Place {writer::PlaceText(tree.place)},
	    tree.type,     static_cast<int32_t>(tree.nonScenic ? 1 : 0),
	    tree.yAngle,   tree.scale,
	    tree.maxScale,
	};
	return writer::CommandLine("CREATE_NEW_TREE", "NANNFFF", arguments);
}

std::string FeatureText(const FeatureLine& feature)
{
	constexpr double k_Thousandths = 1000.0;
	const std::array<writer::Argument, 5> arguments {
	    writer::Place {writer::PlaceText(feature.place)},
	    writer::Name {feature.name},
	    static_cast<int32_t>(x87::Mul(feature.yAngle, k_Thousandths)),
	    static_cast<int32_t>(x87::Mul(feature.scale, k_Thousandths)),
	    int32_t {0},
	};
	return writer::CommandLine("CREATE_NEW_FEATURE", "ALNNN", arguments);
}

std::string MobileStaticText(const MobileStaticLine& thing)
{
	const std::array<writer::Argument, 7> arguments {
	    writer::Place {writer::PlaceText(thing.place)},
	    thing.row,
	    thing.altitude,
	    thing.angles.x,
	    thing.angles.y,
	    thing.angles.z,
	    thing.scale,
	};
	return writer::CommandLine("CREATE_MOBILE_STATIC", "ANFFFFF", arguments);
}

std::string MobileObjectText(const MobileObjectLine& thing)
{
	// The game holds 1000 as a float, and each product rounds to a float before it is cut
	constexpr double k_Thousandths = 1000.0;
	const std::array<writer::Argument, 4> arguments {
	    writer::Place {writer::PlaceText(thing.place)},
	    thing.row,
	    static_cast<int32_t>(x87::Mul(thing.yAngle, k_Thousandths)),
	    static_cast<int32_t>(x87::Mul(thing.scale, k_Thousandths)),
	};
	return writer::CommandLine("CREATE_MOBILEOBJECT", "ANNN", arguments);
}

std::string VillagerText(const VillagerLine& villager)
{
	const writer::Place place {writer::PlaceText(villager.place)};
	if (villager.special)
	{
		const std::array<writer::Argument, 4> arguments {villager.town, place, villager.row, villager.age};
		return writer::CommandLine("CREATE_SPECIAL_TOWN_VILLAGER", "NANN", arguments);
	}
	if (villager.door.has_value())
	{
		const std::array<writer::Argument, 4> arguments {
		    writer::Place {writer::PlaceText(*villager.door)},
		    place,
		    writer::Name {villager.name},
		    villager.age,
		};
		return writer::CommandLine("CREATE_VILLAGER_POS", "AALN", arguments);
	}
	// The command takes a place as its third argument, thus the name has no quotes
	const std::array<writer::Argument, 4> arguments {villager.town, place, writer::Place {villager.name}, villager.age};
	return writer::CommandLine("CREATE_TOWN_VILLAGER", "NAAN", arguments);
}

std::string DispenserText(const DispenserLine& dispenser)
{
	const std::array<writer::Argument, 7> arguments {
	    dispenser.town,
	    writer::Place {writer::PlaceText(dispenser.place)},
	    writer::Name {dispenser.building},
	    writer::Name {dispenser.miracle},
	    dispenser.yAngle,
	    dispenser.scale,
	    static_cast<double>(dispenser.period),
	};
	return writer::CommandLine("CREATE_SPELL_DISPENSER", "NALLFFF", arguments);
}

std::string OneShotMiracleText(glm::ivec2 place, std::string_view miracle)
{
	const std::array<writer::Argument, 2> arguments {
	    writer::Place {writer::PlaceText(place)},
	    writer::Name {std::string(miracle)},
	};
	return writer::CommandLine("CREATE_ONE_SHOT_SPELL_PU", "AL", arguments);
}

std::string CreatureText(glm::ivec2 place, int32_t row, int32_t player)
{
	const std::array<writer::Argument, 3> arguments {writer::Place {writer::PlaceText(place)}, row, player};
	return writer::CommandLine("CREATE_CREATURE", "ANN", arguments);
}

namespace
{

/// A turn, a size or a share as the game writes it in a line: times 1000, rounded to a float and cut to a whole number
[[nodiscard]] int32_t Thousandths(float value)
{
	constexpr double k_Thousandths = 1000.0;
	return static_cast<int32_t>(x87::Mul(value, k_Thousandths));
}

} // namespace

std::string FieldText(const FieldLine& field)
{
	const writer::Place place {writer::PlaceText(field.place)};
	if (field.town == -1)
	{
		return writer::CommandLine("CREATE_FIELD", "AN", std::array<writer::Argument, 2> {place, field.row});
	}
	return writer::CommandLine("CREATE_NEW_TOWN_FIELD", "NANF",
	                           std::array<writer::Argument, 4> {field.town, place, field.row, field.yAngle});
}

std::string AbodeText(const AbodeLine& abode)
{
	// The game writes the food and the wood with the form of a signed number
	const std::array<writer::Argument, 7> arguments {
	    abode.town,
	    writer::Place {writer::PlaceText(abode.place)},
	    writer::Name {abode.name},
	    Thousandths(abode.yAngle),
	    Thousandths(abode.scale),
	    static_cast<int32_t>(abode.food),
	    static_cast<int32_t>(abode.wood),
	};
	return writer::CommandLine(abode.planned ? "CREATE_PLANNED_ABODE" : "CREATE_ABODE", "NALNNNN", arguments);
}

std::string TownCentreText(const TownCentreLine& centre)
{
	const std::array<writer::Argument, 6> arguments {
	    centre.town,
	    writer::Place {writer::PlaceText(centre.place)},
	    writer::Name {centre.name},
	    Thousandths(centre.yAngle),
	    Thousandths(centre.scale),
	    Thousandths(centre.worshipShare),
	};
	return writer::CommandLine("CREATE_TOWN_CENTRE", "NALNNN", arguments);
}

std::string GameText(const GameLine& game)
{
	std::string text = writer::CommandLine("SET_LAND_NUMBER", "N", std::array<writer::Argument, 1> {game.landNumber});
	if (!game.landscape.empty())
	{
		text += writer::CommandLine("LOAD_LANDSCAPE", "L", std::array<writer::Argument, 1> {writer::Name {game.landscape}});
	}
	text += writer::CommandLine("START_CAMERA_POS", "A",
	                            std::array<writer::Argument, 1> {writer::Place {writer::PlaceText(game.startCamera)}});
	for (size_t index = 0; index < game.landBalance.size(); ++index)
	{
		const float balance = game.landBalance.at(index);
		if (balance != 1.0f)
		{
			text += writer::CommandLine("SET_GLOBAL_LAND_BALANCE", "NF",
			                            std::array<writer::Argument, 2> {static_cast<int32_t>(index), balance});
		}
	}
	if (!game.flyByFile.empty())
	{
		text += writer::CommandLine("FLY_BY_FILE", "L", std::array<writer::Argument, 1> {writer::Name {game.flyByFile}});
	}
	text += writer::CommandLine("SET_TOWN_INFLUENCE_MULTIPLIER", "F",
	                            std::array<writer::Argument, 1> {game.townInfluenceMultiplier});
	text += writer::CommandLine("SET_PLAYER_INFLUENCE_MULTIPLIER", "F",
	                            std::array<writer::Argument, 1> {game.playerInfluenceMultiplier});
	text += writer::CommandLine(
	    "SET_NIGHTTIME", "FFF",
	    std::array<writer::Argument, 3> {game.dayCycle.duration, game.dayCycle.night, game.dayCycle.change});
	return text;
}

std::string TownText(const TownLine& town)
{
	constexpr float k_DefaultCap = 10.0f;
	std::string text = writer::CommandLine("CREATE_TOWN", "NALNL",
	                                       std::array<writer::Argument, 5> {
	                                           town.id,
	                                           writer::Place {writer::PlaceText(town.place)},
	                                           writer::Name {town.player},
	                                           town.tribe,
	                                           writer::Name {town.tribeName},
	                                       });
	for (const auto& miracle : town.miracles)
	{
		text += writer::CommandLine("CREATE_NEW_TOWN_SPELL", "NL",
		                            std::array<writer::Argument, 2> {town.id, writer::Name {miracle}});
	}
	for (const auto& belief : town.beliefs)
	{
		if (belief.belief != 0.0f)
		{
			text += writer::CommandLine("SET_TOWN_BELIEF", "NLF",
			                            std::array<writer::Argument, 3> {town.id, writer::Name {belief.player}, belief.belief});
		}
		if (belief.cap != k_DefaultCap)
		{
			text += writer::CommandLine("SET_TOWN_BELIEF_CAP", "NLF",
			                            std::array<writer::Argument, 3> {town.id, writer::Name {belief.player}, belief.cap});
		}
	}
	for (const auto& desire : town.desires)
	{
		if (desire.boost != 0.0f)
		{
			text += writer::CommandLine("TOWN_DESIRE_BOOST", "NLF",
			                            std::array<writer::Argument, 3> {town.id, writer::Name {desire.desire}, desire.boost});
		}
	}
	if (town.uninhabitable)
	{
		text += writer::CommandLine("SET_TOWN_UNINHABITABLE", "N", std::array<writer::Argument, 1> {town.id});
	}
	if (town.balanceBeliefScale != 1.0f)
	{
		text += writer::CommandLine("SET_TOWN_BALANCE_BELIEF_SCALE", "NF",
		                            std::array<writer::Argument, 2> {town.id, town.balanceBeliefScale});
	}
	if (town.congregation.has_value() && *town.congregation != glm::ivec2(0, 0))
	{
		text += writer::CommandLine(
		    "SET_TOWN_CONGREGATION_POS", "NA",
		    std::array<writer::Argument, 2> {town.id, writer::Place {writer::PlaceText(*town.congregation)}});
	}
	return text;
}

std::string PlayerHeading(std::string_view player)
{
	return fmt::format("\nrem *********************** {} ***********************\n", player);
}

std::string TownHeading(int32_t id)
{
	return fmt::format("rem ---------------- Town: {} ----------------\n", id);
}

std::string StreamText(int32_t id, std::span<const glm::ivec2> places)
{
	std::string text = writer::CommandLine("CREATE_STREAM", "N", std::array<writer::Argument, 1> {id});
	for (const auto place : places)
	{
		text += writer::CommandLine("CREATE_STREAM_POINT", "NA",
		                            std::array<writer::Argument, 2> {id, writer::Place {writer::PlaceText(place)}});
	}
	return text;
}

std::string FootpathText(std::span<const glm::ivec2> places)
{
	// The command makes the new footpath the script's footpath 0, whatever number the line gives
	std::string text = writer::CommandLine("CREATE_FOOTPATH", "N", std::array<writer::Argument, 1> {int32_t {0}});
	for (const auto place : places)
	{
		text += writer::CommandLine("CREATE_FOOTPATH_NODE", "NA",
		                            std::array<writer::Argument, 2> {int32_t {0}, writer::Place {writer::PlaceText(place)}});
	}
	return text;
}

SavedPaths ReadSavedPaths(const std::string& script)
{
	SavedPaths paths;
	std::vector<std::pair<int32_t, size_t>> streamNumbers;
	for (const auto& command : lhscriptx::Script::Read(script).commands)
	{
		const auto& arguments = command.arguments;
		const bool numbered = !arguments.empty() && arguments.front().GetType() == lhscriptx::Token::Type::Integer;
		if (command.name == "CREATE_STREAM" && arguments.size() == 1 && numbered)
		{
			streamNumbers.emplace_back(*arguments.front().IntegerValue(), paths.streams.size());
			paths.streams.emplace_back();
		}
		else if (command.name == "CREATE_STREAM_POINT" && arguments.size() == 2 && numbered && arguments.back().IsString())
		{
			// The point goes to the newest stream of its number
			const auto number = *arguments.front().IntegerValue();
			const auto found =
			    std::ranges::find(streamNumbers.rbegin(), streamNumbers.rend(), number, &std::pair<int32_t, size_t>::first);
			if (found != streamNumbers.rend())
			{
				paths.streams.at(found->second).push_back(arguments.back().StringValue());
			}
		}
		else if (command.name == "CREATE_FOOTPATH")
		{
			paths.footpaths.emplace_back();
		}
		else if (command.name == "CREATE_FOOTPATH_NODE" && arguments.size() == 2 && arguments.back().IsString() &&
		         !paths.footpaths.empty())
		{
			// A node goes to the newest footpath, which is the script's footpath 0
			paths.footpaths.back().push_back(arguments.back().StringValue());
		}
	}
	return paths;
}

std::string MadeText(std::span<const entt::entity> made)
{
	std::string text;
	for (const auto entity : made)
	{
		text += fmt::format("{}\n", entt::to_integral(entity));
	}
	return text;
}

std::vector<entt::entity> ReadMade(std::string_view text)
{
	std::vector<entt::entity> made;
	while (!text.empty())
	{
		const auto end = text.find('\n');
		auto line = text.substr(0, end);
		text = end == std::string_view::npos ? std::string_view {} : text.substr(end + 1);
		if (!line.empty() && line.back() == '\r')
		{
			line.remove_suffix(1);
		}
		std::underlying_type_t<entt::entity> number {};
		const auto [last, error] = std::from_chars(line.data(), line.data() + line.size(), number);
		if (error == std::errc {} && last == line.data() + line.size() && !line.empty())
		{
			made.push_back(static_cast<entt::entity>(number));
		}
	}
	return made;
}

std::string FileName(std::string_view landName, std::string_view extension)
{
	std::string name {landName.empty() ? std::string_view("land") : landName};
	name += '.';
	name += extension;
	return name;
}

} // namespace openblack::editor::land_save
