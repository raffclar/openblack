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
#include "ECS/Components/Abode.h"
#include "ECS/Components/CreatureMind.h"
#include "ECS/Components/Dance.h"
#include "ECS/Components/ScriptControl.h"
#include "ECS/Components/Town.h"
#include "ECS/Components/Villager.h"
#include "ECS/Registry.h"
#include "ECS/ScriptFlocks.h"
#include "ECS/Systems/ScriptObjectsSystemInterface.h"
#include "Locator.h"
#include "ScriptHeaders/ScriptEnums.h"

using namespace openblack;

namespace
{
/// Every object a script names is itself
class ContainerPassThroughScriptObjects final: public ecs::systems::ScriptObjectsSystemInterface
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

constexpr uint32_t k_SetScriptState = 17;
constexpr uint32_t k_SetScriptUlong = 20;
constexpr uint32_t k_GetFirstInContainer = 455;
constexpr uint32_t k_GetNextInContainer = 456;
constexpr uint32_t k_GetProperty = 21;
constexpr uint32_t k_SetProperty = 22;

class ScriptContainerProperties: public ::testing::Test
{
protected:
	void SetUp() override
	{
		Locator::entitiesRegistry::emplace();
		Locator::scriptObjects::emplace<ContainerPassThroughScriptObjects>();
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

	static Results Call(uint32_t native, const std::vector<lhvm::NativeArgument>& arguments)
	{
		Results results;
		EXPECT_TRUE(Locator::vm::value().CallNative(native, arguments, results));
		return results;
	}

	static lhvm::NativeArgument Object(entt::entity entity)
	{
		return std::pair(lhvm::VMValue(static_cast<uint32_t>(entity)), lhvm::DataType::Object);
	}

	static lhvm::NativeArgument Float(float value) { return std::pair(lhvm::VMValue(value), lhvm::DataType::Float); }

	static lhvm::NativeArgument Int(int32_t value) { return std::pair(lhvm::VMValue(value), lhvm::DataType::Int); }

	static lhvm::NativeArgument Property(script::ObjectPropertyType prop) { return Int(static_cast<int32_t>(prop)); }

	static entt::entity CreateVillager()
	{
		auto& registry = Locator::entitiesRegistry::value();
		const auto entity = registry.Create();
		registry.Assign<ecs::components::Villager>(entity);
		return entity;
	}

	static float LifeOf(entt::entity villager)
	{
		return Locator::entitiesRegistry::value().Get<const ecs::components::Villager>(villager).life;
	}

	/// A town with one house of two people and one person with no home
	struct TestTown
	{
		entt::entity town;
		std::vector<entt::entity> villagers;
	};
	static TestTown CreateTown()
	{
		auto& registry = Locator::entitiesRegistry::value();
		TestTown made {.town = registry.Create(), .villagers = {CreateVillager(), CreateVillager(), CreateVillager()}};
		const auto house = registry.Create();
		auto& abode = registry.Assign<ecs::components::Abode>(house);
		abode.inhabitants = {made.villagers[0], made.villagers[1]};
		auto& town = registry.Assign<ecs::components::Town>(made.town);
		town.abodes = {house};
		town.homelessVillagers = {made.villagers[2]};
		return made;
	}

	static float AskProperty(entt::entity object, script::ObjectPropertyType prop)
	{
		const auto results = Call(k_GetProperty, {Property(prop), Object(object)});
		EXPECT_EQ(results.size(), 1u);
		return results.empty() ? -1.0f : results.front().first.floatVal;
	}

	chlapi::CHLApi _api;
};
} // namespace

// The healer's town has a health of 0 when the healer goes: each of its villagers has no life
// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables): external macro
TEST_F(ScriptContainerProperties, AHealthSetOnATownGoesToEachOfItsVillagers)
{
	const auto made = CreateTown();
	Call(k_SetProperty, {Property(script::ObjectPropertyType::Health), Object(made.town), Float(0.0f)});
	for (const auto villager : made.villagers)
	{
		EXPECT_EQ(LifeOf(villager), 0.0f);
		// The property does not take control of the villagers, so they die at their next check
		EXPECT_FALSE(Locator::entitiesRegistry::value().AllOf<ecs::components::ScriptControlled>(villager));
	}
	Call(k_SetProperty, {Property(script::ObjectPropertyType::Health), Object(made.town), Float(1.0f)});
	for (const auto villager : made.villagers)
	{
		EXPECT_EQ(LifeOf(villager), 1.0f);
	}
}

// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables): external macro
TEST_F(ScriptContainerProperties, AHealthSetOnAFlockGoesToEachMember)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto flock = ecs::script_flocks::Create(registry, map_coords::MapCoords {});
	const auto first = CreateVillager();
	const auto second = CreateVillager();
	ASSERT_TRUE(ecs::script_flocks::AddLiving(registry, flock, first));
	ASSERT_TRUE(ecs::script_flocks::AddLiving(registry, flock, second));
	const auto outside = CreateVillager();
	Call(k_SetProperty, {Property(script::ObjectPropertyType::Health), Object(flock), Float(0.25f)});
	EXPECT_EQ(LifeOf(first), 0.25f);
	EXPECT_EQ(LifeOf(second), 0.25f);
	EXPECT_EQ(LifeOf(outside), 1.0f);
}

// A script reads a property of a container as 0, whatever its members have; only its player is its own
// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables): external macro
TEST_F(ScriptContainerProperties, AContainerAnswersNoneForItsProperties)
{
	const auto made = CreateTown();
	auto& registry = Locator::entitiesRegistry::value();
	const auto flock = ecs::script_flocks::Create(registry, map_coords::MapCoords {});
	ASSERT_TRUE(ecs::script_flocks::AddLiving(registry, flock, CreateVillager()));
	for (const auto container : {made.town, flock})
	{
		EXPECT_EQ(AskProperty(container, script::ObjectPropertyType::Health), 0.0f);
		EXPECT_EQ(AskProperty(container, script::ObjectPropertyType::Age), 0.0f);
		EXPECT_EQ(AskProperty(container, script::ObjectPropertyType::Speed), 0.0f);
		EXPECT_EQ(AskProperty(container, script::ObjectPropertyType::Scale), 0.0f);
	}
	EXPECT_EQ(_api.GetStubCalls().Total(), 0u);
}

// The script takes control of each member of a group that it gives a clip. Only a villager gets the clip.
// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables): external macro
TEST_F(ScriptContainerProperties, AClipGivenToAFlockTakesControlOfEachMember)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto flock = ecs::script_flocks::Create(registry, map_coords::MapCoords {});
	const auto villager = CreateVillager();
	const auto creature = registry.Create();
	registry.Assign<ecs::components::CreatureMindState>(creature);
	ASSERT_TRUE(ecs::script_flocks::AddLiving(registry, flock, villager));
	ASSERT_TRUE(ecs::script_flocks::AddLiving(registry, flock, creature));
	Call(k_SetScriptUlong, {Object(flock), Int(343), Int(1)});
	EXPECT_TRUE(registry.AllOf<ecs::components::ScriptControlled>(villager));
	EXPECT_TRUE(registry.AllOf<ecs::components::ScriptControlled>(creature));
	EXPECT_FALSE(registry.AllOf<ecs::components::ScriptControlled>(flock));
	const auto& play = registry.Get<const ecs::components::CreatureMindState>(creature).scriptPlay;
	EXPECT_EQ(play.animation, 0);
	EXPECT_EQ(play.plays, 0u);
	// A creature alone keeps the clip until the script tells it to start
	Call(k_SetScriptUlong, {Object(creature), Int(343), Int(1)});
	EXPECT_EQ(registry.Get<const ecs::components::CreatureMindState>(creature).scriptPlay.animation, 343);
}

// A thing that is not a villager or a creature takes no clip. The game gives a message and this is no gap.
// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables): external macro
TEST_F(ScriptContainerProperties, OnlyALivingThingTakesAClip)
{
	const auto rock = Locator::entitiesRegistry::value().Create();
	EXPECT_TRUE(Call(k_SetScriptUlong, {Object(rock), Int(343), Int(1)}).empty());
	EXPECT_EQ(_api.GetStubCalls().Total(), 0u);
}

// A state given to a group gives each member to the script, also a member that takes no state
// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables): external macro
TEST_F(ScriptContainerProperties, AStateGivenToAFlockTakesControlOfEachMember)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto flock = ecs::script_flocks::Create(registry, map_coords::MapCoords {});
	const auto first = CreateVillager();
	const auto second = CreateVillager();
	ASSERT_TRUE(ecs::script_flocks::AddLiving(registry, flock, first));
	ASSERT_TRUE(ecs::script_flocks::AddLiving(registry, flock, second));
	Call(k_SetScriptState, {Object(flock), Int(5)});
	EXPECT_TRUE(registry.AllOf<ecs::components::ScriptControlled>(first));
	EXPECT_TRUE(registry.AllOf<ecs::components::ScriptControlled>(second));
	EXPECT_FALSE(registry.AllOf<ecs::components::ScriptControlled>(flock));
}

// A script gets the first dancer of the first group of a dance, and never a next thing in a dance
// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables): external macro
TEST_F(ScriptContainerProperties, AScriptGetsOnlyTheFirstDancerOfTheFirstGroup)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto dance = registry.Create();
	auto& made = registry.Assign<ecs::components::Dance>(dance);
	const auto first = CreateVillager();
	const auto second = CreateVillager();
	const auto other = CreateVillager();
	made.groups.all.resize(2);
	made.groups.all[0].dancers = {first, second};
	made.groups.all[1].dancers = {other};
	auto results = Call(k_GetFirstInContainer, {Object(dance)});
	ASSERT_EQ(results.size(), 1u);
	EXPECT_EQ(results.front().first.uintVal, static_cast<uint32_t>(first));
	results = Call(k_GetNextInContainer, {Object(dance), Object(first)});
	ASSERT_EQ(results.size(), 1u);
	EXPECT_EQ(results.front().first.uintVal, 0u);
	// The first group has no dancer: the script gets nothing, also with a dancer in a later group
	made.groups.all[0].dancers.clear();
	results = Call(k_GetFirstInContainer, {Object(dance)});
	ASSERT_EQ(results.size(), 1u);
	EXPECT_EQ(results.front().first.uintVal, 0u);
	// A dance with no groups gives nothing
	made.groups.all.clear();
	results = Call(k_GetFirstInContainer, {Object(dance)});
	ASSERT_EQ(results.size(), 1u);
	EXPECT_EQ(results.front().first.uintVal, 0u);
	EXPECT_EQ(_api.GetStubCalls().Total(), 0u);
}
