/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The age of an animal as its needs see it: Animal::ProcessNeeds, Cow::ReactToAnimalNeeds and Animal::IsChild ask
// Living::GetAge, (turn - birth turn) / 1500, every time.

#define LOCATOR_IMPLEMENTATIONS

#include <cstdint>

#include <entt/entity/entity.hpp>
#include <glm/mat3x3.hpp>
#include <glm/vec3.hpp>
#include <gtest/gtest.h>

#include "ECS/AnimalAIDetail.h"
#include "ECS/Components/Animal.h"
#include "ECS/Components/AnimalBrain.h"
#include "ECS/Components/Flock.h"
#include "ECS/Components/Transform.h"
#include "ECS/Registry.h"
#include "Enums.h"
#include "GameClock.h"
#include "InfoConstants.h"
#include "Locator.h"
#include "support/WorldSystems.h"

using namespace openblack;
using namespace openblack::ecs::components;
namespace ai = openblack::ecs::animal_ai::detail;

namespace
{
constexpr uint32_t k_Created = 100000;

/// info.dat, the cow's GAnimalInfo: grownUpAge 13, hunger 50, sleep 1000, needToBreed 3000
GAnimalInfo CowInfo()
{
	GAnimalInfo info {};
	info.grownUpAge = 13;
	info.hunger = 50;
	info.sleep = 1000;
	info.needToBreed = 3000;
	return info;
}

class AnimalAgeTest: public ::testing::Test
{
protected:
	void SetUp() override
	{
		Locator::entitiesRegistry::emplace<ecs::Registry>();
		openblack::test::EmplaceWorldSystems();
		game_clock::SetTurn(k_Created);
	}

	void TearDown() override
	{
		game_clock::SetTurn(0);
		Locator::entitiesRegistry::reset();
		openblack::test::ResetWorldSystems();
	}

	static ecs::Registry& Reg() { return Locator::entitiesRegistry::value(); }
};
} // namespace

TEST_F(AnimalAgeTest, AYoungAnimalGrowsUp)
{
	const auto info = CowInfo();
	auto& registry = Reg();
	const auto flock = registry.Create();
	const auto a = registry.Create();
	const auto b = registry.Create();
	// a herd of two with room for a third: the breeding tests pass but for the age
	auto& herd = registry.Assign<Flock>(flock);
	herd.members = {a, b};
	herd.maxMembers = 3;
	// made aged 12: Living::SetAge, birth turn = turn - 12 x 1500; the component keeps the 12 it was made with
	auto& animal = registry.Assign<Animal>(a, AnimalInfo::Cow, 12u, flock, true);
	auto& brain = registry.Assign<AnimalBrain>(a);
	brain.birthTurn = static_cast<int32_t>(k_Created) - 12 * 1500;
	auto& transform = registry.Assign<Transform>(a, glm::vec3(0.0f), glm::mat3(1.0f), glm::vec3(1.0f));
	ai::Context ctx {a, animal, brain, transform, info};

	// 1499 turns on: (12 x 1500 + 1499) / 1500 = 12, still young for the three readers
	game_clock::SetTurn(k_Created + 1499);
	EXPECT_EQ(ai::AgeOf(brain), 12u);
	EXPECT_TRUE(ai::IsChild(ctx));
	ai::ProcessNeeds(ctx);
	EXPECT_EQ(brain.breed, 0);
	EXPECT_EQ(ai::CowReactToAnimalNeeds(ctx), ai::k_Nothing);
	EXPECT_EQ(brain.breed, 0);

	// 1500 turns on: 13 = grownUpAge, an adult for the three readers
	game_clock::SetTurn(k_Created + 1500);
	EXPECT_EQ(ai::AgeOf(brain), 13u);
	EXPECT_FALSE(ai::IsChild(ctx));
	ai::ProcessNeeds(ctx);
	EXPECT_EQ(brain.breed, 1);
	EXPECT_EQ(ai::CowReactToAnimalNeeds(ctx), ai::k_Nothing);
	EXPECT_EQ(brain.breed, 2);
	// the stored copy is not what they read
	EXPECT_EQ(animal.age, 12u);
}
