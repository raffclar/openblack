/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#define LOCATOR_IMPLEMENTATIONS

#include <cstddef>
#include <cstdint>

#include <map>
#include <optional>
#include <string_view>
#include <utility>
#include <vector>

#include <LHVM.h>
#include <gtest/gtest.h>

#include "CHLApi.h"
#include "ECS/Registry.h"
#include "ECS/Systems/PlayerSystemInterface.h"
#include "Locator.h"

using namespace openblack;

namespace
{
/// Players that keep each alliance that a script sets, and give it back when a script asks
class FakeAllies final: public ecs::systems::PlayerSystemInterface
{
public:
	void RegisterPlayers() override {}
	void AddPlayer(entt::entity /*playerEntity*/) override {}
	[[nodiscard]] std::optional<entt::entity> FindPlayer(PlayerNames /*name*/) const override { return std::nullopt; }
	entt::entity EnsurePlayer(PlayerNames /*name*/) override { return entt::null; }
	void AddCreature(entt::entity /*creature*/) override {}
	[[nodiscard]] std::optional<entt::entity> GetPrimaryCreature(PlayerNames /*name*/) const override { return std::nullopt; }
	void KeepForNextLand() override {}
	void ForgetKept() override {}
	[[nodiscard]] bool KeepAlignmentAt(uint32_t /*turn*/) override { return false; }
	void TakeUpKept(entt::entity /*playerEntity*/) override {}
	void SetAlly(PlayerNames player, PlayerNames other, float strength) override
	{
		set.push_back({player, other, strength});
		strengths[{player, other}] = strength;
		strengths[{other, player}] = strength;
	}
	[[nodiscard]] float AllyStrength(PlayerNames player, PlayerNames other) const override
	{
		asked.emplace_back(player, other);
		const auto found = strengths.find({player, other});
		return found != strengths.end() ? found->second : 0.0f;
	}
	[[nodiscard]] size_t PlayerNumbersInGame() const override { return 8; }

	struct Set
	{
		PlayerNames player;
		PlayerNames other;
		float strength;
	};
	std::vector<Set> set;
	std::map<std::pair<PlayerNames, PlayerNames>, float> strengths;
	mutable std::vector<std::pair<PlayerNames, PlayerNames>> asked;
};

using Results = std::vector<std::pair<lhvm::VMValue, lhvm::DataType>>;

constexpr uint32_t k_SetPlayerAlly = 382;
constexpr uint32_t k_GetPlayerAlly = 398;

class ScriptAllyNatives: public ::testing::Test
{
protected:
	void SetUp() override
	{
		Locator::entitiesRegistry::emplace();
		Locator::playerSystem::emplace<FakeAllies>();
		Locator::vm::emplace();
		Locator::vm::value().Initialise(&_api.GetFunctionsTable(), nullptr, nullptr, nullptr, nullptr, nullptr, nullptr);
	}

	void TearDown() override
	{
		Locator::vm::reset();
		Locator::playerSystem::reset();
		Locator::entitiesRegistry::reset();
	}

	static FakeAllies& Allies() { return static_cast<FakeAllies&>(Locator::playerSystem::value()); }

	static lhvm::NativeArgument Float(float value) { return std::pair(lhvm::VMValue(value), lhvm::DataType::Float); }

	static void SetAlly(float player, float other, float strength)
	{
		Results results;
		const std::vector<lhvm::NativeArgument> arguments {Float(player), Float(other), Float(strength)};
		EXPECT_TRUE(Locator::vm::value().CallNative(k_SetPlayerAlly, arguments, results));
		EXPECT_TRUE(results.empty());
	}

	static float GetAlly(float player, float other)
	{
		Results results;
		const std::vector<lhvm::NativeArgument> arguments {Float(player), Float(other)};
		EXPECT_TRUE(Locator::vm::value().CallNative(k_GetPlayerAlly, arguments, results));
		EXPECT_EQ(results.size(), 1u);
		return results.empty() ? -1.0f : results.front().first.floatVal;
	}

	chlapi::CHLApi _api;
};
} // namespace

// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables): external macro
TEST_F(ScriptAllyNatives, TheTwoCommandsHaveTheirVanillaNamesAndStacks)
{
	const auto& table = _api.GetFunctionsTable();
	ASSERT_GT(table.size(), k_GetPlayerAlly);
	EXPECT_EQ(std::string_view(table[k_SetPlayerAlly].name), "SET_PLAYER_ALLY");
	EXPECT_EQ(table[k_SetPlayerAlly].stackIn, 3);
	EXPECT_EQ(table[k_SetPlayerAlly].stackOut, 0u);
	EXPECT_EQ(std::string_view(table[k_GetPlayerAlly].name), "GET_PLAYER_ALLY");
	EXPECT_EQ(table[k_GetPlayerAlly].stackIn, 2);
	EXPECT_EQ(table[k_GetPlayerAlly].stackOut, 1u);
}

// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables): external macro
TEST_F(ScriptAllyNatives, AScriptSetsAnAllianceAndAsksForIt)
{
	// Script player 1 is the first player and 2 the second
	SetAlly(1.0f, 2.0f, 1.0f);
	ASSERT_EQ(Allies().set.size(), 1u);
	EXPECT_EQ(Allies().set[0].player, PlayerNames::PLAYER_TWO);
	EXPECT_EQ(Allies().set[0].other, PlayerNames::PLAYER_ONE);
	EXPECT_EQ(Allies().set[0].strength, 1.0f);
	EXPECT_EQ(GetAlly(1.0f, 2.0f), 1.0f);
	EXPECT_EQ(GetAlly(2.0f, 1.0f), 1.0f);
	// The question asks the second player for the first
	ASSERT_FALSE(Allies().asked.empty());
	EXPECT_EQ(Allies().asked.front(), std::pair(PlayerNames::PLAYER_TWO, PlayerNames::PLAYER_ONE));
}

// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables): external macro
TEST_F(ScriptAllyNatives, AScriptResetsAnAllianceToZero)
{
	// A later land's script ends the alliance with a strength of 0
	SetAlly(1.0f, 2.0f, 1.0f);
	SetAlly(1.0f, 2.0f, 0.0f);
	EXPECT_EQ(GetAlly(1.0f, 2.0f), 0.0f);
	EXPECT_EQ(GetAlly(2.0f, 1.0f), 0.0f);
}

// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables): external macro
TEST_F(ScriptAllyNatives, ScriptPlayerZeroIsTheFirstPlayer)
{
	SetAlly(0.0f, 3.0f, 0.5f);
	ASSERT_EQ(Allies().set.size(), 1u);
	EXPECT_EQ(Allies().set[0].player, PlayerNames::PLAYER_THREE);
	EXPECT_EQ(Allies().set[0].other, PlayerNames::PLAYER_ONE);
	EXPECT_EQ(GetAlly(0.0f, 3.0f), 0.5f);
}

// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables): external macro
TEST_F(ScriptAllyNatives, ANumberThatNamesNoPlayerDoesNothingAndGivesZero)
{
	SetAlly(1.0f, 9.0f, 1.0f);
	SetAlly(-1.0f, 2.0f, 1.0f);
	EXPECT_TRUE(Allies().set.empty());
	EXPECT_EQ(GetAlly(1.0f, 9.0f), 0.0f);
	EXPECT_TRUE(Allies().asked.empty());
}
