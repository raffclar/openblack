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

#include <array>
#include <bit>
#include <limits>
#include <utility>
#include <vector>

#include <LHVM.h>
#include <glm/vec3.hpp>
#include <gtest/gtest.h>

#include "3D/MapCoords.h"
#include "Animals/AnimalMove.h"
#include "CHLApi.h"
#include "ECS/Components/Abode.h"
#include "ECS/Components/Animal.h"
#include "ECS/Components/Flock.h"
#include "ECS/Components/Graveyard.h"
#include "ECS/Components/Indestructible.h"
#include "ECS/Components/MiracleImpression.h"
#include "ECS/Components/Physics.h"
#include "ECS/Components/ScriptControl.h"
#include "ECS/Components/Town.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/Villager.h"
#include "ECS/Registry.h"
#include "ECS/ScriptContainerWalk.h"
#include "ECS/ScriptFlocks.h"
#include "ECS/Systems/ScriptObjectsSystemInterface.h"
#include "Locator.h"
#include "ScriptHeaders/ScriptCastHistory.h"

using namespace openblack;

namespace
{
/// Every object a script names is itself
class PassThroughScriptObjects final: public ecs::systems::ScriptObjectsSystemInterface
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
using Arguments = std::vector<lhvm::NativeArgument>;

constexpr uint32_t k_GetSlowestSpeed = 198;
constexpr uint32_t k_SetSkeleton = 291;
constexpr uint32_t k_IsSkeleton = 292;
constexpr uint32_t k_PlayerSpellCastTime = 293;
constexpr uint32_t k_SetIndestructable = 298;
constexpr uint32_t k_PlayerSpellLastCast = 294;
constexpr uint32_t k_GetLastSpellCastPos = 295;
constexpr uint32_t k_GetObjectFlock = 324;
constexpr uint32_t k_FlockWithinLimits = 333;
constexpr uint32_t k_CallBuildingInTown = 375;
constexpr uint32_t k_SetObjectBeliefScale = 428;

lhvm::NativeArgument Object(entt::entity object)
{
	return std::pair(lhvm::VMValue(static_cast<uint32_t>(object)), lhvm::DataType::Object);
}

lhvm::NativeArgument Float(float value)
{
	return std::pair(lhvm::VMValue(value), lhvm::DataType::Float);
}

lhvm::NativeArgument Int(int32_t value)
{
	return std::pair(lhvm::VMValue(value), lhvm::DataType::Int);
}

lhvm::NativeArgument Bool(bool value)
{
	return std::pair(lhvm::VMValue(value ? 1 : 0), lhvm::DataType::Boolean);
}

class ScriptNativesRows3: public ::testing::Test
{
protected:
	void SetUp() override
	{
		Locator::entitiesRegistry::emplace();
		Locator::scriptObjects::emplace<PassThroughScriptObjects>();
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

	static Results Call(uint32_t native, const Arguments& arguments)
	{
		Results results;
		EXPECT_TRUE(Locator::vm::value().CallNative(native, arguments, results));
		return results;
	}

	static entt::entity Animal(float x, float z, uint16_t speed)
	{
		auto& registry = Locator::entitiesRegistry::value();
		const auto animal = registry.Create();
		registry.Assign<ecs::components::Transform>(animal).position = glm::vec3(x, 0.0f, z);
		registry.Assign<ecs::components::Animal>(animal).move.speed = speed;
		return animal;
	}

	chlapi::CHLApi _api;
};

TEST(ScriptContainerRules, TheSlowestSpeedIsTheLowestAndAnEmptyGroupGivesTheLargestFloat)
{
	EXPECT_EQ(ecs::script_containers::SlowestSpeed({}), std::numeric_limits<float>::max());
	const std::array speeds {0.5f, 0.25f, 0.75f};
	EXPECT_EQ(ecs::script_containers::SlowestSpeed(speeds), 0.25f);
}

TEST(ScriptContainerRules, TheCapacityAddsTheRoomForAdultsAndChildren)
{
	EXPECT_EQ(ecs::script_containers::TownCapacity(12, 7), 19.0f);
	EXPECT_EQ(ecs::script_containers::AbodeCapacity(4, 3), 7.0f);
	// The sum of a building's counts wraps round as an unsigned number
	EXPECT_EQ(ecs::script_containers::AbodeCapacity(0xFFFFFFFFu, 2u), 1.0f);
}

TEST(ScriptContainerRules, ABuildingInATownAgreesInKindBuiltShareAndScriptHold)
{
	using ecs::script_containers::TownBuilding;
	const std::array buildings {
	    TownBuilding {.entity = entt::entity {1}, .number = AbodeNumber::A, .built = 1.0f},
	    TownBuilding {.entity = entt::entity {2}, .number = AbodeNumber::B, .built = 0.5f},
	    TownBuilding {.entity = entt::entity {3}, .number = AbodeNumber::B, .built = 1.0f, .inScript = true},
	    TownBuilding {.entity = entt::entity {4}, .number = AbodeNumber::B, .built = 0.9f},
	};
	EXPECT_EQ(ecs::script_containers::BuildingInTown(buildings, AbodeNumber::B, 0.5f, true), entt::entity {2});
	EXPECT_EQ(ecs::script_containers::BuildingInTown(buildings, AbodeNumber::B, 0.6f, true), entt::entity {4});
	EXPECT_EQ(ecs::script_containers::BuildingInTown(buildings, AbodeNumber::C, 0.0f, true), entt::entity {entt::null});
	// Without the exclusion the game gives the first building of the list, of any kind
	EXPECT_EQ(ecs::script_containers::BuildingInTown(buildings, AbodeNumber::C, 0.0f, false), entt::entity {1});
	EXPECT_EQ(ecs::script_containers::BuildingInTown({}, AbodeNumber::C, 0.0f, false), entt::entity {entt::null});
}

TEST(ScriptCastHistory, TheTimeSinceTheLastCastCountsFromTheStartValueOfMinusOne)
{
	const script::cast_history::LastCast cast {};
	// Turn 0 is one turn after the start value: 100 milliseconds
	EXPECT_EQ(std::bit_cast<uint32_t>(script::cast_history::SecondsSinceCast(0, cast, 100)),
	          std::bit_cast<uint32_t>(100.0f * script::cast_history::k_SecondsPerMillisecond));
	EXPECT_EQ(std::bit_cast<uint32_t>(script::cast_history::SecondsSinceCast(9, cast, 100)),
	          std::bit_cast<uint32_t>(1000.0f * script::cast_history::k_SecondsPerMillisecond));
	EXPECT_EQ(std::bit_cast<uint32_t>(script::cast_history::k_SecondsPerMillisecond), 0x3A83126Fu);
}

TEST_F(ScriptNativesRows3, CastHistoryOfAPlayerThatIsNotThere)
{
	auto time = Call(k_PlayerSpellCastTime, {Float(0.0f)});
	ASSERT_EQ(time.size(), 1u);
	EXPECT_EQ(time[0].first.floatVal, std::numeric_limits<float>::max());
	auto type = Call(k_PlayerSpellLastCast, {Float(0.0f)});
	ASSERT_EQ(type.size(), 1u);
	EXPECT_EQ(type[0].first.intVal, 0);
	auto place = Call(k_GetLastSpellCastPos, {Float(0.0f)});
	ASSERT_EQ(place.size(), 3u);
	for (const auto& [value, kind] : place)
	{
		EXPECT_EQ(value.floatVal, 0.0f);
	}
}

TEST_F(ScriptNativesRows3, TheFlockOfALivingThing)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto flock = ecs::script_flocks::Create(registry, map_coords::MapCoords {});
	const auto member = Animal(0.0f, 0.0f, 0);
	ecs::script_flocks::AddLiving(registry, flock, member);
	const auto alone = Animal(5.0f, 0.0f, 0);
	const auto town = registry.Create();
	registry.Assign<ecs::components::Town>(town);

	EXPECT_EQ(Call(k_GetObjectFlock, {Object(member)})[0].first.uintVal, static_cast<uint32_t>(flock));
	EXPECT_EQ(Call(k_GetObjectFlock, {Object(alone)})[0].first.uintVal, 0u);
	EXPECT_EQ(Call(k_GetObjectFlock, {Object(town)})[0].first.uintVal, 0u);
}

TEST_F(ScriptNativesRows3, AFlockIsWithinLimitsWhileEachMemberIsWithinItsDistanceOfTheLeader)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto flock = ecs::script_flocks::Create(registry, map_coords::MapCoords {});
	const auto leader = Animal(0.0f, 0.0f, 0);
	ecs::script_flocks::AddLeader(registry, flock, leader);
	const auto follower = Animal(29.0f, 0.0f, 0);
	ecs::script_flocks::AddLiving(registry, flock, follower);

	EXPECT_TRUE(Call(k_FlockWithinLimits, {Object(flock)})[0].first.intVal != 0);
	registry.Get<ecs::components::Transform>(follower).position.x = 31.0f;
	EXPECT_FALSE(Call(k_FlockWithinLimits, {Object(flock)})[0].first.intVal != 0);
	// A thing that is not a flock gives true
	EXPECT_TRUE(Call(k_FlockWithinLimits, {Object(follower)})[0].first.intVal != 0);
}

TEST_F(ScriptNativesRows3, TheSlowestSpeedOfAFlock)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto flock = ecs::script_flocks::Create(registry, map_coords::MapCoords {});
	ecs::script_flocks::AddLiving(registry, flock, Animal(0.0f, 0.0f, 6554));
	ecs::script_flocks::AddLiving(registry, flock, Animal(1.0f, 0.0f, 3277));
	const auto empty = ecs::script_flocks::Create(registry, map_coords::MapCoords {});

	const auto slowest = Call(k_GetSlowestSpeed, {Object(flock)})[0].first.floatVal;
	EXPECT_EQ(slowest, animals::SpeedForScript(3277, false));
	EXPECT_EQ(Call(k_GetSlowestSpeed, {Object(empty)})[0].first.floatVal, std::numeric_limits<float>::max());
	// A thing that is not a container gives 0
	EXPECT_EQ(Call(k_GetSlowestSpeed, {Object(Animal(0.0f, 0.0f, 100))})[0].first.floatVal, 0.0f);
}

TEST_F(ScriptNativesRows3, ABuildingInATownByKindAndBuiltShare)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto town = registry.Create();
	auto& data = registry.Assign<ecs::components::Town>(town);
	const auto MakeAbode = [&](AbodeNumber number, float built) {
		const auto abode = registry.Create();
		registry.Assign<ecs::components::Abode>(abode).type = number;
		registry.Assign<ecs::components::BuildProgress>(abode).built = built;
		data.abodes.push_back(abode);
		return abode;
	};
	const auto first = MakeAbode(AbodeNumber::A, 1.0f);
	const auto half = MakeAbode(AbodeNumber::B, 0.5f);
	const auto Find = [&](AbodeNumber number, float minBuilt, bool excluding) {
		return Call(k_CallBuildingInTown,
		            {Int(static_cast<int32_t>(number)), Object(town), Float(minBuilt), Bool(excluding)})[0]
		    .first.uintVal;
	};
	EXPECT_EQ(Find(AbodeNumber::B, 0.5f, true), static_cast<uint32_t>(half));
	EXPECT_EQ(Find(AbodeNumber::B, 0.75f, true), 0u);
	EXPECT_EQ(Find(AbodeNumber::B, 0.75f, false), static_cast<uint32_t>(first));
	registry.Assign<ecs::components::InScript>(half);
	EXPECT_EQ(Find(AbodeNumber::B, 0.5f, true), 0u);
}

TEST_F(ScriptNativesRows3, TheBeliefScaleOfATown)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto town = registry.Create();
	registry.Assign<ecs::components::Town>(town);
	registry.Assign<ecs::components::TownImpression>(town);
	EXPECT_TRUE(Call(k_SetObjectBeliefScale, {Object(town), Float(2.5f)}).empty());
	EXPECT_EQ(registry.Get<ecs::components::TownImpression>(town).belief.scale, 2.5f);
}

TEST_F(ScriptNativesRows3, AContainerIsMadeIndestructibleThroughEachOfItsMembers)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto flock = ecs::script_flocks::Create(registry, map_coords::MapCoords {});
	const auto sheep = Animal(0.0f, 0.0f, 0);
	ecs::script_flocks::AddLiving(registry, flock, sheep);

	const auto town = registry.Create();
	const auto house = registry.Create();
	const auto resident = registry.Create();
	const auto homeless = registry.Create();
	registry.Assign<ecs::components::Abode>(house).inhabitants = {resident};
	auto& data = registry.Assign<ecs::components::Town>(town);
	data.abodes = {house};
	data.homelessVillagers = {homeless};

	const auto rock = Animal(9.0f, 0.0f, 0);

	EXPECT_TRUE(Call(k_SetIndestructable, {Bool(true), Object(flock)}).empty());
	EXPECT_TRUE(Call(k_SetIndestructable, {Bool(true), Object(town)}).empty());
	EXPECT_TRUE(Call(k_SetIndestructable, {Bool(true), Object(rock)}).empty());
	EXPECT_TRUE(registry.AllOf<ecs::components::Indestructible>(sheep));
	EXPECT_TRUE(registry.AllOf<ecs::components::Indestructible>(resident));
	EXPECT_TRUE(registry.AllOf<ecs::components::Indestructible>(homeless));
	EXPECT_TRUE(registry.AllOf<ecs::components::Indestructible>(rock));
	// The containers themselves are not marked
	EXPECT_FALSE(registry.AllOf<ecs::components::Indestructible>(flock));
	EXPECT_FALSE(registry.AllOf<ecs::components::Indestructible>(town));

	EXPECT_TRUE(Call(k_SetIndestructable, {Bool(false), Object(flock)}).empty());
	EXPECT_FALSE(registry.AllOf<ecs::components::Indestructible>(sheep));
	EXPECT_TRUE(registry.AllOf<ecs::components::Indestructible>(resident));
}

TEST_F(ScriptNativesRows3, AnAnimalThatAScriptMakesASkeletonKeepsTheMark)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto animal = Animal(0.0f, 0.0f, 100);
	const auto IsSkeleton = [](entt::entity thing) { return Call(k_IsSkeleton, {Object(thing)})[0].first.intVal != 0; };
	EXPECT_FALSE(IsSkeleton(animal));
	EXPECT_TRUE(Call(k_SetSkeleton, {Bool(true), Object(animal)}).empty());
	EXPECT_TRUE(registry.AllOf<ecs::components::WalkingSkeleton>(animal));
	EXPECT_TRUE(IsSkeleton(animal));
	EXPECT_TRUE(Call(k_SetSkeleton, {Bool(false), Object(animal)}).empty());
	EXPECT_FALSE(IsSkeleton(animal));
	// A thing that does not live gets no mark
	const auto stone = registry.Create();
	registry.Assign<ecs::components::Transform>(stone);
	EXPECT_TRUE(Call(k_SetSkeleton, {Bool(true), Object(stone)}).empty());
	EXPECT_FALSE(registry.AllOf<ecs::components::WalkingSkeleton>(stone));
}

} // namespace
