/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// A temple's destruction as a system, in a world of its own: its sounds, spot visuals and end come on the turns the
// game's do, a start over while the loop plays sounds a second loop, and the game ends only outside skirmishes.

#define LOCATOR_IMPLEMENTATIONS

#include <algorithm>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include <entt/core/hashed_string.hpp>
#include <gtest/gtest.h>

#include "ECS/Components/Temple.h"
#include "ECS/Components/Transform.h"
#include "ECS/Registry.h"
#include "ECS/Systems/Implementations/TempleDestructionSystem.h"
#include "ECS/TempleDestructionWorld.h"

using namespace openblack;
using namespace openblack::ecs;
using namespace openblack::ecs::components;
using namespace openblack::ecs::systems;

namespace
{
/// Records what the destruction asks of the game, turn by turn
class FakeWorld final: public temple_world::World
{
public:
	struct Visual
	{
		int turn;
		SpotVisualType type;
		int32_t turns;
		PlayerNames player;
	};

	Registry* Entities() override { return &registry; }
	[[nodiscard]] uint32_t MillisecondsPerTurn() const override { return 100; }
	[[nodiscard]] std::optional<PlayerNames> LocalPlayer() const override { return PlayerNames::PLAYER_ONE; }
	entt::entity StartLoop(entt::id_type sound, glm::vec3 /*position*/, entt::entity /*owner*/) override
	{
		loopsStarted.push_back(turn);
		loopSound = sound;
		const auto emitter = registry.Create();
		playing.push_back(emitter);
		return emitter;
	}
	void PlayOnce(entt::id_type sound, glm::vec3 /*position*/, entt::entity /*owner*/) override
	{
		onceSound = sound;
		played.push_back(turn);
	}
	void StopSound(entt::entity emitter) override
	{
		std::erase(playing, emitter);
		stopped.push_back(turn);
	}
	std::optional<uint32_t> StartSpotVisual(SpotVisualType type, glm::vec3 /*position*/, int32_t turns,
	                                        PlayerNames player) override
	{
		visuals.push_back({.turn = turn, .type = type, .turns = turns, .player = player});
		return static_cast<uint32_t>(visuals.size());
	}
	void FollowWithSpotVisual(uint32_t visual, entt::entity target) override { followed.emplace_back(visual, target); }
	float RandomShare(float /*spread*/) override { return share; }
	void StartScript(std::string_view name) override { scripts.emplace_back(name); }
	void Remove(entt::entity object) override
	{
		removed.push_back(turn);
		registry.Destroy(object);
	}

	Registry registry;
	int turn {0};
	float share {0.0f};
	entt::id_type loopSound {0};
	entt::id_type onceSound {0};
	std::vector<int> loopsStarted;
	std::vector<entt::entity> playing;
	std::vector<int> stopped;
	std::vector<int> played;
	std::vector<Visual> visuals;
	std::vector<std::pair<uint32_t, entt::entity>> followed;
	std::vector<std::string> scripts;
	std::vector<int> removed;
};

class TempleDestructionSystemTest: public ::testing::Test
{
protected:
	void SetUp() override
	{
		auto world = std::make_unique<FakeWorld>();
		_world = world.get();
		_system = std::make_unique<TempleDestructionSystem>(std::move(world));
	}

	entt::entity MakeTemple(PlayerNames owner)
	{
		auto& registry = _world->registry;
		const auto temple = registry.Create();
		registry.Assign<Transform>(temple, glm::vec3(10.0f, 0.0f, 20.0f), glm::mat3(1.0f), glm::vec3(1.0f));
		registry.Assign<Temple>(temple, owner);
		const auto entrance = registry.Create();
		registry.Assign<TempleEntrance>(entrance, temple);
		return temple;
	}

	/// One game turn: the destruction steps first, the game's end is looked at last
	void Turn()
	{
		++_world->turn;
		_system->ProcessTurn();
		_system->EndTurn();
	}

	FakeWorld* _world {nullptr};
	std::unique_ptr<TempleDestructionSystem> _system;
};

TEST_F(TempleDestructionSystemTest, EachStepComesOnTheGameTurnOfIt)
{
	const auto temple = MakeTemple(PlayerNames::PLAYER_TWO);
	_world->share = 0.4f;
	_system->Start(temple);
	for (int i = 0; i < 230 && _world->registry.Valid(temple); ++i)
	{
		Turn();
	}
	EXPECT_EQ(_world->loopsStarted, std::vector<int> {1});
	EXPECT_EQ(_world->loopSound, entt::hashed_string("InGame.sad/171").value());
	ASSERT_EQ(_world->visuals.size(), 3u);
	EXPECT_EQ(_world->visuals[0].turn, 100);
	EXPECT_EQ(_world->visuals[0].type, SpotVisualType::MagicFxOnCitadel);
	EXPECT_EQ(_world->visuals[0].turns, 60);
	EXPECT_EQ(_world->visuals[0].player, PlayerNames::PLAYER_TWO);
	ASSERT_EQ(_world->followed.size(), 1u);
	EXPECT_EQ(_world->followed[0].second, temple);
	// The explosion: the loop stops and the last sound plays
	EXPECT_EQ(_world->visuals[1].turn, 215);
	EXPECT_EQ(_world->visuals[1].type, SpotVisualType::ExplosionCitadel);
	EXPECT_EQ(_world->visuals[1].turns, 150);
	EXPECT_EQ(_world->stopped, std::vector<int> {215});
	EXPECT_EQ(_world->played, std::vector<int> {215});
	EXPECT_EQ(_world->onceSound, entt::hashed_string("InGame.sad/168").value());
	// The smoke, and the temple and its way in going, on the same last turn
	EXPECT_EQ(_world->visuals[2].turn, 220);
	EXPECT_EQ(_world->visuals[2].type, SpotVisualType::EvilSmoke);
	EXPECT_EQ(_world->visuals[2].turns, 90);
	EXPECT_EQ(_world->removed, (std::vector<int> {220, 220}));
	EXPECT_FALSE(_world->registry.Valid(temple));
	// Another player's temple ends nothing
	EXPECT_TRUE(_world->scripts.empty());
}

TEST_F(TempleDestructionSystemTest, StartedAgainWhileTheLoopPlaysASecondLoopSoundsAndBothStopAtTheExplosion)
{
	const auto temple = MakeTemple(PlayerNames::PLAYER_TWO);
	_system->Start(temple);
	for (int i = 0; i < 50; ++i)
	{
		Turn();
	}
	_system->Start(temple);
	Turn();
	EXPECT_EQ(_world->loopsStarted, (std::vector<int> {1, 51}));
	EXPECT_EQ(_world->playing.size(), 2u);
	// The explosion comes 21.5 seconds after the second start, and stops both
	for (int i = 0; i < 214; ++i)
	{
		Turn();
	}
	EXPECT_EQ(_world->played, std::vector<int> {265});
	EXPECT_TRUE(_world->playing.empty());
	EXPECT_EQ(_world->stopped, (std::vector<int> {265, 265}));
}

TEST_F(TempleDestructionSystemTest, ALoopStopsWhenItsTempleGoesAnotherWay)
{
	const auto temple = MakeTemple(PlayerNames::PLAYER_TWO);
	_system->Start(temple);
	Turn();
	ASSERT_EQ(_world->playing.size(), 1u);
	_world->registry.Destroy(temple);
	Turn();
	EXPECT_TRUE(_world->playing.empty());
}

TEST_F(TempleDestructionSystemTest, TheLocalPlayersLossEndsTheGameOnceOnTheTurnItStarts)
{
	const auto temple = MakeTemple(PlayerNames::PLAYER_ONE);
	Turn();
	EXPECT_TRUE(_world->scripts.empty());
	_system->Start(temple);
	Turn();
	EXPECT_EQ(_world->scripts, std::vector<std::string> {"GameOver"});
	EXPECT_TRUE(_world->registry.Context().gameOver);
	Turn();
	EXPECT_EQ(_world->scripts.size(), 1u);
}

TEST_F(TempleDestructionSystemTest, LosingATempleInASkirmishDoesntEndTheGame)
{
	_world->registry.Context().skirmish = true;
	const auto temple = MakeTemple(PlayerNames::PLAYER_ONE);
	_system->Start(temple);
	Turn();
	Turn();
	EXPECT_TRUE(_world->scripts.empty());
	EXPECT_FALSE(_world->registry.Context().gameOver);
}

} // namespace
