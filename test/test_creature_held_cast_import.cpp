/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#define LOCATOR_IMPLEMENTATIONS

#include <memory>
#include <optional>
#include <vector>

#include <entt/entity/entity.hpp>
#include <glm/vec3.hpp>
#include <gtest/gtest.h>
#include <spdlog/sinks/null_sink.h>
#include <spdlog/spdlog.h>

#include "ECS/Components/Creature.h"
#include "ECS/Components/Spell.h"
#include "ECS/Registry.h"
#include "ECS/Systems/EffectRequestSystemInterface.h"
#include "ECS/Systems/Implementations/EffectRequestSystem.h"
#include "ECS/Systems/Implementations/MagicSystem.h"
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
	void Stop(EffectHandle) override {}
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
		    .position = {10.0f, 2.0f, 20.0f},
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
	EXPECT_EQ(made.position, glm::vec3(10.0f, 2.0f, 20.0f));
	EXPECT_EQ(made.processInfo.handPosition, glm::vec3(10.0f, 12.0f, 20.0f));
	EXPECT_EQ(made.processInfo.power, 0.75f);
	EXPECT_FALSE(made.closedDown);
}

// The import makes no spell of a kind that keeps more values in the file, such as the heal
TEST_F(CreatureHeldCastImport, TheImportMakesNoSpellOfAKindWithMoreValues)
{
	auto heal = Water();
	heal.type = MagicType::Heal;
	EXPECT_TRUE(_magic->ImportCreatureCast(_creature, heal) == entt::null);
	EXPECT_TRUE(_magic->CreatureCastOf(_creature) == entt::null);
	auto none = Water();
	none.type = MagicType::None;
	EXPECT_TRUE(_magic->ImportCreatureCast(_creature, none) == entt::null);
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
