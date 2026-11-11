/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cstdint>

#include <chrono>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <vector>

#include <entt/core/hashed_string.hpp>
#include <glm/vec3.hpp>
#include <gtest/gtest.h>
#include <spdlog/sinks/null_sink.h>
#include <spdlog/spdlog.h>

#include "3D/FilmCameraPath.h"
#include "Common/GameRandom.h"
#include "ECS/Components/Creature.h"
#include "ECS/Components/CreatureBody.h"
#include "ECS/Components/Mesh.h"
#include "ECS/DrawnRegistry.h"
#include "ECS/Registry.h"
#include "ECS/Systems/CreatureAnimationSystemInterface.h"
#include "ECS/Systems/PlayerSystemInterface.h"
#include "Locator.h"
#include "Video/FilmCreature.h"

#define LOCATOR_IMPLEMENTATIONS

#include "ECS/Systems/Implementations/FilmCreatureSystem.h"
#include "Resources/Resources.h"

using namespace openblack;
using namespace openblack::ecs;
using namespace openblack::ecs::components;
using namespace openblack::ecs::systems;

namespace
{
/// Animations that keep the order of the poses and the eye steps of the film's copy
class StepLog final: public CreatureAnimationSystemInterface
{
public:
	void ProcessTurn() override {}
	void Update(std::chrono::duration<float, std::milli>) override {}
	void PoseTurn(entt::entity) override {}
	void PoseTurns() override {}
	void HoldTurnPose(entt::entity) override {}
	void PoseCopy(Registry&, entt::entity, float milliseconds, const Registry&) override
	{
		steps.push_back("pose " + std::to_string(static_cast<int32_t>(milliseconds)));
	}
	void PlaceCopyEyes(Registry&, entt::entity, const Registry&) override { steps.emplace_back("eyes"); }
	[[nodiscard]] std::optional<glm::vec3> BoneInAnimation(entt::entity, size_t, float, uint32_t, bool) override
	{
		return std::nullopt;
	}
	[[nodiscard]] std::optional<glm::vec3> MiddleOfBonesInAnimation(entt::entity, size_t, float,
	                                                                std::span<const uint32_t>) override
	{
		return std::nullopt;
	}
	[[nodiscard]] std::optional<float> AnimationDuration(entt::entity, size_t) override { return std::nullopt; }
	[[nodiscard]] std::optional<glm::vec3> AnimationTravel(entt::entity, size_t) override { return std::nullopt; }
	[[nodiscard]] std::optional<creature_kiss::KissPoints> KissPointsOf(entt::entity) override { return std::nullopt; }
	void KickSway(entt::entity, glm::vec3, glm::vec3) override {}
	void PlaceHandGlows(Registry&, entt::entity, int32_t, int32_t) override {}
	void SetCreedPower(entt::entity, creature_hand_glow::Place, float, float) override {}
	void LoadCreedPowers(entt::entity,
	                     std::span<const creature_hand_glow::SavedPower, creature_hand_glow::k_PlaceCount>) override
	{
	}
	void PlayCreedEvent(entt::entity, int32_t) override {}
	[[nodiscard]] std::optional<glm::vec3> MiddleOfGroinAndLips(const Registry&, entt::entity) override { return std::nullopt; }
	void AddAlignmentMotesOf(const Registry&, entt::entity) override {}

	std::vector<std::string> steps;
};

/// The players of the test: player one has the creature of the film
class OnePlayer final: public PlayerSystemInterface
{
public:
	void RegisterPlayers() override {}
	void AddPlayer(entt::entity /*playerEntity*/) override {}
	[[nodiscard]] std::optional<entt::entity> FindPlayer(PlayerNames /*name*/) const override { return std::nullopt; }
	entt::entity EnsurePlayer(PlayerNames /*name*/) override { return entt::null; }
	void AddCreature(entt::entity /*creature*/) override {}
	[[nodiscard]] std::optional<entt::entity> GetPrimaryCreature(PlayerNames name) const override
	{
		return name == PlayerNames::PLAYER_ONE ? std::optional(creature) : std::nullopt;
	}
	void KeepForNextLand() override {}
	void ForgetKept() override {}
	[[nodiscard]] bool KeepAlignmentAt(uint32_t /*turn*/) override { return false; }
	void TakeUpKept(entt::entity /*playerEntity*/) override {}

	entt::entity creature {entt::null};
};

/// Random numbers that are always zero
class ZeroRandom final: public GameRandomInterface
{
public:
	uint32_t SyncedRand(uint32_t, DrawSite) override { return 0; }
	float SyncedFloatRand(float, DrawSite) override { return 0.0f; }
	uint32_t LocalRand(int32_t) override { return 0; }
	float LocalFloatRand(float) override { return 0.0f; }
	int32_t CrtRand() override { return 0; }
	void CrtSrand(uint32_t) override {}
	[[nodiscard]] GameRandomSeeds GetSeeds() const override { return {}; }
	void SetSeeds(GameRandomSeeds) override {}
	[[nodiscard]] ParticleRandomStream GetParticleStream() const override { return ParticleRandomStream::None; }
	void SetParticleStream(ParticleRandomStream) override {}
};

/// The falling spell film with the player's creature, its copy made at the start
class FilmCreatureEyesTest: public ::testing::Test
{
protected:
	void SetUp() override
	{
		if (spdlog::get("game") == nullptr)
		{
			spdlog::register_logger(std::make_shared<spdlog::logger>("game", std::make_shared<spdlog::sinks::null_sink_mt>()));
		}
		Locator::entitiesRegistry::emplace<Registry>();
		Locator::drawnRegistry::emplace<DrawnRegistry>();
		Locator::resources::emplace<resources::Resources>();
		// The film has a camera path with no points, so that the test reads no file
		Locator::resources::value().GetFilmCameraPaths().Register(entt::hashed_string {"film/fall"}.value(),
		                                                          [] { return std::make_shared<FilmCameraPath>(); });
		Locator::gameRandom::emplace<ZeroRandom>();
		animations = &static_cast<StepLog&>(Locator::creatureAnimationSystem::emplace<StepLog>());
		auto& players = static_cast<OnePlayer&>(Locator::playerSystem::emplace<OnePlayer>());
		auto& land = Locator::entitiesRegistry::value();
		players.creature = land.Create();
		land.Assign<Creature>(players.creature);
		land.Assign<Mesh>(players.creature, Mesh {.id = 0, .submeshId = 0, .bbSubmeshId = 0});
		land.Assign<CreatureMorph>(players.creature);
		film.Start();
		animations->steps.clear();
	}
	void TearDown() override
	{
		film.End();
		Locator::playerSystem::reset();
		Locator::creatureAnimationSystem::reset();
		Locator::gameRandom::reset();
		Locator::resources::reset();
		Locator::drawnRegistry::reset();
		Locator::entitiesRegistry::reset();
	}

	FilmCreatureSystem film;
	StepLog* animations {nullptr};
	std::chrono::steady_clock::time_point now;
};

} // namespace

TEST_F(FilmCreatureEyesTest, TheEyesMoveOnEachShownFrameAlsoWhenTheFilmTimeStands)
{
	ASSERT_TRUE(film.IsRunning());
	// The film time moves: the copy takes its pose, and then its eyes move. The next two frames show the same film
	// time, so the copy keeps its pose, but its eyes move on each frame.
	film.Update(1000, now);
	film.Update(1000, now + std::chrono::milliseconds(10));
	film.Update(1000, now + std::chrono::milliseconds(20));
	film.Update(1040, now + std::chrono::milliseconds(30));
	const std::vector<std::string> expected {
	    "pose " + std::to_string(1000 - film_creature::Clock {}.lastMs), "eyes", "eyes", "eyes", "pose 40", "eyes"};
	EXPECT_EQ(animations->steps, expected);
}

TEST_F(FilmCreatureEyesTest, TheEyesStandWhileTheFilmHidesTheCopy)
{
	ASSERT_TRUE(film.IsRunning());
	// The film hides the copy: it takes its poses, but its eyes do not move until the film shows it again
	film.Update(film_creature::k_HiddenFromMs, now);
	film.Update(film_creature::k_HiddenFromMs, now + std::chrono::milliseconds(10));
	film.Update(film_creature::k_HiddenToMs + 1, now + std::chrono::milliseconds(20));
	const std::vector<std::string> expected {
	    "pose " + std::to_string(film_creature::k_HiddenFromMs - film_creature::Clock {}.lastMs),
	    "pose " + std::to_string(film_creature::k_HiddenToMs + 1 - film_creature::k_HiddenFromMs), "eyes"};
	EXPECT_EQ(animations->steps, expected);
}
