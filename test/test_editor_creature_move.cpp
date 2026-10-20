/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <stdexcept>
#include <utility>
#include <vector>

#include <glm/vec3.hpp>
#include <gtest/gtest.h>

#include "3D/MapCoords.h"
#include "Creature/CreatureRoute.h"
#include "ECS/Components/Animal.h"
#include "ECS/Components/CreatureLocomotion.h"
#include "ECS/Components/Transform.h"
#include "ECS/Registry.h"
#include "ECS/Systems/CreatureLocomotionSystemInterface.h"
#include "Editor/EditorEntities.h"
#include "Locator.h"

using namespace openblack;
using namespace openblack::ecs;
using namespace openblack::ecs::components;
using namespace openblack::ecs::systems;

namespace
{
/// The legs of a creature. They record each move without a walk and each stop.
class PlacedLegs final: public CreatureLocomotionSystemInterface
{
public:
	void ProcessTurn() override {}
	void Update(float) override {}
	void ShowTurnEnd(entt::entity) override {}
	MoveResult MoveTo(entt::entity, glm::vec2, Pace, float, float) override { return MoveResult::Busy; }
	MoveResult LeadTo(entt::entity, glm::vec2, float, float) override { return MoveResult::Busy; }
	MoveResult MoveToObject(entt::entity, entt::entity, Pace, float) override { return MoveResult::Busy; }
	MoveResult Follow(entt::entity, entt::entity, float, Pace) override { return MoveResult::Busy; }
	MoveResult FleeFrom(entt::entity, glm::vec2, Pace) override { return MoveResult::Busy; }
	bool TurnToFace(entt::entity, glm::vec2) override { return false; }
	bool TurnToHeading(entt::entity, GameHeading) override { return false; }
	bool TurnToFaceAtOnce(entt::entity, glm::vec2) override { return false; }
	bool TurnToHeadingAtOnce(entt::entity, GameHeading) override { return false; }
	void Stop(entt::entity creature) override { stopped.push_back(creature); }
	void StopAtOnce(entt::entity creature) override { stopped.push_back(creature); }
	void ResetPace(entt::entity) override {}
	void Place(entt::entity, glm::vec3) override {}
	void PlaceWithoutWalking(entt::entity creature, glm::vec3 position) override { placed.emplace_back(creature, position); }
	[[nodiscard]] bool IsMoving(entt::entity) const override { return false; }
	[[nodiscard]] float KeptFraction(entt::entity) const override { return 0.0f; }
	[[nodiscard]] const creature_route::WalkableLand& GetWalkableLand() override
	{
		throw std::logic_error("a move without a walk does not ask for the walkable land");
	}
	[[nodiscard]] std::vector<creature_route::Circle> ThingCirclesIn(entt::entity, glm::ivec2) override { return {}; }

	std::vector<std::pair<entt::entity, glm::vec3>> placed;
	std::vector<entt::entity> stopped;
};

class EditorCreatureMove: public ::testing::Test
{
protected:
	void SetUp() override
	{
		Locator::entitiesRegistry::emplace<Registry>();
		legs = &static_cast<PlacedLegs&>(Locator::creatureLocomotionSystem::emplace<PlacedLegs>());
		auto& registry = Locator::entitiesRegistry::value();
		creature = registry.Create();
		registry.Assign<Transform>(creature, glm::vec3(10.0f, 2.0f, 20.0f), glm::mat3(1.0f), glm::vec3(1.0f));
		registry.Assign<CreatureLocomotion>(creature);
	}
	void TearDown() override
	{
		Locator::creatureLocomotionSystem::reset();
		Locator::entitiesRegistry::reset();
	}

	PlacedLegs* legs {nullptr};
	entt::entity creature {entt::null};
};
} // namespace

// The editor and the inspector move a creature as the game moves one without a walk: its walk, look and pose start
// again at the new place, so it does not walk on from the old place
TEST_F(EditorCreatureMove, MovesACreatureAsTheGameDoesWithoutAWalk)
{
	const glm::vec3 to {40.0f, 3.0f, 60.0f};
	editor::MoveTo(creature, to);

	ASSERT_EQ(legs->placed.size(), 1);
	EXPECT_EQ(legs->placed.front().first, creature);
	EXPECT_EQ(legs->placed.front().second, to);
	EXPECT_TRUE(legs->stopped.empty());
	EXPECT_EQ(Locator::entitiesRegistry::value().Get<Transform>(creature).position, to);
}

TEST_F(EditorCreatureMove, AnAnimalContinuesFromWhereItIsPut)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto sheep = registry.Create();
	registry.Assign<Transform>(sheep, glm::vec3(10.0f, 2.0f, 20.0f), glm::mat3(1.0f), glm::vec3(1.0f));
	auto& before = registry.Assign<Animal>(sheep);
	before.position = {10.0f, 2.0f, 20.0f};
	before.previousPosition = before.position;
	before.height = 1.5f;
	before.move.position = {map_coords::ToFixed(10.0f), map_coords::ToFixed(20.0f)};
	before.move.goal = {map_coords::ToFixed(15.0f), map_coords::ToFixed(25.0f)};

	const glm::vec3 to {40.0f, 3.0f, 60.0f};
	editor::MoveTo(sheep, to);

	const auto& animal = registry.Get<Animal>(sheep);
	EXPECT_EQ(animal.position, to);
	EXPECT_EQ(animal.previousPosition, to);
	EXPECT_EQ(animal.height, 0.0f);
	const glm::ivec2 place {map_coords::ToFixed(40.0f), map_coords::ToFixed(60.0f)};
	EXPECT_EQ(animal.move.position, place);
	EXPECT_EQ(animal.move.goal, place);
	EXPECT_EQ(registry.Get<Transform>(sheep).position, to);
}
