/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <algorithm>
#include <optional>
#include <string_view>
#include <utility>
#include <vector>

#include <LHVM.h>
#include <gtest/gtest.h>
#include <spdlog/sinks/null_sink.h>
#include <spdlog/spdlog.h>

#include "CHLApi.h"
#include "ECS/Components/Mobile.h"
#include "ECS/Components/NeverFades.h"
#include "ECS/Registry.h"
#include "ECS/Systems/ScriptObjectsSystemInterface.h"
#include "ECS/VillagerScriptRules.h"
#include "Enums.h"
#include "Help/Spirits.h"
#include "Locator.h"
#include "ScriptHeaders/ScriptEnums.h"

using namespace openblack;

// The natives the Throwing Stones challenge asks of its boulder and of the stones the player throws at it

namespace
{
/// Every object a script names is itself, as the scripts' table gives back what it holds
class StonesScriptObjects final: public ecs::systems::ScriptObjectsSystemInterface
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

class ScriptStonesNatives: public ::testing::Test
{
protected:
	void SetUp() override
	{
		// The natives report a bad call to the scripts' log
		if (spdlog::get("scripting") == nullptr)
		{
			spdlog::create<spdlog::sinks::null_sink_mt>("scripting");
		}
		Locator::entitiesRegistry::emplace();
		Locator::scriptObjects::emplace<StonesScriptObjects>();
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

	/// The native's number in the scripts' table
	[[nodiscard]] uint32_t Native(std::string_view name)
	{
		const auto& table = _api.GetFunctionsTable();
		const auto found = std::ranges::find_if(table, [name](const auto& function) { return function.name == name; });
		return static_cast<uint32_t>(std::distance(table.begin(), found));
	}

	static lhvm::NativeArgument Object(entt::entity object)
	{
		return std::pair(lhvm::VMValue(static_cast<uint32_t>(object)), lhvm::DataType::Object);
	}
	static lhvm::NativeArgument Int(int32_t value) { return std::pair(lhvm::VMValue(value), lhvm::DataType::Int); }

	/// A script's "OBJECT type TYPE SUBTYPE"
	[[nodiscard]] bool IsOfType(entt::entity object, script::ObjectType type, int32_t subtype)
	{
		return IsOfType(object, static_cast<int32_t>(type), subtype);
	}
	[[nodiscard]] bool IsOfType(entt::entity object, int32_t type, int32_t subtype)
	{
		const std::vector<lhvm::NativeArgument> arguments {Object(object), Int(type), Int(subtype)};
		Results results;
		EXPECT_TRUE(Locator::vm::value().CallNative(Native("IS_OF_TYPE"), arguments, results));
		EXPECT_EQ(results.size(), 1u);
		return !results.empty() && results.front().first.floatVal != 0.0f;
	}

	/// A script's "enable/disable jc special SPECIAL on OBJECT"
	void ThingJcSpecial(bool on, int32_t special, entt::entity object)
	{
		const std::vector<lhvm::NativeArgument> arguments {Int(on ? 1 : 0), Int(special), Object(object)};
		Results results;
		EXPECT_TRUE(Locator::vm::value().CallNative(Native("THING_JC_SPECIAL"), arguments, results));
		EXPECT_TRUE(results.empty());
	}

	/// A mobile static of a kind, a rock when it is of the rocks' class
	[[nodiscard]] static entt::entity MobileStaticOf(MobileStaticInfo info, bool rock)
	{
		auto& registry = Locator::entitiesRegistry::value();
		const auto entity = registry.Create();
		registry.Assign<ecs::components::MobileStatic>(entity, info);
		if (rock)
		{
			registry.Assign<ecs::components::Rock>(entity);
		}
		return entity;
	}

	chlapi::CHLApi _api;
};
} // namespace

// The fisherman watches only what the player holds that is a rock of the rocks' kind; the subtype 5000 is any
// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables): external macro
TEST_F(ScriptStonesNatives, IsOfTypeMatchesTheTypeAndTheSubtypeOrAny)
{
	using script::ObjectType;
	const auto stone = MobileStaticOf(MobileStaticInfo::Rock, true);
	const auto rock = static_cast<int32_t>(MobileStaticInfo::Rock);
	EXPECT_TRUE(IsOfType(stone, ObjectType::Rock, rock));
	EXPECT_TRUE(IsOfType(stone, ObjectType::Rock, 5000));
	EXPECT_FALSE(IsOfType(stone, ObjectType::Rock, static_cast<int32_t>(MobileStaticInfo::Boulder3Lime)));
	EXPECT_FALSE(IsOfType(stone, ObjectType::MobileStatic, rock));

	const auto fence = MobileStaticOf(MobileStaticInfo::CeltFenceShort, false);
	EXPECT_TRUE(IsOfType(fence, ObjectType::MobileStatic, 5000));
	EXPECT_FALSE(IsOfType(fence, ObjectType::Rock, 5000));
}

// Types out of range and things that are gone are never of a type
// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables): external macro
TEST_F(ScriptStonesNatives, IsOfTypeIsFalseForABadTypeOrNoThing)
{
	const auto stone = MobileStaticOf(MobileStaticInfo::Rock, true);
	EXPECT_FALSE(IsOfType(stone, 0, 5000));
	EXPECT_FALSE(IsOfType(stone, 42, 5000));
	// A script's object 0 is none
	EXPECT_FALSE(IsOfType(static_cast<entt::entity>(0), script::ObjectType::Rock, 5000));
	// A thing no longer in the world
	EXPECT_FALSE(IsOfType(static_cast<entt::entity>(1234), script::ObjectType::Rock, 5000));
}

// The challenge keeps its boulder in view however far off the camera goes, whatever the order's on/off says; any thing
// takes it, not only those drawn in high detail
// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables): external macro
TEST_F(ScriptStonesNatives, AlwaysVisibleKeepsAnyThingFromFading)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto boulder = MobileStaticOf(MobileStaticInfo::Boulder3Lime, true);
	const auto other = MobileStaticOf(MobileStaticInfo::Boulder3Lime, true);
	EXPECT_FALSE(registry.AllOf<ecs::components::NeverFades>(boulder));
	ThingJcSpecial(true, 19, boulder);
	EXPECT_TRUE(registry.AllOf<ecs::components::NeverFades>(boulder));
	ThingJcSpecial(false, 19, other);
	EXPECT_TRUE(registry.AllOf<ecs::components::NeverFades>(other));
	// Asking again changes nothing
	ThingJcSpecial(true, 19, boulder);
	EXPECT_TRUE(registry.AllOf<ecs::components::NeverFades>(boulder));
}

// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables): external macro
TEST_F(ScriptStonesNatives, TakeAndGiveWhatTheGameDoes)
{
	const auto& table = _api.GetFunctionsTable();
	EXPECT_EQ(Native("IS_OF_TYPE"), 192u);
	EXPECT_EQ(table[192].stackIn, 3);
	EXPECT_EQ(table[192].stackOut, 1u);
	EXPECT_EQ(Native("LOOK_GAME_THING"), 100u);
	EXPECT_EQ(table[100].stackIn, 2);
	EXPECT_EQ(table[100].stackOut, 0u);
	EXPECT_EQ(Native("THING_JC_SPECIAL"), 349u);
	EXPECT_EQ(table[349].stackIn, 3);
	EXPECT_EQ(table[349].stackOut, 0u);
}

// The challenge waits while "[Rock1] near [Rock1Pos] radius 2". The boulder stands where the land's script puts it,
// (2030.81, 2325.50), 10.18 above the land; the marker is at (2031.129, 12.272, 2327.405). The game measures along the
// ground only, so the boulder is 1.9316 from the marker however high either stands (1.975 counting heights), and it
// is on its pillar until something moves it 0.07 across the ground
// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables): external macro
TEST(ScriptStonesBoulder, TheBoulderIsNearItsMarkerAlongTheGround)
{
	const glm::vec3 marker {2031.129f, 12.272f, 2327.405f};
	const glm::vec3 boulder {2030.81f, 1.675f + 10.184691f, 2325.50f};
	const float distance = ecs::villager_script_rules::ScriptDistance(boulder, marker);
	// Within the rounding of single-precision positions this far from the map's corner
	EXPECT_NEAR(distance, 1.9316f, 0.001f);
	EXPECT_LT(distance, 2.0f);
	// Heights don't count
	EXPECT_FLOAT_EQ(ecs::villager_script_rules::ScriptDistance({boulder.x, 100.0f, boulder.z}, marker), distance);
	// Knocked 0.07 further across the ground, it is off
	EXPECT_GT(ecs::villager_script_rules::ScriptDistance({boulder.x, boulder.y, boulder.z - 0.07f}, marker), 2.0f);
}

namespace
{
/// The advisors, with one thing in the world they can be asked to look at, object 7
struct LookingAdvisors
{
	std::optional<help::spirits::ObjectInfo> thing {
	    help::spirits::ObjectInfo {.position = {10.0f, 2.0f, 30.0f}, .height = 4.0f}};
	help::spirits::DudeData good;
	help::spirits::DudeData evil;
	help::spirits::AdvisorSpiritController control {good, evil, MakeQueries(), help::spirits::Screen {640, 480}};

	help::spirits::Queries MakeQueries()
	{
		help::spirits::Queries queries;
		queries.localRand = [](int32_t) { return 0u; };
		queries.localFloatRand = [](float) { return 0.0f; };
		queries.random = [](float a, float) { return a; };
		queries.object = [this](uint32_t object) { return object == 7 ? thing : std::nullopt; };
		return queries;
	}
};
} // namespace

// The good advisor told to look at the boulder comes out and keeps looking at it until it is gone
// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables): external macro
TEST(ScriptStonesLook, AnAdvisorComesOutAndLooksAtAThingUntilItIsGone)
{
	LookingAdvisors advisors;
	constexpr int32_t k_Good = 1;
	constexpr int k_GoodDude = 0;
	EXPECT_FALSE(advisors.control.LookOn(k_GoodDude));
	EXPECT_EQ(advisors.control.State(k_GoodDude), help::spirits::ControlState::Home);

	// Nothing to look at: nothing happens
	advisors.control.SpiritLookObject(k_Good, 8);
	EXPECT_FALSE(advisors.control.LookOn(k_GoodDude));
	EXPECT_EQ(advisors.control.State(k_GoodDude), help::spirits::ControlState::Home);

	advisors.control.SpiritLookObject(k_Good, 7);
	EXPECT_TRUE(advisors.control.LookOn(k_GoodDude));
	EXPECT_NE(advisors.control.State(k_GoodDude), help::spirits::ControlState::Home);
	EXPECT_FALSE(advisors.control.LookOn(1));

	advisors.control.ProcessTurn();
	EXPECT_TRUE(advisors.control.LookOn(k_GoodDude));
	advisors.thing.reset();
	advisors.control.ProcessTurn();
	EXPECT_FALSE(advisors.control.LookOn(k_GoodDude));
}
