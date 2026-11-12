/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cstdint>

#include <optional>
#include <span>
#include <vector>

#include <gtest/gtest.h>

#include "ECS/Components/MagicPile.h"
#include "ECS/Components/Pot.h"
#include "ECS/PotCall.h"
#include "ECS/Registry.h"
#include "ECS/Systems/ReactionSystemInterface.h"

using namespace openblack;
using namespace openblack::ecs;
using namespace openblack::ecs::components;

namespace
{

/// Keeps what the calls of the pots ask of the reactions, and does nothing else
class CallReactions final: public systems::ReactionSystemInterface
{
public:
	uint32_t Create(const Source& source) override
	{
		made.push_back(source);
		return static_cast<uint32_t>(made.size());
	}
	void Move(uint32_t, const glm::vec3&, const glm::vec3&, float) override {}
	[[nodiscard]] std::optional<Active> Find(uint32_t) const override { return std::nullopt; }
	void RemoveFrom(entt::entity initiator) override { removedAll.push_back(initiator); }
	void RemoveFrom(entt::entity, Reaction) override { ++removedOfKind; }
	void Remove(uint32_t) override {}
	void SetInitiator(uint32_t, entt::entity) override {}
	void UpdateDeletionStop(entt::entity initiator) override { updated.push_back(initiator); }
	[[nodiscard]] bool IsActive(uint32_t) const override { return false; }
	void StopReacting(entt::entity) override {}
	[[nodiscard]] bool HasReaction(entt::entity) const override { return false; }
	void ProcessTurn() override {}
	void Reset() override {}
	void SetLandBalance(Balance, float) override {}
	[[nodiscard]] float GetLandBalance(Balance) const override { return 1.0f; }
	[[nodiscard]] std::span<const Active> GetReactions() const override { return {}; }
	[[nodiscard]] std::vector<Active> ReactionsAt(const glm::vec3&) const override { return {}; }

	std::vector<Source> made;
	std::vector<entt::entity> removedAll;
	std::vector<entt::entity> updated;
	int removedOfKind {0};
};

constexpr auto k_Pot = static_cast<entt::entity>(7);

systems::ReactionSystemInterface::Source CallOf(Reaction type)
{
	return {.initiator = k_Pot, .type = type, .player = PlayerNames::NEUTRAL, .position = glm::vec3(1.0f)};
}

} // namespace

TEST(PotCall, APotCallsOneTimeUntilTheRemovalOfItsCall)
{
	CallReactions reactions;
	Pot pot {.amount = 10, .maxAmount = 100, .type = PotInfo::FoodPile};
	EXPECT_TRUE(pot_call::Make(k_Pot, pot, reactions, CallOf(Reaction::ReactToFood)).has_value());
	EXPECT_TRUE(pot.calling);
	ASSERT_EQ(reactions.made.size(), 1u);
	EXPECT_EQ(reactions.made[0].type, Reaction::ReactToFood);
	// The pot keeps that it called after its call ends, so it does not call again
	EXPECT_FALSE(pot_call::Make(k_Pot, pot, reactions, CallOf(Reaction::ReactToFood)).has_value());
	EXPECT_EQ(reactions.made.size(), 1u);
	// After the removal of its call, it calls again
	pot_call::Remove(k_Pot, pot, reactions);
	EXPECT_FALSE(pot.calling);
	EXPECT_TRUE(pot_call::Make(k_Pot, pot, reactions, CallOf(Reaction::ReactToFood)).has_value());
	EXPECT_EQ(reactions.made.size(), 2u);
}

TEST(PotCall, TheRemovalOfTheCallEndsAllTheReactionsOfAPotThatCalled)
{
	CallReactions reactions;
	Pot pot {.amount = 10, .maxAmount = 100, .type = PotInfo::WoodPot};
	// A pot that did not call keeps its reactions
	pot_call::Remove(k_Pot, pot, reactions);
	EXPECT_TRUE(reactions.removedAll.empty());
	// A pot that called ends all its reactions, of each kind, and not only its call
	pot_call::Make(k_Pot, pot, reactions, CallOf(Reaction::ReactToWood));
	pot_call::Remove(k_Pot, pot, reactions);
	EXPECT_EQ(reactions.removedAll, std::vector<entt::entity> {k_Pot});
	EXPECT_EQ(reactions.removedOfKind, 0);
	// A second removal finds no call
	pot_call::Remove(k_Pot, pot, reactions);
	EXPECT_EQ(reactions.removedAll.size(), 1u);
}

TEST(PotCall, APotWithNoKindOfCallDoesNotCall)
{
	CallReactions reactions;
	Pot pot {.amount = 10, .maxAmount = 100, .type = PotInfo::FoodPot};
	EXPECT_FALSE(pot_call::Make(k_Pot, pot, reactions, CallOf(Reaction::None)).has_value());
	EXPECT_FALSE(pot.calling);
	EXPECT_TRUE(reactions.made.empty());
	EXPECT_FALSE(pot_call::Calls(false, Reaction::None));
	EXPECT_FALSE(pot_call::Calls(true, Reaction::ReactToFood));
	EXPECT_TRUE(pot_call::Calls(false, Reaction::ReactToFood));
}

TEST(PotCall, TheCallMakesTheOtherReactionsOfThePotStopTheirLivingWhenThePotGoes)
{
	CallReactions reactions;
	Pot pot {.amount = 10, .maxAmount = 100, .type = PotInfo::FoodPot};
	pot_call::Make(k_Pot, pot, reactions, CallOf(Reaction::ReactToFood));
	EXPECT_EQ(reactions.updated, std::vector<entt::entity> {k_Pot});
}

TEST(PotCall, TheCallOfAPileThatMagicMadeBelongsToThePlayerOfThePile)
{
	Registry registry;
	const auto loose = registry.Create();
	registry.Assign<Pot>(loose, Pot {.amount = 10, .maxAmount = 100, .type = PotInfo::HandFood});
	const auto magic = registry.Create();
	registry.Assign<Pot>(magic, Pot {.amount = 10, .maxAmount = 100, .type = PotInfo::MagicFood});
	registry.Assign<MagicPile>(magic, ResourceType::Food, PlayerNames::PLAYER_TWO);
	EXPECT_EQ(pot_call::FreePlayerOf(registry, loose), PlayerNames::NEUTRAL);
	EXPECT_EQ(pot_call::FreePlayerOf(registry, magic), PlayerNames::PLAYER_TWO);
	EXPECT_EQ(pot_call::FreePlayerOf(registry, entt::null), PlayerNames::NEUTRAL);
}
