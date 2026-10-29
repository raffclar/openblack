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

#include <optional>

#include <entt/entity/entity.hpp>
#include <glm/mat3x3.hpp>
#include <glm/vec3.hpp>
#include <gtest/gtest.h>

#include "Creature/CreatureDecisionTree.h"
#include "ECS/Components/Abode.h"
#include "ECS/Components/BuildingDamage.h"
#include "ECS/Components/Construction.h"
#include "ECS/Components/CreatureLeash.h"
#include "ECS/Components/Firefly.h"
#include "ECS/Components/MagicFireBall.h"
#include "ECS/Components/MagicShield.h"
#include "ECS/Components/ScriptHighlight.h"
#include "ECS/Components/Spell.h"
#include "ECS/Components/StreetLantern.h"
#include "ECS/Components/Temple.h"
#include "ECS/Components/TownDesireFlag.h"
#include "ECS/Components/Transform.h"
#include "ECS/Registry.h"
#include "ECS/Systems/Implementations/CreatureMindSystemDetail.h"
#include "Enums.h"

using namespace openblack;
using namespace openblack::ecs::components;
namespace mind_detail = openblack::ecs::systems::mind_detail;
namespace kinds = openblack::creature_tree::belief_types;

namespace
{

/// A thing at a place on the land, with one component
template <typename Component>
entt::entity Make(ecs::Registry& registry, const Component& component)
{
	const auto thing = registry.Create();
	registry.Assign<Transform>(thing, glm::vec3(5.0f, 0.0f, 5.0f), glm::mat3(1.0f), glm::vec3(1.0f));
	registry.Assign<Component>(thing, component);
	return thing;
}

/// A thing at a place on the land, with one component that has its first values
template <typename Component>
entt::entity Make(ecs::Registry& registry)
{
	return Make(registry, Component {});
}

/// The number that a belief gives to a player
uint32_t NumberOf(PlayerNames player)
{
	return static_cast<uint32_t>(player);
}

} // namespace

TEST(CreatureBeliefKinds, ThingsOfNoKindOfTheirOwnAreOfTheOtherKind)
{
	ecs::Registry registry;
	EXPECT_EQ(mind_detail::BeliefKindOf(registry, Make<ScriptHighlight>(registry)), kinds::k_Other);
	EXPECT_EQ(mind_detail::BeliefKindOf(registry, Make<MagicShield>(registry)), kinds::k_Other);
	EXPECT_EQ(mind_detail::BeliefKindOf(registry, Make<StreetLantern>(registry)), kinds::k_Other);
	EXPECT_EQ(mind_detail::BeliefKindOf(registry, Make<Firefly>(registry)), kinds::k_Other);
	EXPECT_EQ(mind_detail::BeliefKindOf(registry, Make<MagicFireBall>(registry)), kinds::k_Other);
	EXPECT_EQ(mind_detail::BeliefKindOf(registry, Make<LeashPost>(registry)), kinds::k_Other);
	EXPECT_EQ(mind_detail::BeliefKindOf(registry, Make<TempleEntrance>(registry)), kinds::k_Other);
	EXPECT_EQ(mind_detail::BeliefKindOf(registry, Make<TownDesireFlag>(registry)), kinds::k_Other);
	EXPECT_EQ(mind_detail::BeliefKindOf(registry, Make<PlannedAbode>(registry)), kinds::k_Other);
	EXPECT_EQ(mind_detail::BeliefKindOf(registry, Make<PlannedTemple>(registry)), kinds::k_Other);
}

TEST(CreatureBeliefKinds, AFootballPitchIsAFeature)
{
	ecs::Registry registry;
	EXPECT_EQ(mind_detail::BeliefKindOf(registry, Make<Abode>(registry, Abode {.type = AbodeNumber::FootballPitch})),
	          kinds::k_Feature);
}

TEST(CreatureBeliefKinds, AThingWithNoKnownKindGivesNoBelief)
{
	ecs::Registry registry;
	const auto thing = registry.Create();
	registry.Assign<Transform>(thing, glm::vec3(0.0f), glm::mat3(1.0f), glm::vec3(1.0f));
	EXPECT_EQ(mind_detail::BeliefKindOf(registry, thing), std::nullopt);
}

TEST(CreatureBeliefKinds, AScrollIsANaturalThingOfTheNeutralPlayer)
{
	ecs::Registry registry;
	const auto scroll = Make<ScriptHighlight>(registry);
	const auto belief = mind_detail::BeliefNowOf(registry, scroll, entt::null);
	ASSERT_TRUE(belief.has_value());
	EXPECT_EQ(belief->type, kinds::k_Other);
	EXPECT_EQ(belief->Value(creature_tree::Attribute::Origin), creature_tree::origin::k_Natural);
	EXPECT_EQ(belief->Value(creature_tree::Attribute::PlayerNumber), NumberOf(PlayerNames::NEUTRAL));
}

TEST(CreatureBeliefKinds, AShieldIsOfThePlayerOfItsMiracleUntilTheMiracleLetsGo)
{
	ecs::Registry registry;
	auto spell = ecs::components::Spell {};
	spell.caster = {.kind = SpellCaster::Kind::Player, .player = PlayerNames::PLAYER_TWO};
	const auto miracle = Make<ecs::components::Spell>(registry, spell);
	const auto shield = Make<MagicShield>(registry, MagicShield {.spell = miracle});
	auto belief = mind_detail::BeliefNowOf(registry, shield, entt::null);
	ASSERT_TRUE(belief.has_value());
	EXPECT_EQ(belief->Value(creature_tree::Attribute::PlayerNumber), NumberOf(PlayerNames::PLAYER_TWO));

	registry.Get<MagicShield>(shield).spell = entt::null;
	belief = mind_detail::BeliefNowOf(registry, shield, entt::null);
	ASSERT_TRUE(belief.has_value());
	EXPECT_EQ(belief->Value(creature_tree::Attribute::PlayerNumber), NumberOf(PlayerNames::NEUTRAL));
}

TEST(CreatureBeliefKinds, AFireballIsOfThePlayerOfItsMiracle)
{
	ecs::Registry registry;
	const auto owned = Make<MagicFireBall>(registry, MagicFireBall {.player = PlayerNames::PLAYER_THREE, .hasPlayer = true});
	const auto unowned = Make<MagicFireBall>(registry, MagicFireBall {});
	const auto ownedBelief = mind_detail::BeliefNowOf(registry, owned, entt::null);
	const auto unownedBelief = mind_detail::BeliefNowOf(registry, unowned, entt::null);
	ASSERT_TRUE(ownedBelief.has_value());
	ASSERT_TRUE(unownedBelief.has_value());
	EXPECT_EQ(ownedBelief->Value(creature_tree::Attribute::PlayerNumber), NumberOf(PlayerNames::PLAYER_THREE));
	EXPECT_EQ(unownedBelief->Value(creature_tree::Attribute::PlayerNumber), NumberOf(PlayerNames::NEUTRAL));
}

TEST(CreatureBeliefKinds, APieceThatBrokeOffABuildingIsARock)
{
	ecs::Registry registry;
	EXPECT_EQ(mind_detail::BeliefKindOf(registry, Make<BuildingPiece>(registry)), kinds::k_Rock);
}
