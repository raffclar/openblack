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

#include <gtest/gtest.h>

#include "ECS/Components/Creature.h"
#include "ECS/Components/CreatureTallies.h"
#include "ECS/Registry.h"
#include "ECS/Statistics.h"
#include "ECS/Systems/PlayerSystemInterface.h"
#include "Locator.h"

using namespace openblack;
using namespace openblack::ecs;
using namespace openblack::ecs::components;
using namespace openblack::ecs::systems;

namespace
{
/// The players of the test: player one has one creature
class OnePlayer final: public PlayerSystemInterface
{
public:
	void RegisterPlayers() override {}
	void AddPlayer(entt::entity /*playerEntity*/) override {}
	[[nodiscard]] std::optional<entt::entity> FindPlayer(PlayerNames /*name*/) const override { return std::nullopt; }
	entt::entity EnsurePlayer(PlayerNames /*name*/) override { return entt::null; }
	void AddCreature(entt::entity /*creature*/) override {}
	[[nodiscard]] std::optional<entt::entity> GetPrimaryCreature(PlayerNames name) const override
	{
		return name == PlayerNames::PLAYER_ONE ? std::optional(creature) : std::nullopt;
	}
	void KeepForNextLand() override {}
	void ForgetKept() override {}
	[[nodiscard]] bool KeepAlignmentAt(uint32_t /*turn*/) override { return false; }
	void TakeUpKept(entt::entity /*playerEntity*/) override {}

	entt::entity creature {entt::null};
};

class CreatureMushroomCountTest: public ::testing::Test
{
protected:
	void SetUp() override
	{
		auto& registry = Locator::entitiesRegistry::emplace<Registry>();
		auto& players = static_cast<OnePlayer&>(Locator::playerSystem::emplace<OnePlayer>());
		creature = registry.Create();
		registry.Assign<Creature>(creature);
		players.creature = creature;
	}
	void TearDown() override
	{
		Locator::playerSystem::reset();
		Locator::entitiesRegistry::reset();
	}

	entt::entity creature {entt::null};
};

} // namespace

// The game keeps one count of the magic mushrooms that a creature ate. The creature's mind and its player's statistics
// read the same count.
TEST_F(CreatureMushroomCountTest, TheStatisticsShowTheCountOfTheCreature)
{
	auto& registry = Locator::entitiesRegistry::value();
	registry.Get<Creature>(creature).magicMushroomsEaten = 205;

	EXPECT_EQ(player_statistics::CreatureTalliesOf(PlayerNames::PLAYER_ONE).mushroomsEaten, 205u);
}

// A count that changes on the creature, as after a mind load or a swap of minds, changes in the statistics too
TEST_F(CreatureMushroomCountTest, TheStatisticsFollowTheCountAfterItChanges)
{
	auto& registry = Locator::entitiesRegistry::value();
	registry.Assign<CreatureTallies>(creature).poos = 3;
	registry.Get<Creature>(creature).magicMushroomsEaten = 7;
	EXPECT_EQ(player_statistics::CreatureTalliesOf(PlayerNames::PLAYER_ONE).mushroomsEaten, 7u);

	registry.Get<Creature>(creature).magicMushroomsEaten = 100;
	const auto tallies = player_statistics::CreatureTalliesOf(PlayerNames::PLAYER_ONE);
	EXPECT_EQ(tallies.mushroomsEaten, 100u);
	EXPECT_EQ(tallies.poos, 3u);
}
