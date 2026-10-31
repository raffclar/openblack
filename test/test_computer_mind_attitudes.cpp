/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cstdint>

#include <optional>
#include <vector>

#include <entt/entity/registry.hpp>
#include <entt/meta/context.hpp>
#include <gtest/gtest.h>

#include "3D/MapCoords.h"
#include "Common/GUtilsDistance.h"
#include "ECS/Components/PlayerRelations.h"
#include "ECS/ComputerMind.h"
#include "ECS/ComputerMindDomains.h"
#include "ECS/ComputerMindRules.h"
#include "ECS/PlayerRelations.h"
#include "ECS/Reflection/ComponentReflection.h"
#include "ECS/Reflection/WorldArchive.h"
#include "ECS/SyncedState.h"
#include "ScriptHeaders/ScriptInfluence.h"

using namespace openblack;
using namespace openblack::computer_mind;
using namespace openblack::computer_mind_rules;

TEST(ComputerMindAttitudes, OwnsItsKindsOfNodes)
{
	EXPECT_EQ(DomainOf(NodeKind::ActionSendNiceTextToPlayer), MindDomain::Attitudes);
	EXPECT_EQ(DomainOf(NodeKind::ActionGiveToPlayer), MindDomain::Attitudes);
	EXPECT_EQ(DomainOf(NodeKind::TopDefeatPlayer), MindDomain::Attitudes);
	EXPECT_EQ(DomainOf(NodeKind::NeedGiveToPlayer), MindDomain::Attitudes);
	EXPECT_EQ(DomainOf(NodeKind::NeedTalktoPlayer), MindDomain::Attitudes);
	EXPECT_EQ(DomainOf(NodeKind::NeedDefeatPlayer), MindDomain::Attitudes);
	EXPECT_EQ(DomainOf(NodeKind::NeedPlayerWithMoreSpells), MindDomain::Attitudes);
	EXPECT_EQ(DomainOf(NodeKind::NeedPlayerWithBetterCreature), MindDomain::Attitudes);
	EXPECT_EQ(DomainOf(NodeKind::NeedPlayerWithMoreWorshippers), MindDomain::Attitudes);
}

// The game has the actions to give things to other players and to talk to them, but no rules for them: they find no
// value and have no action of their own
TEST(ComputerMindAttitudes, TheGiveAndTalkActionsHaveNoRules)
{
	for (const auto kind :
	     {NodeKind::ActionSendNiceTextToPlayer, NodeKind::ActionSendNastyTextToPlayer, NodeKind::ActionTauntPlayer,
	      NodeKind::ActionScarePlayer, NodeKind::ActionGiveCreatureThingToPlayer, NodeKind::ActionGiveToPlayer})
	{
		EXPECT_FALSE(InfoOf(kind).intensity) << static_cast<int>(kind);
		EXPECT_FALSE(InfoOf(kind).perform) << static_cast<int>(kind);
	}
}

// A script gives an attitude, the mind keeps two times it, and a script reads half of what the mind keeps
TEST(ComputerMindAttitudes, KeepsTwiceTheAttitudeAndReadsHalf)
{
	EXPECT_EQ(player_relations::KeptAttitude(2.0f), 4.0f);
	EXPECT_EQ(player_relations::KeptAttitude(-0.75f), -1.5f);
	EXPECT_EQ(player_relations::ReadAttitude(4.0f), 2.0f);
	// The start value that the mind keeps reads as one half
	EXPECT_EQ(player_relations::ReadAttitude(1.0f), 0.5f);
	EXPECT_EQ(player_relations::ReadAttitude(player_relations::KeptAttitude(0.3f)), 0.3f);
}

// An alliance above 0 makes an ally. Only an alliance above one tenth, compared wide, lends influence, so a script's 0.1
// lends.
TEST(ComputerMindAttitudes, AlliesAndTheInfluenceTheyLend)
{
	EXPECT_FALSE(player_relations::IsAlly(0.0f));
	EXPECT_FALSE(player_relations::IsAlly(-0.5f));
	EXPECT_TRUE(player_relations::IsAlly(0.01f));
	EXPECT_FALSE(player_relations::LendsInfluence(0.05f));
	EXPECT_FALSE(player_relations::LendsInfluence(0.099999994f));
	EXPECT_TRUE(player_relations::LendsInfluence(0.1f));
	EXPECT_TRUE(player_relations::LendsInfluence(1.0f));
	// Where the player has no influence, the first ally with some there lends it
	const std::vector<float> lent {0.0f, 0.4f, 0.9f};
	EXPECT_EQ(script::influence::Answer(0.0f, false, lent), 0.4f);
	EXPECT_EQ(script::influence::Answer(0.2f, false, lent), 0.2f);
	EXPECT_EQ(script::influence::Answer(-0.3f, false, {}), 0.0f);
}

TEST(ComputerMindAttitudes, AverageOfTheInfluencePower)
{
	EXPECT_EQ(player_relations::NextAverageInfluence(0.0f, 100.0f), 50.0f);
	EXPECT_EQ(player_relations::NextAverageInfluence(50.0f, 100.0f), 75.0f);
	EXPECT_EQ(player_relations::NextAverageInfluence(80.0f, 80.0f), 80.0f);
}

TEST(ComputerMindAttitudes, NearestAbodeWithin100Metres)
{
	const auto hand = map_coords::FromMetres({1000.0f, 1000.0f});
	EXPECT_EQ(player_relations::NearestAbode(hand, {}), 100.0f);
	const std::vector<map_coords::MapCoords> far {map_coords::FromMetres({1300.0f, 1000.0f})};
	EXPECT_EQ(player_relations::NearestAbode(hand, far), 100.0f);
	const std::vector<map_coords::MapCoords> abodes {map_coords::FromMetres({1050.0f, 1000.0f}),
	                                                 map_coords::FromMetres({1020.0f, 1000.0f}),
	                                                 map_coords::FromMetres({1040.0f, 1000.0f})};
	EXPECT_EQ(player_relations::NearestAbode(hand, abodes), gutils::GetDistanceInMetres(hand, abodes[1]));
}

// The first rival is the choice until a later rival has a strictly larger value
TEST(ComputerMindAttitudes, ChoosesTheRival)
{
	EXPECT_FALSE(player_relations::ChooseRival({}).has_value());
	EXPECT_EQ(player_relations::ChooseRival(std::vector<float> {0.0f, 0.0f}), 0u);
	EXPECT_EQ(player_relations::ChooseRival(std::vector<float> {-1.0f, -2.0f}), 0u);
	EXPECT_EQ(player_relations::ChooseRival(std::vector<float> {0.2f, 0.5f, 0.5f}), 1u);
	EXPECT_EQ(player_relations::ChooseRival(std::vector<float> {0.0f, -1.0f, 0.1f}), 2u);
	EXPECT_EQ(player_relations::DefeatIntensity(0.4f), 0.4f);
	EXPECT_EQ(player_relations::DefeatIntensity(1.0f), 1.0f);
	EXPECT_EQ(player_relations::DefeatIntensity(7.5f), 1.0f);
}

// The difference of two alignments from -1 to 1 counts in quarters of a whole number, so it never counts
TEST(ComputerMindAttitudes, AlignmentNeverCounts)
{
	player_relations::AttackSide good {.power = 100.0f, .averagePower = 100.0f, .alignment = 1.0f};
	player_relations::AttackSide evil {.power = 100.0f, .averagePower = 100.0f, .alignment = -1.0f};
	player_relations::AttackSide neutral {.power = 100.0f, .averagePower = 100.0f, .alignment = 1.0f};
	good.place = map_coords::FromMetres({1000.0f, 1000.0f});
	evil.place = map_coords::FromMetres({1500.0f, 1000.0f});
	neutral.place = evil.place;
	EXPECT_EQ(player_relations::DesireToAttack(good, evil, 100.0f), player_relations::DesireToAttack(good, neutral, 100.0f));
}

// A rival far away does not count, a weak rival near counts, and harm from the rival makes the god want revenge
TEST(ComputerMindAttitudes, DesireToAttackParts)
{
	const player_relations::AttackSide god {
	    .power = 300.0f, .averagePower = 300.0f, .place = map_coords::FromMetres({1000.0f, 1000.0f})};
	player_relations::AttackSide rival {.power = 300.0f, .averagePower = 300.0f};
	rival.place = map_coords::FromMetres({1000.0f, 1500.0f});
	const float equal = player_relations::DesireToAttack(god, rival, 100.0f);
	rival.power = 30.0f;
	rival.averagePower = 30.0f;
	const float weak = player_relations::DesireToAttack(god, rival, 100.0f);
	EXPECT_GT(weak, equal);
	rival.power = 300.0f;
	rival.averagePower = 300.0f;
	auto harmed = god;
	harmed.harmFromOther = 50.0f;
	EXPECT_GT(player_relations::DesireToAttack(harmed, rival, 100.0f), equal);
	// Abodes near the hand count
	EXPECT_GT(player_relations::DesireToAttack(god, rival, 10.0f), equal);
	// Far beyond the reach of 5120 metres between the edges, almost nothing counts
	rival.place = map_coords::FromMetres({1000.0f, 1000.0f + 6000.0f + 600.0f});
	EXPECT_LT(player_relations::DesireToAttack(god, rival, 10.0f), 0.001f);
}

// The alliances, the attitudes and the influence power of a player are in the synced hash of each turn, and come back
// whole from a world state
TEST(ComputerMindAttitudes, RecordsAreInTheSyncedStateAndTheWorldState)
{
	using namespace openblack::ecs::components;
	using namespace openblack::ecs::reflection;
	entt::meta_ctx context;
	RegisterComponents(context);
	entt::registry registry;
	const auto player = registry.create();
	auto& allies = registry.emplace<PlayerAllies>(player);
	auto& attitudes = registry.emplace<ComputerAttitudes>(player);
	auto& power = registry.emplace<PlayerInfluencePower>(player);
	EXPECT_EQ(attitudes.kept.at(31), ComputerAttitudes::k_StartValue);
	allies.strength.at(2) = 0.5f;
	attitudes.kept.at(1) = 4.0f;
	power = {.power = 120.0f, .average = 90.0f};
	const auto before = synced_state::HashOf(registry, context, synced_state::PresentationParts(), true);
	EXPECT_TRUE(before.unregistered.empty());
	const auto changes = [&](auto&& change) {
		change();
		return synced_state::HashOf(registry, context, synced_state::PresentationParts(), true).hash != before.hash;
	};
	EXPECT_TRUE(changes([&] { allies.strength.at(30) = 1.0f; }));
	allies.strength.at(30) = 0.0f;
	EXPECT_TRUE(changes([&] { attitudes.kept.at(5) = 0.0f; }));
	attitudes.kept.at(5) = ComputerAttitudes::k_StartValue;
	EXPECT_TRUE(changes([&] { power.average = 91.0f; }));
	power.average = 90.0f;

	WorldSaveReport report;
	const auto bytes = SaveWorld(registry, context, &report);
	EXPECT_TRUE(report.unsavedValues.empty());
	entt::registry loaded;
	ASSERT_TRUE(LoadWorld(loaded, context, bytes, WorldLoadMode::Exact).has_value());
	EXPECT_EQ(loaded.get<PlayerAllies>(player).strength, allies.strength);
	EXPECT_EQ(loaded.get<ComputerAttitudes>(player).kept, attitudes.kept);
	EXPECT_EQ(loaded.get<PlayerInfluencePower>(player).average, 90.0f);
	EXPECT_EQ(synced_state::HashOf(loaded, context, synced_state::PresentationParts(), false).hash,
	          synced_state::HashOf(registry, context, synced_state::PresentationParts(), false).hash);
}
