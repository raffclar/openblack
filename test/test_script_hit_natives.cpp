/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <algorithm>
#include <string_view>
#include <utility>
#include <vector>

#include <LHVM.h>
#include <gtest/gtest.h>

#include "CHLApi.h"
#include "ECS/Registry.h"
#include "ECS/Systems/DynamicsSystemInterface.h"
#include "ECS/Systems/ScriptObjectsSystemInterface.h"
#include "Locator.h"

using namespace openblack;

namespace
{
/// Keeps only the thing last hit and what hit it, as the physics records them for the scripts
class FakeHitDynamics final: public ecs::systems::DynamicsSystemInterface
{
public:
	void RecordHit(entt::entity hit, entt::entity hitter) override
	{
		_hit = hit;
		_hitter = hitter;
	}
	[[nodiscard]] entt::entity GetHitObject() const override { return _hit; }
	[[nodiscard]] entt::entity GetObjectWhichHit() const override { return _hitter; }

private:
	entt::entity _hit {entt::null};
	entt::entity _hitter {entt::null};
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

class ScriptHitNatives: public ::testing::Test
{
protected:
	void SetUp() override
	{
		Locator::entitiesRegistry::emplace();
		Locator::dynamicsSystem::emplace<FakeHitDynamics>();
		Locator::scriptObjects::emplace<PassThroughScriptObjects>();
		Locator::vm::emplace();
		Locator::vm::value().Initialise(&_api.GetFunctionsTable(), nullptr, nullptr, nullptr, nullptr, nullptr, nullptr);
	}

	void TearDown() override
	{
		Locator::vm::reset();
		Locator::scriptObjects::reset();
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

	/// Asks a script's question of an object: whether it is the thing hit
	[[nodiscard]] bool IsHit(entt::entity object)
	{
		const std::vector<std::pair<lhvm::VMValue, lhvm::DataType>> arguments {
		    {lhvm::VMValue(static_cast<uint32_t>(object)), lhvm::DataType::Object}};
		std::vector<std::pair<lhvm::VMValue, lhvm::DataType>> results;
		EXPECT_TRUE(Locator::vm::value().CallNative(Native("GAME_THING_HIT"), arguments, results));
		EXPECT_EQ(results.size(), 1u);
		return !results.empty() && results.front().first.floatVal != 0.0f;
	}

	void ClearHit()
	{
		std::vector<std::pair<lhvm::VMValue, lhvm::DataType>> results;
		EXPECT_TRUE(Locator::vm::value().CallNative(Native("CLEAR_HIT_OBJECT"), {}, results));
	}

	chlapi::CHLApi _api;
};
} // namespace

// The creature gates: a script asks every turn whether the gates were hit, so the advisor can say battering them won't
// work. Only the thing the physics last saw hit counts, until a script clears it
// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables): external macro
TEST_F(ScriptHitNatives, AThingIsHitOnlyUntilAScriptClearsIt)
{
	auto& registry = Locator::entitiesRegistry::value();
	// A script's object 0 is none, so nothing the scripts are given is the registry's first entity
	[[maybe_unused]] const auto none = registry.Create();
	const auto gates = registry.Create();
	const auto stone = registry.Create();
	EXPECT_FALSE(IsHit(gates));

	Locator::dynamicsSystem::value().RecordHit(gates, stone);
	EXPECT_TRUE(IsHit(gates));
	EXPECT_FALSE(IsHit(stone));
	// Asking doesn't clear it
	EXPECT_TRUE(IsHit(gates));

	ClearHit();
	EXPECT_FALSE(IsHit(gates));
}

// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables): external macro
TEST_F(ScriptHitNatives, TakeAndGiveWhatTheGameDoes)
{
	const auto& table = _api.GetFunctionsTable();
	ASSERT_LT(Native("GAME_THING_HIT"), table.size());
	EXPECT_EQ(Native("GAME_THING_HIT"), 194u);
	EXPECT_EQ(table[194].stackIn, 1);
	EXPECT_EQ(table[194].stackOut, 1u);
	EXPECT_EQ(Native("CLEAR_HIT_OBJECT"), 193u);
}
