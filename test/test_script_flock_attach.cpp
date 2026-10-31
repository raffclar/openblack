/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#define LOCATOR_IMPLEMENTATIONS

#include <cstdint>

#include <utility>
#include <vector>

#include <LHVM.h>
#include <gtest/gtest.h>

#include "3D/MapCoords.h"
#include "CHLApi.h"
#include "ECS/Components/Animal.h"
#include "ECS/Components/Flock.h"
#include "ECS/Components/Town.h"
#include "ECS/Registry.h"
#include "ECS/ScriptFlocks.h"
#include "ECS/Systems/ScriptObjectsSystemInterface.h"
#include "Locator.h"

using namespace openblack;

namespace
{
/// Every object a script names is itself
class AttachPassThroughScriptObjects final: public ecs::systems::ScriptObjectsSystemInterface
{
public:
	bool Register(entt::entity, bool) override { return true; }
	void EnterNative(uint32_t) override {}
	entt::entity Fetch(entt::entity object) override { return object; }
	void AddReference(entt::entity) override {}
	void RemoveReference(entt::entity) override {}
	void ReleaseFromScript(entt::entity) override {}
	void Replace(entt::entity, entt::entity) override {}
	void Reset() override {}
	void ReleaseUnreferenced() override {}
	bool Disband(entt::entity) override { return false; }
	[[nodiscard]] std::vector<ecs::systems::ScriptObjectPlace> Places() const override { return {}; }
	[[nodiscard]] uint32_t TimesFull() const override { return 0; }
	void ClearForNewLand() override {}
};

using Results = std::vector<std::pair<lhvm::VMValue, lhvm::DataType>>;

constexpr uint32_t k_FlockAttach = 37;

class ScriptFlockAttach: public ::testing::Test
{
protected:
	void SetUp() override
	{
		Locator::entitiesRegistry::emplace();
		Locator::scriptObjects::emplace<AttachPassThroughScriptObjects>();
		Locator::vm::emplace();
		Locator::vm::value().Initialise(&_api.GetFunctionsTable(), nullptr, nullptr, nullptr, nullptr, nullptr, nullptr);
		// A script's object 0 is none, so nothing the scripts are given is the registry's first entity
		[[maybe_unused]] const auto none = Locator::entitiesRegistry::value().Create();
	}

	void TearDown() override
	{
		Locator::vm::reset();
		Locator::scriptObjects::reset();
		Locator::entitiesRegistry::reset();
	}

	static Results Attach(entt::entity object, entt::entity target)
	{
		Results results;
		const std::vector<lhvm::NativeArgument> arguments {
		    std::pair(lhvm::VMValue(static_cast<uint32_t>(object)), lhvm::DataType::Object),
		    std::pair(lhvm::VMValue(static_cast<uint32_t>(target)), lhvm::DataType::Object),
		    std::pair(lhvm::VMValue(0), lhvm::DataType::Boolean)};
		EXPECT_TRUE(Locator::vm::value().CallNative(k_FlockAttach, arguments, results));
		return results;
	}

	/// A flock of two animals, as a script's herd
	static std::pair<entt::entity, std::vector<entt::entity>> CreateHerd()
	{
		auto& registry = Locator::entitiesRegistry::value();
		const auto herd = ecs::script_flocks::Create(registry, map_coords::MapCoords {});
		std::vector<entt::entity> animals;
		for (int i = 0; i < 2; ++i)
		{
			const auto animal = registry.Create();
			registry.Assign<ecs::components::Animal>(animal);
			ecs::script_flocks::AddLiving(registry, herd, animal);
			animals.push_back(animal);
		}
		return {herd, animals};
	}

	chlapi::CHLApi _api;
};

TEST_F(ScriptFlockAttach, HerdAttachedToTownStaysAndLeavesAZeroForEachAnimal)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto [herd, animals] = CreateHerd();
	const auto town = registry.Create();
	registry.Assign<ecs::components::Town>(town);

	const auto results = Attach(herd, town);

	// Each animal that is not a villager leaves a 0, then the town comes last
	ASSERT_EQ(results.size(), 3u);
	EXPECT_EQ(results[0].first.uintVal, 0u);
	EXPECT_EQ(results[1].first.uintVal, 0u);
	EXPECT_EQ(results[2].first.uintVal, static_cast<uint32_t>(town));
	for (const auto animal : animals)
	{
		EXPECT_EQ(ecs::script_flocks::FlockOf(registry, animal), herd);
	}
}

TEST_F(ScriptFlockAttach, HerdAttachedToFlockMovesItsMembers)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto [herd, animals] = CreateHerd();
	const auto other = ecs::script_flocks::Create(registry, map_coords::MapCoords {});

	const auto results = Attach(herd, other);

	ASSERT_EQ(results.size(), 1u);
	EXPECT_EQ(results[0].first.uintVal, static_cast<uint32_t>(other));
	for (const auto animal : animals)
	{
		EXPECT_EQ(ecs::script_flocks::FlockOf(registry, animal), other);
	}
}

TEST_F(ScriptFlockAttach, ThingAttachedToALivingIsNotAdded)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto [herd, animals] = CreateHerd();
	const auto living = registry.Create();
	registry.Assign<ecs::components::Animal>(living);

	const auto results = Attach(herd, living);

	ASSERT_EQ(results.size(), 1u);
	EXPECT_EQ(results[0].first.uintVal, 0u);
	EXPECT_EQ(ecs::script_flocks::FlockOf(registry, living), entt::entity {entt::null});
	EXPECT_EQ(ecs::script_flocks::Size(registry, herd), 2u);
}

} // namespace
