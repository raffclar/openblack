/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#define LOCATOR_IMPLEMENTATIONS 1

#include <algorithm>
#include <memory>
#include <optional>
#include <vector>

#include <entt/entity/entity.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <gtest/gtest.h>
#include <spdlog/sinks/null_sink.h>
#include <spdlog/spdlog.h>

#include "3D/MapCoords.h"
#include "ECS/Components/Creature.h"
#include "ECS/Components/FlockSpell.h"
#include "ECS/Components/LivingAction.h"
#include "ECS/Components/MagicShield.h"
#include "ECS/Components/Spell.h"
#include "ECS/Components/Transform.h"
#include "ECS/Registry.h"
#include "ECS/Systems/EffectRequestSystemInterface.h"
#include "ECS/Systems/Implementations/EffectRequestSystem.h"
#include "ECS/Systems/Implementations/MagicShieldSystem.h"
#include "ECS/Systems/Implementations/MagicSystem.h"
#include "ECS/Systems/Implementations/TeleportSystem.h"
#include "InfoConstants.h"
#include "Locator.h"
#include "Particles/ParticleDrawFrame.h"

using namespace openblack;
using namespace openblack::ecs;
using namespace openblack::ecs::components;
using namespace openblack::ecs::systems;

namespace
{

/// Shows each effect that it is asked for, so that the miracles take their turns
class ShownEffects final: public EffectServerInterface
{
public:
	void SetLifeListener(EffectLifeListener* lifeListener) override { listener = lifeListener; }
	EffectHandle Serve(const EffectRequest& /*request*/, EffectHandle handle) override
	{
		if (listener != nullptr)
		{
			listener->EffectStarted(handle);
		}
		return handle;
	}
	void Move(EffectHandle, glm::vec3) override {}
	void Stop(EffectHandle handle) override { stopped.push_back(handle); }
	void Remove(EffectHandle) override {}
	void RemoveOwnedBy(entt::entity) override {}
	void SetPlayer(EffectHandle, int) override {}
	void SetPower(EffectHandle, float) override {}
	void SetMagnitude(EffectHandle, float) override {}
	void SetAlpha(EffectHandle, float) override {}
	void SetProcessInfo(EffectHandle, const particles::ProcessInfo&) override {}
	void SetEnabled(EffectHandle, bool) override {}
	void AddTarget(EffectHandle, entt::entity) override {}
	void AddTargetPosition(EffectHandle, glm::vec3) override {}
	void AddPlasma(EffectHandle, const particles::PlasmaCommand&) override {}
	void AddManaPathSpark(EffectHandle, const particles::mana_path::Spark&) override {}
	bool ProcessForSpell(EffectHandle, const particles::ProcessInfo&, float) override { return true; }
	void RestoreSink(EffectHandle, particles::SpellSink*) override {}
	void SetHidden(EffectHandle, bool) override {}
	void SetDrawOffset(EffectHandle, glm::vec3) override {}
	void SetLiveHand(EffectHandle, glm::vec3) override {}
	[[nodiscard]] std::shared_ptr<particles::ShieldSphere> FindShield(glm::vec3, float) const override { return nullptr; }
	void AddBeliefSprite(const particles::BeliefSprite&) override {}
	void AddHandManaPathSpark(const particles::mana_path::Spark&) override {}
	void AddStormBolt(glm::vec3) override {}
	void SetFightMarks(std::vector<particles::draw::FightMark>) override {}
	void AddLightSheet(const std::shared_ptr<particles::LightSheet>&) override {}
	void SetPlayerSymbols(std::vector<particles::draw::PlayerSymbol>) override {}
	void SetShown(EffectHandle, bool) override {}
	bool Renumber(EffectHandle, EffectHandle) override { return false; }

	EffectLifeListener* listener {nullptr};
	/// The effects that were told to stop
	std::vector<EffectHandle> stopped;
};

class CreatureHeldCastImport: public ::testing::Test
{
protected:
	void SetUp() override
	{
		for (const auto* name : {"game", "audio", "magic", "ai"})
		{
			if (spdlog::get(name) == nullptr)
			{
				spdlog::create<spdlog::sinks::null_sink_st>(name);
			}
		}
		// The tables are large: made on the heap
		_info = std::make_unique<InfoConstants>();
		Locator::infoConstants::emplace(*_info);
		Locator::entitiesRegistry::emplace<Registry>();
		Locator::effectRequestSystem::emplace<EffectRequestSystem>([this]() -> EffectServerInterface* { return &_effects; });
		_magic = &static_cast<MagicSystem&>(Locator::magicSystem::emplace<MagicSystem>());
		auto& registry = Locator::entitiesRegistry::value();
		_creature = registry.Create();
		registry.Assign<Creature>(_creature, Creature {.owner = PlayerNames::PLAYER_ONE});
	}

	void TearDown() override
	{
		Locator::magicSystem::reset();
		Locator::effectRequestSystem::reset();
		Locator::entitiesRegistry::reset();
		Locator::infoConstants::reset();
	}

	/// A water spell of a file, which ran for 3.25 of its 12.5 seconds
	static magic::ImportedCast Water()
	{
		return {
		    .type = MagicType::Water,
		    .place = {.x = map_coords::ToFixed(10.0f), .z = map_coords::ToFixed(20.0f), .altitude = 2.0f},
		    .chants = 30.0f,
		    .initialChants = 50.0f,
		    .closedDown = false,
		    .duration = 12.5f,
		    .age = 3.25f,
		    .magnitude = 40.0f,
		    .processInfo = {.handPosition = {10.0f, 12.0f, 20.0f}, .power = 0.75f, .enabled = true},
		    .firstDone = std::nullopt,
		    .lastRipple = 2.0f,
		};
	}

	[[nodiscard]] const Spell& SpellOf(entt::entity spell) const
	{
		return Locator::entitiesRegistry::value().Get<const Spell>(spell);
	}

	std::unique_ptr<InfoConstants> _info;
	ShownEffects _effects;
	MagicSystem* _magic {nullptr};
	entt::entity _creature {entt::null};
};

} // namespace

// The creature holds the spell of the file again, with the time, the size and the prayer power of the file
TEST_F(CreatureHeldCastImport, TheCreatureHoldsTheSpellWithTheValuesOfTheFile)
{
	const auto spell = _magic->ImportCreatureCast(_creature, Water());
	ASSERT_TRUE(spell != entt::null);
	EXPECT_EQ(_magic->CreatureCastOf(_creature), spell);
	const auto& made = SpellOf(spell);
	EXPECT_EQ(made.magicType, MagicType::Water);
	EXPECT_EQ(made.caster.kind, SpellCaster::Kind::Creature);
	EXPECT_EQ(made.caster.entity, _creature);
	EXPECT_EQ(made.caster.player, PlayerNames::PLAYER_ONE);
	EXPECT_EQ(made.duration, 12.5f);
	EXPECT_EQ(made.age, 3.25f);
	EXPECT_EQ(made.magnitude, 40.0f);
	EXPECT_EQ(made.chants.chants, 30.0f);
	EXPECT_EQ(made.chants.initialChants, 50.0f);
	EXPECT_EQ(made.lastRipple, 2.0f);
	EXPECT_EQ(made.place, Water().place);
	EXPECT_EQ(made.processInfo.handPosition, glm::vec3(10.0f, 12.0f, 20.0f));
	EXPECT_EQ(made.processInfo.power, 0.75f);
	EXPECT_FALSE(made.closedDown);
}

// The import makes the forest, the shields and the teleport, and the heal and the storm. The import of the world makes
// the things of the forest, the shields and the teleport after the spell.
TEST_F(CreatureHeldCastImport, TheImportMakesEveryKindOfSpell)
{
	auto none = Water();
	none.type = MagicType::None;
	EXPECT_TRUE(_magic->ImportCreatureCast(_creature, none) == entt::null);
	for (const auto type : {MagicType::Forest, MagicType::Shield, MagicType::PhysicalShield, MagicType::Teleport,
	                        MagicType::Heal, MagicType::StormWindRain, MagicType::Tornado})
	{
		auto cast = Water();
		cast.type = type;
		cast.lastRipple = std::nullopt;
		const auto spell = _magic->ImportCreatureCast(_creature, cast);
		ASSERT_TRUE(spell != entt::null) << static_cast<int>(type);
		EXPECT_EQ(SpellOf(spell).magicType, type);
		EXPECT_EQ(SpellOf(spell).age, 3.25f);
		EXPECT_EQ(_magic->CreatureCastOf(_creature), spell);
	}
}

// A forest knows that it planted its trees and how many trees it can make, as the file says
TEST_F(CreatureHeldCastImport, AForestKeepsItsPlantingAndItsMostTrees)
{
	auto cast = Water();
	cast.type = MagicType::Forest;
	cast.duration = -1.0f;
	cast.lastRipple = std::nullopt;
	cast.forest = magic::ImportedForestCast {.planted = true, .mostTrees = 6};
	const auto spell = _magic->ImportCreatureCast(_creature, cast);
	ASSERT_TRUE(spell != entt::null);
	EXPECT_TRUE(SpellOf(spell).forestPlanted);
	EXPECT_EQ(SpellOf(spell).maxObjectsToCreate, 6);
}

// A creature spell of the file acts on the creature that the file gives
TEST_F(CreatureHeldCastImport, ACreatureSpellActsOnItsCreature)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto other = registry.Create();
	registry.Assign<Creature>(other, Creature {.owner = PlayerNames::PLAYER_TWO});
	auto cast = Water();
	cast.type = MagicType::CreatureSpellStrong;
	cast.duration = -1.0f;
	cast.lastRipple = std::nullopt;
	cast.target = other;
	const auto spell = _magic->ImportCreatureCast(_creature, cast);
	ASSERT_TRUE(spell != entt::null);
	EXPECT_EQ(SpellOf(spell).target, other);
	EXPECT_EQ(SpellOf(spell).duration, -1.0f);
}

// A flock spell continues with the flock, the animals and the sweep of the file. The trail of the hand does not start
// again.
TEST_F(CreatureHeldCastImport, AFlockSpellContinuesWithItsFlockAndItsSweep)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto flock = registry.Create();
	auto cast = Water();
	cast.type = MagicType::FlockFlying;
	cast.lastRipple = std::nullopt;
	cast.flock = magic::ImportedFlock {
	    .flock = flock, .created = 4, .emitted = 4.5f, .lastSpawn = {0x10000, 0x20000}, .lastSpawnHeight = 3.0f};
	const auto spell = _magic->ImportCreatureCast(_creature, cast);
	ASSERT_TRUE(spell != entt::null);
	const auto* data = registry.TryGet<const FlockSpell>(spell);
	ASSERT_NE(data, nullptr);
	EXPECT_EQ(data->flock, flock);
	EXPECT_EQ(data->created, 4);
	EXPECT_EQ(data->emitted, 4.5f);
	EXPECT_EQ(data->lastSpawn, glm::ivec2(0x10000, 0x20000));
	EXPECT_EQ(data->lastSpawnHeight, 3.0f);
	EXPECT_EQ(data->castEffect, 0u);
	EXPECT_TRUE(data->castEffectStarted);
	EXPECT_FALSE(data->closedDown);
}

// The beams from the hands of the file follow the creature until it lets go of its spell
TEST_F(CreatureHeldCastImport, TheBeamsOfTheFileStopWhenTheCreatureLetsGo)
{
	auto& effects = Locator::effectRequestSystem::value();
	const auto left = effects.Start({.type = SpotVisualType::MagicBeam, .owner = _creature});
	const auto right = effects.Start({.type = SpotVisualType::MagicBeam, .owner = _creature});
	ASSERT_TRUE(effects.IsRunning(left));
	ASSERT_TRUE(effects.IsRunning(right));
	auto food = Water();
	food.type = MagicType::Food;
	food.duration = -1.0f;
	food.lastRipple = std::nullopt;
	ASSERT_TRUE(_magic->ImportCreatureCast(_creature, food) != entt::null);
	_magic->ImportCreatureCastBeams(_creature, {left, right});
	EXPECT_TRUE(_effects.stopped.empty());
	_magic->ReleaseCreatureCast(_creature);
	EXPECT_NE(std::ranges::find(_effects.stopped, left), _effects.stopped.end());
	EXPECT_NE(std::ranges::find(_effects.stopped, right), _effects.stopped.end());
}

// A spell that lasts only while the creature holds it stops when the creature lets go. A closed spell of the file
// stays closed.
TEST_F(CreatureHeldCastImport, AHeldSpellOfTheFileStopsAsACastSpellStops)
{
	auto food = Water();
	food.type = MagicType::Food;
	food.duration = -1.0f;
	food.firstDone = true;
	food.lastRipple = std::nullopt;
	const auto spell = _magic->ImportCreatureCast(_creature, food);
	ASSERT_TRUE(spell != entt::null);
	EXPECT_TRUE(SpellOf(spell).resourceFirstDone);
	_magic->ReleaseCreatureCast(_creature);
	EXPECT_TRUE(_magic->CreatureCastOf(_creature) == entt::null);
	EXPECT_TRUE(SpellOf(spell).closedDown);

	auto closed = Water();
	closed.closedDown = true;
	const auto other = _magic->ImportCreatureCast(_creature, closed);
	ASSERT_TRUE(other != entt::null);
	EXPECT_TRUE(SpellOf(other).closedDown);
}

// A forest of the file that the hand cast continues with the player as its caster. No creature holds it.
TEST_F(CreatureHeldCastImport, ASpellOfTheHandContinuesWithItsPlayer)
{
	auto cast = Water();
	cast.type = MagicType::Forest;
	cast.duration = -1.0f;
	cast.lastRipple = std::nullopt;
	cast.forest = magic::ImportedForestCast {.planted = true, .mostTrees = 4};
	const SpellCaster hand {.kind = SpellCaster::Kind::Player, .player = PlayerNames::PLAYER_TWO, .withoutIcon = true};
	const auto spell = _magic->ImportCast(hand, cast);
	ASSERT_TRUE(spell != entt::null);
	const auto& made = SpellOf(spell);
	EXPECT_EQ(made.magicType, MagicType::Forest);
	EXPECT_EQ(made.caster.kind, SpellCaster::Kind::Player);
	EXPECT_EQ(made.caster.player, PlayerNames::PLAYER_TWO);
	EXPECT_TRUE(made.caster.withoutIcon);
	EXPECT_EQ(made.age, 3.25f);
	EXPECT_EQ(made.chants.chants, 30.0f);
	EXPECT_TRUE(made.forestPlanted);
	EXPECT_EQ(made.maxObjectsToCreate, 4);
	EXPECT_FALSE(made.closedDown);
	EXPECT_TRUE(_magic->CreatureCastOf(_creature) == entt::null);

	// A closed spell of the file stays closed, and a spell with no magic type is not made
	cast.closedDown = true;
	const auto closed = _magic->ImportCast(hand, cast);
	ASSERT_TRUE(closed != entt::null);
	EXPECT_TRUE(SpellOf(closed).closedDown);
	cast.type = MagicType::None;
	EXPECT_TRUE(_magic->ImportCast(hand, cast) == entt::null);
}

// Each kind of spell of the hand continues with the values of the file: the storm, the heal, water, food, wood and the
// lightning. The size of the file stays, also outside the limits of a new cast. The storm has no swirl after the load.
TEST_F(CreatureHeldCastImport, EachSpellOfTheHandContinuesWithTheValuesOfTheFile)
{
	const SpellCaster hand {.kind = SpellCaster::Kind::Player, .player = PlayerNames::PLAYER_ONE, .withoutIcon = true};
	for (const auto type : {MagicType::StormWindRain, MagicType::StormWindRainLightning, MagicType::Tornado, MagicType::Heal,
	                        MagicType::HealPowerUpOne, MagicType::Water, MagicType::Food, MagicType::Wood,
	                        MagicType::LightningBolt, MagicType::LightningBoltPowerUpTwo})
	{
		auto cast = Water();
		cast.type = type;
		cast.magnitude = 55.0f;
		cast.fromHand = true;
		cast.humanCasting = true;
		const bool resource = type == MagicType::Food || type == MagicType::Wood;
		cast.firstDone = resource ? std::optional {true} : std::nullopt;
		cast.lastRipple = type == MagicType::Water ? std::optional {2.0f} : std::nullopt;
		const auto spell = _magic->ImportCast(hand, cast);
		ASSERT_TRUE(spell != entt::null) << static_cast<int>(type);
		const auto& made = SpellOf(spell);
		EXPECT_EQ(made.magicType, type);
		EXPECT_EQ(made.caster.kind, SpellCaster::Kind::Player);
		EXPECT_EQ(made.age, 3.25f);
		EXPECT_EQ(made.duration, 12.5f);
		EXPECT_EQ(made.magnitude, 55.0f) << static_cast<int>(type);
		EXPECT_EQ(made.chants.chants, 30.0f);
		EXPECT_EQ(made.processInfo.handPosition, glm::vec3(10.0f, 12.0f, 20.0f));
		EXPECT_EQ(made.resourceFirstDone, resource);
		EXPECT_EQ(made.lastRipple, type == MagicType::Water ? 2.0f : 0.0f);
		EXPECT_EQ(made.castEffect, 0u);
		EXPECT_TRUE(made.fromLocalHand);
		EXPECT_TRUE(made.humanCasting);
		// The hand does not hold the seed of the spell, so the spell does not follow the hand
		EXPECT_FALSE(made.castFromHand);
		EXPECT_FALSE(made.closedDown);
	}
}

// A flock of the hand of a person continues to make its animals at the hand, with the flock and the sweep of the file
TEST_F(CreatureHeldCastImport, AFlockOfTheHandContinuesAtTheHand)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto flock = registry.Create();
	const SpellCaster hand {.kind = SpellCaster::Kind::Player, .player = PlayerNames::PLAYER_ONE, .withoutIcon = true};
	for (const auto type : {MagicType::FlockFlying, MagicType::FlockGround})
	{
		auto cast = Water();
		cast.type = type;
		cast.lastRipple = std::nullopt;
		cast.fromHand = true;
		cast.humanCasting = true;
		cast.flock = magic::ImportedFlock {
		    .flock = flock, .created = 2, .emitted = 2.5f, .lastSpawn = {0x30000, 0x40000}, .lastSpawnHeight = 1.0f};
		const auto spell = _magic->ImportCast(hand, cast);
		ASSERT_TRUE(spell != entt::null) << static_cast<int>(type);
		const auto& made = SpellOf(spell);
		EXPECT_TRUE(made.fromLocalHand);
		EXPECT_TRUE(made.humanCasting);
		const auto* data = registry.TryGet<const FlockSpell>(spell);
		ASSERT_NE(data, nullptr);
		EXPECT_EQ(data->flock, flock);
		EXPECT_EQ(data->created, 2);
		EXPECT_EQ(data->emitted, 2.5f);
		EXPECT_EQ(data->lastSpawn, glm::ivec2(0x30000, 0x40000));
		EXPECT_TRUE(data->castEffectStarted);
	}
}

// A spell that a script cast is not a spell of the hand. The file decides that no person casts it, also for the player
// of this computer.
TEST_F(CreatureHeldCastImport, AScriptSpellIsNotASpellOfTheHand)
{
	const SpellCaster script {.kind = SpellCaster::Kind::Player, .player = PlayerNames::PLAYER_ONE};
	auto cast = Water();
	cast.type = MagicType::StormWindRain;
	cast.lastRipple = std::nullopt;
	cast.humanCasting = false;
	const auto spell = _magic->ImportCast(script, cast);
	ASSERT_TRUE(spell != entt::null);
	EXPECT_FALSE(SpellOf(spell).fromLocalHand);
	EXPECT_FALSE(SpellOf(spell).humanCasting);
}

// A creature spell that its creature cast and let go of continues on the creature of the file. The creature that cast
// it does not hold it.
TEST_F(CreatureHeldCastImport, ACreatureSpellThatNoCreatureHoldsActsOnItsCreature)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto other = registry.Create();
	registry.Assign<Creature>(other, Creature {.owner = PlayerNames::PLAYER_TWO});
	auto cast = Water();
	cast.type = MagicType::CreatureSpellFreeze;
	cast.duration = -1.0f;
	cast.lastRipple = std::nullopt;
	cast.target = other;
	const SpellCaster creature {.kind = SpellCaster::Kind::Creature, .player = PlayerNames::PLAYER_ONE, .entity = _creature};
	const auto spell = _magic->ImportCast(creature, cast);
	ASSERT_TRUE(spell != entt::null);
	EXPECT_EQ(SpellOf(spell).target, other);
	EXPECT_EQ(SpellOf(spell).caster.entity, _creature);
	EXPECT_FALSE(SpellOf(spell).fromLocalHand);
	EXPECT_TRUE(_magic->CreatureCastOf(_creature) == entt::null);
}

// The stones of a player take the order of the list of the file, the head first. The stones of another player and the
// stones that the list does not give stay as they were, after the stones of the list.
TEST_F(CreatureHeldCastImport, TheStonesOfAPlayerTakeTheOrderOfTheFile)
{
	auto& teleports = static_cast<TeleportSystem&>(Locator::teleportSystem::emplace<TeleportSystem>());
	const auto one = teleports.RestoreStone({10.0f, 0.0f, 10.0f}, PlayerNames::PLAYER_ONE, entt::null);
	const auto two = teleports.RestoreStone({20.0f, 0.0f, 10.0f}, PlayerNames::PLAYER_ONE, entt::null);
	const auto three = teleports.RestoreStone({30.0f, 0.0f, 10.0f}, PlayerNames::PLAYER_ONE, entt::null);
	const auto other = teleports.RestoreStone({40.0f, 0.0f, 10.0f}, PlayerNames::PLAYER_TWO, entt::null);
	const auto unlisted = teleports.RestoreStone({50.0f, 0.0f, 10.0f}, PlayerNames::PLAYER_ONE, entt::null);
	// The newest stone is the head of the list
	EXPECT_EQ(teleports.GetStones(PlayerNames::PLAYER_ONE), (std::vector {unlisted, three, two, one}));

	const std::vector headFirst {one, three, other, two};
	teleports.OrderStones(PlayerNames::PLAYER_ONE, headFirst);
	EXPECT_EQ(teleports.GetStones(PlayerNames::PLAYER_ONE), (std::vector {one, three, two, unlisted}));
	EXPECT_EQ(teleports.GetStones(PlayerNames::PLAYER_TWO), (std::vector {other}));
	Locator::teleportSystem::reset();
}

// A villager that shelters under a shield of the file looks at the point of the file. With no point, it looks at the
// centre of the shield.
TEST_F(CreatureHeldCastImport, AVillagerUnderAShieldOfTheFileLooksAtThePointOfTheFile)
{
	auto& shields = static_cast<MagicShieldSystem&>(Locator::magicShieldSystem::emplace<MagicShieldSystem>());
	auto& registry = Locator::entitiesRegistry::value();
	const auto object = registry.Create();
	registry.Assign<Transform>(object, glm::vec3 {100.0f, 0.0f, 200.0f}, glm::mat3(1.0f), glm::vec3(1.0f));
	registry.Assign<MagicShield>(object, MagicShield {.spell = registry.Create(), .radius = 30.0f});
	const auto looking = registry.Create();
	registry.Assign<LivingAction>(looking);
	const auto other = registry.Create();
	registry.Assign<LivingAction>(other);

	const std::vector<MagicShieldSystemInterface::RestoredTaker> takers {
	    {.villager = looking, .lookAt = glm::ivec2 {12345, 67890}},
	    {.villager = other, .lookAt = std::nullopt},
	};
	shields.RestoreReaction(object, 7, Reaction::ReactToMagicShield, 60.0f, 0, takers);
	EXPECT_EQ(registry.Get<const MagicShield>(object).reaction, 7U);
	const auto& first = registry.Get<const VillagerShieldReaction>(looking);
	EXPECT_EQ(first.reaction, 7U);
	EXPECT_EQ(first.shield, object);
	EXPECT_EQ(first.lookAt, glm::ivec2(12345, 67890));
	const auto centre = map_coords::WholeXZ(map_coords::FromMetres(glm::vec2 {100.0f, 200.0f}));
	EXPECT_EQ(registry.Get<const VillagerShieldReaction>(other).lookAt, centre);
	Locator::magicShieldSystem::reset();
}

// A spell of the file keeps the map positions where it was cast, as the file keeps them. Far from the corner of the map,
// metres cannot keep each map position, so the spell does not take them from its point.
TEST_F(CreatureHeldCastImport, TheSpellKeepsTheMapPositionsOfTheFileWhereItWasCast)
{
	constexpr map_coords::MapCoords k_Moved {.x = 8090858, .z = 3276801, .altitude = 0.0f};
	constexpr map_coords::MapCoords k_First {.x = 8090859, .z = 3276803, .altitude = 0.0f};
	ASSERT_NE(map_coords::ToFixed(map_coords::ToMetres(k_Moved.x)), k_Moved.x);
	auto cast = Water();
	cast.castPlace = k_Moved;
	cast.originalCastPlace = k_First;
	const auto spell = _magic->ImportCreatureCast(_creature, cast);
	ASSERT_TRUE(spell != entt::null);
	const auto& made = SpellOf(spell);
	EXPECT_EQ(made.castPlace, k_Moved);
	EXPECT_EQ(made.originalCastPlace, k_First);
	EXPECT_EQ(made.place, Water().place);
}

// A spell of a file without the map positions where it was cast takes the map position where it acts now for both
TEST_F(CreatureHeldCastImport, ASpellWithoutTheMapPositionsTakesTheMapPositionOfItsPoint)
{
	const auto spell = _magic->ImportCreatureCast(_creature, Water());
	ASSERT_TRUE(spell != entt::null);
	const auto& made = SpellOf(spell);
	const map_coords::MapCoords expected {.x = map_coords::ToFixed(10.0f), .z = map_coords::ToFixed(20.0f), .altitude = 2.0f};
	EXPECT_EQ(made.castPlace, expected);
	EXPECT_EQ(made.originalCastPlace, expected);
}

// A miracle cast at an exact map position keeps that map position, also where metres cannot keep it
TEST_F(CreatureHeldCastImport, AMiracleCastAtAMapPositionKeepsItExactly)
{
	constexpr map_coords::MapCoords k_Place {.x = 8090858, .z = 3276801, .altitude = 0.0f};
	const magic::SpellCastData cast {.magnitude = 40.0f, .chants = 50.0f, .duration = 12.5f, .maxObjectsToCreate = -1};
	const auto spell = _magic->CastAtPlace(MagicType::StormWindRain, PlayerNames::PLAYER_ONE, k_Place, cast, {});
	ASSERT_TRUE(spell != entt::null);
	const auto& made = SpellOf(spell);
	EXPECT_EQ(made.castPlace, k_Place);
	EXPECT_EQ(made.originalCastPlace, k_Place);
	EXPECT_EQ(made.place, k_Place);
}
