/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#define LOCATOR_IMPLEMENTATIONS

#include <cmath>
#include <cstdint>

#include <array>
#include <limits>
#include <memory>
#include <optional>
#include <string_view>
#include <utility>
#include <vector>

#include <LHVM.h>
#include <gtest/gtest.h>

#include "3D/AllMeshes.h"
#include "CHLApi.h"
#include "ECS/Components/Abode.h"
#include "ECS/Components/Flock.h"
#include "ECS/Components/Physics.h"
#include "ECS/Components/Player.h"
#include "ECS/Components/Town.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/VillageTotem.h"
#include "ECS/Components/Villager.h"
#include "ECS/Registry.h"
#include "ECS/ScriptContainerWalk.h"
#include "ECS/ScriptFind.h"
#include "ECS/Systems/PlayerSystemInterface.h"
#include "ECS/Systems/ScriptObjectsSystemInterface.h"
#include "InfoConstants.h"
#include "Locator.h"
#include "ScriptHeaders/ScriptEnums.h"

using namespace openblack;
using namespace openblack::ecs::components;

namespace
{
/// Every object a script names is itself
class LessonScriptObjects final: public ecs::systems::ScriptObjectsSystemInterface
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

/// Players that know one player entity for one name
class FakePlayers final: public ecs::systems::PlayerSystemInterface
{
public:
	void RegisterPlayers() override {}
	void AddPlayer(entt::entity /*playerEntity*/) override {}
	[[nodiscard]] std::optional<entt::entity> FindPlayer(PlayerNames name) const override
	{
		return name == known ? std::optional(entity) : std::nullopt;
	}
	entt::entity EnsurePlayer(PlayerNames /*name*/) override { return entt::null; }
	void AddCreature(entt::entity /*creature*/) override {}
	[[nodiscard]] std::optional<entt::entity> GetPrimaryCreature(PlayerNames /*name*/) const override { return std::nullopt; }
	void KeepForNextLand() override {}
	void ForgetKept() override {}
	[[nodiscard]] bool KeepAlignmentAt(uint32_t /*turn*/) override { return false; }
	void TakeUpKept(entt::entity /*playerEntity*/) override {}
	[[nodiscard]] bool IsComputerPlayer(PlayerNames /*name*/) const override { return true; }
	[[nodiscard]] bool IsHumanPlayer(PlayerNames /*name*/) const override { return false; }

	PlayerNames known {PlayerNames::PLAYER_TWO};
	entt::entity entity {entt::null};
};

using Results = std::vector<std::pair<lhvm::VMValue, lhvm::DataType>>;

constexpr uint32_t k_CallInNear = 67;
constexpr uint32_t k_CallInNotNear = 141;
constexpr uint32_t k_CallComputerPlayer = 374;
constexpr uint32_t k_IdAdultSize = 387;
constexpr uint32_t k_ObjectAdultCapacity = 389;
constexpr uint32_t k_GetTotemStatue = 425;
constexpr uint32_t k_SetObjectCarrying = 440;
constexpr uint32_t k_GetFirstInContainer = 455;
constexpr uint32_t k_GetNextInContainer = 456;

class ScriptLessonNatives: public ::testing::Test
{
protected:
	void SetUp() override
	{
		Locator::entitiesRegistry::emplace();
		Locator::scriptObjects::emplace<LessonScriptObjects>();
		Locator::vm::emplace();
		Locator::vm::value().Initialise(&_api.GetFunctionsTable(), nullptr, nullptr, nullptr, nullptr, nullptr, nullptr);
		// A script's object 0 is none, so nothing the scripts are given is the registry's first entity
		[[maybe_unused]] const auto none = Registry().Create();
	}

	void TearDown() override
	{
		Locator::infoConstants::reset();
		Locator::vm::reset();
		Locator::scriptObjects::reset();
		Locator::entitiesRegistry::reset();
	}

	static ecs::Registry& Registry() { return Locator::entitiesRegistry::value(); }

	static Results Call(uint32_t native, const std::vector<lhvm::NativeArgument>& arguments)
	{
		Results results;
		EXPECT_TRUE(Locator::vm::value().CallNative(native, arguments, results));
		return results;
	}

	static entt::entity CallForObject(uint32_t native, const std::vector<lhvm::NativeArgument>& arguments)
	{
		const auto results = Call(native, arguments);
		EXPECT_EQ(results.size(), 1u);
		if (results.empty())
		{
			return entt::null;
		}
		EXPECT_EQ(results.front().second, lhvm::DataType::Object);
		const auto id = results.front().first.uintVal;
		return id == 0 ? entt::entity {entt::null} : static_cast<entt::entity>(id);
	}

	static float CallForFloat(uint32_t native, const std::vector<lhvm::NativeArgument>& arguments)
	{
		const auto results = Call(native, arguments);
		EXPECT_EQ(results.size(), 1u);
		if (results.empty())
		{
			return -1.0f;
		}
		EXPECT_EQ(results.front().second, lhvm::DataType::Float);
		return results.front().first.floatVal;
	}

	static lhvm::NativeArgument Object(entt::entity entity)
	{
		const auto id = entity == entt::null ? 0u : static_cast<uint32_t>(entity);
		return std::pair(lhvm::VMValue(id), lhvm::DataType::Object);
	}
	static lhvm::NativeArgument Float(float value) { return std::pair(lhvm::VMValue(value), lhvm::DataType::Float); }
	static lhvm::NativeArgument Int(int32_t value) { return std::pair(lhvm::VMValue(value), lhvm::DataType::Int); }
	static lhvm::NativeArgument Bool(bool value)
	{
		return std::pair(lhvm::VMValue(static_cast<int32_t>(value)), lhvm::DataType::Boolean);
	}

	static entt::entity MakeVillager(Villager::LifeStage stage, glm::vec3 position = glm::vec3(0.0f))
	{
		auto& registry = Registry();
		const auto entity = registry.Create();
		Villager villager {};
		villager.lifeStage = stage;
		villager.town = entt::null;
		villager.abode = entt::null;
		registry.Assign<Villager>(entity, villager);
		registry.Assign<Transform>(entity, position, glm::mat3(1.0f), glm::vec3(1.0f));
		return entity;
	}

	static entt::entity MakeAbode(std::vector<entt::entity> inhabitants)
	{
		auto& registry = Registry();
		const auto entity = registry.Create();
		Abode abode {};
		abode.type = AbodeNumber::A;
		abode.inhabitants = std::move(inhabitants);
		registry.Assign<Abode>(entity, abode);
		return entity;
	}

	static entt::entity MakeTown(std::vector<entt::entity> abodes, std::vector<entt::entity> homeless)
	{
		auto& registry = Registry();
		const auto entity = registry.Create();
		Town town {};
		town.abodes = std::move(abodes);
		town.homelessVillagers = std::move(homeless);
		registry.Assign<Town>(entity, town);
		return entity;
	}

	static entt::entity MakeFlock(std::vector<entt::entity> members)
	{
		auto& registry = Registry();
		const auto entity = registry.Create();
		Flock flock {};
		flock.members = std::move(members);
		registry.Assign<Flock>(entity, flock);
		return entity;
	}

	chlapi::CHLApi _api;
};
} // namespace

// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables): external macro
TEST(ScriptContainerWalk, TheFirstThingOfATownLooksOnlyAtItsFirstBuilding)
{
	const std::array people {static_cast<entt::entity>(5), static_cast<entt::entity>(6)};
	const std::array homeless {static_cast<entt::entity>(9)};
	EXPECT_EQ(ecs::script_containers::FirstInTown(people, homeless), people.front());
	// The first building is empty: the town gives its first homeless person, though later buildings have people
	EXPECT_EQ(ecs::script_containers::FirstInTown({}, homeless), homeless.front());
	EXPECT_EQ(ecs::script_containers::FirstInTown({}, {}), entt::entity {entt::null});
}

// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables): external macro
TEST(ScriptContainerWalk, TheNextThingFollowsTheOneGiven)
{
	const std::array members {static_cast<entt::entity>(3), static_cast<entt::entity>(4), static_cast<entt::entity>(7)};
	EXPECT_EQ(ecs::script_containers::NextAfter(members, members[0]), members[1]);
	EXPECT_EQ(ecs::script_containers::NextAfter(members, members[1]), members[2]);
	EXPECT_EQ(ecs::script_containers::NextAfter(members, members[2]), entt::entity {entt::null});
	EXPECT_EQ(ecs::script_containers::NextAfter(members, static_cast<entt::entity>(99)), entt::entity {entt::null});
}

// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables): external macro
TEST(ScriptContainerWalk, NotNearIsMoreThanTheRadius)
{
	EXPECT_TRUE(ecs::script_containers::IsNotNear(10.5f, 10.0f));
	EXPECT_FALSE(ecs::script_containers::IsNotNear(10.0f, 10.0f));
	EXPECT_FALSE(ecs::script_containers::IsNotNear(3.0f, 10.0f));
	EXPECT_FALSE(ecs::script_containers::IsNotNear(std::numeric_limits<float>::quiet_NaN(), 10.0f));
}

// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables): external macro
TEST(ScriptContainerWalk, CountsAndHeldObjectsAsTheScriptsReadThem)
{
	EXPECT_EQ(ecs::script_containers::TownAdults(16777217u), 16777216.0f);
	EXPECT_EQ(ecs::script_containers::TownAdultRoom(-3), -3.0f);
	// A building counts its adults in one byte
	EXPECT_EQ(ecs::script_containers::AbodeAdults(257u), 1.0f);
	EXPECT_EQ(ecs::script_containers::AbodeAdultRoom(6u), 6.0f);
	EXPECT_EQ(ecs::script_containers::CarriedObjectFromScript(9u), 9u);
	EXPECT_EQ(ecs::script_containers::CarriedObjectFromScript(0x102u), 2u);
}

// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables): external macro
TEST_F(ScriptLessonNatives, TakeAndGiveWhatTheGameDoes)
{
	struct Expected
	{
		uint32_t native;
		std::string_view name;
		int stackIn;
		uint32_t stackOut;
	};
	constexpr auto k_Natives = std::to_array<Expected>({
	    {k_CallInNotNear, "CALL_IN_NOT_NEAR", 8, 1},
	    {k_CallComputerPlayer, "CALL_COMPUTER_PLAYER", 1, 1},
	    {k_IdAdultSize, "ID_ADULT_SIZE", 1, 1},
	    {k_ObjectAdultCapacity, "OBJECT_ADULT_CAPACITY", 1, 1},
	    {k_GetTotemStatue, "GET_TOTEM_STATUE", 1, 1},
	    {k_SetObjectCarrying, "SET_OBJECT_CARRYING", 2, 0},
	    {k_GetFirstInContainer, "GET_FIRST_IN_CONTAINER", 1, 1},
	    {k_GetNextInContainer, "GET_NEXT_IN_CONTAINER", 2, 1},
	});
	const auto& table = _api.GetFunctionsTable();
	for (const auto& expected : k_Natives)
	{
		ASSERT_LT(expected.native, table.size());
		EXPECT_EQ(table[expected.native].name, expected.name);
		EXPECT_EQ(table[expected.native].stackIn, expected.stackIn) << expected.name;
		EXPECT_EQ(table[expected.native].stackOut, expected.stackOut) << expected.name;
	}
}

// A town centre gives the totem that stands on it; any other thing gives nothing
// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables): external macro
TEST_F(ScriptLessonNatives, TotemStatueOfATownCentre)
{
	auto& registry = Registry();
	const auto centre = MakeAbode({});
	const auto other = MakeAbode({});
	const auto totem = registry.Create();
	registry.Assign<VillageTotem>(totem, VillageTotem {.townCentre = centre});
	EXPECT_EQ(CallForObject(k_GetTotemStatue, {Object(centre)}), totem);
	EXPECT_EQ(CallForObject(k_GetTotemStatue, {Object(other)}), entt::entity {entt::null});
	EXPECT_EQ(CallForObject(k_GetTotemStatue, {Object(entt::null)}), entt::entity {entt::null});
	EXPECT_EQ(_api.GetStubCalls().Total(), 0u);
}

// A villager holds the object that the script gives; nothing else changes
// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables): external macro
TEST_F(ScriptLessonNatives, SetObjectCarryingGivesAVillagerItsObject)
{
	const auto villager = MakeVillager(Villager::LifeStage::Adult);
	const auto abode = MakeAbode({});
	EXPECT_TRUE(Call(k_SetObjectCarrying, {Object(villager), Int(static_cast<int32_t>(CarriedObject::MalletHeavy))}).empty());
	EXPECT_EQ(Registry().Get<const Villager>(villager).carried, CarriedObject::MalletHeavy);
	EXPECT_TRUE(Call(k_SetObjectCarrying, {Object(abode), Int(2)}).empty());
	EXPECT_TRUE(Call(k_SetObjectCarrying, {Object(entt::null), Int(2)}).empty());
	EXPECT_EQ(Registry().Get<const Villager>(villager).carried, CarriedObject::MalletHeavy);
	EXPECT_EQ(_api.GetStubCalls().Total(), 0u);
}

// A town gives the adults and the room for adults that it counted; a building gives its adults and the room of its kind
// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables): external macro
TEST_F(ScriptLessonNatives, AdultSizeAndCapacityOfATownOrABuilding)
{
	auto info = std::make_unique<InfoConstants>();
	// The first row of the building's number and tribe gives its room
	auto& row = info->abode.at(0);
	row.abodeNumber = AbodeNumber::A;
	row.tribeType = Tribe::NORSE;
	row.maxVillagersInAbode = 4;
	row.maxChildrenInAbode = 2;
	Locator::infoConstants::emplace(*info);

	auto& registry = Registry();
	const auto man = MakeVillager(Villager::LifeStage::Adult);
	const auto woman = MakeVillager(Villager::LifeStage::Adult);
	const auto child = MakeVillager(Villager::LifeStage::Child);
	const auto stranger = MakeVillager(Villager::LifeStage::Adult);
	const auto house = MakeAbode({child, woman, man});
	const auto second = MakeAbode({});
	// A building that is not built yet gives no room
	const auto site = MakeAbode({});
	registry.Assign<BuildProgress>(site, BuildProgress {.built = 0.5f});
	const auto town = MakeTown({house, second, site}, {});
	registry.Get<Town>(town).id = 3;
	registry.Assign<Tribe>(town, Tribe::NORSE);
	for (const auto building : {house, second, site})
	{
		registry.Get<Abode>(building).townId = 3;
	}
	for (const auto person : {man, woman, child})
	{
		registry.Get<Villager>(person).town = town;
	}
	registry.Get<Villager>(stranger).town = MakeTown({}, {});

	// The town counts its adults and the room for adults of its built houses at the time of the call
	EXPECT_EQ(CallForFloat(k_IdAdultSize, {Object(town)}), 2.0f);
	EXPECT_EQ(CallForFloat(k_ObjectAdultCapacity, {Object(town)}), 8.0f);
	EXPECT_EQ(CallForFloat(k_IdAdultSize, {Object(house)}), 2.0f);
	EXPECT_EQ(CallForFloat(k_ObjectAdultCapacity, {Object(house)}), 4.0f);
	// A villager is neither a town nor a building, and nothing is not alive
	EXPECT_EQ(CallForFloat(k_IdAdultSize, {Object(man)}), 0.0f);
	EXPECT_EQ(CallForFloat(k_ObjectAdultCapacity, {Object(entt::null)}), 0.0f);
	EXPECT_EQ(_api.GetStubCalls().Total(), 0u);
}

// A script walks through a flock in its order, and the thing after the last is nothing
// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables): external macro
TEST_F(ScriptLessonNatives, FirstAndNextWalkThroughAFlock)
{
	const auto first = MakeVillager(Villager::LifeStage::Adult);
	const auto second = MakeVillager(Villager::LifeStage::Adult);
	const auto flock = MakeFlock({first, second});
	EXPECT_EQ(CallForObject(k_GetFirstInContainer, {Object(flock)}), first);
	EXPECT_EQ(CallForObject(k_GetNextInContainer, {Object(flock), Object(first)}), second);
	EXPECT_EQ(CallForObject(k_GetNextInContainer, {Object(flock), Object(second)}), entt::entity {entt::null});
	// With nothing to start after, the next is the first
	EXPECT_EQ(CallForObject(k_GetNextInContainer, {Object(flock), Object(entt::null)}), first);
	// A thing that is not a container gives nothing
	EXPECT_EQ(CallForObject(k_GetFirstInContainer, {Object(first)}), entt::entity {entt::null});
	EXPECT_EQ(CallForObject(k_GetNextInContainer, {Object(first), Object(second)}), entt::entity {entt::null});
	EXPECT_EQ(_api.GetStubCalls().Total(), 0u);
}

// A town's first thing comes from its first building only; the next walks every building, then the homeless
// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables): external macro
TEST_F(ScriptLessonNatives, FirstAndNextWalkThroughATown)
{
	const auto resident = MakeVillager(Villager::LifeStage::Adult);
	const auto homeless = MakeVillager(Villager::LifeStage::Adult);
	const auto empty = MakeAbode({});
	const auto house = MakeAbode({resident});
	const auto town = MakeTown({empty, house}, {homeless});
	EXPECT_EQ(CallForObject(k_GetFirstInContainer, {Object(town)}), homeless);
	EXPECT_EQ(CallForObject(k_GetNextInContainer, {Object(town), Object(resident)}), homeless);
	EXPECT_EQ(CallForObject(k_GetNextInContainer, {Object(town), Object(homeless)}), entt::entity {entt::null});
	const auto full = MakeTown({house}, {homeless});
	EXPECT_EQ(CallForObject(k_GetFirstInContainer, {Object(full)}), resident);
}

// The search outside a radius finds the first villager of the flock that is farther away than the radius
// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables): external macro
TEST_F(ScriptLessonNatives, CallInNotNearFindsTheFirstThingOutsideTheRadius)
{
	// The kinds of the villagers come from the tables
	auto info = std::make_unique<InfoConstants>();
	Locator::infoConstants::emplace(*info);
	const auto inside = MakeVillager(Villager::LifeStage::Adult, glm::vec3(1003.0f, 0.0f, 1000.0f));
	const auto close = MakeVillager(Villager::LifeStage::Adult, glm::vec3(1009.5f, 0.0f, 1000.0f));
	const auto outside = MakeVillager(Villager::LifeStage::Adult, glm::vec3(1000.0f, 0.0f, 1025.0f));
	const auto flock = MakeFlock({inside, close, outside});
	const auto args = [flock](float radius) {
		return std::vector<lhvm::NativeArgument> {Int(static_cast<int32_t>(script::ObjectType::Villager)),
		                                          Int(static_cast<int32_t>(ecs::script_find::k_AnySubtype)),
		                                          Object(flock),
		                                          Float(1000.0f),
		                                          Float(0.0f),
		                                          Float(1000.0f),
		                                          Float(radius),
		                                          Bool(false)};
	};
	EXPECT_EQ(CallForObject(k_CallInNotNear, args(10.0f)), outside);
	EXPECT_EQ(CallForObject(k_CallInNear, args(10.0f)), inside);
	EXPECT_EQ(CallForObject(k_CallInNotNear, args(2.0f)), inside);
	EXPECT_EQ(CallForObject(k_CallInNotNear, args(30.0f)), entt::entity {entt::null});
	EXPECT_EQ(_api.GetStubCalls().Total(), 0u);
}

// A script's player 2 gives the entity of the second player, which keeps its computer player; an unknown player gives
// nothing
// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables): external macro
TEST_F(ScriptLessonNatives, CallComputerPlayerGivesThePlayer)
{
	auto& players = static_cast<FakePlayers&>(Locator::playerSystem::emplace<FakePlayers>());
	players.entity = Registry().Create();
	EXPECT_EQ(CallForObject(k_CallComputerPlayer, {Float(2.0f)}), players.entity);
	EXPECT_EQ(CallForObject(k_CallComputerPlayer, {Float(3.0f)}), entt::entity {entt::null});
	Locator::playerSystem::reset();
	EXPECT_EQ(_api.GetStubCalls().Total(), 0u);
}
