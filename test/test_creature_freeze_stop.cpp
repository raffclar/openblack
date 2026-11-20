/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#define LOCATOR_IMPLEMENTATIONS 1

#include <cstddef>

#include <functional>
#include <memory>
#include <optional>
#include <vector>

#include <MindFile.h>
#include <entt/entity/entity.hpp>
#include <glm/vec3.hpp>
#include <gtest/gtest.h>
#include <spdlog/sinks/null_sink.h>
#include <spdlog/spdlog.h>

#include "Creature/CreatureSpells.h"
#include "ECS/Components/Creature.h"
#include "ECS/Components/CreatureSpells.h"
#include "ECS/Components/ScriptControl.h"
#include "ECS/Registry.h"
#include "ECS/Systems/CreatureMindSystemInterface.h"
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

/// A mind that keeps each way that something stopped what the creature did, and does nothing else
class StoppedMinds final: public CreatureMindSystemInterface
{
public:
	void ProcessTurn() override {}
	void PlanTurn() override {}
	void LearnTurn() override {}
	void LoadMind(entt::entity, std::shared_ptr<const creaturemind::MindFileData>) override {}
	[[nodiscard]] std::optional<creaturemind::MindFileData> SaveMind(entt::entity) const override { return std::nullopt; }
	void ClearLearning(entt::entity) override {}
	void SeeSkill(const glm::vec3&, size_t) override {}
	void SeeMiracle(const glm::vec3&, size_t) override {}
	void PlayerDid(size_t, const glm::vec3&, std::optional<entt::entity>, std::optional<PlayerNames>) override {}
	[[nodiscard]] const creature_mind_tables::Tables* GetTables() override { return nullptr; }
	bool PlayAction(entt::entity, size_t) override { return false; }
	bool PlayGesture(entt::entity, size_t) override { return false; }
	void PullFace(entt::entity, size_t) override {}
	std::optional<creature_face::Request> ShowFeeling(entt::entity, creature_face::Cue) override { return std::nullopt; }
	bool SitDown(entt::entity) override { return false; }
	void StandUp(entt::entity) override {}
	void ReceiveFeedback(entt::entity, float) override {}
	bool ForceAction(entt::entity, size_t, bool, double) override { return false; }
	bool Sleep(entt::entity) override { return false; }
	bool Eat(entt::entity, std::optional<entt::entity>) override { return false; }
	bool Drink(entt::entity) override { return false; }
	bool Poo(entt::entity) override { return false; }
	bool Puke(entt::entity) override { return false; }
	bool Faint(entt::entity) override { return false; }
	void Wake(entt::entity) override {}
	void FoughtFight(entt::entity, bool) override {}
	void ChangeDesires(entt::entity, const std::function<void(creature_desires::Desires&)>&) override {}

	void AbandonAction(entt::entity creature) override { abandoned.push_back(creature); }
	void GiveUpAction(entt::entity creature) override { givenUp.push_back(creature); }
	void StopAndGiveUpAction(entt::entity creature) override { stopped.push_back(creature); }

	std::vector<entt::entity> abandoned;
	std::vector<entt::entity> givenUp;
	std::vector<entt::entity> stopped;
};

class CreatureFreezeStop: public ::testing::Test
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
		_minds = &static_cast<StoppedMinds&>(Locator::creatureMindSystem::emplace<StoppedMinds>());
		_magic = &static_cast<MagicSystem&>(Locator::magicSystem::emplace<MagicSystem>());
		auto& registry = Locator::entitiesRegistry::value();
		_creature = registry.Create();
		registry.Assign<Creature>(_creature, Creature {.owner = PlayerNames::PLAYER_ONE});
		registry.Assign<CreatureSpells>(_creature);
	}
	void TearDown() override
	{
		Locator::magicSystem::reset();
		Locator::creatureMindSystem::reset();
		Locator::effectRequestSystem::reset();
		Locator::entitiesRegistry::reset();
		Locator::infoConstants::reset();
	}

	/// The spell takes hold of the creature: it waits for its delay, then starts
	void Cast(creature_spells::Spell spell)
	{
		auto& spells = Locator::entitiesRegistry::value().Get<CreatureSpells>(_creature).spells;
		static_cast<void>(creature_spells::Receive(spells, spell, 100, entt::null));
		for (int turn = 0; turn < 40; ++turn)
		{
			_magic->ProcessTurn();
		}
	}
	[[nodiscard]] creature_spells::Phase PhaseOf(creature_spells::Spell spell) const
	{
		return Locator::entitiesRegistry::value().Get<const CreatureSpells>(_creature).spells[spell].phase;
	}

	std::unique_ptr<InfoConstants> _info;
	ShownEffects _effects;
	StoppedMinds* _minds {nullptr};
	MagicSystem* _magic {nullptr};
	entt::entity _creature {entt::null};
};
} // namespace

// The freeze stops all that the creature does as a failure, as a plan that it cannot make does. Thus a walk to a thing,
// such as a walk to pick up a rock, stops too, and the frozen creature does not continue to the thing.
// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables): external macro
TEST_F(CreatureFreezeStop, TheFreezeStopsAllThatTheCreatureDoes)
{
	Cast(creature_spells::Spell::Freeze);
	ASSERT_NE(PhaseOf(creature_spells::Spell::Freeze), creature_spells::Phase::Off);
	EXPECT_EQ(_minds->stopped, (std::vector<entt::entity> {_creature}));
	EXPECT_TRUE(_minds->abandoned.empty());
	EXPECT_TRUE(_minds->givenUp.empty());
}

// The freeze also stops a creature that a script controls
// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables): external macro
TEST_F(CreatureFreezeStop, TheFreezeAlsoStopsACreatureThatAScriptControls)
{
	Locator::entitiesRegistry::value().Assign<ScriptControlled>(_creature);
	Cast(creature_spells::Spell::Freeze);
	EXPECT_EQ(_minds->stopped, (std::vector<entt::entity> {_creature}));
}

// A mood spell stops all that the creature does in the same way, but not when a script controls the creature
// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables): external macro
TEST_F(CreatureFreezeStop, AMoodStopsAllThatTheCreatureDoesUnlessAScriptControlsIt)
{
	Cast(creature_spells::Spell::Itchy);
	EXPECT_EQ(_minds->stopped, (std::vector<entt::entity> {_creature}));

	_minds->stopped.clear();
	Locator::entitiesRegistry::value().Assign<ScriptControlled>(_creature);
	Cast(creature_spells::Spell::Nice);
	EXPECT_TRUE(_minds->stopped.empty());
}

// A spell that does not change what the creature does stops nothing
// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables): external macro
TEST_F(CreatureFreezeStop, ASizeSpellStopsNothing)
{
	Cast(creature_spells::Spell::Small);
	EXPECT_TRUE(_minds->stopped.empty());
	EXPECT_TRUE(_minds->abandoned.empty());
}
