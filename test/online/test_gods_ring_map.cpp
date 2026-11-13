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
#include <iterator>
#include <numbers>
#include <optional>
#include <ranges>
#include <set>
#include <string>
#include <string_view>
#include <vector>

#include <fmt/format.h>
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
	// The neutral town of the template is in the middle of its land, so the map has it in its middle too
	EXPECT_EQ(towns.size(), 33u);
	EXPECT_TRUE(towns.contains(fmt::format("{}", MiddleTownNumber(village))));
	// Each village has a cap for each of the 32 gods; the town in the middle has no cap in the template
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

namespace
{
/// A land script with the things between the gods: two gods, a town of each kind in the gap between their citadels, a
/// town in the middle and a town outside the gap. The first god's citadel is east of the middle and the second god's is
/// toward the high z, 600 from the middle each.
constexpr std::string_view k_ContentScript =
    "SET_LAND_NUMBER(0)\n"
    "CREATE_TOWN(0, \"3100.00,2560.00\", \"PLAYER_ONE\", 0, \"CELTIC\")\n"
    "SET_TOWN_BELIEF_CAP(0, \"PLAYER_ONE\", 2.000000)\n"
    "CREATE_ABODE(0, \"3080.00,2600.00\", \"CELTIC_ABODE_STORAGE_PIT\", 6000, 1000, 500, 500)\n"
    "CREATE_ABODE(0, \"2960.00,2620.00\", \"CELTIC_ABODE_A\", 6000, 1000, 0, 0)\n"
    "CREATE_CITADEL(\"3160.00,2560.00\", 0, \"PLAYER_ONE\", 0, 1000)\n"
    "CREATE_TOWN(1, \"2560.00,3100.00\", \"PLAYER_TWO\", 0, \"CELTIC\")\n"
    "CREATE_CITADEL(\"2560.00,3160.00\", 0, \"PLAYER_TWO\", 0, 1000)\n"
    "CREATE_TOWN(2, \"2842.84,2842.84\", \"NEUTRAL\", 3, \"JAPANESE\")\n"
    "SET_TOWN_BELIEF_CAP(2, \"PLAYER_ONE\", 1.000000)\n"
    "SET_TOWN_BELIEF_CAP(2, \"PLAYER_TWO\", 1.500000)\n"
    "CREATE_NEW_TOWN_FIELD(2, \"2880.00,2880.00\", 2, 0.000000)\n"
    "CREATE_TOWN_CENTRE(2, \"2842.84,2842.84\", \"JAPANESE_ABODE_TOWN_CENTRE\", 0, 1000, 0)\n"
    "CREATE_ABODE(2, \"2850.00,2830.00\", \"JAPANESE_ABODE_STORAGE_PIT\", 5170, 1000, 500, 500)\n"
    "CREATE_ABODE(2, \"2810.00,2810.00\", \"JAPANESE_ABODE_A\", 100, 1000, 0, 0)\n"
    "CREATE_VILLAGER_POS(\"2810.00,2810.00\", \"2812.00,2812.00\", \"JAPANESE_FARMER\", 21)\n"
    "CREATE_TOWN(3, \"2701.42,2701.42\", \"NEUTRAL\", 4, \"INDIAN\")\n"
    "CREATE_TOWN_CENTRE(3, \"2701.42,2701.42\", \"INDIAN_ABODE_TOWN_CENTRE\", 0, 1000, 0)\n"
    "CREATE_TOWN(4, \"2570.00,2560.00\", \"NEUTRAL\", 8, \"TIBETAN\")\n"
    "CREATE_TOWN(5, \"2000.00,2000.00\", \"NEUTRAL\", 3, \"JAPANESE\")\n"
    "CREATE_FLOCK(0, \"3050.00,2560.00\", \"3050.00,2560.00\", 22, 10, 0)\n"
    "CREATE_NEW_ANIMAL(\"3051.00,2561.00\", 4, 0, 0, 14)\n"
    "CREATE_FLOCK(1, \"3150.00,2500.00\", \"3150.00,2500.00\", 165, 38, -1)\n"
    "CREATE_NEW_ANIMAL(\"3151.00,2501.00\", 4, 1, -1, 10)\n"
    "CREATE_FLOCK(2, \"2860.00,2860.00\", \"2860.00,2860.00\", 22, 10, 2)\n"
    "CREATE_NEW_ANIMAL(\"2861.00,2861.00\", 4, 2, 2, 9)\n"
    "CREATE_FLOCK(3, \"2575.00,2565.00\", \"2575.00,2565.00\", 80, 30, -1)\n"
    "CREATE_NEW_ANIMAL(\"2576.00,2566.00\", 12, 3, -1, 6)\n"
    "CREATE_FLOCK(4, \"2000.00,2010.00\", \"2000.00,2010.00\", 22, 10, -1)\n"
    "CREATE_NEW_ANIMAL(\"2001.00,2011.00\", 4, 4, -1, 6)\n"
    "CREATE_FOREST(10, \"3060.00,2620.00\")\n"
    "CREATE_NEW_TREE(10, \"3061.00,2621.00\", 4, 1, 1.000000, 1.000000, 1.000000)\n"
    "CREATE_NEW_TREE(10, \"3062.00,2622.00\", 4, 1, 2.000000, 1.000000, 1.000000)\n"
    "CREATE_FOREST(11, \"2720.00,2700.00\")\n"
    "CREATE_NEW_TREE(11, \"2721.00,2701.00\", 4, 1, 0.500000, 1.000000, 1.000000)\n"
    "CREATE_FOREST(12, \"2560.00,2000.00\")\n"
    "CREATE_NEW_TREE(12, \"2561.00,2001.00\", 4, 1, 0.500000, 1.000000, 1.000000)\n"
    "CREATE_NEW_TREE(47, \"2830.00,2870.00\", 8, 1, 3.000000, 0.990000, 0.990000)\n"
    "CREATE_NEW_TREE(47, \"2870.00,2850.00\", 8, 1, 3.500000, 0.990000, 0.990000)\n"
    "FIRE_FLY_SPELL_REWARD_PROB(CREATURE_SPELL_ANGRY, 0.200000)\n"
    "CREATE_NEW_BIG_FOREST(\"3000.00,2600.00\", 0, 0, 0.000000, 0.888000)\n"
    "CREATE_WEATHER_CLIMATE(0, 0, \"2560.00,2560.00\", 5120.000000, 5120.000000)\n";

VillageTemplate ContentTemplate()
{
	const auto village = ReadTemplate(k_ContentScript, PlayerNames::PLAYER_ONE, k_Middle);
	EXPECT_TRUE(village.has_value());
	return village.value_or(VillageTemplate {});
}

std::vector<std::string> CommandsOf(std::span<const Line> lines)
{
	std::vector<std::string> commands;
	std::ranges::transform(lines, std::back_inserter(commands), &Line::command);
	return commands;
}

/// The lines of the town with the number, without its villagers and its things
std::vector<Line> LinesOfTown(std::span<const Line> lines, size_t town)
{
	const auto number = fmt::format("{}", town);
	std::vector<Line> found;
	std::ranges::copy_if(lines, std::back_inserter(found), [&number](const Line& line) {
		return line.command.starts_with("CREATE_TOWN") || line.command.starts_with("CREATE_NEW_TOWN") ||
		               line.command == "CREATE_ABODE"
		           ? line.arguments.front().text == number
		           : false;
	});
	return found;
}

/// The direction of a place from the middle, from a slot's direction, from minus a half turn to a half turn
double DirectionFrom(glm::vec2 place, float outward)
{
	double direction = std::atan2(place.y - k_Middle.y, place.x - k_Middle.x) - static_cast<double>(outward);
	while (direction > std::numbers::pi)
	{
		direction -= 2.0 * std::numbers::pi;
	}
	while (direction <= -std::numbers::pi)
	{
		direction += 2.0 * std::numbers::pi;
	}
	return direction;
}

const std::set<std::string> k_Buildings {"CREATE_NEW_TOWN_FIELD", "CREATE_TOWN_CENTRE", "CREATE_ABODE", "CREATE_CITADEL"};

/// The place of a building's line
std::optional<glm::vec2> BuildingPlace(const Line& line)
{
	if (!k_Buildings.contains(line.command) || line.arguments.size() < 2)
	{
		return std::nullopt;
	}
	return PlaceOf(line.command == "CREATE_CITADEL" ? line.arguments.front() : line.arguments.at(1));
}
} // namespace

TEST(GodsRingMap, TheTemplateHasTheThingsOfTheVillageAndOfTheTownsBetweenTheGods)
{
	const auto village = ContentTemplate();
	// The village has its own flock, the flock of no town nearest to its citadel, and the forest by its storage pit
	EXPECT_EQ(village.things.flocks, (std::vector<std::string> {"0", "1"}));
	EXPECT_EQ(village.things.forests, (std::vector<std::string> {"10"}));
	EXPECT_EQ(CommandsOf(village.things.lines),
	          (std::vector<std::string> {"CREATE_FLOCK", "CREATE_NEW_ANIMAL", "CREATE_FLOCK", "CREATE_NEW_ANIMAL",
	                                     "CREATE_FOREST", "CREATE_NEW_TREE", "CREATE_NEW_TREE"}));
	EXPECT_FLOAT_EQ(village.godsRadius, 600.0f);

	// The two neutral towns between the gods' citadels, in the order of the script; not the town outside the gap
	ASSERT_EQ(village.gapTowns.size(), 2u);
	const auto& japanese = village.gapTowns.at(0);
	EXPECT_EQ(japanese.number, "2");
	EXPECT_EQ(japanese.town.size(), 8u);
	EXPECT_EQ(japanese.things.flocks, (std::vector<std::string> {"2"}));
	EXPECT_TRUE(japanese.things.forests.empty());
	// The trees in no forest near its storage pit are its own
	EXPECT_EQ(CommandsOf(japanese.things.lines),
	          (std::vector<std::string> {"CREATE_FLOCK", "CREATE_NEW_ANIMAL", "CREATE_NEW_TREE", "CREATE_NEW_TREE"}));
	const auto& indian = village.gapTowns.at(1);
	EXPECT_EQ(indian.number, "3");
	EXPECT_EQ(indian.things.forests, (std::vector<std::string> {"11"}));
	EXPECT_EQ(CommandsOf(indian.things.lines), (std::vector<std::string> {"CREATE_FOREST", "CREATE_NEW_TREE"}));

	// The town in the middle has the flock of no town that is nearest to it
	ASSERT_TRUE(village.middleTown.has_value());
	EXPECT_EQ(village.middleTown->number, "4");
	EXPECT_EQ(village.middleTown->things.flocks, (std::vector<std::string> {"3"}));

	// The chances of the fireflies' spells are part of the land's settings
	EXPECT_NE(Find(village.settings, "FIRE_FLY_SPELL_REWARD_PROB"), nullptr);
	// The big forest between the gods' citadels goes in each gap
	ASSERT_EQ(village.gapBigForests.size(), 1u);
	EXPECT_EQ(village.gapBigForests.front().command, "CREATE_NEW_BIG_FOREST");
}

TEST(GodsRingMap, TheThingsOfAVillageHaveTheNumbersOfItsSlot)
{
	const auto village = ContentTemplate();
	const auto slots = LayOutSlots(k_Middle);
	const std::array gods {PlayerNames::PLAYER_ONE};
	const auto lines = VillageInSlot(village, slots.at(3), gods);
	std::vector<std::string> flocks;
	for (const auto& line : lines)
	{
		if (line.command == "CREATE_FLOCK")
		{
			flocks.push_back(line.arguments.front().text);
		}
		if (line.command == "CREATE_NEW_ANIMAL")
		{
			// An animal stays in its flock, and in the town of its flock
			const auto& flock = line.arguments.at(2).text;
			EXPECT_TRUE(flock == "1300" || flock == "1301") << line.Text();
			EXPECT_EQ(line.arguments.at(3).text, flock == "1300" ? "3" : "-1");
		}
		if (line.command == "CREATE_FOREST" || line.command == "CREATE_NEW_TREE")
		{
			EXPECT_EQ(line.arguments.front().text, "1300");
		}
	}
	EXPECT_EQ(flocks, (std::vector<std::string> {"1300", "1301"}));
	// The angle of a tree turns with the slot
	const auto* tree = Find(lines, "CREATE_NEW_TREE");
	ASSERT_NE(tree, nullptr);
	const float turn = slots.at(3).outward - village.outward;
	const float angle = std::stof(tree->arguments.at(4).text);
	EXPECT_NEAR(std::cos(angle), std::cos(1.0f + turn), 1e-5f);
	EXPECT_NEAR(std::sin(angle), std::sin(1.0f + turn), 1e-5f);
	EXPECT_EQ(Find(lines, "CREATE_NEW_BIG_FOREST"), nullptr);
}

TEST(GodsRingMap, EachGapHasACopyOfEachGapTownAtItsDistanceInFromTheGods)
{
	const auto village = ContentTemplate();
	const auto slots = LayOutSlots(k_Middle);
	const std::array gods {PlayerNames::PLAYER_ONE, PlayerNames::PLAYER_TWO, PlayerNames::PLAYER_THREE};
	for (const auto& slot : slots)
	{
		const auto lines = GapTowns(village, slot, gods);
		for (size_t i = 0; i < village.gapTowns.size(); ++i)
		{
			const auto& original = village.gapTowns.at(i);
			const auto number = GapTownNumber(village, slot.index, i);
			EXPECT_EQ(number, k_Slots + (slot.index * 2) + i);
			const auto copy = LinesOfTown(lines, number);
			const auto* town = Find(copy, "CREATE_TOWN");
			ASSERT_NE(town, nullptr) << slot.index;
			EXPECT_EQ(town->arguments.at(2).text, "NEUTRAL");
			const auto centre = PlaceOf(town->arguments.at(1)).value();
			// The town keeps its distance in from the gods' circle
			const float depth = village.godsRadius - glm::distance(original.centre, k_Middle);
			EXPECT_NEAR(glm::distance(centre, k_Middle), k_RingRadius - depth, 0.01f) << slot.index;
			// The town stands in the gap after its slot
			const double direction = DirectionFrom(centre, slot.outward);
			EXPECT_GT(direction, 0.0) << slot.index;
			EXPECT_LT(direction, static_cast<double>(k_SlotStep)) << slot.index;
			const auto* centreLine = Find(copy, "CREATE_TOWN_CENTRE");
			ASSERT_NE(centreLine, nullptr);
			EXPECT_NEAR(glm::distance(PlaceOf(centreLine->arguments.at(1)).value(), centre), 0.0f, 0.01f);
		}
	}
}

TEST(GodsRingMap, AGapTownKeepsTheShapeOfTheTemplateTown)
{
	const auto village = ContentTemplate();
	const auto slots = LayOutSlots(k_Middle);
	const std::array gods {PlayerNames::PLAYER_ONE};
	const auto& original = village.gapTowns.front();
	const auto lines = GapTowns(village, slots.at(7), gods);
	// The copy has the lines of the town in their order, and then its things
	const auto isCap = [](const Line& line) { return line.command == "SET_TOWN_BELIEF_CAP"; };
	std::vector<Line> expected;
	std::ranges::remove_copy_if(original.town, std::back_inserter(expected), isCap);
	std::ranges::copy(original.things.lines, std::back_inserter(expected));
	std::vector<Line> placed;
	std::ranges::remove_copy_if(lines, std::back_inserter(placed), isCap);
	ASSERT_GE(placed.size(), expected.size());
	placed.resize(expected.size());
	EXPECT_EQ(CommandsOf(placed), CommandsOf(expected));
	const auto from = PlacesOf(expected);
	const auto to = PlacesOf(placed);
	ASSERT_EQ(from.size(), to.size());
	for (size_t i = 1; i < from.size(); ++i)
	{
		EXPECT_NEAR(glm::distance(to.at(i), to.front()), glm::distance(from.at(i), from.front()), 0.02f) << i;
		EXPECT_NEAR(glm::distance(to.at(i), to.at(i - 1)), glm::distance(from.at(i), from.at(i - 1)), 0.02f) << i;
	}
	// The only god of the map has the template god's cap in the town
	const auto caps = std::ranges::count_if(lines, [](const Line& line) {
		return line.command == "SET_TOWN_BELIEF_CAP" && line.arguments.at(1).text == "PLAYER_ONE" &&
		       line.arguments.at(2).text == "1.000000";
	});
	EXPECT_EQ(caps, 1);
	EXPECT_EQ(std::ranges::count_if(lines, isCap), 1);
}

TEST(GodsRingMap, TheThingsOfAGapTownHaveTheNumbersOfItsCopy)
{
	const auto village = ContentTemplate();
	const auto slots = LayOutSlots(k_Middle);
	const std::array gods {PlayerNames::PLAYER_ONE};
	const auto lines = GapTowns(village, slots.front(), gods);
	const auto* flock = Find(lines, "CREATE_FLOCK");
	ASSERT_NE(flock, nullptr);
	EXPECT_EQ(flock->arguments.front().text, "4200");
	EXPECT_EQ(flock->arguments.at(5).text, "32");
	const auto* animal = Find(lines, "CREATE_NEW_ANIMAL");
	ASSERT_NE(animal, nullptr);
	EXPECT_EQ(animal->arguments.at(2).text, "4200");
	EXPECT_EQ(animal->arguments.at(3).text, "32");
	const auto* forest = Find(lines, "CREATE_FOREST");
	ASSERT_NE(forest, nullptr);
	EXPECT_EQ(forest->arguments.front().text, "4300");
	std::set<std::string> treeForests;
	for (const auto& line : lines)
	{
		if (line.command == "CREATE_NEW_TREE")
		{
			treeForests.insert(line.arguments.front().text);
		}
	}
	// The trees in no forest keep the number that no forest has
	EXPECT_EQ(treeForests, (std::set<std::string> {"47", "4300"}));
}

TEST(GodsRingMap, AGapTownStandsHalfWayBetweenTheBuildingsOfTheVillagesBesideIt)
{
	const auto village = ContentTemplate();
	const auto slots = LayOutSlots(k_Middle);
	const std::array gods {PlayerNames::PLAYER_ONE};
	const auto& slot = slots.front();
	const auto& next = slots.at(1);
	const auto copy = LinesOfTown(GapTowns(village, slot, gods), GapTownNumber(village, slot.index, 0));
	double first = 10.0;
	double last = -10.0;
	float nearest = 1e9f;
	float farthest = 0.0f;
	for (const auto& line : copy)
	{
		if (const auto place = BuildingPlace(line); place.has_value())
		{
			first = std::min(first, DirectionFrom(*place, slot.outward));
			last = std::max(last, DirectionFrom(*place, slot.outward));
			nearest = std::min(nearest, glm::distance(*place, k_Middle));
			farthest = std::max(farthest, glm::distance(*place, k_Middle));
		}
	}
	// The buildings of the villages at the same distances from the middle as the town's buildings
	const auto edge = [&](const Slot& of, bool high) {
		double found = high ? -10.0 : 10.0;
		for (const auto& line : VillageInSlot(village, of, gods))
		{
			const auto place = BuildingPlace(line);
			if (!place.has_value() || glm::distance(*place, k_Middle) < nearest || glm::distance(*place, k_Middle) > farthest)
			{
				continue;
			}
			const double direction = DirectionFrom(*place, slot.outward);
			found = high ? std::max(found, direction) : std::min(found, direction);
		}
		return found;
	};
	const double left = edge(slot, true);
	const double right = edge(next, false);
	ASSERT_LT(left, 1.0);
	ASSERT_GT(right, -1.0);
	EXPECT_NEAR((first + last) * 0.5, (left + right) * 0.5, 1e-4);
	EXPECT_GT(first, left);
	EXPECT_LT(last, right);
}

TEST(GodsRingMap, TheMapHasATownInTheMiddleAndEachNumberOnce)
{
	const auto village = ContentTemplate();
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
	const auto numbers = [&lines](std::string_view command) {
		std::vector<std::string> found;
		for (const auto& line : lines)
		{
			if (line.command == command)
			{
				found.push_back(line.arguments.front().text);
			}
		}
		return found;
	};
	const auto count = [&lines](std::string_view command) {
		return std::ranges::count_if(lines, [command](const Line& line) { return line.command == command; });
	};
	const auto towns = numbers("CREATE_TOWN");
	// 32 villages, 2 towns in each of the 32 gaps and the town in the middle
	EXPECT_EQ(towns.size(), 32u + 64u + 1u);
	EXPECT_EQ(std::set(towns.begin(), towns.end()).size(), towns.size());
	EXPECT_EQ(MiddleTownNumber(village), 96u);
	EXPECT_NE(std::ranges::find(towns, "96"), towns.end());
	for (const std::string_view command : {"CREATE_FLOCK", "CREATE_FOREST"})
	{
		const auto found = numbers(command);
		EXPECT_EQ(std::set(found.begin(), found.end()).size(), found.size()) << command;
	}
	// 2 flocks for each village, 1 for each copy of the Japanese town and 1 in the middle
	EXPECT_EQ(count("CREATE_FLOCK"), (32 * 2) + 32 + 1);
	EXPECT_EQ(count("CREATE_NEW_ANIMAL"), (32 * 2) + 32 + 1);
	// A forest by each village and by each copy of the Indian town
	EXPECT_EQ(count("CREATE_FOREST"), 32 + 32);
	EXPECT_EQ(count("FIRE_FLY_SPELL_REWARD_PROB"), 1);
	EXPECT_EQ(count("CREATE_NEW_BIG_FOREST"), 32);
	// The town outside the gap and the forest far from each town are not on the map
	EXPECT_EQ(std::ranges::count_if(
	              lines, [](const Line& l) { return l.command == "CREATE_TOWN" && l.arguments.at(4).text == "JAPANESE"; }),
	          32);
	EXPECT_EQ(MapScript(village, slots), script);
}

TEST(GodsRingMap, TheBigForestsOfAGapStandBetweenItsTowns)
{
	const auto village = ContentTemplate();
	const auto slots = LayOutSlots(k_Middle);
	const std::array gods {PlayerNames::PLAYER_ONE};
	for (const auto& slot : std::span(slots).first(3))
	{
		const auto lines = GapTowns(village, slot, gods);
		const auto forests = std::ranges::count_if(lines, [](const Line& l) { return l.command == "CREATE_NEW_BIG_FOREST"; });
		ASSERT_EQ(forests, 1);
		const auto& forest = lines.back();
		ASSERT_EQ(forest.command, "CREATE_NEW_BIG_FOREST");
		const auto place = PlaceOf(forest.arguments.front()).value();
		// One big forest stands on the line half-way across the gap
		EXPECT_NEAR(DirectionFrom(place, slot.outward), static_cast<double>(k_SlotStep) * 0.5, 1e-4) << slot.index;
		// It is between the two towns: nearer the middle than each place of the Japanese town, farther than the Indian town
		const auto japanese = LinesOfTown(lines, GapTownNumber(village, slot.index, 0));
		const auto indian = LinesOfTown(lines, GapTownNumber(village, slot.index, 1));
		const float distance = glm::distance(place, k_Middle);
		for (const auto& p : PlacesOf(japanese))
		{
			EXPECT_LT(distance, glm::distance(p, k_Middle));
		}
		for (const auto& p : PlacesOf(indian))
		{
			EXPECT_GT(distance, glm::distance(p, k_Middle));
		}
		// Its angle turns with it, and its type, wood and size do not change
		const auto original = village.gapBigForests.front();
		const double turn = std::atan2(place.y - k_Middle.y, place.x - k_Middle.x) - std::atan2(40.0, 440.0);
		const float angle = std::stof(forest.arguments.at(3).text);
		EXPECT_NEAR(std::cos(angle), std::cos(turn), 1e-4);
		EXPECT_NEAR(std::sin(angle), std::sin(turn), 1e-4);
		EXPECT_EQ(forest.arguments.at(1).text, original.arguments.at(1).text);
		EXPECT_EQ(forest.arguments.at(4).text, "0.888000");
	}
}
