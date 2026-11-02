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

#include <glm/mat3x3.hpp>
#include <glm/vec3.hpp>
#include <gtest/gtest.h>

#include "3D/CreatureBody.h"
#include "Creature/CreatureReach.h"
#include "Creature/CreatureRig.h"
#include "ECS/Components/Creature.h"
#include "ECS/Components/CreatureBody.h"
#include "ECS/Components/CreatureObjectAction.h"
#include "ECS/Components/Mobile.h"
#include "ECS/Components/Transform.h"
#include "ECS/Registry.h"
#include "ECS/Systems/CreatureAnimationSystemInterface.h"
#include "Locator.h"

#define LOCATOR_IMPLEMENTATIONS

#include "ECS/Systems/Implementations/CreatureObjectActionSystem.h"
#include "Resources/Resources.h"

using namespace openblack;
using namespace openblack::ecs;
using namespace openblack::ecs::components;
using namespace openblack::ecs::systems;

namespace
{
/// Animations in which the finger ends get to the same point in each pick-up animation, and that count the measures
class FingerEndsAtOnePoint final: public CreatureAnimationSystemInterface
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
		++measures;
		return point;
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

	glm::vec3 point {0.0f, 0.0f, -1000.0f};
	uint32_t measures {0};
};

/// A creature with a rig that has action points, and a thing on the land that the hand can give it
class CreatureReachReadOnlyTest: public ::testing::Test
{
protected:
	void SetUp() override
	{
		Locator::entitiesRegistry::emplace<Registry>();
		Locator::resources::emplace<resources::Resources>();
		animations = &static_cast<FingerEndsAtOnePoint&>(Locator::creatureAnimationSystem::emplace<FingerEndsAtOnePoint>());
		auto& registry = Locator::entitiesRegistry::value();
		creature = registry.Create();
		registry.Assign<Creature>(creature);
		registry.Assign<Transform>(creature, glm::vec3(0.0f), glm::mat3(1.0f), glm::vec3(1.0f));
		registry.Assign<CreatureAnimation>(creature);
		const auto species = registry.Get<const Creature>(creature).species;
		Locator::resources::value().GetCreatureRigs().Register(creature::GetRigId(species), [] {
			auto rig = std::make_shared<creature::CreatureRig>();
			rig->actionPoints.emplace();
			rig->actionPoints->pickUpMs = 100.0f;
			return rig;
		});
		thing = registry.Create();
		registry.Assign<Transform>(thing, glm::vec3(5.0f, 0.0f, 0.0f), glm::mat3(1.0f), glm::vec3(1.0f));
		registry.Assign<MobileObject>(thing, MobileObject {.type = MobileObjectInfo::None});
	}
	void TearDown() override
	{
		Locator::creatureAnimationSystem::reset();
		Locator::resources::reset();
		Locator::entitiesRegistry::reset();
	}

	[[nodiscard]] bool Kept() const { return Locator::entitiesRegistry::value().AllOf<PickUpReach>(creature); }

	CreatureObjectActionSystem hands;
	FingerEndsAtOnePoint* animations {nullptr};
	entt::entity creature {entt::null};
	entt::entity thing {entt::null};
};
} // namespace

// The frames of the hand ask whether the creature would take what the hand holds. The answer measures the reach, but no
// frame may change the synced world, so the creature does not keep the measure.
// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables): external macro
TEST_F(CreatureReachReadOnlyTest, TheGiveTestMeasuresTheReachAndDoesNotKeepIt)
{
	EXPECT_TRUE(hands.CanTakeFromHand(creature, thing));
	EXPECT_GT(animations->measures, 0u);
	EXPECT_FALSE(Kept());
	// The next frame measures again, because nothing kept the measure
	const auto first = animations->measures;
	EXPECT_TRUE(hands.CanTakeFromHand(creature, thing));
	EXPECT_EQ(animations->measures, 2 * first);
	EXPECT_FALSE(Kept());
}

// A turn that tests a pick-up keeps the measure, and the give test of a frame then uses it and measures nothing
// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables): external macro
TEST_F(CreatureReachReadOnlyTest, ATurnKeepsTheReachAndTheGiveTestUsesIt)
{
	// The thing is in no cells of the map, so the creature may not pick it up from the land, but the test keeps the reach
	EXPECT_FALSE(hands.CanPickUp(creature, thing));
	ASSERT_TRUE(Kept());
	const auto kept = Locator::entitiesRegistry::value().Get<const PickUpReach>(creature).points;
	const auto measures = animations->measures;
	EXPECT_TRUE(hands.CanTakeFromHand(creature, thing));
	EXPECT_EQ(animations->measures, measures);
	EXPECT_EQ(Locator::entitiesRegistry::value().Get<const PickUpReach>(creature).points, kept);
}

// The give test gives the same answer whether a turn kept the reach or not, for a reach that lifts the thing and for one
// that does not
// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables): external macro
TEST_F(CreatureReachReadOnlyTest, TheGiveTestAnswersTheSameWithOrWithoutAKeptReach)
{
	for (const auto& point : {glm::vec3(0.0f, 0.0f, -1000.0f), glm::vec3(0.0f)})
	{
		animations->point = point;
		Locator::entitiesRegistry::value().Remove<PickUpReach>(creature);
		const bool notKept = hands.CanTakeFromHand(creature, thing);
		ASSERT_FALSE(Kept());
		static_cast<void>(hands.CanPickUp(creature, thing));
		ASSERT_TRUE(Kept());
		EXPECT_EQ(hands.CanTakeFromHand(creature, thing), notKept) << point.z;
	}
}
