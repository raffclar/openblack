/*******************************************************************************
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

#include <glm/mat3x3.hpp>
#include <glm/vec3.hpp>
#include <gtest/gtest.h>

#include "ECS/Components/Transform.h"
#include "ECS/Registry.h"
#include "ECS/SyncedState.h"
#include "Locator.h"

// Enable this define because we use a custom locator
#define LOCATOR_IMPLEMENTATIONS
#include "ECS/Systems/Implementations/SyncChecksumSystem.h"
#include "ECS/Systems/Implementations/TimeSystem.h"

using namespace openblack;
using openblack::ecs::systems::SyncChecksumSystem;

namespace
{
/// A world of a few things, and the clock of its turns
class SyncChecksumBesideDraw: public ::testing::Test
{
protected:
	void SetUp() override
	{
		auto& registry = Locator::entitiesRegistry::emplace<ecs::Registry>();
		Locator::time::emplace<ecs::systems::TimeSystem>([] { return uint32_t {1000}; });
		for (int i = 0; i < 40; ++i)
		{
			const auto thing = registry.Create();
			registry.Assign<ecs::components::Transform>(thing, glm::vec3(static_cast<float>(i), 2.0f, 3.0f), glm::mat3(1.0f),
			                                            glm::vec3(1.0f));
		}
	}
	void TearDown() override
	{
		Locator::time::reset();
		Locator::entitiesRegistry::reset();
	}

	/// Moves the first thing, as a turn changes the world
	static void MoveFirstThing(float x)
	{
		auto& registry = Locator::entitiesRegistry::value();
		registry.Each<ecs::components::Transform>([x, done = false](ecs::components::Transform& transform) mutable {
			if (!done)
			{
				transform.position.x = x;
				done = true;
			}
		});
	}

	/// The checksum of the world as it is now, made in the turn as usual
	[[nodiscard]] static uint64_t ChecksumInTurn()
	{
		SyncChecksumSystem checksums;
		checksums.SetEnabled(true);
		checksums.ProcessTurn();
		return checksums.GetLast().value().checksum;
	}
};
} // namespace

TEST(SyncChecksumNeeds, TheHashWaitsOnlyWhenTheFrameLetsItAndNoToolNeedsIt)
{
	EXPECT_TRUE(synced_state::HashCanWait({.besideDraw = true}));
	EXPECT_FALSE(synced_state::HashCanWait({}));
	EXPECT_FALSE(synced_state::HashCanWait({.besideDraw = true, .keepParts = true}));
	EXPECT_FALSE(synced_state::HashCanWait({.besideDraw = true, .dumpThisTurn = true}));
	EXPECT_FALSE(synced_state::HashCanWait({.besideDraw = true, .rowDump = true}));
	EXPECT_FALSE(synced_state::HashCanWait({.besideDraw = true, .worldStates = true}));
}

// The hash beside the draw gives the same checksum as the hash in the turn, and the listeners get it when it ends
TEST_F(SyncChecksumBesideDraw, TheChecksumIsTheSameAsInTheTurn)
{
	const auto expected = ChecksumInTurn();
	SyncChecksumSystem checksums;
	checksums.SetEnabled(true);
	checksums.SetHashBesideDraw(true);
	std::vector<uint64_t> heard;
	std::vector<uint32_t> hashed;
	checksums.SetTurnListener([&heard](const auto& turn) { heard.push_back(turn.checksum); });
	checksums.SetHashListener([&hashed](uint32_t turn, const synced_state::Hash&) { hashed.push_back(turn); });
	checksums.ProcessTurn();
	EXPECT_TRUE(checksums.HashWaits(0));
	EXPECT_FALSE(checksums.GetLast().has_value());
	EXPECT_TRUE(heard.empty());

	checksums.StartHashBesideDraw();
	checksums.FinishHash();
	EXPECT_FALSE(checksums.HashWaits(0));
	ASSERT_TRUE(checksums.GetLast().has_value());
	EXPECT_EQ(checksums.GetLast()->checksum, expected);
	EXPECT_EQ(heard, std::vector<uint64_t> {expected});
	EXPECT_EQ(hashed, std::vector<uint32_t> {0});
	EXPECT_NE(checksums.GetHashOfTurn(0, false), nullptr);

	// A second finish has nothing to do
	checksums.FinishHash();
	EXPECT_EQ(heard.size(), 1u);
}

// A hash that waits and that no draw started is made when the system finishes it, and the next turns use the memo of
// the hash before them with the same results
TEST_F(SyncChecksumBesideDraw, AHashThatDidNotStartIsMadeAtTheFinish)
{
	SyncChecksumSystem checksums;
	checksums.SetEnabled(true);
	checksums.SetHashBesideDraw(true);
	for (int turn = 0; turn < 4; ++turn)
	{
		MoveFirstThing(static_cast<float>(100 + turn));
		const auto expected = ChecksumInTurn();
		checksums.ProcessTurn();
		if (turn % 2 == 0)
		{
			checksums.StartHashBesideDraw();
		}
		checksums.FinishHash();
		ASSERT_TRUE(checksums.GetLast().has_value());
		EXPECT_EQ(checksums.GetLast()->checksum, expected) << turn;
	}
}

// A tool that keeps the parts needs the hash in the turn, so it does not wait
TEST_F(SyncChecksumBesideDraw, KeptPartsDoNotWait)
{
	SyncChecksumSystem checksums;
	checksums.SetEnabled(true);
	checksums.SetHashBesideDraw(true);
	checksums.SetKeepParts(true, false);
	checksums.ProcessTurn();
	EXPECT_FALSE(checksums.HashWaits(0));
	ASSERT_TRUE(checksums.GetLast().has_value());
	EXPECT_FALSE(checksums.GetLast()->parts.empty());
}
