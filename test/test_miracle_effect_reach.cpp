/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <glm/gtc/matrix_transform.hpp>
#include <glm/mat3x3.hpp>
#include <gtest/gtest.h>

#include "ECS/Components/DeadTree.h"
#include "ECS/Components/Field.h"
#include "ECS/Components/FishFarm.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/WorshipSite.h"
#include "ECS/Registry.h"
#include "ECS/Systems/Implementations/ObjectMeasures.h"
#include "ECS/WorshipSites.h"

using namespace openblack;
using namespace openblack::ecs::components;
namespace object_measures = openblack::ecs::systems::object_measures;

namespace
{
/// Things of the world with no models: the reach of their fire and of an area effect on them
class MiracleEffectReach: public ::testing::Test
{
protected:
	entt::entity Made(glm::vec3 position = {100.0f, 3.0f, 200.0f})
	{
		const auto entity = registry.Create();
		registry.Assign<Transform>(entity, position, glm::mat3(1.0f), glm::vec3(1.0f));
		return entity;
	}

	ecs::Registry registry;
};
} // namespace

TEST_F(MiracleEffectReach, AFieldAndAFishFarmReachFiveMetres)
{
	const auto field = Made();
	registry.Assign<Field>(field);
	const auto farm = Made();
	registry.Assign<FishFarm>(farm);
	EXPECT_FLOAT_EQ(object_measures::FireRadius(registry, field), 5.0f);
	EXPECT_FLOAT_EQ(object_measures::FireRadius(registry, farm), 5.0f);
	EXPECT_EQ(object_measures::FireMiddle(registry, field), glm::vec2(100.0f, 200.0f));
}

TEST_F(MiracleEffectReach, AWorshipSiteReachesFourteenMetresFromItsMiddle)
{
	const auto site = Made();
	auto& transform = registry.Get<Transform>(site);
	transform.rotation = glm::mat3(glm::rotate(glm::mat4(1.0f), 0.7f, glm::vec3(0.0f, 1.0f, 0.0f)));
	transform.scale = glm::vec3(1.5f);
	registry.Assign<WorshipSite>(site);
	EXPECT_FLOAT_EQ(object_measures::FireRadius(registry, site), 14.0f);
	EXPECT_EQ(object_measures::FireMiddle(registry, site),
	          ecs::worship_site::CentreOnLand(transform.position, transform.rotation, transform.scale.x));
}

TEST_F(MiracleEffectReach, ADeadTreeWithoutAModelReachesNothingFromWhereItLies)
{
	const auto tree = Made();
	registry.Assign<DeadTree>(tree);
	EXPECT_FLOAT_EQ(object_measures::FireRadius(registry, tree), 0.0f);
	EXPECT_FLOAT_EQ(object_measures::Height(registry, tree), 0.0f);
	EXPECT_EQ(object_measures::FireMiddle(registry, tree), glm::vec2(100.0f, 200.0f));
}

TEST_F(MiracleEffectReach, AThingWithoutAModelHasNoSize)
{
	const auto thing = Made();
	EXPECT_FLOAT_EQ(object_measures::FireRadius(registry, thing), 0.0f);
	EXPECT_FLOAT_EQ(object_measures::Height(registry, thing), 0.0f);
	EXPECT_EQ(object_measures::FireMiddle(registry, thing), glm::vec2(100.0f, 200.0f));
}

TEST_F(MiracleEffectReach, APointOfAModelIsScaledTurnedAndMovedAcrossTheLand)
{
	// A quarter turn round the vertical axis takes the first axis of the model to minus z
	const glm::mat3 turn(glm::vec3(0.0f, 0.0f, -1.0f), glm::vec3(0.0f, 1.0f, 0.0f), glm::vec3(1.0f, 0.0f, 0.0f));
	const auto across = object_measures::PlacedAcrossLand({1.0f, 5.0f, 2.0f}, turn, 2.0f, {10.0f, 7.0f, 20.0f});
	EXPECT_FLOAT_EQ(across.x, 14.0f);
	EXPECT_FLOAT_EQ(across.y, 18.0f);
}

TEST_F(MiracleEffectReach, ThePointAddsTheAxesInTheOrderOfTheGame)
{
	// The game adds the third axis first. Near 1e8 the floats are 8 apart, and a sum half way rounds to the even float.
	const glm::mat3 turn(glm::vec3(1.0f, 0.0f, 0.0f), glm::vec3(1.0f, 1.0f, 0.0f), glm::vec3(1.0f, 0.0f, 1.0f));
	const auto across = object_measures::PlacedAcrossLand({4.0f, 4.0f, 1.0e8f}, turn, 1.0f, {-1.0e8f, 0.0f, 0.0f});
	EXPECT_EQ(across.x, 0.0f);
	// The other order gives 8: 4 and 4 first make 8, which 1e8 then keeps
}
