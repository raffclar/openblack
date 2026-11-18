/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#define LOCATOR_IMPLEMENTATIONS

#include <functional>
#include <memory>
#include <optional>
#include <vector>

#include <MindFile.h>
#include <gtest/gtest.h>
#include <spdlog/sinks/null_sink.h>
#include <spdlog/spdlog.h>

#include "Creature/LeashRules.h"
#include "ECS/Components/Abode.h"
#include "ECS/Components/Creature.h"
#include "ECS/Components/CreatureLeash.h"
#include "ECS/Components/CreatureMind.h"
#include "ECS/Components/Transform.h"
#include "ECS/Registry.h"
#include "ECS/Systems/CreatureMindSystemInterface.h"
#include "ECS/Systems/Implementations/LeashSystem.h"
#include "Locator.h"

namespace
{

using namespace openblack::ecs::components;
using openblack::AbodeNumber;
using openblack::LeashType;
using openblack::Locator;
using openblack::PlayerNames;
using openblack::creature_desires::Desire;
using openblack::ecs::systems::LeashSystem;
namespace leash = openblack::creature_leash;

/// A desire that the tie made dominant
struct Dominant
{
	entt::entity creature;
	Desire desire;
	float seconds;
	bool needsToo;
};

/// A mind system that only keeps each desire that something makes dominant
class DominantMinds final: public openblack::ecs::systems::CreatureMindSystemInterface
{
public:
	void ProcessTurn() override {}
	void PlanTurn() override {}
	void LearnTurn() override {}
	void LoadMind(entt::entity, std::shared_ptr<const openblack::creaturemind::MindFileData>) override {}
	[[nodiscard]] std::optional<openblack::creaturemind::MindFileData> SaveMind(entt::entity) const override
	{
		return std::nullopt;
	}
	void ClearLearning(entt::entity) override {}
	void SeeSkill(const glm::vec3&, size_t) override {}
	void SeeMiracle(const glm::vec3&, size_t) override {}
	void PlayerDid(size_t, const glm::vec3&, std::optional<entt::entity>, std::optional<PlayerNames>) override {}
	[[nodiscard]] const openblack::creature_mind_tables::Tables* GetTables() override { return nullptr; }
	bool PlayAction(entt::entity, size_t) override { return false; }
	bool PlayGesture(entt::entity, size_t) override { return false; }
	void PullFace(entt::entity, size_t) override {}
	std::optional<openblack::creature_face::Request> ShowFeeling(entt::entity, openblack::creature_face::Cue) override
	{
		return std::nullopt;
	}
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
	void MakeDesireDominant(entt::entity creature, Desire desire, float seconds, bool needsToo) override
	{
		made.push_back({.creature = creature, .desire = desire, .seconds = seconds, .needsToo = needsToo});
	}

	std::vector<Dominant> made;
};

/// A creature with a mind, a town centre and a house, with a mind system that keeps the dominant desires
class LeashTownCentreDesire: public ::testing::Test
{
protected:
	void SetUp() override
	{
		// The leash writes to the game log
		if (spdlog::get("game") == nullptr)
		{
			spdlog::create<spdlog::sinks::null_sink_st>("game");
		}
		Locator::entitiesRegistry::emplace<openblack::ecs::Registry>();
		_minds = &static_cast<DominantMinds&>(Locator::creatureMindSystem::emplace<DominantMinds>());
		auto& registry = Locator::entitiesRegistry::value();
		_creature = registry.Create();
		registry.Assign<Transform>(_creature, glm::vec3(100.0f, 0.0f, 100.0f), glm::mat3(1.0f), glm::vec3(1.0f));
		registry.Assign<Creature>(_creature, Creature {.owner = PlayerNames::PLAYER_TWO, .leashable = true});
		registry.Assign<CreatureLeash>(_creature);
		registry.Assign<CreatureMindState>(_creature);
		_centre = MakeAbode(AbodeNumber::TownCentre);
		_house = MakeAbode(AbodeNumber::A);
	}
	void TearDown() override
	{
		Locator::creatureMindSystem::reset();
		Locator::entitiesRegistry::reset();
	}

	static entt::entity MakeAbode(AbodeNumber type)
	{
		auto& registry = Locator::entitiesRegistry::value();
		const auto abode = registry.Create();
		registry.Assign<Transform>(abode, glm::vec3(120.0f, 0.0f, 100.0f), glm::mat3(1.0f), glm::vec3(1.0f));
		registry.Assign<Abode>(abode, Abode {.type = type, .townId = 0, .foodAmount = 0, .woodAmount = 0});
		return abode;
	}

	void KnowEveryLeash()
	{
		for (const auto type : leash::k_Types)
		{
			_leashes.SetKnown(_creature, type, true);
		}
	}

	void Choose(LeashType type) { Locator::entitiesRegistry::value().Get<CreatureLeash>(_creature).selected = type; }

	void ExpectImpressForTwoMinutes() const
	{
		ASSERT_EQ(_minds->made.size(), 1U);
		EXPECT_EQ(_minds->made.front().creature, _creature);
		EXPECT_EQ(_minds->made.front().desire, Desire::Impress);
		EXPECT_EQ(_minds->made.front().seconds, 120.0f);
		EXPECT_FALSE(_minds->made.front().needsToo);
	}

	LeashSystem _leashes;
	DominantMinds* _minds {nullptr};
	entt::entity _creature {entt::null};
	entt::entity _centre {entt::null};
	entt::entity _house {entt::null};
};

TEST(LeashRules, ATieToATownCentreImpressesOnEveryLeashButAggression)
{
	EXPECT_FALSE(leash::TieToTownCentreImpresses(LeashType::Evil));
	EXPECT_TRUE(leash::TieToTownCentreImpresses(LeashType::Rope));
	EXPECT_TRUE(leash::TieToTownCentreImpresses(LeashType::Good));
	// Only a tie that occurs now can give the desire, not a tie that a saved game puts back
	EXPECT_TRUE(leash::EffectsOf(leash::TieSource::Hand).townCentreDesire);
	EXPECT_TRUE(leash::EffectsOf(leash::TieSource::Script).townCentreDesire);
	EXPECT_TRUE(leash::EffectsOf(leash::TieSource::ComputerGod).townCentreDesire);
	EXPECT_FALSE(leash::EffectsOf(leash::TieSource::LoadedGame).townCentreDesire);
}

TEST_F(LeashTownCentreDesire, TheLearningLeashTiedToATownCentreMakesImpressDominantForTwoMinutes)
{
	KnowEveryLeash();
	Choose(LeashType::Rope);
	ASSERT_TRUE(_leashes.TieTo(_creature, _centre));
	ExpectImpressForTwoMinutes();
}

TEST_F(LeashTownCentreDesire, TheCompassionLeashTiedToATownCentreMakesImpressDominant)
{
	KnowEveryLeash();
	Choose(LeashType::Good);
	ASSERT_TRUE(_leashes.TieTo(_creature, _centre));
	ExpectImpressForTwoMinutes();
}

TEST_F(LeashTownCentreDesire, TheAggressionLeashTiedToATownCentreGivesNoDesire)
{
	KnowEveryLeash();
	Choose(LeashType::Evil);
	ASSERT_TRUE(_leashes.TieTo(_creature, _centre));
	EXPECT_TRUE(_minds->made.empty());
}

TEST_F(LeashTownCentreDesire, ATieToAnotherBuildingGivesNoDesire)
{
	KnowEveryLeash();
	ASSERT_TRUE(_leashes.TieTo(_creature, _house));
	EXPECT_TRUE(_minds->made.empty());
}

TEST_F(LeashTownCentreDesire, ATieThatASavedGamePutsBackGivesNoDesire)
{
	KnowEveryLeash();
	ASSERT_TRUE(_leashes.RestoreTie(_creature, _centre, 5));
	EXPECT_TRUE(_minds->made.empty());
}

TEST_F(LeashTownCentreDesire, AComputerGodTieWithNoLeashStillMakesImpressDominant)
{
	// The creature knows no leash, so the leash does not hold. The leash that its player chose last still counts.
	EXPECT_FALSE(_leashes.ComputerGodTie(_creature, _centre));
	EXPECT_FALSE(Locator::entitiesRegistry::value().Get<const CreatureLeash>(_creature).worn.has_value());
	ExpectImpressForTwoMinutes();
}

TEST_F(LeashTownCentreDesire, AComputerGodTieWithNoLeashOnTheAggressionLeashGivesNoDesire)
{
	Choose(LeashType::Evil);
	EXPECT_FALSE(_leashes.ComputerGodTie(_creature, _centre));
	EXPECT_TRUE(_minds->made.empty());
}

} // namespace
