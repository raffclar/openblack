/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <memory>
#include <optional>
#include <vector>

#include <gtest/gtest.h>
#include <spdlog/sinks/null_sink.h>
#include <spdlog/spdlog.h>

#include "ECS/Components/MagicShield.h"
#include "ECS/Components/Spell.h"
#include "ECS/Registry.h"
#include "ECS/Systems/MagicSystemInterface.h"
#include "InfoConstants.h"
#include "Locator.h"

#define LOCATOR_IMPLEMENTATIONS 1
#include "ECS/Systems/Implementations/MagicShieldSystem.h"
#include "ECS/Systems/Implementations/ReactionSystem.h"

using namespace openblack;
using namespace openblack::ecs;
using namespace openblack::ecs::components;
using namespace openblack::ecs::systems;

namespace
{

constexpr glm::vec3 k_Here {500.0f, 0.0f, 500.0f};

/// Magic that only records the miracles that the shields delete, and deletes them
class DeletingMagic final: public MagicSystemInterface
{
public:
	entt::entity CastAtPoint(MagicType, PlayerNames, glm::vec3, const magic::SpellCastData&,
	                         const particles::ProcessInfo&) override
	{
		return entt::null;
	}
	entt::entity CastOnObject(MagicType, PlayerNames, entt::entity, const magic::SpellCastData&,
	                          const particles::ProcessInfo&) override
	{
		return entt::null;
	}
	void CloseDown(entt::entity) override {}
	void DeleteSpell(entt::entity spell) override
	{
		deleted.push_back(spell);
		Locator::entitiesRegistry::value().Destroy(spell);
	}
	[[nodiscard]] bool CanCastAt(MagicType, PlayerNames, glm::vec3) override { return false; }
	[[nodiscard]] bool MeetsKindRule(MagicType, const map_coords::MapCoords&, bool) override { return false; }
	bool SpellEvent(entt::entity, const particles::SpellEventInfo&) override { return false; }
	void PayForSpell(entt::entity, float) override {}
	entt::entity CreateDispenser(glm::vec3, MagicType, float) override { return entt::null; }
	void SetDispenserPeriod(entt::entity, float) override {}
	entt::entity CreateOneOffSeed(glm::vec3, SpellSeedType, int, float) override { return entt::null; }
	entt::entity CreateOneOffSeedFor(glm::vec3, MagicType) override { return entt::null; }
	bool Remove(entt::entity) override { return false; }
	entt::entity GiveSeedToHand(PlayerNames, SpellSeedType, int, float) override { return entt::null; }
	entt::entity SummonSeed(PlayerNames, SpellSeedType, int) override { return entt::null; }
	entt::entity SummonSeedAtSite(entt::entity, SpellSeedType, int) override { return entt::null; }
	void DiscardHeldSeed() override {}
	void DiscardHeldSeedOf(PlayerNames) override {}
	void DeleteSeed(entt::entity) override {}
	void UpdateHand(const HandFrame&, const input_steps::Steps&) override {}
	bool TapFireBall(entt::entity) override { return false; }
	bool TapGlobe(entt::entity, PlayerNames) override { return false; }
	bool PressFireBall(uint32_t, uint32_t) override { return false; }
	void HoldFireBall(uint32_t, uint32_t) override {}
	void ReleaseFireBall(uint32_t, uint32_t) override {}
	[[nodiscard]] std::optional<entt::entity> OrbUnderHand() const override { return std::nullopt; }
	bool PressAction() override { return false; }
	void ReleaseAction() override {}
	[[nodiscard]] bool IsHandBusy() const override { return false; }
	[[nodiscard]] bool IsHandBusyOf(PlayerNames) const override { return false; }
	[[nodiscard]] magic::PourPose GetHandPour(float) const override { return {}; }
	[[nodiscard]] HandCastState GetHandCastState() const override { return {}; }
	[[nodiscard]] std::optional<entt::entity> GetHeldSeed() const override { return std::nullopt; }
	[[nodiscard]] std::optional<entt::entity> GetHeldSeedOf(PlayerNames) const override { return std::nullopt; }
	void SetIconSeedPowerUp(entt::entity, int) override {}
	entt::entity TakeIconSeed(entt::entity, PlayerNames, SpellSeedType, int, float) override { return entt::null; }
	[[nodiscard]] HandResult GetLastHandResult() const override { return {}; }
	bool SendSpellEvent(entt::entity, const particles::SpellEventInfo&) override { return false; }
	float ForcePayForSpell(entt::entity, float) override { return 0.0f; }
	[[nodiscard]] float SpellStrength(entt::entity) override { return 1.0f; }
	void ProcessTurn() override {}
	void Update(float) override {}
	void Reset() override {}
	void SetIgnoreInfluence(bool) override {}
	void DriveHand(std::optional<HandFrame>) override {}
	[[nodiscard]] std::optional<HandFrame> GetDrivenHand() const override { return std::nullopt; }
	[[nodiscard]] bool IsIgnoringInfluence() const override { return false; }
	[[nodiscard]] std::vector<SpellInfo> GetSpells() const override { return {}; }
	void RainOnFire(const glm::vec3&) override {}
	[[nodiscard]] std::vector<DispenserInfo> GetDispensers() const override { return {}; }
	void ReleaseCreatureCast(entt::entity) override {}

	std::vector<entt::entity> deleted;
};

class ShieldCloseDown: public ::testing::Test
{
protected:
	void SetUp() override
	{
		if (spdlog::get("game") == nullptr)
		{
			spdlog::create<spdlog::sinks::null_sink_st>("game");
		}
		Locator::entitiesRegistry::emplace<Registry>();
		_info = std::make_unique<InfoConstants>();
		for (const auto type :
		     {Reaction::ReactToMagicShield, Reaction::ReactToMagicShieldStruck, Reaction::ReactToMagicShieldDestroyed})
		{
			_info->reaction.at(static_cast<size_t>(type)).maxReactionDistance = 60.0f;
		}
		Locator::infoConstants::emplace(*_info);
		_magic = &static_cast<DeletingMagic&>(Locator::magicSystem::emplace<DeletingMagic>());
		Locator::reactionSystem::emplace<ReactionSystem>();
	}

	void TearDown() override
	{
		Locator::reactionSystem::reset();
		Locator::magicSystem::reset();
		Locator::infoConstants::reset();
		Locator::entitiesRegistry::reset();
	}

	/// A shield miracle of the first player, cast here
	entt::entity Cast(MagicType type)
	{
		auto& registry = Locator::entitiesRegistry::value();
		const auto spell = registry.Create();
		auto& miracle = registry.Assign<Spell>(spell);
		miracle.magicType = type;
		miracle.spellClass = magic::SpellClass::Shield;
		miracle.caster.player = PlayerNames::PLAYER_ONE;
		miracle.castPlace = map_coords::FromMetres({k_Here.x, k_Here.z});
		miracle.magnitude = 30.0f;
		return spell;
	}

	/// The miracle closes down, as the magic does it
	void CloseDown(entt::entity spell, bool inMiracleTurn)
	{
		Locator::entitiesRegistry::value().Get<Spell>(spell).closedDown = true;
		_shields.MiracleClosedDown(spell, inMiracleTurn);
	}

	/// The turns of the domes until the dome has gone, at most a limit
	uint32_t TurnsUntilGone(entt::entity object)
	{
		auto& registry = Locator::entitiesRegistry::value();
		uint32_t turns = 0;
		while (registry.Valid(object) && turns < 100)
		{
			_shields.ProcessTurn();
			++turns;
		}
		return turns;
	}

	std::unique_ptr<InfoConstants> _info;
	DeletingMagic* _magic {nullptr};
	MagicShieldSystem _shields;
};

TEST_F(ShieldCloseDown, AClosedDownPhysicalShieldKeepsItsDomeAndItsReactionsWhileTheDomeFades)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto spell = Cast(MagicType::PhysicalShield);
	_shields.ProcessTurn();
	const auto object = _shields.ObjectOf(spell);
	ASSERT_NE(object, static_cast<entt::entity>(entt::null));
	ASSERT_TRUE(registry.AllOf<ShieldDome>(object));
	const auto reaction = registry.Get<const MagicShield>(object).reaction;
	ASSERT_NE(reaction, 0u);

	CloseDown(spell, false);
	// The dome lets go of the miracle and is of the neutral player, but the miracle keeps it
	const auto& shield = registry.Get<const MagicShield>(object);
	EXPECT_EQ(shield.spell, static_cast<entt::entity>(entt::null));
	EXPECT_EQ(shield.player, PlayerNames::NEUTRAL);
	EXPECT_TRUE(registry.Get<const ShieldDome>(object).state.dying);
	EXPECT_TRUE(_shields.HasObject(spell));
	EXPECT_FALSE(_shields.LostObject(spell));
	EXPECT_EQ(_shields.KeeperOf(object), spell);
	// Its reactions and its hold on influence stay with the miracle
	EXPECT_EQ(shield.reaction, reaction);
	EXPECT_TRUE(registry.AllOf<AntiInfluence>(object));

	// Through the fade the miracle keeps them all
	for (int turn = 0; turn < 20; ++turn)
	{
		_shields.ProcessTurn();
		ASSERT_TRUE(registry.Valid(object));
		EXPECT_TRUE(_shields.HasObject(spell));
		EXPECT_EQ(registry.Get<const MagicShield>(object).reaction, reaction);
		EXPECT_TRUE(registry.AllOf<AntiInfluence>(object));
	}
	EXPECT_TRUE(_magic->deleted.empty());
}

TEST_F(ShieldCloseDown, TheMiracleGoesInTheTurnThatItsFadedDomeGoes)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto spell = Cast(MagicType::PhysicalShield);
	_shields.ProcessTurn();
	const auto object = _shields.ObjectOf(spell);
	ASSERT_NE(object, static_cast<entt::entity>(entt::null));

	CloseDown(spell, false);
	// The dome fades for 2.25 seconds: it has gone at its 23rd turn of a tenth of a second
	EXPECT_EQ(TurnsUntilGone(object), 23u);
	EXPECT_FALSE(_shields.HasObject(spell));
	EXPECT_FALSE(_shields.LostObject(spell));
	EXPECT_EQ(_shields.KeeperOf(object), static_cast<entt::entity>(entt::null));
	// The miracle goes with it in the same turn
	ASSERT_EQ(_magic->deleted.size(), 1u);
	EXPECT_EQ(_magic->deleted.front(), spell);
	EXPECT_FALSE(registry.Valid(spell));
	EXPECT_EQ(registry.Size<MagicShield>(), 0u);
}

TEST_F(ShieldCloseDown, ADomeWhoseMiracleClosesDownInTheTurnOfTheMiraclesTakesThisTurnAsItStands)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto spell = Cast(MagicType::PhysicalShield);
	_shields.ProcessTurn();
	const auto object = _shields.ObjectOf(spell);
	ASSERT_NE(object, static_cast<entt::entity>(entt::null));

	// Nothing changes until the turn of the domes
	CloseDown(spell, true);
	EXPECT_EQ(registry.Get<const MagicShield>(object).spell, spell);
	EXPECT_FALSE(registry.Get<const ShieldDome>(object).state.dying);

	// The dome takes the turn as it stands, then lets go of the miracle and starts to fade
	_shields.ProcessTurn();
	EXPECT_EQ(registry.Get<const MagicShield>(object).spell, static_cast<entt::entity>(entt::null));
	EXPECT_TRUE(registry.Get<const ShieldDome>(object).state.dying);
	EXPECT_EQ(registry.Get<const ShieldDome>(object).state.dieTime, 0.0f);
	EXPECT_EQ(_shields.KeeperOf(object), spell);
	EXPECT_EQ(TurnsUntilGone(object), 23u);
	ASSERT_EQ(_magic->deleted.size(), 1u);
}

TEST_F(ShieldCloseDown, AClosedDownSpiritualShieldsObjectGoesAtOnce)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto spell = Cast(MagicType::Shield);
	_shields.ProcessTurn();
	const auto object = _shields.ObjectOf(spell);
	ASSERT_NE(object, static_cast<entt::entity>(entt::null));
	ASSERT_FALSE(registry.AllOf<ShieldDome>(object));

	// Also in the turn of the miracles, so that the miracle goes in that same turn
	CloseDown(spell, true);
	EXPECT_FALSE(registry.Valid(object));
	EXPECT_FALSE(_shields.HasObject(spell));
	EXPECT_FALSE(_shields.LostObject(spell));
	EXPECT_EQ(registry.Size<MagicShield>(), 0u);
	// The miracle goes by its own rules, not by the shields
	EXPECT_TRUE(_magic->deleted.empty());
}

TEST_F(ShieldCloseDown, ADeletedPhysicalShieldsDomeFadesAloneWithNoReactions)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto spell = Cast(MagicType::PhysicalShield);
	_shields.ProcessTurn();
	const auto object = _shields.ObjectOf(spell);
	ASSERT_NE(object, static_cast<entt::entity>(entt::null));

	_shields.RemoveWithSpell(spell);
	registry.Destroy(spell);
	ASSERT_TRUE(registry.Valid(object));
	const auto& shield = registry.Get<const MagicShield>(object);
	EXPECT_EQ(shield.spell, static_cast<entt::entity>(entt::null));
	EXPECT_EQ(shield.reaction, 0u);
	EXPECT_FALSE(registry.AllOf<AntiInfluence>(object));
	EXPECT_TRUE(registry.Get<const ShieldDome>(object).state.dying);
	EXPECT_EQ(_shields.KeeperOf(object), static_cast<entt::entity>(entt::null));
	EXPECT_FALSE(_shields.HasObject(spell));
	EXPECT_EQ(TurnsUntilGone(object), 23u);
	EXPECT_TRUE(_magic->deleted.empty());
}

TEST_F(ShieldCloseDown, ADeletedSpiritualShieldsObjectGoesAtOnce)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto spell = Cast(MagicType::Shield);
	_shields.ProcessTurn();
	const auto object = _shields.ObjectOf(spell);
	ASSERT_NE(object, static_cast<entt::entity>(entt::null));
	_shields.RemoveWithSpell(spell);
	EXPECT_FALSE(registry.Valid(object));
}

TEST_F(ShieldCloseDown, TheWorldDestroysAFadingDomeAndItsClosedDownMiracleKeepsNoObject)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto spell = Cast(MagicType::PhysicalShield);
	_shields.ProcessTurn();
	const auto object = _shields.ObjectOf(spell);
	ASSERT_NE(object, static_cast<entt::entity>(entt::null));
	CloseDown(spell, false);

	_shields.ObjectDestroyed(object);
	EXPECT_FALSE(registry.Valid(object));
	EXPECT_FALSE(_shields.HasObject(spell));
	EXPECT_TRUE(_shields.LostObject(spell));
}

/// The reactions of the reaction system of a kind, with their initiator
std::vector<ReactionSystemInterface::Active> ReactionsOfKind(Reaction type)
{
	std::vector<ReactionSystemInterface::Active> found;
	for (const auto& active : Locator::reactionSystem::value().GetReactions())
	{
		if (active.source.type == type)
		{
			found.push_back(active);
		}
	}
	return found;
}

TEST_F(ShieldCloseDown, TheFallOfADomeThatTheWorldDestroysIsAReactionOfItsMiracleUntilTheMiracleGoes)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto spell = Cast(MagicType::PhysicalShield);
	// The miracle's last event was at another place than its cast
	constexpr glm::vec3 k_Strike {510.0f, 0.0f, 495.0f};
	registry.Get<Spell>(spell).place = map_coords::FromMetres({k_Strike.x, k_Strike.z});
	_shields.ProcessTurn();
	const auto object = _shields.ObjectOf(spell);
	ASSERT_NE(object, static_cast<entt::entity>(entt::null));

	// The blast breaks the shield, then destroys its dome
	_shields.ShieldStruck(spell, true);
	ASSERT_EQ(ReactionsOfKind(Reaction::ReactToMagicShieldDestroyed).size(), 1u);
	EXPECT_EQ(registry.Get<const MagicShield>(object).reaction, 0u);
	_shields.ObjectDestroyed(object);
	ASSERT_FALSE(registry.Valid(object));
	EXPECT_TRUE(_shields.LostObject(spell));

	// The fall stays at the place of the last event of the miracle while the miracle runs
	for (int turn = 0; turn < 3; ++turn)
	{
		const auto fall = ReactionsOfKind(Reaction::ReactToMagicShieldDestroyed);
		ASSERT_EQ(fall.size(), 1u);
		EXPECT_EQ(fall.front().source.initiator, spell);
		EXPECT_EQ(fall.front().source.position, k_Strike);
		_shields.ProcessTurn();
	}

	// It goes with the miracle
	registry.Destroy(spell);
	_shields.ProcessTurn();
	EXPECT_TRUE(ReactionsOfKind(Reaction::ReactToMagicShieldDestroyed).empty());
	EXPECT_FALSE(_shields.LostObject(spell));
}

TEST_F(ShieldCloseDown, TheStrikesOfADomeThatTheWorldDestroysStayWithItsMiracleAndTheStandingReactionGoes)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto spell = Cast(MagicType::PhysicalShield);
	_shields.ProcessTurn();
	const auto object = _shields.ObjectOf(spell);
	ASSERT_NE(object, static_cast<entt::entity>(entt::null));
	_shields.ShieldStruck(spell, false);
	ASSERT_EQ(ReactionsOfKind(Reaction::ReactToMagicShieldStruck).size(), 1u);
	ASSERT_EQ(ReactionsOfKind(Reaction::ReactToMagicShield).size(), 1u);

	_shields.ObjectDestroyed(object);
	EXPECT_TRUE(ReactionsOfKind(Reaction::ReactToMagicShield).empty());
	const auto struck = ReactionsOfKind(Reaction::ReactToMagicShieldStruck);
	ASSERT_EQ(struck.size(), 1u);
	EXPECT_EQ(struck.front().source.initiator, spell);
}

TEST_F(ShieldCloseDown, AScriptThatDeletesTheMiracleRemovesTheFallThatItKeeps)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto spell = Cast(MagicType::PhysicalShield);
	_shields.ProcessTurn();
	const auto object = _shields.ObjectOf(spell);
	ASSERT_NE(object, static_cast<entt::entity>(entt::null));
	_shields.ShieldStruck(spell, true);
	_shields.ObjectDestroyed(object);
	ASSERT_EQ(ReactionsOfKind(Reaction::ReactToMagicShieldDestroyed).size(), 1u);

	_shields.RemoveWithSpell(spell);
	EXPECT_TRUE(ReactionsOfKind(Reaction::ReactToMagicShieldDestroyed).empty());
	EXPECT_TRUE(registry.Valid(spell));
}

TEST_F(ShieldCloseDown, TheFallOfADomeThatFadesAloneGoesWithTheDome)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto spell = Cast(MagicType::PhysicalShield);
	_shields.ProcessTurn();
	const auto object = _shields.ObjectOf(spell);
	ASSERT_NE(object, static_cast<entt::entity>(entt::null));
	_shields.ShieldStruck(spell, true);
	ASSERT_EQ(ReactionsOfKind(Reaction::ReactToMagicShieldDestroyed).size(), 1u);

	// The miracle goes first, and the dome then fades alone with no reactions
	registry.Destroy(spell);
	_shields.ProcessTurn();
	EXPECT_TRUE(registry.Valid(object));
	EXPECT_TRUE(ReactionsOfKind(Reaction::ReactToMagicShieldDestroyed).empty());
}

} // namespace
