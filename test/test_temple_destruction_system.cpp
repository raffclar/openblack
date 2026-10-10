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

#include "3D/ModelSurface.h"
#include "Common/GameRandom.h"
#include "ECS/Components/Temple.h"
#include "ECS/Components/Transform.h"
#include "ECS/Registry.h"
#include "ECS/Systems/Implementations/TempleDestructionSystem.h"
#include "ECS/TempleDestructionWorld.h"
#include "Particles/PlasmaCommand.h"
#include "Temple/TempleDestruction.h"

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
		// The beams' spot visuals are kept apart, numbered from 100
		if (type == SpotVisualType::MagicBeamOnCitadel)
		{
			beamSources.push_back({.turn = turn, .type = type, .turns = turns, .player = player});
			return static_cast<uint32_t>(99 + beamSources.size());
		}
		visuals.push_back({.turn = turn, .type = type, .turns = turns, .player = player});
		return static_cast<uint32_t>(visuals.size());
	}
	void FollowWithSpotVisual(uint32_t visual, entt::entity target) override { followed.emplace_back(visual, target); }
	void SetSpotVisualMagnitude(uint32_t visual, float magnitude) override { magnitudes.emplace_back(visual, magnitude); }
	[[nodiscard]] bool SpotVisualRunning(uint32_t visual) const override
	{
		return std::ranges::find(ended, visual) == ended.end();
	}
	void AddPlasma(uint32_t source, const particles::PlasmaCommand& command) override
	{
		beams.push_back({.turn = turn, .source = source, .command = command});
	}
	[[nodiscard]] std::vector<model_surface::Triangle> DrawnTrianglesOf(entt::entity /*object*/) const override
	{
		return triangles;
	}
	[[nodiscard]] glm::mat4 PlacementOf(entt::entity /*object*/) const override { return glm::mat4(1.0f); }
	[[nodiscard]] GameRandomInterface* Random() override { return random; }
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
	struct Beam
	{
		int turn;
		uint32_t source;
		particles::PlasmaCommand command;
	};
	std::vector<Visual> beamSources;
	std::vector<std::pair<uint32_t, float>> magnitudes;
	std::vector<uint32_t> ended;
	std::vector<Beam> beams;
	std::vector<model_surface::Triangle> triangles;
	GameRandomInterface* random {nullptr};
};

/// The game's random numbers on their own seeds
class SeededRandom final: public GameRandomInterface
{
public:
	uint32_t GameRand(uint32_t n) override { return n == 0 ? 0 : game_random::LHRand(n, _seeds.synced); }
	float GameFloatRand(float x) override { return game_random::FloatRand(x, _seeds.synced); }
	uint32_t LocalRand(int32_t n) override { return n == 0 ? 0 : game_random::LHRand(static_cast<uint32_t>(n), _seeds.local); }
	float LocalFloatRand(float x) override { return game_random::FloatRand(x, _seeds.local); }
	int32_t CrtRand() override { return 0; }
	void CrtSrand(uint32_t /*seed*/) override {}
	[[nodiscard]] GameRandomSeeds GetSeeds() const override { return _seeds; }
	void SetSeeds(GameRandomSeeds seeds) override { _seeds = seeds; }
	[[nodiscard]] ParticleRandomStream GetParticleStream() const override { return ParticleRandomStream::None; }
	void SetParticleStream(ParticleRandomStream /*stream*/) override {}

private:
	GameRandomSeeds _seeds;
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
	// Ten times as big as it was made
	EXPECT_EQ(_world->magnitudes, (std::vector<std::pair<uint32_t, float>> {{3u, 10.0f}}));
	EXPECT_EQ(_world->removed, (std::vector<int> {220, 220}));
	EXPECT_FALSE(_world->registry.Valid(temple));
	// Another player's temple ends nothing
	EXPECT_TRUE(_world->scripts.empty());
}

TEST_F(TempleDestructionSystemTest, BeamsLeapBetweenPointsOfTheHeartUntilItFades)
{
	const auto temple = MakeTemple(PlayerNames::PLAYER_TWO);
	// The heart is one roof, facing up
	const glm::vec3 up {0.0f, 1.0f, 0.0f};
	_world->triangles = {{{{{0.0f, 5.0f, 0.0f}, up}, {{4.0f, 5.0f, 0.0f}, up}, {{0.0f, 5.0f, 4.0f}, up}}}};
	SeededRandom random;
	_world->random = &random;
	_system->Start(temple);
	for (int i = 0; i < 230 && _world->registry.Valid(temple); ++i)
	{
		Turn();
	}
	// None on the first turn; the first on the next, at the start of the beaming
	ASSERT_FALSE(_world->beams.empty());
	const auto& first = _world->beams.front();
	EXPECT_EQ(first.turn, 2);
	const float share = 0.2f / 14.0f;
	EXPECT_NEAR(first.command.life, 3.0f - 2.3f * share, 1e-5f);
	EXPECT_NEAR(first.command.speed, 1.0f + 0.5f * share, 1e-5f);
	EXPECT_EQ(first.command.alpha, 52);
	EXPECT_EQ(first.command.start.y, 5.0f);
	EXPECT_EQ(first.command.end.y, 5.0f);
	EXPECT_EQ(first.command.startTangent, up);
	EXPECT_EQ(first.command.endTangent, -up);
	// Fired from one spot visual of the heart's player's that lasts the beaming
	ASSERT_EQ(_world->beamSources.size(), 1u);
	EXPECT_EQ(_world->beamSources[0].turn, 2);
	EXPECT_EQ(_world->beamSources[0].turns, 140);
	EXPECT_EQ(_world->beamSources[0].player, PlayerNames::PLAYER_TWO);
	EXPECT_TRUE(std::ranges::all_of(_world->beams, [](const FakeWorld::Beam& beam) { return beam.source == 100; }));
	// About one every four turns at first and one every two at the end, until fourteen seconds on
	const auto between = [this](int from, int to) {
		return std::ranges::count_if(_world->beams,
		                             [from, to](const FakeWorld::Beam& beam) { return beam.turn >= from && beam.turn <= to; });
	};
	EXPECT_EQ(between(2, 21), 6);
	EXPECT_EQ(between(121, 140), 9);
	EXPECT_EQ(_world->beams.size(), 48u);
	EXPECT_EQ(_world->beams.back().turn, 138);
	EXPECT_EQ(_world->beams.back().command.alpha, 197);
}

TEST_F(TempleDestructionSystemTest, ABeamSourceThatHasEndedIsMadeAgain)
{
	const auto temple = MakeTemple(PlayerNames::PLAYER_TWO);
	const glm::vec3 up {0.0f, 1.0f, 0.0f};
	_world->triangles = {{{{{0.0f, 5.0f, 0.0f}, up}, {{4.0f, 5.0f, 0.0f}, up}, {{0.0f, 5.0f, 4.0f}, up}}}};
	SeededRandom random;
	_world->random = &random;
	_system->Start(temple);
	for (int i = 0; i < 10; ++i)
	{
		Turn();
	}
	ASSERT_EQ(_world->beamSources.size(), 1u);
	_world->ended.push_back(100);
	const auto before = _world->beams.size();
	for (int i = 0; i < 10; ++i)
	{
		Turn();
	}
	ASSERT_EQ(_world->beamSources.size(), 2u);
	ASSERT_GT(_world->beams.size(), before);
	EXPECT_EQ(_world->beams.back().source, 101u);
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
