/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#define LOCATOR_IMPLEMENTATIONS

#include <cstddef>
#include <cstdint>

#include <algorithm>
#include <array>
#include <bit>
#include <optional>
#include <span>
#include <string>
#include <vector>

#include <glm/vec3.hpp>
#include <gtest/gtest.h>

#include "3D/MapCoords.h"
#include "Common/GUtilsDistance.h"
#include "ECS/ComputerMind.h"
#include "ECS/ComputerMindDomains.h"
#include "ECS/ComputerMindRules.h"
#include "ECS/Systems/Implementations/ComputerMindRuleTable.h"

using namespace openblack;
using namespace openblack::computer_mind;
using namespace openblack::computer_mind_rules;
namespace computer_world = openblack::ecs::systems::computer_world;

TEST(ComputerMindCore, EachPartAddsOnlyTheRulesOfItsOwnKinds)
{
	const std::array adders {computer_world::RegisterCoreRules,         computer_world::RegisterTownsRules,
	                         computer_world::RegisterWorshipRules,      computer_world::RegisterMiraclesRules,
	                         computer_world::RegisterCreatureCareRules, computer_world::RegisterCreaturePlayRules,
	                         computer_world::RegisterConquestRules,     computer_world::RegisterWarRules,
	                         computer_world::RegisterDefenceRules,      computer_world::RegisterAttitudesRules};
	static_assert(adders.size() == static_cast<size_t>(MindDomain::_Count));
	size_t rules = 0;
	for (size_t part = 0; part < adders.size(); ++part)
	{
		computer_world::RuleTable table {};
		adders.at(part)(table);
		for (size_t kind = 0; kind < table.size(); ++kind)
		{
			const auto& rule = table.at(kind);
			if (rule.intensity != nullptr || rule.perform != nullptr)
			{
				EXPECT_EQ(DomainOf(static_cast<NodeKind>(kind)), static_cast<MindDomain>(part)) << kind;
				++rules;
			}
		}
	}
	// The table of all the rules has each of them
	const auto& all = computer_world::Rules();
	const auto filled = std::ranges::count_if(
	    all, [](const computer_world::Rule& rule) { return rule.intensity != nullptr || rule.perform != nullptr; });
	EXPECT_EQ(static_cast<size_t>(filled), rules);
	EXPECT_NE(all.at(static_cast<size_t>(NodeKind::ActionChargeUpSpell)).perform, nullptr);
}

TEST(ComputerMindCore, OwnsItsKindsOfNodes)
{
	EXPECT_EQ(DomainOf(NodeKind::Root), MindDomain::Core);
	EXPECT_EQ(DomainOf(NodeKind::NeedAttendTownDesires), MindDomain::Core);
	EXPECT_EQ(DomainOf(NodeKind::ObjectDIRECTPickupAndDropObject), MindDomain::Core);
}

TEST(ComputerMindRules, NearestEdgeOfTheInfluence)
{
	const auto at = [](float x, float z) {
		return map_coords::MapCoords {.x = map_coords::ToFixed(x), .z = map_coords::ToFixed(z), .altitude = 0.0f};
	};
	EXPECT_FALSE(NearestInfluenceEdge(at(0.0f, 0.0f), {}).has_value());
	// The citadel at (1000, 1000) reaches 100 m; a town at (1500, 1000) reaches 300 m. The place is east of both.
	const std::vector<InfluenceCircle> circles {{.centre = at(1000.0f, 1000.0f), .radius = 100.0f},
	                                            {.centre = at(1500.0f, 1000.0f), .radius = 300.0f}};
	const auto place = at(1900.0f, 1000.0f);
	const auto edge = NearestInfluenceEdge(place, circles);
	ASSERT_TRUE(edge.has_value());
	// The town's edge is nearer: 100 m from the place, east of the town
	EXPECT_NEAR(map_coords::ToMetres(edge->x), 1800.0f, 0.5f);
	EXPECT_NEAR(map_coords::ToMetres(edge->z), 1000.0f, 0.5f);
	// Of two circles at the same distance the first wins
	const std::vector<InfluenceCircle> same {{.centre = at(1000.0f, 1000.0f), .radius = 100.0f},
	                                         {.centre = at(1000.0f, 1000.0f), .radius = 100.0f}};
	EXPECT_TRUE(NearestInfluenceEdge(place, same).has_value());
}

TEST(ComputerMindRules, PlaceOutsideTheInfluencePullsLessFurtherOut)
{
	// The pull is the square of the distance modifier, nearly full at the edge
	const float edge = gutils::GetDistanceModifier(0.0f, 200.0f);
	EXPECT_FLOAT_EQ(OutsidePlacePull(0.0f, 200.0f), edge * edge);
	EXPECT_GT(OutsidePlacePull(0.0f, 200.0f), 0.99f);
	const float modifier = gutils::GetDistanceModifier(50.0f, 200.0f);
	EXPECT_FLOAT_EQ(OutsidePlacePull(50.0f, 200.0f), modifier * modifier);
	// Past the reach it is nothing
	EXPECT_FLOAT_EQ(OutsidePlacePull(400.0f, 200.0f), 0.0f);
}

// The personality file has each node in the order of the tree: the name with the number of its text, a zero byte and
// the weight with the low byte first
TEST(ComputerMindCore, ThePersonalityFileHasTheNameAndTheWeightOfEachNode)
{
	auto mind = MakeMind();
	ASSERT_TRUE(SetPersonality(mind, "PlayerInteract", 0.5f));
	const auto bytes = PersonalityFile(mind);

	EXPECT_EQ(PersonalityName(NodeKind::Root), "Master Decision Tree");
	EXPECT_EQ(PersonalityName(NodeKind::TopPlayerInteract), "PlayerInteract: 13 ");
	EXPECT_EQ(PersonalityName(NodeKind::NeedAttendTownDesires), "AttendTownDesires: 32 ");
	const std::string root = "Master Decision Tree";
	const std::string first = "PlayerInteract: 13 ";
	ASSERT_GE(bytes.size(), root.size() + first.size() + 10);
	EXPECT_EQ(std::string(bytes.begin(), bytes.begin() + static_cast<std::ptrdiff_t>(root.size())), root);
	EXPECT_EQ(bytes.at(root.size()), 0);
	// The root weight 1 has the bits 0x3F800000
	EXPECT_EQ(bytes.at(root.size() + 1), 0x00);
	EXPECT_EQ(bytes.at(root.size() + 2), 0x00);
	EXPECT_EQ(bytes.at(root.size() + 3), 0x80);
	EXPECT_EQ(bytes.at(root.size() + 4), 0x3F);
	const auto at = root.size() + 5;
	EXPECT_EQ(std::string(bytes.begin() + static_cast<std::ptrdiff_t>(at),
	                      bytes.begin() + static_cast<std::ptrdiff_t>(at + first.size())),
	          first);
	EXPECT_EQ(bytes.at(at + first.size()), 0);
	// The first child of the root has the weight 0.5, with the bits 0x3F000000
	EXPECT_EQ(bytes.at(at + first.size() + 3), 0x00);
	EXPECT_EQ(bytes.at(at + first.size() + 4), 0x3F);

	size_t size = 0;
	for (const auto& node : mind.nodes)
	{
		size += PersonalityName(node.kind).size() + 1 + 4;
	}
	EXPECT_EQ(bytes.size(), size);
}

// A load gives each node the weight of the file, and a save of the loaded mind gives the same bytes
TEST(ComputerMindCore, APersonalityFileLoadsBackTheWeights)
{
	auto saved = MakeMind();
	for (size_t i = 0; i < saved.nodes.size(); ++i)
	{
		saved.nodes.at(i).weight = static_cast<float>(i) * 0.125f - 3.0f;
	}
	const auto bytes = PersonalityFile(saved);

	auto loaded = MakeMind();
	loaded.nodes.at(5).suppression = 0.5f;
	auto names = MakeNodeNames();
	ReadPersonalityFile(loaded, names, bytes);
	for (size_t i = 0; i < saved.nodes.size(); ++i)
	{
		EXPECT_EQ(loaded.nodes.at(i).weight, saved.nodes.at(i).weight) << i;
	}
	// A load changes only the weights. The names in a file that the game saved are the names of the nodes.
	EXPECT_EQ(loaded.nodes.at(5).suppression, 0.5f);
	EXPECT_EQ(names, FirstNodeNames());
	EXPECT_EQ(PersonalityFile(loaded, names), bytes);
}

// A load does not compare the names: it reads the weights by the place of each node in the file
TEST(ComputerMindCore, APersonalityLoadDoesNotCompareTheNames)
{
	auto saved = MakeMind();
	saved.nodes.at(1).weight = 0.75f;
	auto bytes = PersonalityFile(saved);
	std::fill_n(bytes.begin(), 6, static_cast<uint8_t>('x'));

	auto loaded = MakeMind();
	auto names = MakeNodeNames();
	ReadPersonalityFile(loaded, names, bytes);
	EXPECT_EQ(loaded.nodes.at(1).weight, 0.75f);
}

// A short file stops the load. A weight that the file cuts keeps its high bytes, and the nodes after it keep their
// weights.
TEST(ComputerMindCore, AShortPersonalityFileStopsTheLoad)
{
	auto saved = MakeMind();
	saved.nodes.at(0).weight = 0.25f;
	saved.nodes.at(1).weight = std::bit_cast<float>(0x40AABBCCu);
	saved.nodes.at(2).weight = 4.0f;
	const auto full = PersonalityFile(saved);
	const auto cut = PersonalityName(NodeKind::Root).size() + 5 + PersonalityName(NodeKind::TopPlayerInteract).size() + 1 + 2;
	const std::span<const uint8_t> bytes(full.data(), cut);

	auto loaded = MakeMind();
	const float third = loaded.nodes.at(2).weight;
	auto names = MakeNodeNames();
	ReadPersonalityFile(loaded, names, bytes);
	EXPECT_EQ(loaded.nodes.at(0).weight, 0.25f);
	// The weight before the load is 1, with the bits 0x3F800000. The file gives its two low bytes 0xCC and 0xBB.
	EXPECT_EQ(std::bit_cast<uint32_t>(loaded.nodes.at(1).weight), 0x3F80BBCCu);
	EXPECT_EQ(loaded.nodes.at(2).weight, third);

	auto empty = MakeMind();
	ReadPersonalityFile(empty, names, {});
	EXPECT_EQ(PersonalityFile(empty, names), PersonalityFile(MakeMind()));
}

namespace
{

/// The four bytes of the file at the place, low byte first
uint32_t BitsAt(const std::vector<uint8_t>& bytes, size_t at)
{
	return static_cast<uint32_t>(bytes.at(at)) | (static_cast<uint32_t>(bytes.at(at + 1)) << 8) |
	       (static_cast<uint32_t>(bytes.at(at + 2)) << 16) | (static_cast<uint32_t>(bytes.at(at + 3)) << 24);
}

} // namespace

// The names in a file become the names of the nodes for all the gods. A land script then finds the nodes by the new
// names, and a save of any god writes the new names.
TEST(ComputerMindCore, APersonalityLoadRenamesTheNodesOfAllTheGods)
{
	auto bytes = PersonalityFile(MakeMind());
	// The second node is the first child of the root: "PlayerInteract: 13 "
	const auto at = PersonalityName(NodeKind::Root).size() + 5;
	bytes.at(at + 6) = 'X';

	auto loaded = MakeMind();
	auto names = MakeNodeNames();
	ReadPersonalityFile(loaded, names, bytes);
	EXPECT_EQ(NameOf(names, NodeKind::TopPlayerInteract), "PlayerXnteract: 13 ");
	EXPECT_EQ(NameOf(FirstNodeNames(), NodeKind::TopPlayerInteract), "PlayerInteract: 13 ");

	const auto other = MakeMind();
	EXPECT_TRUE(FindNodes(other, "PlayerInteract", names).empty());
	EXPECT_EQ(FindNodes(other, "PlayerXnteract", names), FindNodes(other, "PlayerInteract"));
	auto weights = MakeMind();
	EXPECT_TRUE(SetPersonality(weights, "PlayerXnteract", 0.5f, names));
	EXPECT_EQ(weights.nodes.at(1).weight, 0.5f);
	EXPECT_EQ(PersonalityFile(other, names), bytes);
}

// A zero byte early in a name of the file makes the name shorter. The next save and the next load use the shorter
// name, so they write and read fewer bytes for it.
TEST(ComputerMindCore, AZeroByteInAFileNameMakesTheNameShorter)
{
	auto bytes = PersonalityFile(MakeMind());
	bytes.at(4) = 0;
	auto names = MakeNodeNames();
	auto loaded = MakeMind();
	ReadPersonalityFile(loaded, names, bytes);
	EXPECT_EQ(NameOf(names, NodeKind::Root), "Mast");
	// The entry keeps its size and the bytes after the new zero byte
	EXPECT_EQ(names.at(0).size(), FirstNodeNames().at(0).size());

	// The next save writes the root name in five bytes, then the weight
	auto shorter = loaded;
	shorter.nodes.at(0).weight = 2.0f;
	const auto saved = PersonalityFile(shorter, names);
	EXPECT_EQ(saved.size(), bytes.size() - (PersonalityName(NodeKind::Root).size() - 4));
	EXPECT_EQ(BitsAt(saved, 5), std::bit_cast<uint32_t>(2.0f));

	// The next load reads only five bytes for the root name, then the weight
	auto again = MakeMind();
	ReadPersonalityFile(again, names, saved);
	EXPECT_EQ(again.nodes.at(0).weight, 2.0f);
	EXPECT_EQ(again.nodes.at(1).weight, loaded.nodes.at(1).weight);
}

// A file that puts a letter on the zero byte of a name gives a name with no zero byte. The name then fills its entry
// and does not go past it: a save writes the entry with no zero byte, and a load reads only the bytes of the entry.
TEST(ComputerMindCore, ANameWithNoZeroByteStaysInItsEntry)
{
	auto bytes = PersonalityFile(MakeMind());
	const auto root = PersonalityName(NodeKind::Root);
	bytes.at(root.size()) = 'y';
	auto names = MakeNodeNames();
	auto loaded = MakeMind();
	ReadPersonalityFile(loaded, names, bytes);
	EXPECT_EQ(NameOf(names, NodeKind::Root), root + "y");
	EXPECT_EQ(names.at(0).size(), root.size() + 1);
	EXPECT_EQ(loaded.nodes.at(1).weight, 1.0f);

	const auto saved = PersonalityFile(loaded, names);
	EXPECT_EQ(saved, bytes);
	auto again = MakeMind();
	again.nodes.at(0).weight = 3.0f;
	ReadPersonalityFile(again, names, saved);
	EXPECT_EQ(again.nodes.at(0).weight, 1.0f);
	EXPECT_EQ(NameOf(names, NodeKind::Root), root + "y");
}

// A kind of node that is under more than one parent has one name for all its nodes. Each node of the kind in the file
// reads as many bytes as the name has at that time.
TEST(ComputerMindCore, TheNodesOfOneKindShareTheirName)
{
	const auto mind = MakeMind();
	std::optional<size_t> first;
	std::optional<size_t> second;
	for (size_t i = 0; i < mind.nodes.size() && !second.has_value(); ++i)
	{
		for (size_t j = 0; j < i; ++j)
		{
			if (mind.nodes.at(j).kind == mind.nodes.at(i).kind)
			{
				first = j;
				second = i;
				break;
			}
		}
	}
	ASSERT_TRUE(first.has_value() && second.has_value());
	const auto kind = mind.nodes.at(*first).kind;
	const auto name = PersonalityName(kind);
	ASSERT_GE(name.size(), 6u);
	auto bytes = PersonalityFile(mind);
	const auto placeOf = [&mind](size_t node) {
		size_t at = 0;
		for (size_t i = 0; i < node; ++i)
		{
			at += PersonalityName(mind.nodes.at(i).kind).size() + 5;
		}
		return at;
	};
	// The first node of the kind gives a name of one letter
	bytes.at(placeOf(*first) + 1) = 0;
	auto names = MakeNodeNames();
	auto loaded = MakeMind();
	ReadPersonalityFile(loaded, names, bytes);
	// The second node of the kind reads two bytes of the name, then takes the next four bytes of the name as its weight.
	// After that the load is out of step with the file.
	const auto at = placeOf(*second);
	EXPECT_EQ(std::bit_cast<uint32_t>(loaded.nodes.at(*second).weight), BitsAt(bytes, at + 2));
}

// The load of a saved game starts the mind again. It keeps only what the save keeps.
TEST(ComputerMindCore, TheMindStartsAgainAfterTheLoadOfASavedGame)
{
	auto saved = MakeMind();
	for (size_t i = 0; i < saved.nodes.size(); ++i)
	{
		auto& node = saved.nodes.at(i);
		node.suppression = static_cast<float>(i) * 0.001f;
		node.weight = static_cast<float>(i) * 0.5f;
		node.lastWorkedOut = 500;
		node.lastPerformed = 480;
		node.value = 0.75f;
		node.bestLeaf = 3;
		node.object = static_cast<entt::entity>(12);
		node.amount = 40;
		node.argument = static_cast<entt::entity>(13);
		node.place = {1.0f, 2.0f, 3.0f};
	}
	saved.doing = 0.6f;
	saved.lastEmptyTurn = 470;
	saved.wasBusy = false;
	saved.paused = true;
	saved.scriptHold = true;
	saved.workedOut = true;
	saved.castRequested.at(7) = 9;
	saved.queue.push_back({.name = "DIRECTPickupAndDropObject", .turn = 450});
	saved.scriptHand = true;

	const auto mind = StartAgain(saved);
	const auto fresh = MakeMind();
	ASSERT_EQ(mind.nodes.size(), saved.nodes.size());
	for (size_t i = 0; i < mind.nodes.size(); ++i)
	{
		const auto& node = mind.nodes.at(i);
		EXPECT_EQ(node.suppression, saved.nodes.at(i).suppression) << i;
		EXPECT_EQ(node.weight, saved.nodes.at(i).weight) << i;
		EXPECT_EQ(node.kind, fresh.nodes.at(i).kind) << i;
		EXPECT_EQ(node.lastWorkedOut, 0u) << i;
		EXPECT_EQ(node.lastPerformed, 0u) << i;
		EXPECT_EQ(node.value, 0.0f) << i;
		EXPECT_FALSE(node.bestLeaf.has_value()) << i;
		EXPECT_EQ(node.object, entt::entity {entt::null}) << i;
		EXPECT_EQ(node.amount, 0) << i;
		EXPECT_EQ(node.argument, entt::entity {entt::null}) << i;
		EXPECT_EQ(node.place, glm::vec3(0.0f)) << i;
	}
	EXPECT_EQ(mind.doing, 0.0f);
	EXPECT_EQ(mind.lastEmptyTurn, 0u);
	EXPECT_TRUE(mind.wasBusy);
	EXPECT_TRUE(mind.paused);
	EXPECT_TRUE(mind.scriptHold);
	EXPECT_FALSE(mind.workedOut);
	EXPECT_EQ(mind.castRequested, saved.castRequested);
	ASSERT_EQ(mind.queue.size(), 1u);
	EXPECT_EQ(mind.queue.front().name, "DIRECTPickupAndDropObject");
	EXPECT_EQ(mind.queue.front().turn, 450u);
	EXPECT_FALSE(mind.scriptHand);

	// A mind that has no tree yet stays without a tree
	EXPECT_TRUE(StartAgain(Mind {}).nodes.empty());
}
