/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The intro opcodes: the scripts' release of a
// script container (ReleaseControlFromScript: the disband and the deletion of a created flock at a task's end,
// the members' references of one not controlled) and the vagrants' list (the villagers' ReleaseFromScript).

#define LOCATOR_IMPLEMENTATIONS

#include <memory>
#include <vector>

#include <entt/entity/entity.hpp>
#include <glm/mat3x3.hpp>
#include <glm/vec3.hpp>
#include <gtest/gtest.h>
#include <spdlog/sinks/null_sink.h>
#include <spdlog/spdlog.h>

#include "ECS/Components/Transform.h"
#include "ECS/Components/Villager.h"
#include "ECS/Flocks.h"
#include "ECS/Registry.h"
#include "ECS/ScriptContainers.h"
#include "ECS/ScriptHeld.h"
#include "ECS/Town/TownVillagers.h"
#include "InfoConstants.h"
#include "Locator.h"
#include "support/TestServices.h"
#include "support/WorldSystems.h"

using namespace openblack;
using namespace openblack::ecs::components;
namespace flocks = openblack::ecs::flocks;
namespace held = openblack::ecs::script_held;

namespace
{
class ReleaseTest: public ::testing::Test
{
protected:
	void SetUp() override
	{
		test::EmplaceMapAndVillagerDefaults();
		// the scripts' logger (ScriptContainers / ScriptHeld log their errors there): a null sink, as in
		// test_interface_interaction; without one SPDLOG_LOGGER_ERROR dereferences a null logger
		if (!spdlog::get("scripting"))
		{
			spdlog::create<spdlog::sinks::null_sink_mt>("scripting");
		}
		Locator::infoConstants::reset(std::make_unique<InfoConstants>().release());
		Locator::entitiesRegistry::emplace<ecs::Registry>();
		openblack::test::EmplaceWorldSystems();
		// entity 0 is "no thing" for a script id (0 -> none), as the game has made
		// other things before: the test's first entity must not be a scripted one
		Locator::entitiesRegistry::value().Create();
		ecs::town_villagers::ClearVagrants();
	}

	void TearDown() override
	{
		ecs::town_villagers::ClearVagrants();
		Locator::entitiesRegistry::reset();
		openblack::test::ResetWorldSystems();
		test::ResetMapAndVillagerDefaults();
		Locator::infoConstants::reset();
	}

	static ecs::Registry& Reg() { return Locator::entitiesRegistry::value(); }

	static entt::entity MakeVillager()
	{
		const auto e = Reg().Create();
		auto& v = Reg().Assign<Villager>(e);
		v.town = entt::null;
		v.abode = entt::null;
		Reg().Assign<Transform>(e, glm::vec3(0.0f), glm::mat3(1.0f), glm::vec3(1.0f));
		return e;
	}
};
} // namespace

TEST_F(ReleaseTest, VagrantsHeadFirstOnce)
{
	const auto a = MakeVillager();
	const auto b = MakeVillager();
	EXPECT_TRUE(ecs::town_villagers::AddToVagrants(a));
	EXPECT_TRUE(ecs::town_villagers::AddToVagrants(b));
	EXPECT_FALSE(ecs::town_villagers::AddToVagrants(a));
	EXPECT_EQ(ecs::town_villagers::Vagrants(), (std::vector<entt::entity> {b, a}));
}

TEST_F(ReleaseTest, ACreatedFlockGoesAtTheTasksEnd)
{
	const auto f = ecs::script_containers::CreateFlock(glm::vec3(0.0f));
	const auto a = MakeVillager();
	const auto b = MakeVillager();
	flocks::AddMember(f, a);
	flocks::AddMember(f, b);
	// a script variable takes it (controlled: it was created) and lets it go
	held::IncrementReference(f);
	EXPECT_TRUE(held::IsControlledByScript(f));
	held::DecrementReference(f);
	// ReleaseControlFromScript(f, id, 0): disbanded (the villagers only removed), then marked to be deleted
	held::Process();
	EXPECT_FALSE(Reg().Valid(f));
	EXPECT_TRUE(flocks::FlockOf(a) == entt::null);
	EXPECT_TRUE(flocks::FlockOf(b) == entt::null);
}

TEST_F(ReleaseTest, ReleaseFromScriptKeepsTheFlock)
{
	const auto f = ecs::script_containers::CreateFlock(glm::vec3(0.0f));
	const auto a = MakeVillager();
	flocks::AddMember(f, a);
	held::IncrementReference(f);
	// RELEASE_FROM_SCRIPT: its controlled members first (none), then the flock: no longer controlled, not deleted
	held::ReleaseFromScript(f);
	EXPECT_FALSE(held::IsControlledByScript(f));
	EXPECT_TRUE(Reg().Valid(f));
	EXPECT_TRUE(flocks::FlockOf(a) == f);
	// not controlled: nothing more
	held::ReleaseFromScript(f);
	EXPECT_TRUE(Reg().Valid(f));
}
