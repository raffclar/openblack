/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cmath>
#include <cstddef>

#include <algorithm>
#include <array>
#include <numbers>
#include <optional>
#include <ranges>
#include <set>
#include <string>
#include <string_view>
#include <vector>

#include <glm/geometric.hpp>
#include <gtest/gtest.h>

#include "3D/GodsRingLand.h"
#include "Online/GodsRingMap.h"
#include "PlayerSlots.h"

using namespace openblack;
using namespace openblack::gods_ring_map;

namespace
{
constexpr glm::vec2 k_Middle {2560.0f, 2560.0f};

/// A small land script in the form of a network land: two gods, each with a town and a citadel, and a neutral town. The
/// first god's citadel is 600 east of the middle, so its village faces west.
constexpr std::string_view k_Script =
    "VERSION(2.300000)\r\n"
    "SET_LAND_NUMBER(0)\r\n"
    "LOAD_LANDSCAPE(\".\\Online Maps\\TEST.lnd\")\r\n"
    "START_CAMERA_POS(\"0.00,0.00\")\r\n"
    "SET_GLOBAL_LAND_BALANCE(4, 2.000000)\r\n"
    "SET_NIGHTTIME(1700.000000, 0.083000, 0.070000)\r\n"
    "\r\n"
    "rem ---------------- Town: 0 ----------------\r\n"
    "CREATE_TOWN(0, \"3100.00,2560.00\", \"PLAYER_ONE\", 0, \"CELTIC\")\r\n"
    "CREATE_NEW_TOWN_SPELL(0, \"HEAL\")\r\n"
    "SET_TOWN_BELIEF(0, \"PLAYER_ONE\", 1.000000)\r\n"
    "SET_TOWN_BELIEF_CAP(0, \"PLAYER_ONE\", 2.000000)\r\n"
    "SET_TOWN_BELIEF_CAP(0, \"PLAYER_TWO\", 3.000000)\r\n"
    "CREATE_NEW_TOWN_FIELD(0, \"3080.00,2580.00\", 2, 0.500000)\r\n"
    "CREATE_TOWN_CENTRE(0, \"3100.00,2560.00\", \"CELTIC_ABODE_TOWN_CENTRE\", 1000, 1000, 0)\r\n"
    "CREATE_ABODE(0, \"3090.00,2600.00\", \"CELTIC_ABODE_A\", 6000, 1000, 0, 0)\r\n"
    "CREATE_VILLAGER_POS(\"3090.00,2600.00\", \"3085.00,2595.00\", \"CELTIC_FARMER\", 21)\r\n"
    "CREATE_CITADEL(\"3160.00,2560.00\", 0, \"PLAYER_ONE\", 0, 1000)\r\n"
    "\r\n"
    "rem ---------------- Town: 1 ----------------\r\n"
    "CREATE_TOWN(1, \"2000.00,2560.00\", \"PLAYER_TWO\", 0, \"CELTIC\")\r\n"
    "CREATE_CITADEL(\"1960.00,2560.00\", 0, \"PLAYER_TWO\", 3142, 1000)\r\n"
    "CREATE_TOWN(2, \"2560.00,2560.00\", \"NEUTRAL\", 3, \"JAPANESE\")\r\n"
    "CREATE_NEW_TREE(0, \"2500.00,2500.00\", 4, 1000, 1000)\r\n"
    "CREATE_WEATHER_CLIMATE(0, 0, \"2560.00,2560.00\", 5120.000000, 5120.000000)\r\n"
    "q\r\n";

VillageTemplate Template()
{
	const auto village = ReadTemplate(k_Script, PlayerNames::PLAYER_ONE, k_Middle);
	EXPECT_TRUE(village.has_value());
	return village.value_or(VillageTemplate {});
}

/// The places of the lines, in order
std::vector<glm::vec2> PlacesOf(std::span<const Line> lines)
{
	std::vector<glm::vec2> places;
	for (const auto& line : lines)
	{
		for (const auto& argument : line.arguments)
		{
			if (const auto place = PlaceOf(argument); place.has_value())
			{
				places.push_back(*place);
			}
		}
	}
	return places;
}

const Line* Find(std::span<const Line> lines, std::string_view command)
{
	const auto found = std::ranges::find_if(lines, [command](const Line& line) { return line.command == command; });
	return found == lines.end() ? nullptr : &*found;
}
} // namespace

TEST(GodsRingMap, EverySlotHasItsOwnPlaceOnTheRing)
{
	const auto slots = LayOutSlots(k_Middle);
	ASSERT_EQ(slots.size(), 32u);
	for (size_t i = 0; i < slots.size(); ++i)
	{
		const auto& slot = slots.at(i);
		EXPECT_EQ(slot.index, i);
		EXPECT_NEAR(glm::distance(slot.citadel, k_Middle), k_RingRadius, 0.01f);
		const auto direction = slot.citadel - k_Middle;
		EXPECT_NEAR(std::cos(slot.outward), direction.x / k_RingRadius, 1e-5f);
		EXPECT_NEAR(std::sin(slot.outward), direction.y / k_RingRadius, 1e-5f);
	}
	// The first slot is south of the middle
	EXPECT_NEAR(slots.front().citadel.x, k_Middle.x, 0.01f);
	EXPECT_NEAR(slots.front().citadel.y, k_Middle.y - k_RingRadius, 0.01f);
}

TEST(GodsRingMap, EachSlotIsTheLastSlotTurnedByTheSameAngle)
{
	const auto slots = LayOutSlots(k_Middle);
	const float c = std::cos(k_SlotStep);
	const float s = std::sin(k_SlotStep);
	for (size_t i = 0; i < slots.size(); ++i)
	{
		const auto& slot = slots.at(i);
		const auto& next = slots.at((i + 1) % slots.size());
		const auto from = slot.citadel - k_Middle;
		const glm::vec2 turned {(from.x * c) - (from.y * s), (from.x * s) + (from.y * c)};
		EXPECT_NEAR(glm::distance(k_Middle + turned, next.citadel), 0.0f, 0.05f) << i;
		EXPECT_NEAR(glm::distance(slot.citadel, next.citadel), 2.0f * k_RingRadius * std::sin(k_SlotStep * 0.5f), 0.05f);
	}
}

TEST(GodsRingMap, EachSlotHasAGod)
{
	for (size_t i = 0; i < 7; ++i)
	{
		EXPECT_EQ(GodOfSlot(i), static_cast<PlayerNames>(i));
	}
	// The neutral player is not a god: the later slots take the numbers after it
	for (size_t i = 7; i < k_Slots; ++i)
	{
		EXPECT_EQ(GodOfSlot(i), static_cast<PlayerNames>(i + 1)) << i;
	}
	EXPECT_FALSE(GodOfSlot(k_Slots).has_value());
	const auto slots = LayOutSlots(k_Middle);
	EXPECT_EQ(std::ranges::count_if(slots, [](const Slot& slot) { return slot.god.has_value(); }), 32);
}

TEST(GodsRingMap, EveryCitadelStandsOnTheFlatPartOfTheIsland)
{
	for (const auto& slot : LayOutSlots(gods_ring_land::k_MapMiddle))
	{
		const auto cell = glm::ivec2(slot.citadel / gods_ring_land::k_CellSize);
		EXPECT_EQ(gods_ring_land::KindOf(cell.x, cell.y), flat_land::CellKind::Land) << slot.index;
		EXPECT_EQ(gods_ring_land::Altitude(cell.x, cell.y), flat_land::k_Altitude) << slot.index;
	}
}

TEST(GodsRingMap, ALineGivesItsCommandAndItsArguments)
{
	const auto line = Line::Parse("  CREATE_ABODE(0, \"3101.73,2064.61\", \"CELTIC_ABODE_B\", 3926, 1000, 0, 0)\r");
	ASSERT_TRUE(line.has_value());
	EXPECT_EQ(line->command, "CREATE_ABODE");
	ASSERT_EQ(line->arguments.size(), 7u);
	EXPECT_EQ(line->arguments.at(1).text, "3101.73,2064.61");
	EXPECT_TRUE(line->arguments.at(1).quoted);
	EXPECT_FALSE(line->arguments.at(3).quoted);
	EXPECT_EQ(line->Text(), "CREATE_ABODE(0, \"3101.73,2064.61\", \"CELTIC_ABODE_B\", 3926, 1000, 0, 0)");
	const auto place = PlaceOf(line->arguments.at(1));
	ASSERT_TRUE(place.has_value());
	EXPECT_FLOAT_EQ(place->x, 3101.73f);
	EXPECT_FLOAT_EQ(place->y, 2064.61f);
	EXPECT_FALSE(PlaceOf(line->arguments.at(2)).has_value());

	EXPECT_FALSE(Line::Parse("rem ---------------- Town: 0 ----------------").has_value());
	EXPECT_FALSE(Line::Parse("q").has_value());
	EXPECT_FALSE(Line::Parse("").has_value());
	const auto empty = Line::Parse("NO_ARGUMENTS()");
	ASSERT_TRUE(empty.has_value());
	EXPECT_TRUE(empty->arguments.empty());
}

TEST(GodsRingMap, TheTemplateIsTheGodsVillageWithTheLandsSettingsAndWeather)
{
	const auto village = Template();
	ASSERT_EQ(village.settings.size(), 4u);
	EXPECT_EQ(village.settings.front().command, "VERSION");
	// The map has its own landscape and camera
	EXPECT_EQ(Find(village.settings, "LOAD_LANDSCAPE"), nullptr);
	EXPECT_EQ(Find(village.settings, "START_CAMERA_POS"), nullptr);
	ASSERT_EQ(village.weather.size(), 1u);

	EXPECT_EQ(village.town, "0");
	ASSERT_EQ(village.village.size(), 10u);
	EXPECT_EQ(village.village.front().command, "CREATE_TOWN");
	EXPECT_EQ(village.village.back().command, "CREATE_CITADEL");
	EXPECT_FLOAT_EQ(village.citadel.x, 3160.0f);
	EXPECT_FLOAT_EQ(village.citadel.y, 2560.0f);
	EXPECT_NEAR(village.outward, 0.0f, 1e-6f);
	// The other gods' towns, the neutral towns and the trees are not part of it
	EXPECT_EQ(std::ranges::count_if(village.village, [](const Line& l) { return l.command == "CREATE_TOWN"; }), 1);
	EXPECT_EQ(Find(village.village, "CREATE_NEW_TREE"), nullptr);
}

TEST(GodsRingMap, ALandWithNoVillageForTheGodGivesNoTemplate)
{
	EXPECT_FALSE(ReadTemplate(k_Script, PlayerNames::PLAYER_FIVE, k_Middle).has_value());
	EXPECT_FALSE(
	    ReadTemplate("CREATE_TOWN(0, \"1.00,1.00\", \"PLAYER_ONE\", 0, \"CELTIC\")\n", PlayerNames::PLAYER_ONE, k_Middle)
	        .has_value());
}

TEST(GodsRingMap, TheVillageInASlotIsNumberedAndOwnedAsTheSlot)
{
	const auto village = Template();
	const auto slots = LayOutSlots(k_Middle);
	const std::array gods {PlayerNames::PLAYER_ONE, PlayerNames::PLAYER_TWO, PlayerNames::PLAYER_THREE};
	const auto lines = VillageInSlot(village, slots.at(2), gods);

	const auto* town = Find(lines, "CREATE_TOWN");
	ASSERT_NE(town, nullptr);
	EXPECT_EQ(town->arguments.at(0).text, "2");
	EXPECT_EQ(town->arguments.at(2).text, "PLAYER_THREE");
	for (const auto& line : lines)
	{
		if (line.command != "CREATE_VILLAGER_POS" && line.command != "CREATE_CITADEL")
		{
			EXPECT_EQ(line.arguments.front().text, "2") << line.Text();
		}
	}
	const auto* citadel = Find(lines, "CREATE_CITADEL");
	ASSERT_NE(citadel, nullptr);
	EXPECT_EQ(citadel->arguments.at(2).text, "PLAYER_THREE");
	const auto* belief = Find(lines, "SET_TOWN_BELIEF");
	ASSERT_NE(belief, nullptr);
	EXPECT_EQ(belief->arguments.at(1).text, "PLAYER_THREE");
}

TEST(GodsRingMap, EveryGodHasTheTemplateGodsCapInEveryTown)
{
	const auto village = Template();
	const auto slots = LayOutSlots(k_Middle);
	const std::array gods {PlayerNames::PLAYER_ONE, PlayerNames::PLAYER_TWO, PlayerNames::PLAYER_THREE};
	for (const auto& slot : std::span(slots).first(4))
	{
		std::set<std::string> capped;
		for (const auto& line : VillageInSlot(village, slot, gods))
		{
			if (line.command == "SET_TOWN_BELIEF_CAP")
			{
				EXPECT_EQ(line.arguments.at(2).text, "2.000000");
				capped.insert(line.arguments.at(1).text);
			}
		}
		EXPECT_EQ(capped, (std::set<std::string> {"PLAYER_ONE", "PLAYER_TWO", "PLAYER_THREE"})) << slot.index;
	}
}

TEST(GodsRingMap, ASlotWithNoGodHasANeutralVillageThatBelievesInNoGod)
{
	const auto village = Template();
	const auto slots = LayOutSlots(k_Middle);
	const std::array gods {PlayerNames::PLAYER_ONE};
	auto slot = slots.at(20);
	slot.god.reset();
	const auto lines = VillageInSlot(village, slot, gods);
	EXPECT_EQ(Find(lines, "CREATE_TOWN")->arguments.at(2).text, "NEUTRAL");
	EXPECT_EQ(Find(lines, "CREATE_CITADEL")->arguments.at(2).text, "NEUTRAL");
	EXPECT_EQ(Find(lines, "SET_TOWN_BELIEF"), nullptr);
}

TEST(GodsRingMap, EverySlotHasTheSameVillageTurnedToFaceTheMiddle)
{
	const auto village = Template();
	const auto original = PlacesOf(village.village);
	const auto slots = LayOutSlots(k_Middle);
	const std::array gods {PlayerNames::PLAYER_ONE};
	for (const auto& slot : slots)
	{
		const auto places = PlacesOf(VillageInSlot(village, slot, gods));
		ASSERT_EQ(places.size(), original.size());
		// The citadel is the last place; each place keeps its distance and its side from the citadel
		EXPECT_NEAR(glm::distance(places.back(), slot.citadel), 0.0f, 0.01f);
		const float turn = slot.outward - village.outward;
		for (size_t i = 0; i < places.size(); ++i)
		{
			const auto from = original.at(i) - village.citadel;
			const glm::vec2 expected {(from.x * std::cos(turn)) - (from.y * std::sin(turn)),
			                          (from.x * std::sin(turn)) + (from.y * std::cos(turn))};
			EXPECT_NEAR(glm::distance(places.at(i) - slot.citadel, expected), 0.0f, 0.01f) << slot.index << " " << i;
		}
		// The town centre is between the citadel and the middle of the map, as in the template
		const auto town = places.front();
		EXPECT_LT(glm::distance(town, k_Middle), glm::distance(slot.citadel, k_Middle)) << slot.index;
	}
}

TEST(GodsRingMap, TheAnglesOfTheThingsTurnWithTheirSlot)
{
	const auto village = Template();
	const auto slots = LayOutSlots(k_Middle);
	const std::array gods {PlayerNames::PLAYER_ONE};
	// The template's village faces west (its citadel is east of the middle); the first slot's faces north
	const auto lines = VillageInSlot(village, slots.front(), gods);
	const float turn = slots.front().outward - village.outward;
	const auto milli = [turn](float angle) {
		float radians = std::fmod((angle / 1000.0f) + turn, 2.0f * std::numbers::pi_v<float>);
		radians = radians < 0.0f ? radians + (2.0f * std::numbers::pi_v<float>) : radians;
		return std::to_string(std::lround(radians * 1000.0f));
	};
	EXPECT_EQ(Find(lines, "CREATE_TOWN_CENTRE")->arguments.at(3).text, milli(1000.0f));
	EXPECT_EQ(Find(lines, "CREATE_ABODE")->arguments.at(3).text, milli(6000.0f));
	EXPECT_EQ(Find(lines, "CREATE_CITADEL")->arguments.at(3).text, milli(0.0f));
	const auto field = std::stof(Find(lines, "CREATE_NEW_TOWN_FIELD")->arguments.at(3).text);
	EXPECT_NEAR(std::cos(field), std::cos(0.5f + turn), 1e-5f);
	EXPECT_NEAR(std::sin(field), std::sin(0.5f + turn), 1e-5f);
	// The other numbers do not change
	EXPECT_EQ(Find(lines, "CREATE_ABODE")->arguments.at(4).text, "1000");
	EXPECT_EQ(Find(lines, "CREATE_VILLAGER_POS")->arguments.at(3).text, "21");
}

TEST(GodsRingMap, TheVillagesOfTwoSlotsDoNotTouch)
{
	const auto village = Template();
	const auto slots = LayOutSlots(k_Middle);
	const std::array gods {PlayerNames::PLAYER_ONE};
	for (size_t i = 0; i < slots.size(); ++i)
	{
		const auto here = PlacesOf(VillageInSlot(village, slots.at(i), gods));
		const auto next = PlacesOf(VillageInSlot(village, slots.at((i + 1) % slots.size()), gods));
		for (const auto& a : here)
		{
			for (const auto& b : next)
			{
				EXPECT_GT(glm::distance(a, b), 200.0f) << i;
			}
		}
	}
}

TEST(GodsRingMap, TheMapScriptHasTheSettingsAVillageInEachSlotAndTheWeather)
{
	const auto village = Template();
	const auto slots = LayOutSlots(k_Middle);
	const auto script = MapScript(village, slots);
	std::vector<Line> lines;
	for (const auto text : script | std::views::split('\n'))
	{
		if (auto line = Line::Parse(std::string_view(text.begin(), text.end())); line.has_value())
		{
			lines.push_back(std::move(*line));
		}
	}
	ASSERT_FALSE(lines.empty());
	EXPECT_EQ(lines.front().command, "VERSION");
	EXPECT_EQ(lines.back().command, "CREATE_WEATHER_CLIMATE");
	EXPECT_EQ(Find(lines, "LOAD_LANDSCAPE"), nullptr);
	EXPECT_EQ(std::ranges::count_if(lines, [](const Line& l) { return l.command == "CREATE_CITADEL"; }), 32);
	std::set<std::string> towns;
	for (const auto& line : lines)
	{
		if (line.command == "CREATE_TOWN")
		{
			towns.insert(line.arguments.front().text);
		}
	}
	EXPECT_EQ(towns.size(), 32u);
	// Each town has a cap for each of the 32 gods
	EXPECT_EQ(std::ranges::count_if(lines, [](const Line& l) { return l.command == "SET_TOWN_BELIEF_CAP"; }), 32 * 32);
	// Each god has one citadel: the gods after the seventh are PLAYER_8 to PLAYER_32
	for (size_t slot = 0; slot < k_Slots; ++slot)
	{
		const auto name = player_slots::ScriptNameOf(*player_slots::PlayerOfSlot(slot));
		EXPECT_EQ(std::ranges::count_if(
		              lines, [name](const Line& l) { return l.command == "CREATE_CITADEL" && l.arguments.at(2).text == name; }),
		          1)
		    << name;
	}
}

TEST(GodsRingMap, TheListOfLandsGetsALineForTheMap)
{
	const std::string list = "1:Bombardment - 2 players:mpm_2p_1:2:\r\n"
	                         "2:King of the hill - 3 players:mpm_3p_1:3:\r\n"
	                         "3:The four corners of Eden - 4 players:mpm_4p_1:4:\r\n";
	const auto added = AddToLocalMaps(list);
	EXPECT_EQ(added, list + "4:The ring of gods - 32 players:obm_32p_1:32:\r\n");
	// The map is in the list once
	EXPECT_EQ(AddToLocalMaps(added), added);
	EXPECT_EQ(AddToLocalMaps(""), "1:The ring of gods - 32 players:obm_32p_1:32:\n");
	EXPECT_EQ(AddToLocalMaps("7:A:b:2:"), "7:A:b:2:\n8:The ring of gods - 32 players:obm_32p_1:32:\n");
	EXPECT_TRUE(IsThisMap("OBM_32P_1"));
	EXPECT_FALSE(IsThisMap("mpm_4p_1"));
}
