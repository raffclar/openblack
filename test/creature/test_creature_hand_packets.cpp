/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The two packets the hand sends about a creature it held (ECS/CreatureHandPackets.h), with a fake mind and a fake
// leash: the feedback reaches the mind of a creature still there, the click works the player's leash key, and both wait
// for the turn after the one that sent them, the click first

#define LOCATOR_IMPLEMENTATIONS

#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "Creature/LeashKeys.h"
#include "ECS/CreatureHandPackets.h"
#include "Input/GamePackets.h"
#include "creature/CreatureSystemFakes.h"
#include "creature/CreatureSystemWorld.h"
#include "support/CreatureFakes.h"

using namespace openblack;
using openblack::test::creature_fakes::FakeMind;
using openblack::test::creature_loop_fakes::CallLog;
using openblack::test::creature_loop_fakes::FakeLeash;
namespace packets = openblack::ecs::creature_hand_packets;

namespace
{
game_packets::Packet FeedbackFor(entt::entity creature, float feedback)
{
	game_packets::Packet packet {game_packets::Type::CreatureFeedback, creature};
	packet.data[0] = feedback;
	return packet;
}

class CreatureHandPacketsTest: public ::testing::Test
{
protected:
	void SetUp() override { game_packets::Reset(); }
	void TearDown() override
	{
		game_packets::Reset();
		game_packets::ClearHandlers();
	}

	test::creature_world::World _world;
	FakeMind _mind;
	CallLog _log;
	FakeLeash _leash {_log};
};
} // namespace

TEST_F(CreatureHandPacketsTest, TheFeedbackReachesTheCreaturesMind)
{
	const auto creature = test::creature_world::World::MakeCreature();
	packets::ApplyFeedback(&_mind, FeedbackFor(creature, -0.5f));
	ASSERT_EQ(_mind.feedback.size(), 1u);
	EXPECT_EQ(_mind.feedback.front().first, creature);
	EXPECT_FLOAT_EQ(_mind.feedback.front().second, -0.5f);
}

TEST_F(CreatureHandPacketsTest, NoCreatureNoMindNoFeedback)
{
	// gone (the packet's object is null), not a creature, or no minds at all
	packets::ApplyFeedback(&_mind, FeedbackFor(entt::null, 1.0f));
	packets::ApplyFeedback(&_mind, FeedbackFor(test::creature_world::World::Registry().Create(), 1.0f));
	packets::ApplyFeedback(nullptr, FeedbackFor(test::creature_world::World::MakeCreature(), 1.0f));
	EXPECT_TRUE(_mind.feedback.empty());
}

TEST_F(CreatureHandPacketsTest, TheClickIsThePlayersLeashKey)
{
	packets::ApplyLeashClick(&_leash, PlayerNames::PLAYER_ONE);
	ASSERT_EQ(_log.size(), 1u);
	EXPECT_EQ(_log.front().name, "leash.PressKey");
	EXPECT_EQ(_log.front().args, (std::vector<float> {static_cast<float>(PlayerNames::PLAYER_ONE),
	                                                  static_cast<float>(creature_leash::LeashKey::Leash)}));
	packets::ApplyLeashClick(nullptr, PlayerNames::PLAYER_ONE);
	EXPECT_EQ(_log.size(), 1u);
}

TEST_F(CreatureHandPacketsTest, BothWaitForTheNextTurnInTheOrderPushed)
{
	static std::vector<std::string> seen;
	seen.clear();
	static FakeMind* mind = nullptr;
	static FakeLeash* leash = nullptr;
	mind = &_mind;
	leash = &_leash;
	game_packets::SetHandler(game_packets::Type::CreatureFeedback, [](const game_packets::Packet& packet) {
		seen.emplace_back("feedback");
		packets::ApplyFeedback(mind, packet);
	});
	game_packets::SetHandler(game_packets::Type::CreatureLeashClick, [](const game_packets::Packet&) {
		seen.emplace_back("click");
		packets::ApplyLeashClick(leash, PlayerNames::PLAYER_ONE);
	});
	const auto creature = test::creature_world::World::MakeCreature();
	// the frame the hand lets go it pushes the click, then the feedback (that order: test_hand_creature); the turn
	// applies them as pushed
	game_packets::Push({game_packets::Type::CreatureLeashClick});
	game_packets::Push(FeedbackFor(creature, 0.25f));
	game_packets::DispatchQueuedPackets();
	EXPECT_TRUE(seen.empty());
	game_packets::Flush();
	game_packets::DispatchQueuedPackets();
	EXPECT_EQ(seen, (std::vector<std::string> {"click", "feedback"}));
	ASSERT_EQ(_mind.feedback.size(), 1u);
	EXPECT_FLOAT_EQ(_mind.feedback.front().second, 0.25f);
	EXPECT_EQ(_log.size(), 1u);
	mind = nullptr;
	leash = nullptr;
}
