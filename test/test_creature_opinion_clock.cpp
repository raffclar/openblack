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

#include <gtest/gtest.h>

#include "ECS/Components/Creature.h"
#include "ECS/Components/CreatureMind.h"
#include "ECS/Registry.h"
#include "ECS/Systems/Implementations/MagicLiving.h"
#include "ECS/Systems/TimeSystemInterface.h"
#include "InfoConstants.h"
#include "Locator.h"
#include "Magic/SpellRules.h"

using namespace openblack;
using namespace openblack::ecs;
using namespace openblack::ecs::components;

namespace
{

/// A clock that gives the turn the test sets
class SetTurn final: public ecs::systems::TimeSystemInterface
{
public:
	void Start() override {}
	void Update() override {}
	void StartDraw() override {}
	[[nodiscard]] std::chrono::milliseconds GetElapsedTime() const override { return {}; }
	void StartGameClock(bool /*paused*/) override {}
	void RestartTurns() override {}
	void ResetTimerToTurn() override {}
	void ResumeAtTurn(uint32_t /*turn*/, int32_t /*remainderMs*/) override {}
	[[nodiscard]] int32_t GetTurnRemainderMs() const override { return 0; }
	[[nodiscard]] bool IsTurnDue() override { return false; }
	void StartTurn() override {}
	void UpdateFrame() override {}
	void StepInput(std::chrono::microseconds /*stepTime*/) override {}
	[[nodiscard]] const input_steps::Steps& GetInputSteps() const override { return inputSteps; }
	void PinDate(std::optional<int64_t> /*pinnedDate*/) override {}
	[[nodiscard]] uint32_t GetTurn() const override { return turn; }
	[[nodiscard]] float GetTurnFraction() const override { return 0.0f; }
	[[nodiscard]] std::chrono::milliseconds GetFrameGameTime() const override { return {}; }
	[[nodiscard]] std::chrono::milliseconds GetFrameRealTime() const override { return {}; }
	void SetPaused(bool /*paused*/) override {}
	[[nodiscard]] bool IsPaused() const override { return false; }
	void SetPlayerPause(bool /*paused*/) override {}
	void SetHeld(bool /*held*/) override {}
	[[nodiscard]] bool IsHeld() const override { return false; }
	void SetNetworkGame(bool /*value*/) override {}
	[[nodiscard]] bool IsNetworkGame() const override { return false; }
	void SetSpeed(float /*speed*/) override {}
	[[nodiscard]] float GetSpeed() const override { return 1.0f; }
	[[nodiscard]] uint32_t GetTicks() const override { return 0; }
	[[nodiscard]] int64_t GetUnixTime() const override { return 0; }
	void RestartClock(std::optional<int64_t> /*unixTime*/) override {}
	[[nodiscard]] std::optional<int64_t> GetPinnedDate() const override { return std::nullopt; }

	uint32_t turn {0};
	input_steps::Steps inputSteps {};
};

class CreatureOpinionClock: public ::testing::Test
{
protected:
	void SetUp() override
	{
		Locator::entitiesRegistry::emplace<Registry>();
		_time = &static_cast<SetTurn&>(Locator::time::emplace<SetTurn>());
		// The tables are large: made on the heap. A heal counts half against a creature.
		_info = std::make_unique<InfoConstants>();
		_info->alignment.at(static_cast<size_t>(magic::EffectKind::Heal)).creature = 0.5f;
		Locator::infoConstants::emplace(*_info);
		auto& registry = Locator::entitiesRegistry::value();
		_reached = registry.Create();
		registry.Assign<Creature>(_reached).owner = PlayerNames::PLAYER_ONE;
		registry.Assign<CreatureMindState>(_reached);
		_caster = registry.Create();
		registry.Assign<Creature>(_caster).owner = PlayerNames::PLAYER_TWO;
		registry.Assign<CreatureMindState>(_caster);
	}

	void TearDown() override
	{
		Locator::infoConstants::reset();
		Locator::time::reset();
		Locator::entitiesRegistry::reset();
	}

	/// The caster's heal reaches the creature on a turn
	void HealAt(uint32_t turn)
	{
		_time->turn = turn;
		magic::EffectValues heal;
		heal[magic::EffectKind::Heal] = 0.4f;
		ecs::systems::magic_living::AfterEffect(
		    _reached, heal, magic::EffectSource {.player = PlayerNames::PLAYER_TWO, .casterCreature = _caster});
	}

	[[nodiscard]] CreatureMindState& Mind() const
	{
		return Locator::entitiesRegistry::value().Get<CreatureMindState>(_reached);
	}

	SetTurn* _time {nullptr};
	std::unique_ptr<InfoConstants> _info;
	entt::entity _reached {entt::null};
	entt::entity _caster {entt::null};
};

} // namespace

// The heal of another creature changes how nice the creature finds that creature, no more often than every 600 turns
TEST_F(CreatureOpinionClock, AnEffectOfACreatureChangesTheOpinionNoMoreOftenThanTheClockAllows)
{
	HealAt(10);
	ASSERT_EQ(Mind().leash.attitudes.size(), 1u);
	EXPECT_EQ(Mind().leash.attitudes.front().creature, entt::to_integral(_caster));
	EXPECT_EQ(Mind().leash.attitudes.front().change, 0.4f * 0.5f);
	EXPECT_EQ(Mind().leash.attitudeTurn, 10u);
	HealAt(610);
	EXPECT_EQ(Mind().leash.attitudes.size(), 1u);
	HealAt(611);
	EXPECT_EQ(Mind().leash.attitudes.size(), 2u);
}

// A leash that changed how nice a creature finds another creature also holds back the opinion change of an effect: the
// two use the same clock
TEST_F(CreatureOpinionClock, ALeashChangeAndAnEffectChangeShareOneClock)
{
	Mind().leash.attitudeTurn = 100u;
	HealAt(650);
	EXPECT_TRUE(Mind().leash.attitudes.empty());
	HealAt(701);
	ASSERT_EQ(Mind().leash.attitudes.size(), 1u);
	EXPECT_EQ(Mind().leash.attitudeTurn, 701u);
}

// A creature without a mind keeps no opinion
TEST_F(CreatureOpinionClock, ACreatureWithoutAMindKeepsNoOpinion)
{
	Locator::entitiesRegistry::value().Remove<CreatureMindState>(_reached);
	_time->turn = 10;
	magic::EffectValues heal;
	heal[magic::EffectKind::Heal] = 0.4f;
	ecs::systems::magic_living::AfterEffect(_reached, heal,
	                                        magic::EffectSource {.player = PlayerNames::PLAYER_TWO, .casterCreature = _caster});
	EXPECT_FALSE(Locator::entitiesRegistry::value().AllOf<CreatureMindState>(_reached));
}
