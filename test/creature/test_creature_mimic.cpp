/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// What the game tells the players' creatures (ECS/CreatureMimic.h): the deeds published with their player, object,
// place and miracle, the town needs shared only with a player, the handler that passes the deeds on to the minds, the
// minds' own filter (only the player's own creature watches, and only it draws), and two of the game's sites: a
// field watered by a player's miracle and the deed numbers the game's table is read by

#define LOCATOR_IMPLEMENTATIONS

#include <cstddef>
#include <cstdint>

#include <optional>
#include <tuple>
#include <utility>
#include <vector>

#include <entt/core/fwd.hpp>
#include <entt/entity/entity.hpp>
#include <entt/entity/sparse_set.hpp>
#include <glm/mat3x3.hpp>
#include <glm/vec3.hpp>
#include <gtest/gtest.h>

#include "Common/GameRandom.h"
#include "Common/GameRandomTesting.h"
#include "Creature/CreatureDeeds.h"
#include "ECS/Components/Field.h"
#include "ECS/Components/Spell.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/Villager.h"
#include "ECS/Components/VillagerLastInteraction.h"
#include "ECS/CreatureMimic.h"
#include "ECS/Events/CreatureMimicEvents.h"
#include "ECS/Physics/PhysicsObjects.h"
#include "ECS/Systems/Implementations/CreatureMindSystem.h"
#include "Enums.h"
#include "InfoConstants.h"
#include "Locator.h"
#include "Magic/Spells/SpellWater.h"
#include "creature/CreatureSystemWorld.h"
#include "support/CreatureFakes.h"
#include "support/RestoreService.h"
#include "support/WorldSystems.h"

using namespace openblack;
using namespace openblack::ecs::components;
using openblack::creature_watching::Deed;
using openblack::ecs::events::CreatureEmpathyWithTownDesire;
using openblack::ecs::events::PlayerDeedForMimic;
using openblack::ecs::physics::PhysicsObject;
namespace creature_mimic = openblack::ecs::creature_mimic;

namespace
{
/// The registry's storages, each with how many it holds, and the three random streams' seeds
struct Snapshot
{
	std::vector<std::pair<entt::id_type, size_t>> storages;
	uint32_t synced {0};
	uint32_t local {0};
	uint32_t crt {0};

	bool operator==(const Snapshot&) const = default;
};

Snapshot Take()
{
	Snapshot snapshot;
	std::as_const(test::creature_world::World::Registry())
	    .EachStorage([&snapshot](entt::id_type id, const entt::sparse_set& storage) {
		    snapshot.storages.emplace_back(id, storage.size());
	    });
	snapshot.synced = game_random::Current().synced;
	snapshot.local = game_random::Current().local;
	snapshot.crt = game_random::crt::Seed();
	return snapshot;
}

/// What is published, as the tests' own handlers hear it (the game's handler is added by the tests that want it)
struct Heard
{
	Heard()
	{
		auto& events = Locator::events::value();
		events.AddHandler<PlayerDeedForMimic>([this](const PlayerDeedForMimic& event) { deeds.push_back(event); });
		events.AddHandler<CreatureEmpathyWithTownDesire>(
		    [this](const CreatureEmpathyWithTownDesire& event) { empathies.push_back(event); });
	}
	Heard(const Heard&) = delete;
	Heard& operator=(const Heard&) = delete;

	std::vector<PlayerDeedForMimic> deeds;
	std::vector<CreatureEmpathyWithTownDesire> empathies;
};

entt::entity MakeThing(std::optional<glm::vec3> position)
{
	auto& registry = test::creature_world::World::Registry();
	const auto entity = registry.Create();
	if (position.has_value())
	{
		registry.Assign<Transform>(entity, *position, glm::mat3(1.0f), glm::vec3(1.0f));
	}
	return entity;
}

/// A registry, the tables and the events of their own
class CreatureMimicTest: public ::testing::Test
{
protected:
	test::creature_world::World _world;
	Heard _heard;
	std::vector<PlayerDeedForMimic>& deeds = _heard.deeds;
	std::vector<CreatureEmpathyWithTownDesire>& empathies = _heard.empathies;
};

/// The same with the world's lists too (the fires), for the game's own sites
class CreatureMimicSiteTest: public ::testing::Test
{
protected:
	const test::ScopedWorldSystems _worldSystems;
	test::creature_world::World _world;
	Heard _heard;
	std::vector<PlayerDeedForMimic>& deeds = _heard.deeds;
};
} // namespace

TEST(CreatureDeeds, TheRowsOfTheGamesTable)
{
	EXPECT_EQ(static_cast<size_t>(Deed::PutFoodInWorshipSite), 0u);
	EXPECT_EQ(static_cast<size_t>(Deed::PlantTree), 11u);
	EXPECT_EQ(static_cast<size_t>(Deed::DamageByThrowingAt), 16u);
	EXPECT_EQ(static_cast<size_t>(Deed::ThrowInTheSea), 21u);
	EXPECT_EQ(static_cast<size_t>(Deed::CastWaterOnCrops), 33u);
	EXPECT_EQ(static_cast<size_t>(Deed::Sacrifice), 40u);
	EXPECT_EQ(static_cast<size_t>(Deed::PlayWithToy), 41u);
	EXPECT_EQ(static_cast<size_t>(Deed::StealWoodFromStoragePit), 45u);
	EXPECT_EQ(creature_watching::k_DeedCount, 46u);
	EXPECT_EQ(std::tuple_size_v<decltype(InfoConstants::creatureMimic)>, creature_watching::k_DeedCount);
}

TEST(CreatureMimicDecision, ABuildingHitCountsOnlyWithAPlayerAndItsOwnBodyFromTheHand)
{
	EXPECT_FALSE(creature_mimic::ShouldMimicBuildingHit(std::nullopt, PhysicsObject::k_FromHand));
	EXPECT_FALSE(creature_mimic::ShouldMimicBuildingHit(PlayerNames::PLAYER_ONE, 0));
	EXPECT_FALSE(creature_mimic::ShouldMimicBuildingHit(PlayerNames::PLAYER_ONE,
	                                                    PhysicsObject::k_Awake | PhysicsObject::k_NoObjectCollision));
	EXPECT_TRUE(creature_mimic::ShouldMimicBuildingHit(PlayerNames::PLAYER_ONE, PhysicsObject::k_FromHand));
	EXPECT_TRUE(
	    creature_mimic::ShouldMimicBuildingHit(PlayerNames::PLAYER_TWO, PhysicsObject::k_Awake | PhysicsObject::k_FromHand));
}

TEST_F(CreatureMimicTest, ADeedIsPublishedOnceWithItsPlayerObjectPlaceAndMiracle)
{
	const auto tree = MakeThing(glm::vec3(12.0f, 3.0f, -7.0f));
	creature_mimic::Consider(PlayerNames::PLAYER_TWO, Deed::PlantTree, tree);
	ASSERT_EQ(deeds.size(), 1u);
	EXPECT_EQ(deeds.front().player, PlayerNames::PLAYER_TWO);
	EXPECT_EQ(deeds.front().deed, Deed::PlantTree);
	EXPECT_EQ(deeds.front().object, tree);
	EXPECT_EQ(deeds.front().point, glm::vec3(12.0f, 3.0f, -7.0f));
	EXPECT_FALSE(deeds.front().magic.has_value());

	creature_mimic::Consider(PlayerNames::PLAYER_ONE, Deed::CastWaterOnCrops, tree, MagicType::Water);
	ASSERT_EQ(deeds.size(), 2u);
	ASSERT_TRUE(deeds.back().magic.has_value());
	EXPECT_EQ(*deeds.back().magic, MagicType::Water);
	EXPECT_TRUE(empathies.empty());
}

TEST_F(CreatureMimicTest, SomethingWithNoPlaceIsAtTheOrigin)
{
	const auto nowhere = MakeThing(std::nullopt);
	creature_mimic::Consider(PlayerNames::PLAYER_ONE, Deed::DamageByThrowingAt, nowhere);
	creature_mimic::Consider(PlayerNames::PLAYER_ONE, Deed::DamageByThrowingAt, entt::null);
	ASSERT_EQ(deeds.size(), 2u);
	EXPECT_EQ(deeds.at(0).point, glm::vec3(0.0f));
	EXPECT_EQ(deeds.at(1).point, glm::vec3(0.0f));
	EXPECT_TRUE(deeds.at(1).object == entt::null);
}

TEST_F(CreatureMimicTest, AVillagerInTheSeaReportsTheHandThatLastDroppedIt)
{
	auto& registry = test::creature_world::World::Registry();
	const auto thrown = MakeThing(glm::vec3(5.0f, 0.0f, 5.0f));
	registry.Assign<Villager>(thrown);
	registry.AssignState<VillagerLastInteraction>(thrown, PlayerNames::PLAYER_ONE);
	const auto walked = MakeThing(glm::vec3(6.0f, 0.0f, 6.0f));
	registry.Assign<Villager>(walked);

	creature_mimic::ConsiderThrownInTheSea(walked);
	EXPECT_TRUE(deeds.empty());
	creature_mimic::ConsiderThrownInTheSea(thrown);
	ASSERT_EQ(deeds.size(), 1u);
	EXPECT_EQ(deeds.front().player, PlayerNames::PLAYER_ONE);
	EXPECT_EQ(deeds.front().deed, Deed::ThrowInTheSea);
	EXPECT_EQ(deeds.front().object, thrown);
	EXPECT_EQ(deeds.front().point, glm::vec3(5.0f, 0.0f, 5.0f));
}

TEST_F(CreatureMimicTest, ATownNeedIsSharedOnlyWithAPlayer)
{
	const auto villager = MakeThing(glm::vec3(1.0f, 2.0f, 3.0f));
	creature_mimic::EmpathiseWithTownDesire(std::nullopt, TownDesireInfo::ForFood, 0.5f, villager);
	EXPECT_TRUE(empathies.empty());

	creature_mimic::EmpathiseWithTownDesire(PlayerNames::PLAYER_ONE, TownDesireInfo::ForWood, 0.5f, villager);
	ASSERT_EQ(empathies.size(), 1u);
	EXPECT_EQ(empathies.front().player, PlayerNames::PLAYER_ONE);
	EXPECT_EQ(empathies.front().desire, TownDesireInfo::ForWood);
	EXPECT_FLOAT_EQ(empathies.front().weight, 0.5f);
	EXPECT_EQ(empathies.front().point, glm::vec3(1.0f, 2.0f, 3.0f));
	EXPECT_TRUE(deeds.empty());
}

TEST_F(CreatureMimicTest, TheHandlerPassesTheDeedOnToTheMinds)
{
	const test::RestoreService<Locator::creatureMindSystem> restoreMind;
	test::creature_loop_fakes::CallLog log;
	Locator::creatureMindSystem::emplace<test::creature_loop_fakes::FakeMind>(log);
	creature_mimic::AddMimicEventHandlers(Locator::events::value());
	const auto field = MakeThing(glm::vec3(4.0f, 0.5f, 8.0f));

	creature_mimic::Consider(PlayerNames::PLAYER_TWO, Deed::CastWaterOnCrops, field, MagicType::Water);
	creature_mimic::Consider(PlayerNames::PLAYER_ONE, Deed::DamageByThrowingAt, entt::null);

	ASSERT_EQ(log.size(), 2u);
	EXPECT_EQ(log.at(0).name, "mind.PlayerDid");
	EXPECT_EQ(log.at(0).args, (std::vector<float> {static_cast<float>(PlayerNames::PLAYER_TWO), 33.0f, 4.0f, 0.5f, 8.0f,
	                                               static_cast<float>(entt::to_integral(field))}));
	// no object: none passed on
	EXPECT_EQ(log.at(1).args,
	          (std::vector<float> {static_cast<float>(PlayerNames::PLAYER_ONE), 16.0f, 0.0f, 0.0f, 0.0f, -1.0f}));
}

TEST_F(CreatureMimicTest, WithoutMindsTheHandlerDoesNothing)
{
	const test::RestoreService<Locator::creatureMindSystem> restoreMind;
	Locator::creatureMindSystem::reset();
	creature_mimic::AddMimicEventHandlers(Locator::events::value());
	const auto before = Take();
	creature_mimic::Consider(PlayerNames::PLAYER_ONE, Deed::PlantTree, MakeThing(glm::vec3(1.0f)));
	EXPECT_EQ(deeds.size(), 1u);
	const auto after = Take();
	// nothing drawn: the thing made for the deed is the only change
	EXPECT_EQ(after.synced, before.synced);
	EXPECT_EQ(after.local, before.local);
	EXPECT_EQ(after.crt, before.crt);
}

TEST_F(CreatureMimicTest, WithNoCreatureTheMindsMakeNothingAndDrawNothing)
{
	const game_random::testing::ScopedState state;
	ecs::systems::CreatureMindSystem minds;
	auto& registry = test::creature_world::World::Registry();
	const auto villager = MakeThing(glm::vec3(10.0f, 0.0f, 10.0f));
	registry.Assign<Villager>(villager);
	const auto before = Take();
	minds.PlayerDid(PlayerNames::PLAYER_ONE, static_cast<size_t>(Deed::PlantTree), glm::vec3(10.0f, 0.0f, 10.0f), villager);
	EXPECT_EQ(Take(), before);
}

TEST_F(CreatureMimicTest, OnlyThePlayersOwnCreatureWatchesAndDraws)
{
	// a deed every creature that sees it considers, at a chance of 1
	_world.Info().creatureMimic.at(static_cast<size_t>(Deed::PlantTree)).field0x80 = 1.0f;
	const game_random::testing::ScopedState state;
	size_t floats = 0;
	game_random::testing::SetGameRand([](uint32_t) { return 0u; },
	                                  [&floats](float) {
		                                  ++floats;
		                                  // never below the chance of 1, so the creature does not take it up: the test
		                                  // counts the draw only
		                                  return 1.0f;
	                                  });
	ecs::systems::CreatureMindSystem minds;
	const glm::vec3 at(100.0f, 0.0f, 100.0f);
	test::creature_world::World::MakeCreature(at, PlayerNames::PLAYER_TWO);
	// its first turn draws its desires, and sets up what it has learnt
	minds.ProcessTurn();
	floats = 0;

	minds.PlayerDid(PlayerNames::PLAYER_ONE, static_cast<size_t>(Deed::PlantTree), at, std::nullopt);
	EXPECT_EQ(floats, 0u);
	minds.PlayerDid(PlayerNames::PLAYER_TWO, static_cast<size_t>(Deed::PlantTree), at, std::nullopt);
	EXPECT_EQ(floats, 1u);
}

TEST_F(CreatureMimicSiteTest, AFieldWateredByAPlayersMiracleIsTheirDeed)
{
	auto& registry = test::creature_world::World::Registry();
	const auto field = MakeThing(glm::vec3(30.0f, 0.0f, 40.0f));
	registry.Assign<Field>(field).town = -1;
	const auto withPlayer = registry.Create();
	auto& cast = registry.Assign<Spell>(withPlayer);
	cast.magicType = MagicType::WaterPowerUpOne;
	cast.player = PlayerNames::PLAYER_ONE;
	cast.hasPlayer = true;
	const auto withoutPlayer = registry.Create();
	registry.Assign<Spell>(withoutPlayer).magicType = MagicType::Water;

	magic::water::ApplyWaterSpell(field, withoutPlayer);
	EXPECT_TRUE(deeds.empty());
	magic::water::ApplyWaterSpell(field, withPlayer);
	ASSERT_EQ(deeds.size(), 1u);
	EXPECT_EQ(deeds.front().player, PlayerNames::PLAYER_ONE);
	EXPECT_EQ(deeds.front().deed, Deed::CastWaterOnCrops);
	EXPECT_EQ(deeds.front().object, field);
	EXPECT_EQ(deeds.front().point, glm::vec3(30.0f, 0.0f, 40.0f));
	// the plain water miracle, whichever water was cast
	ASSERT_TRUE(deeds.front().magic.has_value());
	EXPECT_EQ(*deeds.front().magic, MagicType::Water);
}
