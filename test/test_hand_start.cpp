/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cstdint>

#include <bit>
#include <map>
#include <optional>

#include <gtest/gtest.h>

#include "3D/MapCoords.h"
#include "Commands/PlayerCommands.h"
#include "ECS/Components/ComputerPlayer.h"
#include "ECS/Components/Hand.h"
#include "ECS/Components/HandStart.h"
#include "ECS/Components/HandStillness.h"
#include "ECS/Components/Player.h"
#include "ECS/Components/ReportedHand.h"
#include "ECS/Components/Transform.h"
#include "ECS/Registry.h"
#include "ECS/ReportedCamera.h"
#include "ECS/Systems/PlayerSystemInterface.h"
#include "Hand/HandStart.h"
#include "Locator.h"

// The test builds its own locator
#define LOCATOR_IMPLEMENTATIONS 1
#include "ECS/Systems/Implementations/ComputerPlayerSystem.h"
#include "ECS/Systems/Implementations/HandSystem.h"

using namespace openblack;
using namespace openblack::ecs::components;

TEST(HandStart, TheStartIsTheMapPositionOfTheCitadelOnTheLand)
{
	// 3.5 cells along x and 7.25 cells along z, 2 m above the ground
	const map_coords::MapCoords place {.x = 0x38000, .z = 0x74000, .altitude = 2.0f};
	const auto point = hand_start::AtCitadel(place, 12.5f);
	EXPECT_EQ(point, glm::vec3(35.0f, 14.5f, 72.5f));
}

TEST(HandStart, AFarMapPositionRoundsOnceToMetres)
{
	// Above 2^24 a float of the integer rounds before the product. The start rounds only the exact product.
	constexpr int32_t k_Far = 0x01FFFFFF;
	const auto point = hand_start::AtCitadel({.x = k_Far, .z = k_Far, .altitude = 0.0f}, 0.0f);
	const auto exact = static_cast<float>(static_cast<double>(k_Far) * 10.0 / 65536.0);
	EXPECT_EQ(std::bit_cast<uint32_t>(point.x), std::bit_cast<uint32_t>(exact));
	EXPECT_EQ(std::bit_cast<uint32_t>(point.z), std::bit_cast<uint32_t>(exact));
}

TEST(HandStart, TheCameraIsTwentyMetresAboveTheStartAndTwentyMetresAlongZ)
{
	EXPECT_EQ(hand_start::CameraOf({100.0f, 30.0f, 200.0f}), glm::vec3(100.0f, 50.0f, 220.0f));
}

TEST(HandStart, AGodWithNoCitadelLooksAtTheOriginOfTheWorld)
{
	const auto still = hand_start::GodCameraOf(std::nullopt, std::nullopt);
	EXPECT_EQ(still.origin, glm::vec3(0.0f));
	EXPECT_EQ(still.focus, glm::vec3(0.0f));
	const auto moved = hand_start::GodCameraOf(glm::vec3(1.0f, 2.0f, 3.0f), std::nullopt);
	EXPECT_EQ(moved.origin, glm::vec3(1.0f, 2.0f, 3.0f));
	EXPECT_EQ(moved.focus, glm::vec3(0.0f));
}

TEST(HandStart, TheCameraOfAGodWithNoMindLooksAtItsCitadelFromAbove)
{
	const auto camera = hand_start::GodCameraOf(std::nullopt, glm::vec3(100.0f, 30.0f, 200.0f));
	EXPECT_EQ(camera.origin, glm::vec3(100.0f, 50.0f, 220.0f));
	EXPECT_EQ(camera.focus, glm::vec3(100.0f, 30.0f, 200.0f));
}

TEST(HandStart, TheMindMovesTheCameraAndTheFocusStaysAtTheCitadel)
{
	const auto camera = hand_start::GodCameraOf(glm::vec3(400.0f, 12.0f, 500.0f), glm::vec3(100.0f, 30.0f, 200.0f));
	EXPECT_EQ(camera.origin, glm::vec3(400.0f, 12.0f, 500.0f));
	EXPECT_EQ(camera.focus, glm::vec3(100.0f, 30.0f, 200.0f));
}

TEST(ReportedCameraRules, ACreatureReadsTheCameraOfItsOwnPlayer)
{
	using ecs::reported_camera::NearestCameraPlayer;
	EXPECT_EQ(NearestCameraPlayer(PlayerNames::PLAYER_THREE, false, PlayerNames::PLAYER_ONE), PlayerNames::PLAYER_THREE);
	EXPECT_EQ(NearestCameraPlayer(PlayerNames::PLAYER_THREE, true, PlayerNames::PLAYER_ONE), PlayerNames::PLAYER_THREE);
}

TEST(ReportedCameraRules, ACreatureWithNoPlayerReadsTheLocalCameraOnlyInAGameAlone)
{
	using ecs::reported_camera::NearestCameraPlayer;
	EXPECT_EQ(NearestCameraPlayer(std::nullopt, false, PlayerNames::PLAYER_TWO), PlayerNames::PLAYER_TWO);
	EXPECT_FALSE(NearestCameraPlayer(std::nullopt, true, PlayerNames::PLAYER_TWO).has_value());
}

class HandsAtTheCitadel: public ::testing::Test
{
protected:
	void SetUp() override
	{
		Locator::entitiesRegistry::emplace<ecs::Registry>();
		auto& registry = Locator::entitiesRegistry::value();
		_hand = registry.Create();
		registry.Assign<Hand>(_hand, false);
		registry.Assign<Transform>(_hand, glm::vec3(0.0f), glm::mat3(1.0f), glm::vec3(1.0f));
		registry.Assign<HandStillness>(_hand);
		registry.Assign<ReportedHand>(_hand);
		_hands.FindAfterLoad();
	}
	void TearDown() override { Locator::entitiesRegistry::reset(); }

	entt::entity MakePlayer(PlayerNames name)
	{
		auto& registry = Locator::entitiesRegistry::value();
		const auto entity = registry.Create();
		registry.Assign<Player>(entity, Player {.name = name});
		return entity;
	}

	static constexpr glm::vec3 k_Citadel {300.0f, 40.0f, 500.0f};
	static constexpr glm::vec3 k_Camera {300.0f, 60.0f, 520.0f};

	ecs::systems::HandSystem _hands;
	entt::entity _hand {entt::null};
};

TEST_F(HandsAtTheCitadel, TheCameraOfThePlayerLooksAtTheCitadelBeforeTheFirstReport)
{
	EXPECT_FALSE(_hands.GetReportedCamera(PlayerNames::PLAYER_ONE).has_value());
	_hands.StartAtCitadel(PlayerNames::PLAYER_ONE, k_Citadel);
	const auto camera = _hands.GetReportedCamera(PlayerNames::PLAYER_ONE);
	ASSERT_TRUE(camera.has_value());
	EXPECT_EQ(camera->origin, k_Camera);
	EXPECT_EQ(camera->focus, k_Citadel);
	// The point of the hand comes only from the reports
	EXPECT_FALSE(_hands.GetPlayerHandPositions()[0].has_value());
}

TEST_F(HandsAtTheCitadel, TheFirstTurnMeasuresTheMovementOfTheHandFromTheCitadel)
{
	// With no report the hand goes from the citadel to the origin, which is a movement
	_hands.StartAtCitadel(PlayerNames::PLAYER_ONE, k_Citadel);
	_hands.ProcessTurn();
	EXPECT_EQ(_hands.GetHandStillTurnsOf(PlayerNames::PLAYER_ONE), 0u);
	const auto& stillness = Locator::entitiesRegistry::value().Get<const HandStillness>(_hand);
	EXPECT_EQ(stillness.velocity, (glm::vec3(0.0f) - k_Citadel) * 10.0f * 0.6f);
	// A report at the citadel is not a movement
	_hands.StartAtCitadel(PlayerNames::PLAYER_ONE, k_Citadel);
	_hands.Report(PlayerNames::PLAYER_ONE,
	              {.collidePoint = k_Citadel, .handPoint = k_Citadel, .camera = k_Camera, .focus = k_Citadel});
	_hands.ProcessTurn();
	EXPECT_EQ(_hands.GetHandStillTurnsOf(PlayerNames::PLAYER_ONE), 1u);
}

TEST_F(HandsAtTheCitadel, AReportMovesTheHandAndTheCameraFromTheCitadel)
{
	_hands.StartAtCitadel(PlayerNames::PLAYER_ONE, k_Citadel);
	_hands.Report(
	    PlayerNames::PLAYER_ONE,
	    {.collidePoint = glm::vec3(1.0f), .handPoint = glm::vec3(2.0f), .camera = glm::vec3(3.0f), .focus = glm::vec3(4.0f)});
	const auto camera = _hands.GetReportedCamera(PlayerNames::PLAYER_ONE);
	ASSERT_TRUE(camera.has_value());
	EXPECT_EQ(camera->origin, glm::vec3(3.0f));
	EXPECT_EQ(camera->focus, glm::vec3(4.0f));
}

TEST_F(HandsAtTheCitadel, TheHandsThatTheGameMakesAfterTheCitadelStartAtTheCitadel)
{
	const auto player = MakePlayer(PlayerNames::PLAYER_THREE);
	MakePlayer(PlayerNames::PLAYER_TWO);
	_hands.StartAtCitadel(PlayerNames::PLAYER_THREE, k_Citadel);
	auto& registry = Locator::entitiesRegistry::value();
	ASSERT_TRUE(registry.AllOf<HandStart>(player));
	EXPECT_EQ(registry.Get<const HandStart>(player).point, k_Citadel);

	const auto three = _hands.MakeHandsOf(PlayerNames::PLAYER_THREE);
	const auto camera = _hands.GetReportedCamera(PlayerNames::PLAYER_THREE);
	ASSERT_TRUE(camera.has_value());
	EXPECT_EQ(camera->origin, k_Camera);
	EXPECT_EQ(registry.Get<const HandStillness>(three[0]).last, k_Citadel);
	// Only the left hand has reports
	EXPECT_EQ(registry.Get<const ReportedHand>(three[1]).camera, glm::vec3(0.0f));
	// A player with no citadel has no start, and the first player keeps its own hands as they are
	std::ignore = _hands.MakeHandsOf(PlayerNames::PLAYER_TWO);
	EXPECT_FALSE(_hands.GetReportedCamera(PlayerNames::PLAYER_TWO).has_value());
	EXPECT_FALSE(_hands.GetReportedCamera(PlayerNames::PLAYER_ONE).has_value());
}

TEST_F(HandsAtTheCitadel, ANewCitadelMovesTheStart)
{
	const auto player = MakePlayer(PlayerNames::PLAYER_TWO);
	_hands.StartAtCitadel(PlayerNames::PLAYER_TWO, k_Citadel);
	_hands.StartAtCitadel(PlayerNames::PLAYER_TWO, {10.0f, 0.0f, 10.0f});
	EXPECT_EQ(Locator::entitiesRegistry::value().Get<const HandStart>(player).point, glm::vec3(10.0f, 0.0f, 10.0f));
}

TEST_F(HandsAtTheCitadel, TheCameraOfAComputerMindLooksAtTheCitadel)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto before = MakePlayer(PlayerNames::PLAYER_TWO);
	registry.Assign<ComputerPlayer>(before);
	_hands.StartAtCitadel(PlayerNames::PLAYER_TWO, k_Citadel);
	EXPECT_EQ(registry.Get<const ComputerPlayer>(before).hand.camera, k_Camera);
}

TEST_F(HandsAtTheCitadel, AComputerMindThatTheGameMakesAfterTheCitadelLooksAtTheCitadel)
{
	const auto player = MakePlayer(PlayerNames::PLAYER_FOUR);
	_hands.StartAtCitadel(PlayerNames::PLAYER_FOUR, k_Citadel);
	ecs::systems::ComputerPlayerSystem minds;
	auto& registry = Locator::entitiesRegistry::value();
	EXPECT_FALSE(registry.AllOf<ComputerPlayer>(player));
	// A script command for the mind makes the mind
	minds.SetCreatureLike(PlayerNames::PLAYER_FOUR, PlayerNames::PLAYER_ONE);
	ASSERT_TRUE(registry.AllOf<ComputerPlayer>(player));
	EXPECT_EQ(registry.Get<const ComputerPlayer>(player).hand.camera, k_Camera);
}

namespace
{
/// The players of the test: a person plays only the players that the test names
class FakeGods final: public ecs::systems::PlayerSystemInterface
{
public:
	void RegisterPlayers() override {}
	void AddPlayer(entt::entity /*playerEntity*/) override {}
	[[nodiscard]] std::optional<entt::entity> FindPlayer(PlayerNames name) const override
	{
		const auto found = entities.find(name);
		return found != entities.end() ? std::optional(found->second) : std::nullopt;
	}
	entt::entity EnsurePlayer(PlayerNames /*name*/) override { return entt::null; }
	void AddCreature(entt::entity /*creature*/) override {}
	[[nodiscard]] std::optional<entt::entity> GetPrimaryCreature(PlayerNames /*name*/) const override { return std::nullopt; }
	void KeepForNextLand() override {}
	void ForgetKept() override {}
	[[nodiscard]] bool KeepAlignmentAt(uint32_t /*turn*/) override { return false; }
	void TakeUpKept(entt::entity /*playerEntity*/) override {}
	[[nodiscard]] bool IsComputerPlayer(PlayerNames name) const override { return !IsHumanPlayer(name); }
	[[nodiscard]] bool IsHumanPlayer(PlayerNames name) const override { return name == PlayerNames::PLAYER_ONE; }

	std::map<PlayerNames, entt::entity> entities;
};

class GodHandsInTheTurn: public ::testing::Test
{
protected:
	void SetUp() override
	{
		Locator::entitiesRegistry::emplace<ecs::Registry>();
		Locator::playerSystem::emplace<FakeGods>();
		auto& registry = Locator::entitiesRegistry::value();
		_god = registry.Create();
		registry.Assign<Player>(_god, Player {.name = PlayerNames::PLAYER_TWO});
		dynamic_cast<FakeGods&>(Locator::playerSystem::value()).entities[PlayerNames::PLAYER_TWO] = _god;
	}
	void TearDown() override
	{
		Locator::playerSystem::reset();
		Locator::entitiesRegistry::reset();
	}

	entt::entity _god {entt::null};
};
} // namespace

TEST_F(GodHandsInTheTurn, TheWorldReadsTheHandBeforeTheStepAndTheReportGivesTheHandAfterIt)
{
	using ecs::PlayerHandOf;
	using ecs::PlayerReportedPointOf;
	auto& registry = Locator::entitiesRegistry::value();
	auto& mind = registry.Assign<ComputerPlayer>(_god);
	// Before the first step the god reports the origin of the world
	EXPECT_EQ(PlayerReportedPointOf(PlayerNames::PLAYER_TWO), std::optional(glm::vec3(0.0f)));
	// The step moved the hand from where the world has it to where the god reports it
	registry.Assign<HandStillness>(_god).last = {10.0f, 5.0f, 20.0f};
	mind.hand.reported = glm::vec3 {110.0f, 5.0f, 20.0f};
	EXPECT_EQ(PlayerHandOf(PlayerNames::PLAYER_TWO), std::optional(glm::vec3(10.0f, 5.0f, 20.0f)));
	EXPECT_EQ(PlayerReportedPointOf(PlayerNames::PLAYER_TWO), std::optional(glm::vec3(110.0f, 5.0f, 20.0f)));
	// The neutral player has no hand
	EXPECT_FALSE(PlayerHandOf(PlayerNames::NEUTRAL).has_value());
	EXPECT_FALSE(PlayerReportedPointOf(PlayerNames::NEUTRAL).has_value());
}
