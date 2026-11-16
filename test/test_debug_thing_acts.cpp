/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cstdint>

#include <limits>
#include <optional>
#include <string>
#include <vector>

#include <glm/geometric.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <gtest/gtest.h>

#include "Common/DebugActs.h"
#include "Common/DebugThingActs.h"

using namespace openblack;
using namespace openblack::debug_thing_acts;
using player_commands::DebugParticle;
using player_commands::DebugPhysics;
using player_commands::DebugTempleVillager;
using player_commands::DebugVillagerPath;

namespace
{

/// A fake world: it keeps each call
class FakeThings final: public ThingTarget
{
public:
	bool Throw(const DebugPhysics& act, PlayerNames player) override
	{
		throws.push_back(act);
		thrower = player;
		return true;
	}
	uint32_t StartEffect(const DebugParticle& act) override
	{
		started.push_back(act);
		return 7;
	}
	void SetEffectsPaused(bool value) override { paused = value; }
	void CloseAllEffects() override { ++closedAll; }
	void DeleteAllEffects() override { ++deletedAll; }
	void CloseEffect(uint32_t effect) override { closed.push_back(effect); }
	void DeleteEffect(uint32_t effect) override { deleted.push_back(effect); }
	bool SetTownShare(entt::entity town, float value) override
	{
		shares.emplace_back(town, value);
		return town != entt::null;
	}
	bool AdjustWorshippers(entt::entity /*town*/, int32_t count) override
	{
		adjusts.push_back(count);
		return true;
	}
	std::optional<entt::entity> MakeVillager(const DebugTempleVillager& /*act*/, glm::vec2 place) override
	{
		places.push_back(place);
		const auto villager = static_cast<entt::entity>(100 + places.size());
		// The order of the calls: each new villager comes before any goes to worship
		calls.push_back("make");
		return villager;
	}
	bool SendToWorship(entt::entity villager) override
	{
		sent.push_back(villager);
		calls.push_back("send");
		return true;
	}
	bool TakeAway(entt::entity villager) override
	{
		taken.push_back(villager);
		return villager != entt::null;
	}
	bool MoveVillager(const DebugVillagerPath& act) override
	{
		moves.push_back(act);
		return true;
	}

	std::vector<DebugPhysics> throws;
	PlayerNames thrower {PlayerNames::PLAYER_ONE};
	std::vector<DebugParticle> started;
	std::optional<bool> paused;
	int closedAll {0};
	int deletedAll {0};
	std::vector<uint32_t> closed;
	std::vector<uint32_t> deleted;
	std::vector<std::pair<entt::entity, float>> shares;
	std::vector<int32_t> adjusts;
	std::vector<glm::vec2> places;
	std::vector<entt::entity> sent;
	std::vector<entt::entity> taken;
	std::vector<DebugVillagerPath> moves;
	std::vector<std::string> calls;
};

constexpr float k_NotANumber = std::numeric_limits<float>::quiet_NaN();

} // namespace

TEST(DebugThingActs, AThrowNeedsATypeOfAStaticThingAndAScaleAboveNone)
{
	EXPECT_FALSE(PhysicsRefusal({.type = 36, .scale = 1.0f}).has_value());
	EXPECT_TRUE(PhysicsRefusal({.type = static_cast<uint16_t>(MobileStaticInfo::_COUNT), .scale = 1.0f}).has_value());
	EXPECT_TRUE(PhysicsRefusal({.type = 36, .scale = 0.0f}).has_value());
	EXPECT_TRUE(PhysicsRefusal({.type = 36, .height = k_NotANumber, .scale = 1.0f}).has_value());

	FakeThings things;
	const DebugPhysics act {.type = 36, .height = 20.0f, .scale = 1.0f, .place = {1.0f, 2.0f}, .velocity = {0.0f, 5.0f, 0.0f}};
	EXPECT_TRUE(Apply(things, act, PlayerNames::PLAYER_TWO).done);
	ASSERT_EQ(things.throws.size(), 1u);
	EXPECT_EQ(things.throws.front(), act);
	EXPECT_EQ(things.thrower, PlayerNames::PLAYER_TWO);
	// A refused throw does nothing
	EXPECT_FALSE(Apply(things, DebugPhysics {.type = 36, .scale = -1.0f}, PlayerNames::PLAYER_ONE).done);
	EXPECT_EQ(things.throws.size(), 1u);
}

TEST(DebugThingActs, AnEffectTakesOnlyTheValuesOfTheParticlesTab)
{
	EXPECT_FALSE(ParticleRefusal({.act = DebugParticle::Act::Start, .file = "SF_Smoke"}).has_value());
	EXPECT_FALSE(ParticleRefusal({.act = DebugParticle::Act::Start, .type = k_ParticleTypes - 1}).has_value());
	EXPECT_TRUE(ParticleRefusal({.act = DebugParticle::Act::Start, .type = k_ParticleTypes}).has_value());
	EXPECT_TRUE(ParticleRefusal({.act = DebugParticle::Act::Start, .file = "a b"}).has_value());
	EXPECT_TRUE(ParticleRefusal({.act = DebugParticle::Act::Start, .file = std::string(65, 'a')}).has_value());
	EXPECT_TRUE(ParticleRefusal({.act = DebugParticle::Act::Start, .magnitude = k_MostMagnitude + 1.0f}).has_value());
	EXPECT_TRUE(ParticleRefusal({.act = DebugParticle::Act::Start, .player = k_EffectPlayers}).has_value());
	EXPECT_TRUE(ParticleRefusal({.act = DebugParticle::Act::Start, .drawPath = k_DrawPaths}).has_value());
	EXPECT_TRUE(ParticleRefusal({.act = DebugParticle::Act::Start, .targets = k_MostTargets + 1}).has_value());
	EXPECT_FALSE(ParticleRefusal({.act = DebugParticle::Act::Pause}).has_value());
}

TEST(DebugThingActs, OnlyAnEffectOfEachComputerClosesByACommand)
{
	EXPECT_TRUE(IsSharedEffect(1));
	EXPECT_TRUE(IsSharedEffect(k_FirstLocalEffect - 1));
	EXPECT_FALSE(IsSharedEffect(0));
	EXPECT_FALSE(IsSharedEffect(k_FirstLocalEffect));
	EXPECT_FALSE(IsSharedEffect(k_FirstSimulationEffect - 1));
	EXPECT_TRUE(IsSharedEffect(k_FirstSimulationEffect));

	FakeThings things;
	EXPECT_TRUE(Apply(things, DebugParticle {.act = DebugParticle::Act::Close, .effect = 3}).done);
	EXPECT_TRUE(Apply(things, DebugParticle {.act = DebugParticle::Act::Delete, .effect = k_FirstSimulationEffect}).done);
	EXPECT_FALSE(Apply(things, DebugParticle {.act = DebugParticle::Act::Close, .effect = k_FirstLocalEffect}).done);
	EXPECT_EQ(things.closed, std::vector<uint32_t> {3});
	EXPECT_EQ(things.deleted, std::vector<uint32_t> {k_FirstSimulationEffect});
}

TEST(DebugThingActs, TheParticleActsGoToTheEffects)
{
	FakeThings things;
	const DebugParticle start {.act = DebugParticle::Act::Start, .file = "SF_Smoke", .magnitude = 2.0f, .targets = 3};
	EXPECT_TRUE(Apply(things, start).done);
	ASSERT_EQ(things.started.size(), 1u);
	EXPECT_EQ(things.started.front(), start);
	Apply(things, DebugParticle {.act = DebugParticle::Act::Pause, .paused = true});
	EXPECT_EQ(things.paused, true);
	Apply(things, DebugParticle {.act = DebugParticle::Act::CloseAll});
	Apply(things, DebugParticle {.act = DebugParticle::Act::DeleteAll});
	EXPECT_EQ(things.closedAll, 1);
	EXPECT_EQ(things.deletedAll, 1);
}

TEST(DebugThingActs, TheTempleActsTakeOnlyTheValuesOfTheWindow)
{
	EXPECT_FALSE(TempleVillagerRefusal({.act = DebugTempleVillager::Act::Share, .value = 1.0f}).has_value());
	EXPECT_TRUE(TempleVillagerRefusal({.act = DebugTempleVillager::Act::Share, .value = 1.5f}).has_value());
	EXPECT_TRUE(TempleVillagerRefusal({.act = DebugTempleVillager::Act::Share, .value = -0.1f}).has_value());
	const DebugTempleVillager make {.act = DebugTempleVillager::Act::Make, .age = 30, .count = 4};
	EXPECT_FALSE(TempleVillagerRefusal(make).has_value());
	auto old = make;
	old.age = k_OldestAge + 1;
	EXPECT_TRUE(TempleVillagerRefusal(old).has_value());
	auto many = make;
	many.count = k_MostMade + 1;
	EXPECT_TRUE(TempleVillagerRefusal(many).has_value());
	auto tribe = make;
	tribe.tribe = static_cast<uint8_t>(Tribe::_COUNT);
	EXPECT_TRUE(TempleVillagerRefusal(tribe).has_value());
	EXPECT_TRUE(TempleVillagerRefusal({.act = DebugTempleVillager::Act::TakeAway}).has_value());
}

TEST(DebugThingActs, TheNewVillagersStandRoundTheHomeAndGoToWorshipAfterTheyAreAllMade)
{
	FakeThings things;
	const DebugTempleVillager make {
	    .act = DebugTempleVillager::Act::Make, .home = {100.0f, 5.0f, 200.0f}, .age = 30, .count = 4, .send = true};
	const auto result = Apply(things, make);
	EXPECT_TRUE(result.done);
	ASSERT_EQ(result.made.size(), 4u);
	ASSERT_EQ(things.places.size(), 4u);
	for (const auto& place : things.places)
	{
		EXPECT_NEAR(glm::distance(place, glm::vec2(100.0f, 200.0f)), k_MadeRing, 1e-4f);
	}
	// The first stands to the side of the home in x, as the window put it
	EXPECT_NEAR(things.places.front().x, 100.0f + k_MadeRing, 1e-4f);
	EXPECT_EQ(things.calls, (std::vector<std::string> {"make", "make", "make", "make", "send", "send", "send", "send"}));
	EXPECT_EQ(things.sent, result.made);

	FakeThings stay;
	auto noSend = make;
	noSend.send = false;
	EXPECT_EQ(Apply(stay, noSend).made.size(), 4u);
	EXPECT_TRUE(stay.sent.empty());
}

TEST(DebugThingActs, TheTakeAwayGoesInActsOfAtMostTheMostVillagers)
{
	std::vector<entt::entity> villagers;
	for (uint32_t i = 0; i < 130; ++i)
	{
		villagers.push_back(static_cast<entt::entity>(i));
	}
	const auto acts = TakeAwayActs(villagers);
	ASSERT_EQ(acts.size(), 3u);
	EXPECT_EQ(acts.at(0).villagers.size(), player_commands::k_MostTakenVillagers);
	EXPECT_EQ(acts.at(1).villagers.size(), player_commands::k_MostTakenVillagers);
	EXPECT_EQ(acts.at(2).villagers.size(), 2u);
	EXPECT_EQ(acts.at(2).villagers.back(), static_cast<entt::entity>(129));
	EXPECT_TRUE(TakeAwayActs({}).empty());

	FakeThings things;
	EXPECT_TRUE(Apply(things, acts.at(2)).done);
	EXPECT_EQ(things.taken, acts.at(2).villagers);
}

TEST(DebugThingActs, TheTownActsGoToTheTown)
{
	FakeThings things;
	const auto town = static_cast<entt::entity>(9);
	EXPECT_TRUE(Apply(things, DebugTempleVillager {.act = DebugTempleVillager::Act::Share, .town = town, .value = 0.25f}).done);
	Apply(things, DebugTempleVillager {.act = DebugTempleVillager::Act::SendOne, .town = town});
	Apply(things, DebugTempleVillager {.act = DebugTempleVillager::Act::CallOneBack, .town = town});
	ASSERT_EQ(things.shares.size(), 1u);
	EXPECT_EQ(things.shares.front().first, town);
	EXPECT_FLOAT_EQ(things.shares.front().second, 0.25f);
	EXPECT_EQ(things.adjusts, (std::vector<int32_t> {1, -1}));
}

TEST(DebugThingActs, APathActNeedsNumbers)
{
	EXPECT_FALSE(VillagerPathRefusal({.act = DebugVillagerPath::Act::Teleport, .position = {1.0f, 2.0f, 3.0f}}).has_value());
	EXPECT_TRUE(VillagerPathRefusal({.act = DebugVillagerPath::Act::Speed, .speed = k_NotANumber}).has_value());
	FakeThings things;
	const DebugVillagerPath move {.act = DebugVillagerPath::Act::MoveTo, .villager = static_cast<entt::entity>(3)};
	EXPECT_TRUE(Apply(things, move).done);
	EXPECT_EQ(things.moves, std::vector<DebugVillagerPath> {move});
}

TEST(DebugThingActs, TheServerFindsTheRefusalsOfTheseActs)
{
	// The game server asks the refusal of each command of the debug windows
	EXPECT_TRUE(debug_acts::RefusalOf(DebugPhysics {.type = 999, .scale = 1.0f}).has_value());
	EXPECT_TRUE(debug_acts::RefusalOf(DebugTempleVillager {.act = DebugTempleVillager::Act::Share, .value = 2.0f}).has_value());
	EXPECT_FALSE(debug_acts::RefusalOf(DebugVillagerPath {}).has_value());
	EXPECT_TRUE(player_commands::OnlyDebuggerSends(DebugParticle {}));
	EXPECT_TRUE(player_commands::OnlyDebuggerSends(DebugTempleVillager {}));
	EXPECT_FALSE(player_commands::OnlyEditorSends(DebugPhysics {}));
	EXPECT_FALSE(player_commands::ObserverSends(DebugVillagerPath {}));
}

TEST(DebugThingActs, EachActHasAName)
{
	EXPECT_EQ(NameOf(DebugParticle::Act::CloseAll), "close_all");
	EXPECT_EQ(NameOf(DebugTempleVillager::Act::SendToWorship), "send_to_worship");
	EXPECT_EQ(NameOf(DebugVillagerPath::Act::MoveTo), "move_to");
}
