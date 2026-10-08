/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The alignment service: the players' alignments go to the player system's, the camera's is the interface alignment,
// and the sky moves toward its target by game time

#define LOCATOR_IMPLEMENTATIONS

#include <cstdlib>

#include <gtest/gtest.h>

#include "ECS/Effects/Alignment.h"
#include "ECS/Systems/Implementations/AlignmentSystem.h"
#include "ECS/Systems/PlayerSystemInterface.h"
#include "EngineConfig.h"
#include "Locator.h"
#include "support/RestoreService.h"

using namespace openblack;
using namespace std::chrono_literals;
using openblack::ecs::components::PlayerMagic;
using openblack::ecs::systems::AlignmentSystem;
using openblack::ecs::systems::PlayerSystemInterface;

namespace
{
/// Hands out one alignment for every player and records which player was asked for
class FakePlayerSystem final: public PlayerSystemInterface
{
public:
	void RegisterPlayers() override {}
	void AddPlayer(entt::entity) override {}
	[[nodiscard]] entt::entity GetPlayer(PlayerNames) const override { return entt::null; }
	[[nodiscard]] PlayerNames LocalPlayer() const override { return PlayerNames::PLAYER_ONE; }
	void ClearPlayers() override {}
	[[nodiscard]] ecs::components::Alignment& Alignment(PlayerNames name) override
	{
		asked = name;
		return alignment;
	}
	[[nodiscard]] PlayerMagic& MagicWithoutEntity(PlayerNames) override { return magic; }
	void ClearMagicWithoutEntity() override {}

	ecs::components::Alignment alignment {};
	PlayerMagic magic {};
	PlayerNames asked {PlayerNames::_COUNT};
};

class AlignmentSystemTest: public ::testing::Test
{
protected:
	void SetUp() override
	{
		_players = &static_cast<FakePlayerSystem&>(Locator::playerSystem::emplace<FakePlayerSystem>());
		Locator::entitiesRegistry::reset();
		Locator::camera::reset();
		Locator::config::reset();
		ecs::effects::alignment::ResetInterfaceAlignment();
	}
	void TearDown() override { ecs::effects::alignment::ResetInterfaceAlignment(); }

	[[nodiscard]] FakePlayerSystem& Players() const { return *_players; }

private:
	test::RestoreService<Locator::playerSystem> _restorePlayers;
	test::RestoreService<Locator::entitiesRegistry> _restoreRegistry;
	test::RestoreService<Locator::camera> _restoreCamera;
	test::RestoreService<Locator::config> _restoreConfig;
	FakePlayerSystem* _players {nullptr};
};
} // namespace

TEST_F(AlignmentSystemTest, readsThePlayerSystemsAlignment)
{
	Players().alignment.value = -0.5f;
	const AlignmentSystem alignment;
	EXPECT_EQ(alignment.GetPlayerAlignment(PlayerNames::PLAYER_TWO), -0.5f);
	EXPECT_EQ(Players().asked, PlayerNames::PLAYER_TWO);
}

TEST_F(AlignmentSystemTest, setHoldsTheAlignmentBetweenMinusOneAndOne)
{
	AlignmentSystem alignment;
	alignment.SetPlayerAlignment(PlayerNames::PLAYER_ONE, 0.25f);
	EXPECT_EQ(Players().alignment.value, 0.25f);
	EXPECT_EQ(Players().asked, PlayerNames::PLAYER_ONE);
	alignment.SetPlayerAlignment(PlayerNames::PLAYER_ONE, 3.0f);
	EXPECT_EQ(Players().alignment.value, 1.0f);
	alignment.SetPlayerAlignment(PlayerNames::PLAYER_ONE, -3.0f);
	EXPECT_EQ(Players().alignment.value, -1.0f);
}

TEST_F(AlignmentSystemTest, addHoldsTheSumAndLeavesThePendingChange)
{
	Players().alignment = {.value = 0.75f, .pending = 0.125f};
	AlignmentSystem alignment;
	alignment.AddPlayerAlignment(PlayerNames::PLAYER_THREE, 0.5f);
	EXPECT_EQ(Players().alignment.value, 1.0f);
	EXPECT_EQ(Players().asked, PlayerNames::PLAYER_THREE);
	alignment.AddPlayerAlignment(PlayerNames::PLAYER_THREE, -0.25f);
	EXPECT_EQ(Players().alignment.value, 0.75f);
	EXPECT_EQ(Players().alignment.pending, 0.125f);
}

TEST_F(AlignmentSystemTest, theCameraStartsNeutralAndATurnWithoutACameraKeepsIt)
{
	Players().alignment.value = 1.0f;
	AlignmentSystem alignment;
	EXPECT_EQ(alignment.GetCameraAlignment(), 0.0f);
	alignment.UpdateTurn();
	EXPECT_EQ(alignment.GetCameraAlignment(), 0.0f);
	EXPECT_EQ(ecs::effects::alignment::GetInterfaceAlignment(), 0.5f);
}

TEST_F(AlignmentSystemTest, theSkyTurnsTowardItsTargetByGameTime)
{
	if (std::getenv("OPENBLACK_TEST_SKY_ALIGNMENT") != nullptr)
	{
		GTEST_SKIP() << "the sky's target is set from the environment";
	}
	// the debug slider sets the sky's target, as it does the renderer's
	Locator::config::emplace().skyAlignment = 1.0f;
	AlignmentSystem alignment;
	EXPECT_EQ(alignment.GetSkyAlignment(), 0.0f);
	alignment.Update(250ms);
	EXPECT_FLOAT_EQ(alignment.GetSkyAlignment(), 0.25f);
	// paused: no game time, no turn
	alignment.Update(0ms);
	EXPECT_FLOAT_EQ(alignment.GetSkyAlignment(), 0.25f);
	// it stops at the target
	alignment.Update(1000ms);
	EXPECT_EQ(alignment.GetSkyAlignment(), 1.0f);
	Locator::config::value().skyAlignment = -1.0f;
	alignment.Update(500ms);
	EXPECT_FLOAT_EQ(alignment.GetSkyAlignment(), 0.5f);
}

TEST_F(AlignmentSystemTest, withoutAnOverrideTheSkyFollowsTheCamera)
{
	if (std::getenv("OPENBLACK_TEST_SKY_ALIGNMENT") != nullptr)
	{
		GTEST_SKIP() << "the sky's target is set from the environment";
	}
	AlignmentSystem alignment;
	alignment.Update(1000ms);
	EXPECT_EQ(alignment.GetSkyAlignment(), alignment.GetCameraAlignment());
}
