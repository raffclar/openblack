/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The town belief (ECS/Town/TownBelief): SetBelief's
// cap, Init and the ctor order, the per-turn Fold (pending, recent, boredom, the desires' cost, the
// conversion), ProcessOncePerTurn, CheckLosingBelief's help sprite and the land script's SET_TOWN_BELIEF*.
// The values are those of info.dat.

#define LOCATOR_IMPLEMENTATIONS

#include <array>
#include <memory>
#include <utility>
#include <vector>

#include <entt/entity/entity.hpp>
#include <glm/vec3.hpp>
#include <gtest/gtest.h>

#include "Common/EventManager.h"
#include "ECS/Archetypes/TownArchetype.h"
#include "ECS/Components/Town.h"
#include "ECS/Events/TownBeliefEvents.h"
#include "ECS/Influence/InfluenceState.h"
#include "ECS/Registry.h"
#include "ECS/Town/TownBelief.h"
#include "Enums.h"
#include "GameClock.h"
#include "InfoConstants.h"
#include "LHScriptX/FeatureScriptCommands.h"
#include "LandBalance.h"
#include "Locator.h"
#include "support/TestServices.h"
#include "support/WorldSystems.h"

using namespace openblack;
using namespace openblack::ecs::components;
namespace tb = openblack::ecs::town_belief;

namespace
{
constexpr size_t P(PlayerNames player)
{
	return static_cast<size_t>(player);
}

constexpr size_t k_One = P(PlayerNames::PLAYER_ONE);
constexpr size_t k_Two = P(PlayerNames::PLAYER_TWO);
constexpr size_t k_Neutral = P(PlayerNames::NEUTRAL);

class TownBeliefTest: public ::testing::Test
{
protected:
	void SetUp() override
	{
		test::EmplaceMapAndVillagerDefaults();
		auto info = std::make_unique<InfoConstants>();
		// the belief, player and town values of info.dat
		info->belief.claimedTownBeliefMultiplier = 1.5f;
		info->belief.lostATownBeliefInPlayerMultiplier = 0.9f;
		info->belief.minimumThreshold = 0.25f;
		info->belief.beliefLeftWhenHelpSpritesWarn = 0.05f;
		info->player.computerPlayerBeliefChangeDecay = 0.997f;
		info->town.beliefInNeutralPlayer = 0.5f;
		// the town desires' belief scale, delay and threshold decay
		const std::array<float, 17> scales = {6e-5f, 5e-5f, 2e-5f, 1.3e-4f, 0.0f,  2e-5f, 3e-5f, 0.0f, 0.0f,
		                                      0.0f,  0.0f,  0.0f,  3e-5f,   2e-5f, 0.0f,  2e-5f, 2e-5f};
		for (size_t d = 0; d < scales.size(); ++d)
		{
			auto& desire = info->townDesire.at(d);
			desire.desireToBeliefScale = scales.at(d);
			desire.desireAffectsBeliefAfter = d == 14 ? 5.0f : 1.0f;
			desire.desireToBeliefThresholdDecay = 2.1e-5f;
		}
		// the reactions' boredom additions: 0.00056, 0.00028 for 0, 3, 5 and 28
		for (size_t k = 0; k < info->reaction.size(); ++k)
		{
			const bool half = k == 0 || k == 3 || k == 5 || k == 28;
			info->reaction.at(k).additionToTownBoredomMultipliers = half ? 0.00028f : 0.00056f;
		}
		Locator::infoConstants::reset(info.release());
		Locator::entitiesRegistry::emplace<ecs::Registry>();
		openblack::test::EmplaceWorldSystems();
		land_balance::Reset();
		game_clock::SetTurn(1);
		influence::detail::MapGlobals().landNumber = 0;
		tb::detail::ClearForTests();
		// A fresh event manager per test: its handlers capture this fixture
		Locator::events::emplace<EventManager>();
		auto& events = Locator::events::value();
		_help.clear();
		events.AddHandler<ecs::events::TownBeliefHelp>(
		    [this](const ecs::events::TownBeliefHelp& event) { _help.emplace_back(event.kind, event.town); });
		_tips.clear();
		events.AddHandler<ecs::events::TownBeliefToolTip>(
		    [this](const ecs::events::TownBeliefToolTip& event) { _tips.emplace_back(event.text, event.value); });
	}

	void TearDown() override
	{
		Locator::events::reset();
		tb::detail::ClearForTests();
		influence::detail::MapGlobals().landNumber = 0;
		land_balance::Reset();
		game_clock::SetTurn(0);
		Locator::entitiesRegistry::reset();
		openblack::test::ResetWorldSystems();
		test::ResetMapAndVillagerDefaults();
		Locator::infoConstants::reset();
	}

	static entt::entity MakeTown(int id, PlayerNames owner)
	{
		return ecs::archetypes::TownArchetype::Create(id, glm::vec3(100.0f * static_cast<float>(id), 0.0f, 0.0f), owner,
		                                              Tribe::CELTIC);
	}

	static Town& TownOf(entt::entity town) { return Locator::entitiesRegistry::value().Get<Town>(town); }
	static TownBelief& BeliefOf(entt::entity town) { return TownOf(town).belief; }

	std::vector<std::pair<tb::detail::HelpSprite, entt::entity>> _help;
	std::vector<std::pair<uint32_t, float>> _tips; ///< help::tooltips::Force(text, value)
};
} // namespace

// 1. SetBelief clamps to the cap only
TEST_F(TownBeliefTest, SetBeliefClampsToCapOnly)
{
	TownBelief belief {};
	tb::SetBelief(belief, PlayerNames::PLAYER_ONE, 12.0f);
	EXPECT_EQ(belief.belief.at(k_One), 10.0f); // the default cap 10 (Init)
	tb::SetBelief(belief, PlayerNames::PLAYER_ONE, -3.0f);
	EXPECT_EQ(belief.belief.at(k_One), -3.0f); // no lower clamp
	tb::SetCap(belief, PlayerNames::PLAYER_ONE, 2.0f);
	tb::SetBelief(belief, PlayerNames::PLAYER_ONE, 5.0f);
	EXPECT_EQ(belief.belief.at(k_One), 2.0f);
	EXPECT_EQ(tb::GetCap(belief, PlayerNames::PLAYER_ONE), 2.0f);
	EXPECT_EQ(tb::GetBeliefInPlayer(belief, PlayerNames::PLAYER_ONE), 2.0f);
	tb::SetBelief(belief, PlayerNames::PLAYER_ONE, 2.0f); // equal: v itself
	EXPECT_EQ(belief.belief.at(k_One), 2.0f);
}

// 2. Init and the ctor order (Init before beliefInNeutralPlayer: the neutral belief 0 until the first fold)
TEST_F(TownBeliefTest, InitAndCtorOrder)
{
	Town town {};
	auto& b = town.belief;
	b.belief.fill(5.0f);
	b.cap.fill(3.0f);
	b.pending.fill(1.0f);
	b.reduceAccumulator.fill(1.0f);
	b.lastAddedTurn.fill(7);
	b.boredom.fill(0.2f);
	b.recent.at(2) = 0.3f;
	b.addedThisPeriod.at(2) = 0.4f;
	b.beliefInNeutralPlayer = 0.7f;
	tb::Init(town);
	for (size_t i = 0; i < tb::k_Players; ++i)
	{
		EXPECT_EQ(b.belief.at(i), i == k_Neutral ? 0.7f : 0.0f) << i;
		EXPECT_EQ(b.cap.at(i), 10.0f);
		EXPECT_EQ(b.pending.at(i), 0.0f);
		EXPECT_EQ(b.reduceAccumulator.at(i), 0.0f);
		EXPECT_EQ(b.lastAddedTurn.at(i), 0u);
	}
	for (const float boredom : b.boredom)
	{
		EXPECT_EQ(boredom, 1.0f);
	}
	for (size_t d = 0; d < tb::k_Desires; ++d)
	{
		EXPECT_EQ(b.desireThreshold.at(d), d == 14 ? 5.0f : 1.0f) << d;
	}
	EXPECT_EQ(b.recent.at(2), 0.3f);          // not reset
	EXPECT_EQ(b.addedThisPeriod.at(2), 0.4f); // not reset

	// the Town ctor: Init, then beliefInNeutralPlayer from info.dat and beliefScale = 1
	const auto made = MakeTown(1, PlayerNames::NEUTRAL);
	EXPECT_EQ(BeliefOf(made).belief.at(k_Neutral), 0.0f);
	EXPECT_EQ(BeliefOf(made).beliefInNeutralPlayer, 0.5f);
	EXPECT_EQ(BeliefOf(made).beliefScale, 1.0f);
	EXPECT_EQ(BeliefOf(made).desireThreshold.at(0), 1.0f);
	tb::Fold(made); // step 4 pins it
	EXPECT_EQ(BeliefOf(made).belief.at(k_Neutral), 0.5f);
}

// 3. The fold's pending -> belief (step 1)
TEST_F(TownBeliefTest, FoldPending)
{
	const auto town = MakeTown(1, PlayerNames::NEUTRAL); // neutral: no desire cost, the thresholds only reset
	auto& b = BeliefOf(town);
	b.desireThreshold.at(3) = 0.4f;
	b.pending.at(k_One) = 0.1f;
	tb::Fold(town);
	EXPECT_EQ(b.belief.at(k_One), 0.1f);
	EXPECT_EQ(b.addedThisPeriod.at(k_One), 0.1f);
	EXPECT_EQ(b.pending.at(k_One), 0.0f);
	EXPECT_EQ(b.desireThreshold.at(3), 1.0f);
	EXPECT_EQ(TownOf(town).owner, PlayerNames::NEUTRAL);
	EXPECT_EQ(tb::BeliefSpriteCount(), 0u); // no town centre: no sprite

	// the belief scale at 0: d == 0, pending kept, nothing else
	const auto other = MakeTown(2, PlayerNames::NEUTRAL);
	auto& o = BeliefOf(other);
	o.beliefScale = 0.0f;
	o.desireThreshold.at(3) = 0.4f;
	o.pending.at(k_One) = 0.1f;
	tb::Fold(other);
	EXPECT_EQ(o.pending.at(k_One), 0.1f);
	EXPECT_EQ(o.belief.at(k_One), 0.0f);
	EXPECT_EQ(o.addedThisPeriod.at(k_One), 0.0f);
	EXPECT_EQ(o.desireThreshold.at(3), 0.4f);

	// the cap applies to the folded value
	b.cap.at(k_One) = 0.15f;
	b.pending.at(k_One) = 0.1f;
	tb::Fold(town);
	EXPECT_EQ(b.belief.at(k_One), 0.15f);
	EXPECT_EQ(b.addedThisPeriod.at(k_One), 0.2f);
}

// 4. recent x 0.997 for the 8 slots every fold
TEST_F(TownBeliefTest, FoldRecentDecay)
{
	const auto town = MakeTown(1, PlayerNames::NEUTRAL);
	auto& b = BeliefOf(town);
	b.recent.fill(1.0f);
	tb::Fold(town);
	for (const float recent : b.recent)
	{
		EXPECT_EQ(recent, 0.997f);
	}
	tb::Fold(town);
	for (const float recent : b.recent)
	{
		EXPECT_EQ(recent, 0.997f * 0.997f);
	}
}

// 5. The boredom (step 2): stored only below 1
TEST_F(TownBeliefTest, FoldBoredom)
{
	const auto town = MakeTown(1, PlayerNames::NEUTRAL);
	auto& b = BeliefOf(town);
	b.boredom.at(1) = 1.0f - 0.0003f; // + 0.00056 >= 1: unchanged
	b.boredom.at(2) = 0.5f;
	b.boredom.at(3) = 0.5f; // 0.00028
	tb::Fold(town);
	EXPECT_EQ(b.boredom.at(1), 1.0f - 0.0003f);
	EXPECT_FLOAT_EQ(b.boredom.at(2), 0.50056f);
	EXPECT_FLOAT_EQ(b.boredom.at(3), 0.50028f);
	EXPECT_EQ(b.boredom.at(4), 1.0f); // from Init: 1 + 0.00056 >= 1

	// x the lost-town scale
	land_balance::SetLostTownScale(0.5f);
	b.boredom.at(2) = 0.5f;
	tb::Fold(town);
	EXPECT_FLOAT_EQ(b.boredom.at(2), 0.50028f);

	// AddToBoredomMultiplier: not below 0
	tb::AddToBoredomMultiplier(b, 2, -1.0f);
	EXPECT_EQ(b.boredom.at(2), 0.0f);
	EXPECT_EQ(tb::GetBoredomMultiplier(town, 2), 0.0f);
	EXPECT_EQ(tb::GetBoredomMultiplier(entt::null, 2), 1.0f);
}

// 6. The desires that cost the owner belief (step 3)
TEST_F(TownBeliefTest, FoldDesireCost)
{
	const auto town = MakeTown(1, PlayerNames::PLAYER_TWO);
	auto& t = TownOf(town);
	auto& b = t.belief;
	b.belief.at(k_Two) = 2.0f;
	t.desire.desire.at(0) = 1.5f; // GetDesire = the sum of its three parts = 1.5 > threshold 1
	b.desireThreshold.at(1) = 0.25f;
	tb::Fold(town);
	EXPECT_NEAR(b.belief.at(k_Two), 2.0f - 3e-5f, 1e-6f); // (1.5 - 1) x 6e-5
	EXPECT_EQ(b.desireThreshold.at(0), 1.0f - 2.1e-5f);
	EXPECT_EQ(b.desireThreshold.at(1), 0.25f); // at the minimum 0.25: no more decay
	EXPECT_EQ(b.desireThreshold.at(14), 5.0f - 2.1e-5f);
	EXPECT_EQ(b.reduceAccumulator.at(k_Two), 0.0f); // no town centre: not accumulated
	EXPECT_EQ(t.owner, PlayerNames::PLAYER_TWO);

	// the neutral owner: nothing
	const auto neutral = MakeTown(2, PlayerNames::NEUTRAL);
	auto& n = TownOf(neutral);
	n.belief.belief.at(k_Neutral) = 0.5f;
	n.desire.desire.at(0) = 1.5f;
	tb::Fold(neutral);
	EXPECT_EQ(n.belief.belief.at(k_Neutral), 0.5f);
	EXPECT_EQ(n.belief.desireThreshold.at(0), 1.0f);
}

// 7. The conversion (steps 5 and 6)
TEST_F(TownBeliefTest, FoldConversion)
{
	// a neutral town, 0.5 against player one's 0.5: the tie goes to the higher slot, the neutral 7
	const auto tie = MakeTown(1, PlayerNames::NEUTRAL);
	BeliefOf(tie).belief.at(k_One) = 0.5f;
	tb::Fold(tie);
	EXPECT_EQ(TownOf(tie).owner, PlayerNames::NEUTRAL);
	EXPECT_EQ(BeliefOf(tie).belief.at(k_One), 0.5f);
	EXPECT_TRUE(_help.empty());

	// 0.51: player one takes it with 1.5 x 0.51
	const auto taken = MakeTown(2, PlayerNames::NEUTRAL);
	BeliefOf(taken).belief.at(k_One) = 0.51f;
	tb::Fold(taken);
	EXPECT_EQ(TownOf(taken).owner, PlayerNames::PLAYER_ONE);
	EXPECT_FLOAT_EQ(BeliefOf(taken).belief.at(k_One), 0.765f);
	ASSERT_EQ(_help.size(), 1u);
	EXPECT_EQ(_help.at(0).first, tb::detail::HelpSprite::GeneralGood); // the new owner is the local player
	EXPECT_EQ(_help.at(0).second, taken);
	_help.clear();

	// a town of player one with 0.4 against the neutral 0.5: back to the neutral player; every town of player one
	// (this one included) x 0.9 x the lost-town scale
	const auto lost = MakeTown(3, PlayerNames::PLAYER_ONE);
	const auto kept = MakeTown(4, PlayerNames::PLAYER_ONE);
	BeliefOf(lost).belief.at(k_One) = 0.4f;
	BeliefOf(kept).belief.at(k_One) = 2.0f;
	tb::Fold(lost);
	EXPECT_EQ(TownOf(lost).owner, PlayerNames::NEUTRAL);
	EXPECT_FLOAT_EQ(BeliefOf(lost).belief.at(k_Neutral), 0.75f); // 1.5 x the pinned 0.5
	EXPECT_FLOAT_EQ(BeliefOf(lost).belief.at(k_One), 0.36f);
	EXPECT_FLOAT_EQ(BeliefOf(kept).belief.at(k_One), 1.8f);
	EXPECT_FLOAT_EQ(BeliefOf(taken).belief.at(k_One), 0.765f * 0.9f); // the town taken above is player one's too
	EXPECT_EQ(TownOf(kept).owner, PlayerNames::PLAYER_ONE);
	ASSERT_EQ(_help.size(), 1u);
	EXPECT_EQ(_help.at(0).first, tb::detail::HelpSprite::GeneralBad); // the old owner is the local player

	// with SET_LOST_TOWN_SCALE 0.5
	land_balance::SetLostTownScale(0.5f);
	const auto second = MakeTown(5, PlayerNames::PLAYER_ONE);
	BeliefOf(second).belief.at(k_One) = 0.1f;
	tb::Fold(second);
	EXPECT_EQ(TownOf(second).owner, PlayerNames::NEUTRAL);
	EXPECT_FLOAT_EQ(BeliefOf(kept).belief.at(k_One), 1.8f * 0.9f * 0.5f);
}

// 8. ProcessOncePerTurn
TEST_F(TownBeliefTest, ProcessOncePerTurn)
{
	const auto first = MakeTown(1, PlayerNames::PLAYER_TWO);
	const auto second = MakeTown(2, PlayerNames::PLAYER_TWO);
	BeliefOf(first).addedThisPeriod.at(k_One) = 0.002f;
	BeliefOf(second).addedThisPeriod.at(k_One) = 0.003f;
	BeliefOf(second).addedThisPeriod.at(k_Two) = 0.5f;

	game_clock::SetTurn(21); // not a multiple of 10: nothing
	tb::ProcessOncePerTurn();
	EXPECT_EQ(BeliefOf(first).addedThisPeriod.at(k_One), 0.002f);
	EXPECT_TRUE(_tips.empty());

	game_clock::SetTurn(30);
	tb::ProcessOncePerTurn();
	for (const auto town : {first, second})
	{
		for (const float added : BeliefOf(town).addedThisPeriod)
		{
			EXPECT_EQ(added, 0.0f);
		}
	}
	ASSERT_EQ(_tips.size(), 1u);
	EXPECT_EQ(_tips.at(0).first, 0xEE1u);
	EXPECT_EQ(_tips.at(0).second, (0.0f + 0.003f + 0.002f) * 1000.0f); // newest first, the local player's only
	_tips.clear();

	// not over 0.001: no tooltip
	BeliefOf(first).addedThisPeriod.at(k_One) = 0.0005f;
	game_clock::SetTurn(40);
	tb::ProcessOncePerTurn();
	EXPECT_TRUE(_tips.empty());
	EXPECT_EQ(BeliefOf(first).addedThisPeriod.at(k_One), 0.0f);
	EXPECT_TRUE(_help.empty()); // the local player has no town
}

// 9. CheckLosingBelief: the "losing belief" help sprite
TEST_F(TownBeliefTest, LosingBelief)
{
	const auto town = MakeTown(1, PlayerNames::PLAYER_ONE);
	const auto other = MakeTown(2, PlayerNames::PLAYER_ONE);
	for (const auto t : {town, other})
	{
		BeliefOf(t).belief.at(k_One) = 0.54f;
		BeliefOf(t).belief.at(k_Neutral) = 0.5f; // 0.54 - 0.05 < 0.5
	}
	tb::CheckLosingBelief(PlayerNames::PLAYER_ONE);
	ASSERT_EQ(_help.size(), 1u); // the first town found only
	EXPECT_EQ(_help.at(0).first, tb::detail::HelpSprite::LosingBelief);
	EXPECT_EQ(_help.at(0).second, town);
	_help.clear();

	// from ProcessOncePerTurn, on a turn % 10 == 0
	game_clock::SetTurn(10);
	tb::ProcessOncePerTurn();
	EXPECT_EQ(_help.size(), 1u);
	_help.clear();

	// on land 1 nothing
	influence::detail::MapGlobals().landNumber = 1;
	tb::CheckLosingBelief(PlayerNames::PLAYER_ONE);
	EXPECT_TRUE(_help.empty());
	influence::detail::MapGlobals().landNumber = 0;

	// 0.56 - 0.05 > 0.5: nothing
	for (const auto t : {town, other})
	{
		BeliefOf(t).belief.at(k_One) = 0.56f;
	}
	tb::CheckLosingBelief(PlayerNames::PLAYER_ONE);
	EXPECT_TRUE(_help.empty());
}

// 10. The land script: SET_TOWN_BELIEF overwrites (and sets beliefInNeutralPlayer for the neutral player), SET_TOWN_BELIEF_CAP
// caps the later sets, SET_TOWN_BALANCE_BELIEF_SCALE, SET_LOST_TOWN_SCALE
TEST_F(TownBeliefTest, Scripts)
{
	using lhscriptx::FeatureScriptCommands;
	const auto town = MakeTown(5, PlayerNames::NEUTRAL);
	FeatureScriptCommands::SetTownBelief(5, "PLAYER_ONE", 0.3f);
	FeatureScriptCommands::SetTownBelief(5, "PLAYER_ONE", 0.8f);
	EXPECT_EQ(BeliefOf(town).belief.at(k_One), 0.8f);
	FeatureScriptCommands::SetTownBelief(5, "NEUTRAL", 0.7f);
	EXPECT_EQ(BeliefOf(town).beliefInNeutralPlayer, 0.7f);
	EXPECT_EQ(BeliefOf(town).belief.at(k_Neutral), 0.7f);
	FeatureScriptCommands::SetTownBelief(5, "PLAYER_ONE", 20.0f);
	EXPECT_EQ(BeliefOf(town).belief.at(k_One), 10.0f); // the cap
	FeatureScriptCommands::SetTownBeliefCap(5, "PLAYER_TWO", 2.0f);
	FeatureScriptCommands::SetTownBelief(5, "PLAYER_TWO", 5.0f);
	EXPECT_EQ(BeliefOf(town).belief.at(k_Two), 2.0f);
	// the player name ignores case, an unknown name is the neutral player
	FeatureScriptCommands::SetTownBelief(5, "player_two", 1.5f);
	EXPECT_EQ(BeliefOf(town).belief.at(k_Two), 1.5f);
	FeatureScriptCommands::SetTownBelief(5, "NOBODY", 0.6f);
	EXPECT_EQ(BeliefOf(town).beliefInNeutralPlayer, 0.6f);
	EXPECT_EQ(BeliefOf(town).belief.at(k_Neutral), 0.6f);
	FeatureScriptCommands::SetTownBelief(99, "PLAYER_ONE", 1.0f); // no such town: nothing
	FeatureScriptCommands::SetTownBalanceBeliefScale(5, 0.5f);
	EXPECT_EQ(BeliefOf(town).beliefScale, 0.5f);
	FeatureScriptCommands::SetLostTownScale(0.25f);
	EXPECT_EQ(tb::LostTownScale(), 0.25f);
	land_balance::Reset();
	EXPECT_EQ(tb::LostTownScale(), 1.0f);
}
