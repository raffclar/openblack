/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cstdint>

#include <chrono>
#include <memory>
#include <optional>
#include <vector>

#include <gtest/gtest.h>

#include "ECS/Components/Creature.h"
#include "ECS/Components/CreatureFight.h"
#include "ECS/Registry.h"
#include "ECS/Systems/CreatureFightSystemInterface.h"
#include "ECS/Systems/FireSystemInterface.h"
#include "ECS/Systems/Implementations/MagicLiving.h"
#include "InfoConstants.h"
#include "Locator.h"
#include "Magic/SpellRules.h"

using namespace openblack;
using namespace openblack::ecs;
using namespace openblack::ecs::components;
using namespace openblack::ecs::systems;

namespace
{

/// A fire that keeps each burn that heats a thing, and does nothing else
class BurnLog final: public FireSystemInterface
{
public:
	void ProcessTurn() override {}
	void MarkBurnPoint() override {}
	void Update(std::chrono::milliseconds /*frameRealTime*/, std::chrono::milliseconds /*frameGameTime*/) override {}
	void Reset() override {}
	void ApplyBurn(entt::entity object, float burn, std::optional<PlayerNames> /*player*/) override
	{
		burns.push_back({object, burn});
	}
	void SetTemperature(entt::entity /*object*/, float /*temperature*/, entt::entity /*source*/) override {}
	void SetOnFire(entt::entity /*object*/, float /*speed*/) override {}
	void PutOut(entt::entity /*object*/) override {}
	void Forget(entt::entity /*object*/) override {}
	void HeatHeldObject(entt::entity /*object*/) override {}
	void StartedMoving(entt::entity /*object*/, bool /*inHand*/) override {}
	void SetCanBeSetOnFire(entt::entity /*object*/, bool /*can*/) override {}
	void SetHurtByFire(entt::entity /*object*/, bool /*hurt*/) override {}
	[[nodiscard]] float GetTemperature(entt::entity /*object*/) const override { return 0.0f; }
	[[nodiscard]] bool IsOnFire(entt::entity /*object*/) const override { return false; }
	[[nodiscard]] bool IsFireNear(const glm::vec3& /*point*/, float /*radius*/) const override { return false; }
	[[nodiscard]] float GetCharring(entt::entity /*object*/) const override { return 0.0f; }
	[[nodiscard]] std::optional<Blaze> GetBlaze(entt::entity /*object*/) const override { return std::nullopt; }
	[[nodiscard]] std::optional<entt::entity> NearestHotMember(entt::entity /*root*/, const glm::vec3& /*point*/) const override
	{
		return std::nullopt;
	}
	[[nodiscard]] std::optional<Reach> GetReach(entt::entity /*object*/) const override { return std::nullopt; }
	[[nodiscard]] std::vector<entt::entity> GetBlazeMembers(entt::entity /*object*/) const override { return {}; }
	[[nodiscard]] std::optional<entt::entity> NearestSafeFire(entt::entity /*object*/,
	                                                          const glm::vec3& /*point*/) const override
	{
		return std::nullopt;
	}
	[[nodiscard]] bool InSameBlaze(entt::entity /*a*/, entt::entity /*b*/) const override { return false; }
	void MergeBlazes(entt::entity /*object*/, entt::entity /*other*/) override {}
	void MoveFire(entt::entity /*from*/, entt::entity /*to*/) override {}
	void CopyFire(entt::entity /*from*/, entt::entity /*to*/) override {}
	void AddFireman(entt::entity /*object*/, entt::entity /*villager*/) override {}
	void RemoveFireman(entt::entity /*object*/, entt::entity /*villager*/) override {}
	[[nodiscard]] bool IsFiremanOf(entt::entity /*object*/, entt::entity /*villager*/) const override { return false; }
	void CollectDrawFrame(float /*turnFraction*/, particles::draw::Frame& /*frame*/) const override {}
	[[nodiscard]] std::optional<uint32_t> GetCharredColour(entt::entity /*object*/) const override { return std::nullopt; }
	[[nodiscard]] std::optional<TreeLook> GetBurningTreeLook(entt::entity /*tree*/) const override { return std::nullopt; }
	[[nodiscard]] std::optional<uint32_t> GetGlowColour(entt::entity /*object*/) const override { return std::nullopt; }
	[[nodiscard]] std::vector<FireInfo> GetFires() const override { return {}; }

	struct Burn
	{
		entt::entity object;
		float burn;
	};
	std::vector<Burn> burns;
};

/// Fights in which every creature blocks, and which do nothing else
class AlwaysBlocks final: public CreatureFightSystemInterface
{
public:
	void ProcessTurn() override {}
	void Reset() override {}
	void Update(std::chrono::duration<float, std::milli> /*gameTime*/) override {}
	StartResult StartFight(entt::entity /*creature*/, entt::entity /*opponent*/) override { return StartResult::Started; }
	void AbortFight(entt::entity /*creature*/) override {}
	[[nodiscard]] bool IsFighting(entt::entity /*creature*/) const override { return true; }
	[[nodiscard]] std::optional<entt::entity> OpponentOf(entt::entity /*creature*/) const override { return std::nullopt; }
	bool QueueMove(entt::entity /*creature*/, const creature_fight::Move& /*move*/, bool /*replace*/) override { return false; }
	void ReleaseCharge(entt::entity /*creature*/, float /*heldMs*/) override {}
	void SetAutoFighting(entt::entity /*creature*/, bool /*autoFight*/) override {}
	[[nodiscard]] bool IsAutoFighting(entt::entity /*creature*/) const override { return false; }
	[[nodiscard]] std::optional<entt::entity> PlayersFighter() const override { return std::nullopt; }
	bool Press(const glm::vec3& /*rayOrigin*/, const glm::vec3& /*rayDirection*/, creature_fight::Button /*button*/,
	           uint32_t /*milliseconds*/, uint32_t /*turn*/) override
	{
		return false;
	}
	void Release(uint32_t /*milliseconds*/, uint32_t /*turn*/) override {}
	[[nodiscard]] bool IsPressed() const override { return false; }
	[[nodiscard]] std::optional<creature_fight::Tip> HandTip(std::optional<entt::entity> /*under*/) const override
	{
		return std::nullopt;
	}
	bool GestureSpecialMove() override { return false; }
	bool GestureSpell(MagicType /*type*/) override { return false; }
	[[nodiscard]] bool IsBlocking(entt::entity /*creature*/) const override { return true; }
	void KnockOut(entt::entity /*creature*/) override {}
	void KillPermanently(entt::entity /*creature*/) override {}
	void Resurrect(entt::entity /*creature*/) override {}
	[[nodiscard]] bool IsKnockedOut(entt::entity /*creature*/) const override { return false; }
	[[nodiscard]] std::optional<creature_fight_hud::Values> GetPanel() const override { return std::nullopt; }
	void SetCameraWatches(bool /*enabled*/) override {}
	[[nodiscard]] bool GetCameraWatches() const override { return false; }
	[[nodiscard]] bool IsCameraOnFight() const override { return false; }
};

class CreatureMiracleEffects: public ::testing::Test
{
protected:
	void SetUp() override
	{
		Locator::entitiesRegistry::emplace<Registry>();
		_fire = &static_cast<BurnLog&>(Locator::fireSystem::emplace<BurnLog>());
		// The tables are large: made on the heap. Every species takes each kind of effect in full.
		_info = std::make_unique<InfoConstants>();
		for (auto& row : _info->creature)
		{
			row.defenceMultiplierBurn = 1.0f;
			row.defenceMultiplierCrush = 1.0f;
			row.defenceMultiplierHit = 1.0f;
			row.defenceMultiplierHeal = 1.0f;
		}
		Locator::infoConstants::emplace(*_info);
		auto& registry = Locator::entitiesRegistry::value();
		_creature = registry.Create();
		registry.Assign<Creature>(_creature).owner = PlayerNames::PLAYER_ONE;
	}

	void TearDown() override
	{
		Locator::creatureFightSystem::reset();
		Locator::infoConstants::reset();
		Locator::fireSystem::reset();
		Locator::entitiesRegistry::reset();
	}

	/// The creature fights a duel, with the health it fights with
	CreatureFighting& Duel(float health)
	{
		auto& fighting = Locator::entitiesRegistry::value().Assign<CreatureFighting>(_creature);
		fighting.stage = CreatureFighting::Stage::Duel;
		fighting.fighter.health = health;
		return fighting;
	}

	[[nodiscard]] size_t Heats() const { return _fire->burns.size(); }

	BurnLog* _fire {nullptr};
	std::unique_ptr<InfoConstants> _info;
	entt::entity _creature {entt::null};
};

/// A fireball of another player
magic::EffectValues Fireball()
{
	magic::EffectValues values;
	values[magic::EffectKind::Burn] = 300.0f;
	values[magic::EffectKind::Crush] = 0.2f;
	return values;
}

} // namespace

// Out of a fight, the burn heats the creature as the game finds the harm and as it cuts and scars the creature. The life
// that takes the effect after this heats it the third time.
TEST_F(CreatureMiracleEffects, OutOfAFightTheBurnHeatsTheCreatureTwiceBeforeItsLife)
{
	auto values = Fireball();
	EXPECT_FALSE(magic_living::TakesEffectItsOwnWay(_creature, values, {.player = PlayerNames::PLAYER_TWO}));
	ASSERT_EQ(Heats(), 2u);
	for (const auto& burn : _fire->burns)
	{
		EXPECT_EQ(burn.object, _creature);
		EXPECT_EQ(burn.burn, 300.0f);
	}
}

// The creature keeps out the harm of its own miracle, but the burn of the miracle still heats it once
TEST_F(CreatureMiracleEffects, ItsOwnMiracleDoesNoHarmButItsBurnStillHeatsIt)
{
	auto values = Fireball();
	EXPECT_TRUE(magic_living::TakesEffectItsOwnWay(_creature, values,
	                                               {.player = PlayerNames::PLAYER_ONE, .casterCreature = _creature}));
	EXPECT_EQ(Heats(), 1u);
}

// In a fight the burn heats the creature once, and the harm goes on the health it fights with
TEST_F(CreatureMiracleEffects, InAFightTheBurnHeatsTheCreatureOnce)
{
	auto& fighting = Duel(0.8f);
	auto values = Fireball();
	EXPECT_TRUE(magic_living::TakesEffectItsOwnWay(_creature, values, {.player = PlayerNames::PLAYER_TWO}));
	EXPECT_EQ(Heats(), 1u);
	EXPECT_LT(fighting.fighter.health, 0.8f);
}

// A thing that strikes the creature has no burn, so it does not heat the creature
TEST_F(CreatureMiracleEffects, ABlowDoesNotHeatTheCreature)
{
	magic::EffectValues values;
	values[magic::EffectKind::Crush] = 0.3f;
	static_cast<void>(magic_living::TakesEffectItsOwnWay(_creature, values, {.blow = true}));
	EXPECT_EQ(Heats(), 0u);
}

// A creature that blocks in a fight takes a tenth of the heal, but all of the harm that the game found before the block.
// The effect stays a tenth for the things that it reaches after the creature.
TEST_F(CreatureMiracleEffects, ABlockingCreatureLeavesATenthOfTheEffectForTheThingsAfterIt)
{
	Locator::creatureFightSystem::emplace<AlwaysBlocks>();
	auto& fighting = Duel(0.5f);
	magic::EffectValues values;
	values[magic::EffectKind::Heal] = 0.4f;
	values[magic::EffectKind::Crush] = 0.2f;
	const auto defence = magic_living::DefenceOf(_creature);
	const float damage = magic::DamageFrom(values, defence);
	EXPECT_TRUE(magic_living::TakesEffectItsOwnWay(_creature, values, {.player = PlayerNames::PLAYER_TWO}));
	EXPECT_EQ(values[magic::EffectKind::Heal], 0.4f * 0.1f);
	EXPECT_EQ(values[magic::EffectKind::Crush], 0.2f * 0.1f);
	const float heal = magic::HealFrom(values, defence);
	EXPECT_EQ(fighting.fighter.health, (0.5f + heal) - damage);
}
