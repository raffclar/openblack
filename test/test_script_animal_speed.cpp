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

#include <map>
#include <optional>
#include <utility>
#include <vector>

#include <LHVM.h>
#include <gtest/gtest.h>

#include "3D/MapCoords.h"
#include "CHLApi.h"
#include "ECS/Components/Animal.h"
#include "ECS/Components/Villager.h"
#include "ECS/Components/WallHug.h"
#include "ECS/Registry.h"
#include "ECS/ScriptFlocks.h"
#include "ECS/Systems/AnimalSystemInterface.h"
#include "ECS/Systems/ScriptObjectsSystemInterface.h"
#include "Locator.h"
#include "ScriptHeaders/ScriptEnums.h"

using namespace openblack;

namespace
{
/// Every object a script names is itself
class SpeedPassThroughScriptObjects final: public ecs::systems::ScriptObjectsSystemInterface
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

/// The animals keep the speeds that the scripts give them, and give them back
class SpeedAnimals final: public ecs::systems::AnimalSystemInterface
{
public:
	void ProcessTurn() override {}
	bool BeginTurn() override { return true; }
	void AnimalTurn(entt::entity) override {}
	void EndTurn() override {}
	void Update(std::chrono::milliseconds, float) override {}
	void ShowTurnEnd() override {}
	void Reset() override {}
	entt::entity CreateFlock(glm::vec2, float, float) override { return entt::null; }
	entt::entity CreateSpellAnimal(AnimalInfo, glm::vec2, float, uint16_t, PlayerNames, entt::entity, entt::entity) override
	{
		return entt::null;
	}
	entt::entity CreateScriptFlock(int32_t, glm::vec2, glm::vec2, float, float) override { return entt::null; }
	[[nodiscard]] entt::entity FindScriptFlock(int32_t) const override { return entt::null; }
	entt::entity CreateBird(AnimalInfo, glm::vec2, uint32_t, entt::entity) override { return entt::null; }
	void SetFlockTown(entt::entity, entt::entity) override {}
	entt::entity CreateGrazer(AnimalInfo, glm::vec2, uint32_t, entt::entity, entt::entity) override { return entt::null; }
	entt::entity CreateScriptAnimal(AnimalInfo, glm::vec2) override { return entt::null; }
	entt::entity CreateStillAnimal(AnimalInfo, glm::vec3, float, float) override { return entt::null; }
	void JoinFlock(entt::entity, entt::entity) override {}
	bool SetScriptState(entt::entity, LivingStates) override { return false; }
	void ScriptMoveTo(entt::entity, glm::vec2) override {}
	[[nodiscard]] std::optional<uint8_t> FinalGameStateOf(entt::entity) const override { return std::nullopt; }
	void SetScale(entt::entity, float) override {}
	[[nodiscard]] float RadiusOf(entt::entity) const override { return 0.0f; }
	[[nodiscard]] glm::vec3 MovementOf(entt::entity) const override { return glm::vec3(0.0f); }
	void SetFlockCentre(entt::entity, glm::vec2) override {}
	void MoveFlockPlace(entt::entity, glm::ivec2) override {}
	entt::entity SeparateIntoOwnFlock(entt::entity) override { return entt::null; }
	void SendToSlaughter(entt::entity, glm::vec2) override {}
	[[nodiscard]] uint16_t SpeedOf(entt::entity) const override { return 0; }
	[[nodiscard]] entt::entity LeaderOf(entt::entity) const override { return entt::null; }
	[[nodiscard]] std::vector<entt::entity> MembersOf(entt::entity) const override { return {}; }
	[[nodiscard]] glm::vec2 GoalOf(entt::entity) const override { return glm::vec2(0.0f); }
	[[nodiscard]] float GoalHeightOf(entt::entity) const override { return 0.0f; }
	void SendBird(entt::entity, glm::vec2, float, bool) override {}
	void SendWolf(entt::entity, glm::vec2, glm::vec2, float) override {}
	void StartFading(entt::entity) override {}
	void KillByEffect(entt::entity, glm::vec3) override {}
	void SetDying(entt::entity) override {}
	[[nodiscard]] bool IsFrighteningToCreature(entt::entity) const override { return false; }
	[[nodiscard]] bool CanPlayerPickUp(entt::entity) const override { return false; }
	[[nodiscard]] std::optional<bool> BirdCanBePickedUpByCreature(entt::entity) const override { return std::nullopt; }
	[[nodiscard]] bool CanBeStompedOnByCreature(entt::entity, float) const override { return false; }
	void SetSpeedFromScript(entt::entity animal, float metresPerTurn) override { speeds[animal] = metresPerTurn; }
	[[nodiscard]] float SpeedForScript(entt::entity animal) const override
	{
		const auto found = speeds.find(animal);
		return found != speeds.end() ? found->second : 0.0f;
	}

	std::map<entt::entity, float> speeds;
};

using Results = std::vector<std::pair<lhvm::VMValue, lhvm::DataType>>;

constexpr uint32_t k_GetProperty = 21;
constexpr uint32_t k_SetProperty = 22;

class ScriptAnimalSpeed: public ::testing::Test
{
protected:
	void SetUp() override
	{
		Locator::entitiesRegistry::emplace();
		Locator::scriptObjects::emplace<SpeedPassThroughScriptObjects>();
		Locator::animalSystem::emplace<SpeedAnimals>();
		Locator::vm::emplace();
		Locator::vm::value().Initialise(&_api.GetFunctionsTable(), nullptr, nullptr, nullptr, nullptr, nullptr, nullptr);
		// A script's object 0 is none, so nothing the scripts are given is the registry's first entity
		[[maybe_unused]] const auto none = Locator::entitiesRegistry::value().Create();
	}

	void TearDown() override
	{
		Locator::vm::reset();
		Locator::animalSystem::reset();
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

	static lhvm::NativeArgument Speed()
	{
		return std::pair(lhvm::VMValue(static_cast<int32_t>(script::ObjectPropertyType::Speed)), lhvm::DataType::Int);
	}

	static void SetSpeed(entt::entity object, float value) { Call(k_SetProperty, {Speed(), Object(object), Float(value)}); }

	static float AskSpeed(entt::entity object)
	{
		const auto results = Call(k_GetProperty, {Speed(), Object(object)});
		EXPECT_EQ(results.size(), 1u);
		return results.empty() ? -1.0f : results.front().first.floatVal;
	}

	/// An animal that walks round with a walk of its own, as the walk round gives it
	static entt::entity CreateAnimal()
	{
		auto& registry = Locator::entitiesRegistry::value();
		const auto entity = registry.Create();
		registry.Assign<ecs::components::Animal>(entity);
		registry.Assign<ecs::components::WallHug>(entity);
		return entity;
	}

	static SpeedAnimals& Animals() { return static_cast<SpeedAnimals&>(Locator::animalSystem::value()); }

	chlapi::CHLApi _api;
};
} // namespace

// The cow of the farmer takes the speed of the script, and the script reads it back
// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables): external macro
TEST_F(ScriptAnimalSpeed, AnAnimalTakesTheSpeedOfTheScript)
{
	const auto cow = CreateAnimal();
	SetSpeed(cow, 0.4f);
	EXPECT_EQ(Animals().speeds.at(cow), 0.4f);
	EXPECT_EQ(AskSpeed(cow), 0.4f);
	// The walk round of the animal does not keep the speed
	EXPECT_EQ(Locator::entitiesRegistry::value().Get<const ecs::components::WallHug>(cow).speed, 0.0f);
	EXPECT_EQ(_api.GetStubCalls().Total(), 0u);
}

// Each animal of a flock takes the speed of the flock, and a villager in the flock takes it as a villager
// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables): external macro
TEST_F(ScriptAnimalSpeed, EachAnimalOfAFlockTakesTheSpeed)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto flock = ecs::script_flocks::Create(registry, map_coords::MapCoords {});
	const auto first = CreateAnimal();
	const auto second = CreateAnimal();
	const auto villager = registry.Create();
	registry.Assign<ecs::components::Villager>(villager);
	registry.Assign<ecs::components::WallHug>(villager);
	ASSERT_TRUE(ecs::script_flocks::AddLiving(registry, flock, first));
	ASSERT_TRUE(ecs::script_flocks::AddLiving(registry, flock, second));
	ASSERT_TRUE(ecs::script_flocks::AddLiving(registry, flock, villager));
	const auto outside = CreateAnimal();
	SetSpeed(flock, 0.5f);
	EXPECT_EQ(Animals().speeds.at(first), 0.5f);
	EXPECT_EQ(Animals().speeds.at(second), 0.5f);
	EXPECT_FALSE(Animals().speeds.contains(villager));
	EXPECT_GT(registry.Get<const ecs::components::WallHug>(villager).speed, 0.0f);
	EXPECT_FALSE(Animals().speeds.contains(outside));
	// The flock itself reads 0
	EXPECT_EQ(AskSpeed(flock), 0.0f);
}
