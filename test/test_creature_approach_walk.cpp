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
#include <optional>
#include <span>
#include <stdexcept>
#include <vector>

#include <glm/mat3x3.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <gtest/gtest.h>

#include "Creature/CreatureObjectActions.h"
#include "ECS/Components/Creature.h"
#include "ECS/Components/CreatureBody.h"
#include "ECS/Components/CreatureObjectAction.h"
#include "ECS/Components/Transform.h"
#include "ECS/Registry.h"
#include "ECS/Systems/CreatureAnimationSystemInterface.h"
#include "ECS/Systems/CreatureLocomotionSystemInterface.h"
#include "Locator.h"

#define LOCATOR_IMPLEMENTATIONS

#include "ECS/Systems/Implementations/CreatureObjectActionSystem.h"

using namespace openblack;
using namespace openblack::ecs;
using namespace openblack::ecs::components;
using namespace openblack::ecs::systems;
using creature_object_actions::Kind;
using creature_object_actions::Status;

namespace
{
/// The legs of a creature whose walk never gets nearer to the thing. The walk always continues, and the creature
/// stays at the same position. The legs count each stop of the walk.
class LegsThatGetNowhere final: public CreatureLocomotionSystemInterface
{
public:
	void ProcessTurn() override {}
	void Update(float) override {}
	void ShowTurnEnd(entt::entity) override {}
	MoveResult MoveTo(entt::entity, glm::vec2, Pace, float, float) override { return MoveResult::Started; }
	MoveResult LeadTo(entt::entity, glm::vec2, float, float) override { return MoveResult::Started; }
	MoveResult MoveToObject(entt::entity, entt::entity, Pace, float) override { return MoveResult::Started; }
	MoveResult Follow(entt::entity, entt::entity, float, Pace) override { return MoveResult::Started; }
	MoveResult FleeFrom(entt::entity, glm::vec2, Pace) override { return MoveResult::Started; }
	bool TurnToFace(entt::entity, glm::vec2) override { return false; }
	bool TurnToHeading(entt::entity, GameHeading) override { return false; }
	bool TurnToFaceAtOnce(entt::entity, glm::vec2) override { return false; }
	bool TurnToHeadingAtOnce(entt::entity, GameHeading) override { return false; }
	void Stop(entt::entity) override { ++stops; }
	void StopAtOnce(entt::entity) override { ++stops; }
	void ResetPace(entt::entity) override {}
	void Place(entt::entity, glm::vec3) override {}
	void PlaceWithoutWalking(entt::entity, glm::vec3) override {}
	[[nodiscard]] bool IsMoving(entt::entity) const override { return true; }
	[[nodiscard]] float KeptFraction(entt::entity) const override { return 0.0f; }
	[[nodiscard]] const creature_route::WalkableLand& GetWalkableLand() override
	{
		throw std::logic_error("a walk that continues does not ask for the walkable land");
	}
	[[nodiscard]] std::vector<creature_route::Circle> ThingCirclesIn(entt::entity, glm::ivec2) override { return {}; }

	uint32_t stops {0};
};

/// Animations that give no measure. The walk does not need them.
class NoAnimations final: public CreatureAnimationSystemInterface
{
public:
	void ProcessTurn() override {}
	void Update(std::chrono::duration<float, std::milli>) override {}
	void PoseTurn(entt::entity) override {}
	void PoseTurns() override {}
	void HoldTurnPose(entt::entity) override {}
	void PoseCopy(Registry&, entt::entity, float, const Registry&) override {}
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
};

/// A creature that walks to a thing to pick it up, and never gets nearer to it
class CreatureApproachWalkTest: public ::testing::Test
{
protected:
	void SetUp() override
	{
		Locator::entitiesRegistry::emplace<Registry>();
		legs = &static_cast<LegsThatGetNowhere&>(Locator::creatureLocomotionSystem::emplace<LegsThatGetNowhere>());
		Locator::creatureAnimationSystem::emplace<NoAnimations>();
		auto& registry = Locator::entitiesRegistry::value();
		creature = registry.Create();
		thing = registry.Create();
		registry.Assign<Creature>(creature);
		registry.Assign<Transform>(
		    creature,
		    Transform {.position = glm::vec3(10.0f, 0.0f, 20.0f), .rotation = glm::mat3(1.0f), .scale = glm::vec3(1.0f)});
		registry.Assign<CreatureAnimation>(creature);
		registry.Assign<CreatureObjectAction>(creature, CreatureObjectAction {.kind = Kind::PickUp,
		                                                                      .phase = CreatureObjectAction::Phase::Approach,
		                                                                      .status = Status::Running,
		                                                                      .target = thing});
	}
	void TearDown() override
	{
		Locator::creatureAnimationSystem::reset();
		Locator::creatureLocomotionSystem::reset();
		Locator::entitiesRegistry::reset();
	}

	CreatureObjectActionSystem hands;
	LegsThatGetNowhere* legs {nullptr};
	entt::entity creature {entt::null};
	entt::entity thing {entt::null};
};
} // namespace

// As in the game, a walk to pick up a thing has no turn limit. A walk that gets nowhere continues for ever.
// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables): external macro
TEST_F(CreatureApproachWalkTest, AWalkThatGetsNowhereContinuesPastFiftyTurns)
{
	constexpr uint32_t k_Turns = 1000;
	for (uint32_t turn = 0; turn < k_Turns; ++turn)
	{
		hands.CreatureTurn(creature);
		const auto& action = Locator::entitiesRegistry::value().Get<const CreatureObjectAction>(creature);
		ASSERT_EQ(action.status, Status::Running) << turn;
		ASSERT_EQ(action.phase, CreatureObjectAction::Phase::Approach) << turn;
	}
	EXPECT_EQ(legs->stops, 0u);
}
