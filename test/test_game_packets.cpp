/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <vector>

#include <gtest/gtest.h>

#include "Input/GamePackets.h"

using namespace openblack;

// The single-player packet path: nothing before the flush, the order kept, the flush by itself at 6 packets
TEST(GamePackets, AppliedAfterTheFlushInOrder)
{
	game_packets::Reset();
	static std::vector<int32_t> seen;
	seen.clear();
	game_packets::SetHandler(game_packets::Type::Tap, [](const game_packets::Packet& p) { seen.push_back(p.value); });
	game_packets::Push({game_packets::Type::Tap, entt::null, {}, 1});
	game_packets::Push({game_packets::Type::Tap, entt::null, {}, 2});
	game_packets::DispatchQueuedPackets();
	EXPECT_TRUE(seen.empty()); // not flushed yet
	game_packets::Flush();
	game_packets::DispatchQueuedPackets();
	EXPECT_EQ(seen, (std::vector<int32_t> {1, 2}));
	EXPECT_EQ(game_packets::Pending(), 0U);
}

TEST(GamePackets, FlushesByItselfAtSix)
{
	game_packets::Reset();
	static int count = 0;
	count = 0;
	game_packets::SetHandler(game_packets::Type::Tap, [](const game_packets::Packet&) { ++count; });
	for (int i = 0; i < 6; ++i)
	{
		game_packets::Push({game_packets::Type::Tap});
	}
	game_packets::DispatchQueuedPackets();
	EXPECT_EQ(count, 6);
}

// A packet a handler sends waits for the next flush: it is not applied in the turn that sent it
TEST(GamePackets, HandlerPushWaitsForTheNextFlush)
{
	game_packets::Reset();
	static std::vector<int32_t> seen;
	seen.clear();
	game_packets::SetHandler(game_packets::Type::Tap, [](const game_packets::Packet& p) {
		seen.push_back(p.value);
		if (p.value == 1)
		{
			game_packets::Push({game_packets::Type::Tap, entt::null, {}, 2});
		}
	});
	game_packets::Push({game_packets::Type::Tap, entt::null, {}, 1});
	game_packets::Flush();
	game_packets::DispatchQueuedPackets();
	EXPECT_EQ(seen, (std::vector<int32_t> {1}));
	game_packets::DispatchQueuedPackets();
	EXPECT_EQ(seen, (std::vector<int32_t> {1}));
	game_packets::Flush();
	game_packets::DispatchQueuedPackets();
	EXPECT_EQ(seen, (std::vector<int32_t> {1, 2}));
	game_packets::ClearHandlers();
}

// An object that is gone (here: no registry at all) reaches the handler as NULL, not dropped
TEST(GamePackets, GoneObjectReachesTheHandlerAsNull)
{
	game_packets::Reset();
	static int calls = 0;
	static bool gotNull = false;
	calls = 0;
	gotNull = false;
	game_packets::SetHandler(game_packets::Type::PlaceInHand, [](const game_packets::Packet& p) {
		++calls;
		gotNull = p.object == entt::null;
	});
	game_packets::Push({game_packets::Type::PlaceInHand, static_cast<entt::entity>(7)});
	game_packets::Flush();
	game_packets::DispatchQueuedPackets();
	EXPECT_EQ(calls, 1);
	EXPECT_TRUE(gotNull);
	game_packets::ClearHandlers();
}

// The creature hand's two packets carry the type bytes of the game's own
TEST(GamePackets, TheCreatureHandsTypes)
{
	EXPECT_EQ(static_cast<int>(game_packets::Type::CreatureFeedback), 0x59);
	EXPECT_EQ(static_cast<int>(game_packets::Type::CreatureLeashClick), 0x5F);
}
