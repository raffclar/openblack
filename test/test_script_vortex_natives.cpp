/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cstdint>

#include <algorithm>
#include <bit>
#include <memory>
#include <optional>
#include <string_view>
#include <utility>
#include <vector>

#include <LHVM.h>
#include <glm/geometric.hpp>
#include <gtest/gtest.h>
#include <spdlog/sinks/null_sink.h>
#include <spdlog/spdlog.h>

#include "CHLApi.h"
#include "Common/GameRandom.h"
#include "ECS/Components/Creature.h"
#include "ECS/Components/Flock.h"
#include "ECS/Components/Physics.h"
#include "ECS/Components/Town.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/Vortex.h"
#include "ECS/Registry.h"
#include "ECS/ScriptSpotVisuals.h"
#include "ECS/Systems/DynamicsSystemInterface.h"
#include "ECS/Systems/EffectRequestSystemInterface.h"
#include "ECS/Systems/ScriptObjectsSystemInterface.h"
#include "Locator.h"
#include "Particles/ParticleDrawFrame.h"
#include "ScriptHeaders/ScriptEnums.h"
#include "ScriptHeaders/ScriptPropertyRules.h"

// Enable this define because we use a custom locator
#define LOCATOR_IMPLEMENTATIONS
#include "ECS/Systems/Implementations/EffectRequestSystem.h"
#include "ECS/Systems/Implementations/VortexSystem.h"

using namespace openblack;
using namespace openblack::script::property_rules;

namespace
{
/// Records how the last thing the scripts set off was started
class FakeThrowDynamics final: public ecs::systems::DynamicsSystemInterface
{
public:
	ecs::systems::PhysicsStarted InitialisePhysics(entt::entity object, const ecs::systems::PhysicsStart& start) override
	{
		thrown = object;
		started = start;
		return {};
	}

	entt::entity thrown {entt::null};
	std::optional<ecs::systems::PhysicsStart> started;
};

/// Draws each float as a quarter of its range, and counts the draws
class FakeGameRandom final: public GameRandomInterface
{
public:
	uint32_t SyncedRand(uint32_t, DrawSite) override { return 0; }
	float SyncedFloatRand(float x, DrawSite /*site*/) override
	{
		++draws;
		return x * 0.25f;
	}
	uint32_t LocalRand(int32_t) override { return 0; }
	float LocalFloatRand(float) override { return 0.0f; }
	int32_t CrtRand() override { return 0; }
	void CrtSrand(uint32_t) override {}
	[[nodiscard]] GameRandomSeeds GetSeeds() const override { return {}; }
	void SetSeeds(GameRandomSeeds) override {}
	[[nodiscard]] ParticleRandomStream GetParticleStream() const override { return ParticleRandomStream::None; }
	void SetParticleStream(ParticleRandomStream) override {}

	int draws {0};
};

/// Shows each effect it is asked for and keeps the last magnitude given to each
class FakeEffectServer final: public ecs::systems::EffectServerInterface
{
public:
	void SetLifeListener(ecs::systems::EffectLifeListener* lifeListener) override { listener = lifeListener; }
	ecs::systems::EffectHandle Serve(const ecs::systems::EffectRequest& /*request*/, ecs::systems::EffectHandle handle) override
	{
		if (listener != nullptr)
		{
			listener->EffectStarted(handle);
		}
		return handle;
	}
	void Move(ecs::systems::EffectHandle, glm::vec3) override {}
	void Stop(ecs::systems::EffectHandle) override {}
	void Remove(ecs::systems::EffectHandle) override {}
	void SetPlayer(ecs::systems::EffectHandle, int) override {}
	void SetPower(ecs::systems::EffectHandle, float) override {}
	void SetMagnitude(ecs::systems::EffectHandle handle, float magnitude) override
	{
		magnitudes.emplace_back(handle, magnitude);
	}
	void SetAlpha(ecs::systems::EffectHandle, float) override {}
	void SetProcessInfo(ecs::systems::EffectHandle, const particles::ProcessInfo&) override {}
	void SetEnabled(ecs::systems::EffectHandle, bool) override {}
	void AddTarget(ecs::systems::EffectHandle, entt::entity) override {}
	void AddTargetPosition(ecs::systems::EffectHandle, glm::vec3) override {}
	void AddPlasma(ecs::systems::EffectHandle, const particles::PlasmaCommand&) override {}
	void AddManaPathSpark(ecs::systems::EffectHandle, const particles::mana_path::Spark&) override {}
	bool ProcessForSpell(ecs::systems::EffectHandle, const particles::ProcessInfo&, float) override { return true; }
	void RestoreSink(ecs::systems::EffectHandle, particles::SpellSink*) override {}
	void SetHidden(ecs::systems::EffectHandle, bool) override {}
	void SetDrawOffset(ecs::systems::EffectHandle, glm::vec3) override {}
	void SetLiveHand(ecs::systems::EffectHandle, glm::vec3) override {}
	[[nodiscard]] std::shared_ptr<particles::ShieldSphere> FindShield(glm::vec3, float) const override { return nullptr; }
	void AddBeliefSprite(const particles::BeliefSprite&) override {}
	void AddHandManaPathSpark(const particles::mana_path::Spark&) override {}
	void AddStormBolt(glm::vec3) override {}
	void SetFightMarks(std::vector<particles::draw::FightMark>) override {}
	void AddLightSheet(const std::shared_ptr<particles::LightSheet>&) override {}
	void SetPlayerSymbols(std::vector<particles::draw::PlayerSymbol>) override {}
	void SetShown(ecs::systems::EffectHandle, bool) override {}
	bool Renumber(ecs::systems::EffectHandle, ecs::systems::EffectHandle) override { return false; }

	ecs::systems::EffectLifeListener* listener {nullptr};
	std::vector<std::pair<ecs::systems::EffectHandle, float>> magnitudes;
};

/// Every object a script names is itself, as the scripts' table gives back what it holds
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

class ScriptVortexNatives: public ::testing::Test
{
protected:
	void SetUp() override
	{
		// The natives tell of a thing that isn't what they need in the scripts' log
		if (spdlog::get("scripting") == nullptr)
		{
			spdlog::create<spdlog::sinks::null_sink_st>("scripting");
		}
		Locator::entitiesRegistry::emplace();
		Locator::dynamicsSystem::emplace<FakeThrowDynamics>();
		Locator::gameRandom::emplace<FakeGameRandom>();
		Locator::scriptObjects::emplace<PassThroughScriptObjects>();
		Locator::vortexSystem::emplace<ecs::systems::VortexSystem>();
		Locator::vm::emplace();
		Locator::vm::value().Initialise(&_api.GetFunctionsTable(), nullptr, nullptr, nullptr, nullptr, nullptr, nullptr);
		// A script's object 0 is none, so nothing the scripts are given is the registry's first entity
		[[maybe_unused]] const auto none = Locator::entitiesRegistry::value().Create();
	}

	void TearDown() override
	{
		Locator::vm::reset();
		Locator::vortexSystem::reset();
		Locator::scriptObjects::reset();
		Locator::gameRandom::reset();
		Locator::dynamicsSystem::reset();
		Locator::entitiesRegistry::reset();
	}

	/// The native's number in the scripts' table
	[[nodiscard]] uint32_t Native(std::string_view name)
	{
		const auto& table = _api.GetFunctionsTable();
		const auto found = std::ranges::find_if(table, [name](const auto& function) { return function.name == name; });
		return static_cast<uint32_t>(std::distance(table.begin(), found));
	}

	Results Call(std::string_view name, const std::vector<lhvm::NativeArgument>& arguments)
	{
		Results results;
		EXPECT_TRUE(Locator::vm::value().CallNative(Native(name), arguments, results)) << name;
		return results;
	}

	[[nodiscard]] float GetProperty(script::ObjectPropertyType property, entt::entity object)
	{
		const auto results = Call("GET_PROPERTY", {Int(static_cast<int32_t>(property)), Object(object)});
		EXPECT_EQ(results.size(), 1u);
		return results.empty() ? -1.0f : results.front().first.floatVal;
	}

	void SetProperty(script::ObjectPropertyType property, entt::entity object, float value)
	{
		Call("SET_PROPERTY", {Int(static_cast<int32_t>(property)), Object(object), Float(value)});
	}

	[[nodiscard]] int32_t SubType(entt::entity object)
	{
		const auto results = Call("GAME_SUB_TYPE", {Object(object)});
		EXPECT_EQ(results.size(), 1u);
		return results.empty() ? -1 : results.front().first.intVal;
	}

	entt::entity MakeCreature(CreatureType species)
	{
		auto& registry = Locator::entitiesRegistry::value();
		const auto creature = registry.Create();
		registry.Assign<ecs::components::Creature>(creature, ecs::components::Creature {.species = species});
		return creature;
	}

	entt::entity MakeVortex(VortexType type)
	{
		auto& registry = Locator::entitiesRegistry::value();
		const auto vortex = registry.Create();
		registry.Assign<ecs::components::Vortex>(vortex, ecs::components::Vortex {.type = type});
		return vortex;
	}

	chlapi::CHLApi _api;
};
} // namespace

// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables): external macro
TEST_F(ScriptVortexNatives, TakeAndGiveWhatTheGameDoes)
{
	const auto& table = _api.GetFunctionsTable();
	struct Expected
	{
		uint32_t index;
		std::string_view name;
		int32_t in;
		uint32_t out;
	};
	for (const auto& [index, name, in, out] :
	     {Expected {127, "SET_HEADING_AND_SPEED", 5, 0}, Expected {221, "GAME_SUB_TYPE", 1, 1},
	      Expected {328, "VORTEX_PARAMETERS", 8, 0}})
	{
		ASSERT_EQ(Native(name), index);
		EXPECT_EQ(table[index].stackIn, in) << name;
		EXPECT_EQ(table[index].stackOut, out) << name;
	}
	EXPECT_EQ(static_cast<int>(script::ObjectPropertyType::CreatureCanGoThroughVortex), 36);
}

// Land 1 forbids the creature the vortex, then lets it through once the scroll is clicked
// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables): external macro
TEST_F(ScriptVortexNatives, ScriptsLetTheCreatureThroughTheVortex)
{
	using script::ObjectPropertyType;
	const auto creature = MakeCreature(CreatureType::Ogre);
	auto& registry = Locator::entitiesRegistry::value();
	// No creature may until told
	EXPECT_FALSE(registry.Get<ecs::components::Creature>(creature).mayGoThroughVortex);
	EXPECT_EQ(GetProperty(ObjectPropertyType::CreatureCanGoThroughVortex, creature), 0.0f);

	SetProperty(ObjectPropertyType::CreatureCanGoThroughVortex, creature, 0.5f);
	EXPECT_TRUE(registry.Get<ecs::components::Creature>(creature).mayGoThroughVortex);
	EXPECT_EQ(GetProperty(ObjectPropertyType::CreatureCanGoThroughVortex, creature), 1.0f);

	SetProperty(ObjectPropertyType::CreatureCanGoThroughVortex, creature, 0.0f);
	EXPECT_FALSE(registry.Get<ecs::components::Creature>(creature).mayGoThroughVortex);

	// Anything else is not a creature, and answers 0
	const auto stone = registry.Create();
	SetProperty(ObjectPropertyType::CreatureCanGoThroughVortex, stone, 1.0f);
	EXPECT_EQ(GetProperty(ObjectPropertyType::CreatureCanGoThroughVortex, stone), 0.0f);
}

// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables): external macro
TEST_F(ScriptVortexNatives, SubTypesAreTheRowsTheScriptsKnow)
{
	// The scripts number the species from the giant ape
	EXPECT_EQ(SubType(MakeCreature(CreatureType::GiantApe)), 0);
	EXPECT_EQ(SubType(MakeCreature(CreatureType::Ogre)), static_cast<int32_t>(CreatureType::Ogre));
	EXPECT_EQ(SubType(MakeVortex(VortexType::Out)), 1);
	EXPECT_EQ(SubType(MakeVortex(VortexType::Volcano)), 2);
	// A thing whose kind can't be told gives a number no kind has; a thing that is gone gives 0
	auto& registry = Locator::entitiesRegistry::value();
	EXPECT_EQ(SubType(registry.Create()), 9999);
	const auto gone = registry.Create();
	registry.Destroy(gone);
	EXPECT_EQ(SubType(gone), 0);
}

// Land 1's returning shepherd is flung out of the vortex towards the camera
// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables): external macro
TEST_F(ScriptVortexNatives, AThingAtRestIsThrownTowardsThePlace)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto shepherd = registry.Create();
	registry.Assign<ecs::components::Transform>(shepherd, glm::vec3(10.0f, 2.0f, 20.0f), glm::mat3(1.0f), glm::vec3(1.0f));
	Call("SET_HEADING_AND_SPEED", {Object(shepherd), Float(10.0f), Float(2.0f), Float(24.0f), Float(16.0f)});
	const auto& dynamics = static_cast<FakeThrowDynamics&>(Locator::dynamicsSystem::value());
	ASSERT_EQ(dynamics.thrown, shepherd);
	ASSERT_TRUE(dynamics.started.has_value());
	EXPECT_NEAR(dynamics.started->velocity.x, 0.0f, 1e-6f);
	EXPECT_NEAR(dynamics.started->velocity.y, 0.0f, 1e-6f);
	EXPECT_NEAR(dynamics.started->velocity.z, 16.0f, 1e-5f);
	// It tumbles: each axis draws, a quarter of the draw's range being -0.5
	EXPECT_EQ(static_cast<FakeGameRandom&>(Locator::gameRandom::value()).draws, 3);
	EXPECT_FLOAT_EQ(dynamics.started->spin.x, -0.5f);
	EXPECT_EQ(dynamics.started->thrower, entt::entity {entt::null});
}

// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables): external macro
TEST_F(ScriptVortexNatives, AnArrivingVortexIsToldItsTownAndFlock)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto vortex = MakeVortex(VortexType::Out);
	const auto town = registry.Create();
	registry.Assign<ecs::components::Town>(town);
	const auto flock = registry.Create();
	registry.Assign<ecs::components::Flock>(flock);
	Call("VORTEX_PARAMETERS", {Object(vortex), Object(town), Float(3157.0f), Float(18.0f), Float(3226.0f), Float(27.0f),
	                           Float(48.0f), Object(flock)});
	const auto& arrival = registry.Get<ecs::components::Vortex>(vortex).arrival;
	EXPECT_EQ(arrival.town, town);
	EXPECT_EQ(arrival.gatheringPlace, glm::vec3(3157.0f, 18.0f, 3226.0f));
	EXPECT_EQ(arrival.flockDistance, 27.0f);
	EXPECT_EQ(arrival.flockRadius, 48.0f);
	EXPECT_EQ(arrival.flock, flock);

	// Given no town, it is still told the rest, its town none
	const auto stone = registry.Create();
	Call("VORTEX_PARAMETERS",
	     {Object(vortex), Object(stone), Float(1.0f), Float(2.0f), Float(3.0f), Float(4.0f), Float(5.0f), Object(stone)});
	const auto& again = registry.Get<ecs::components::Vortex>(vortex).arrival;
	EXPECT_EQ(again.town, entt::entity {entt::null});
	EXPECT_EQ(again.flock, entt::entity {entt::null});
	EXPECT_EQ(again.flockRadius, 5.0f);

	// A vortex that leads away takes no notice
	const auto leaving = MakeVortex(VortexType::In);
	Call("VORTEX_PARAMETERS",
	     {Object(leaving), Object(town), Float(1.0f), Float(2.0f), Float(3.0f), Float(4.0f), Float(5.0f), Object(flock)});
	EXPECT_EQ(registry.Get<ecs::components::Vortex>(leaving).arrival.town, entt::entity {entt::null});
}

TEST(ScriptHeading, TheSpeedIsAlongTheLineToThePlace)
{
	const auto velocity = HeadingVelocity({0.0f, 0.0f, 0.0f}, {3.0f, 0.0f, 4.0f}, 10.0f);
	EXPECT_FLOAT_EQ(velocity.x, 6.0f);
	EXPECT_FLOAT_EQ(velocity.z, 8.0f);
	EXPECT_FLOAT_EQ(glm::length(HeadingVelocity({1.0f, 2.0f, 3.0f}, {-4.0f, 9.0f, 1.0f}, 16.0f)), 16.0f);
	// Already there: no velocity at all
	EXPECT_EQ(HeadingVelocity({1.0f, 2.0f, 3.0f}, {1.0f, 2.0f, 3.0f}, 16.0f), glm::vec3(0.0f));
}

TEST(ScriptHeading, TheTumbleIsMinusOneToOne)
{
	EXPECT_FLOAT_EQ(HeadingSpin(0.0f), -1.0f);
	EXPECT_FLOAT_EQ(HeadingSpin(100.0f), 0.0f);
	EXPECT_FLOAT_EQ(HeadingSpin(200.0f), 1.0f);
}

// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables): external macro
TEST(ScriptHeading, TheTumbleRoundsEachStepToAFloat)
{
	// The draw less 100 rounds to a float before the multiplication by the float 0.01
	EXPECT_EQ(std::bit_cast<uint32_t>(HeadingSpin(37.3f)), 0xBF208312u);
	EXPECT_EQ(std::bit_cast<uint32_t>(HeadingSpin(3.3f)), 0xBF778D4Fu);
}

// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables): external macro
TEST(ScriptTarget, TheThrowGetsToThePlaceInTheTime)
{
	const auto velocity = TargetVelocity({10.0f, 2.0f, 20.0f}, {14.0f, 2.0f, 26.0f}, 2.0f);
	EXPECT_EQ(velocity.x, 2.0f);
	EXPECT_EQ(velocity.z, 3.0f);
	// Level with the place, it goes up at half the fall in the time, so that it comes back down at the place
	EXPECT_EQ(velocity.y, -k_TargetGravity);
	// The gravity is the bits of the game's constant
	EXPECT_EQ(std::bit_cast<uint32_t>(k_TargetGravity), 0xC11CF5C3u);
}

// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables): external macro
TEST_F(ScriptVortexNatives, AThingIsThrownToGetToTheTarget)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto villager = registry.Create();
	registry.Assign<ecs::components::Transform>(villager, glm::vec3(10.0f, 2.0f, 20.0f), glm::mat3(1.0f), glm::vec3(1.0f));
	Call("SET_TARGET", {Object(villager), Float(14.0f), Float(2.0f), Float(26.0f), Float(2.0f)});
	const auto& dynamics = static_cast<FakeThrowDynamics&>(Locator::dynamicsSystem::value());
	ASSERT_EQ(dynamics.thrown, villager);
	ASSERT_TRUE(dynamics.started.has_value());
	EXPECT_EQ(dynamics.started->velocity, TargetVelocity({10.0f, 2.0f, 20.0f}, {14.0f, 2.0f, 26.0f}, 2.0f));
	EXPECT_EQ(static_cast<FakeGameRandom&>(Locator::gameRandom::value()).draws, 3);
	EXPECT_FLOAT_EQ(dynamics.started->spin.z, -0.5f);
}

// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables): external macro
TEST_F(ScriptVortexNatives, AThrowNeedsATimeAboveZero)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto villager = registry.Create();
	registry.Assign<ecs::components::Transform>(villager, glm::vec3(10.0f, 2.0f, 20.0f), glm::mat3(1.0f), glm::vec3(1.0f));
	Call("SET_TARGET", {Object(villager), Float(14.0f), Float(2.0f), Float(26.0f), Float(0.0f)});
	const auto& dynamics = static_cast<FakeThrowDynamics&>(Locator::dynamicsSystem::value());
	EXPECT_EQ(dynamics.thrown, entt::entity {entt::null});
	EXPECT_EQ(static_cast<FakeGameRandom&>(Locator::gameRandom::value()).draws, 0);
}

// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables): external macro
TEST_F(ScriptVortexNatives, ACreatureAndAnUnknownThingHaveNoState)
{
	const auto creature = MakeCreature(CreatureType::Ogre);
	const auto rock = Locator::entitiesRegistry::value().Create();
	for (const auto thing : {creature, rock})
	{
		const auto results = Call("GET_OBJECT_STATE", {Object(thing)});
		ASSERT_EQ(results.size(), 1u);
		EXPECT_EQ(results.front().first.intVal, 0);
	}
}

TEST_F(ScriptVortexNatives, AScriptsVisualTakesTheSizeAndItsThingKeepsItsOwn)
{
	FakeEffectServer server;
	Locator::effectRequestSystem::emplace<ecs::systems::EffectRequestSystem>(
	    [&server]() -> ecs::systems::EffectServerInterface* { return &server; });
	auto& registry = Locator::entitiesRegistry::value();
	const auto effect = Locator::effectRequestSystem::value().Start({.type = SpotVisualType::SingingStonesHeal});
	const auto thing = ecs::script_spot_visuals::MakeThing(registry, effect, glm::vec3(0.0f));
	const auto scaleBefore = registry.Get<ecs::components::Transform>(thing).scale;
	SetProperty(script::ObjectPropertyType::Scale, thing, 13.5f);
	// The visual is made 13.5 times its size, and the thing that holds it is not changed
	ASSERT_EQ(server.magnitudes.size(), 1u);
	EXPECT_EQ(server.magnitudes.front().first, effect);
	EXPECT_EQ(server.magnitudes.front().second, 13.5f);
	EXPECT_EQ(registry.Get<ecs::components::Transform>(thing).scale, scaleBefore);
	Locator::effectRequestSystem::reset();
}
