/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cstddef>
#include <cstdint>

#include <memory>

#include <gtest/gtest.h>

#include "ECS/Components/Abode.h"
#include "ECS/Components/Physics.h"
#include "ECS/Components/Player.h"
#include "ECS/Components/Town.h"
#include "ECS/Components/Wonder.h"
#include "ECS/Registry.h"
#include "InfoConstants.h"
#include "Locator.h"

// Enable this define because we use a custom locator
#define LOCATOR_IMPLEMENTATIONS
#include "ECS/Systems/Implementations/WonderSystem.h"

using namespace openblack;
using namespace openblack::ecs::components;

namespace
{
constexpr uint32_t k_TownId = 3;
constexpr auto k_Celtic = static_cast<size_t>(Tribe::CELTIC);

class WonderSystemTest: public ::testing::Test
{
protected:
	void SetUp() override
	{
		Locator::entitiesRegistry::emplace<ecs::Registry>();
		_info = std::make_unique<InfoConstants>();
		auto& row = _info->abode.at(static_cast<size_t>(AbodeInfo::CelticWonder));
		row.tribeType = Tribe::CELTIC;
		row.abodeNumber = AbodeNumber::Wonder;
		Locator::infoConstants::emplace(*_info);
		auto& registry = Locator::entitiesRegistry::value();
		_one = MakePlayer(PlayerNames::PLAYER_ONE);
		_two = MakePlayer(PlayerNames::PLAYER_TWO);
		_town = registry.Create();
		registry.Assign<Town>(_town, Town {.id = k_TownId, .owner = PlayerNames::PLAYER_ONE});
		registry.Context().towns.emplace(k_TownId, _town);
	}
	void TearDown() override
	{
		Locator::infoConstants::reset();
		Locator::entitiesRegistry::reset();
	}

	static entt::entity MakePlayer(PlayerNames name)
	{
		auto& registry = Locator::entitiesRegistry::value();
		const auto entity = registry.Create();
		registry.Assign<Player>(entity, name);
		return entity;
	}

	/// A Celtic wonder of the town, finished or still under construction
	entt::entity MakeWonder(bool finished)
	{
		auto& registry = Locator::entitiesRegistry::value();
		const auto entity = registry.Create();
		registry.Assign<Abode>(entity, AbodeNumber::Wonder, k_TownId, 0U, 0U).info = AbodeInfo::CelticWonder;
		if (!finished)
		{
			registry.Assign<BuildProgress>(entity, 0.0f);
		}
		registry.Get<Town>(_town).abodes.push_back(entity);
		return entity;
	}

	[[nodiscard]] static float Most(entt::entity player)
	{
		return Locator::entitiesRegistry::value().Get<const Player>(player).miracles.maxTribalPower.at(k_Celtic);
	}
	[[nodiscard]] static float Power(entt::entity player)
	{
		return Locator::entitiesRegistry::value().Get<const Player>(player).miracles.tribalPower.at(k_Celtic);
	}

	std::unique_ptr<InfoConstants> _info;
	ecs::systems::WonderSystem _system;
	entt::entity _one {entt::null};
	entt::entity _two {entt::null};
	entt::entity _town {entt::null};
};
} // namespace

TEST_F(WonderSystemTest, AFinishedWonderAddsThePowerOfItsSizeWhenItIsMade)
{
	const auto wonder = MakeWonder(true);
	_system.Made(wonder, 1.5f);
	EXPECT_EQ(Locator::entitiesRegistry::value().Get<const Wonder>(wonder).power, 1.5f);
	EXPECT_EQ(Most(_one), 2.5f);
	// The power of the tribe follows at the next turn
	EXPECT_EQ(Power(_one), 1.0f);
	_system.ProcessTurn();
	EXPECT_EQ(Power(_one), 2.5f);
	EXPECT_EQ(Most(_two), 1.0f);
}

TEST_F(WonderSystemTest, AWonderUnderConstructionAddsItsPowerOnlyWhenItIsBuilt)
{
	const auto wonder = MakeWonder(false);
	_system.Made(wonder, 1.0f);
	_system.SetPower(wonder, 3.25f);
	EXPECT_EQ(Most(_one), 1.0f);
	Locator::entitiesRegistry::value().Remove<BuildProgress>(wonder);
	_system.Built(wonder);
	EXPECT_EQ(Most(_one), 4.25f);
}

TEST_F(WonderSystemTest, ALostWonderTakesItsPowerAway)
{
	const auto wonder = MakeWonder(true);
	_system.Made(wonder, 2.0f);
	_system.Lost(wonder);
	EXPECT_EQ(Most(_one), 1.0f);
	// An unfinished wonder that goes takes nothing away
	const auto site = MakeWonder(false);
	_system.Made(site, 2.0f);
	_system.Lost(site);
	EXPECT_EQ(Most(_one), 1.0f);
}

TEST_F(WonderSystemTest, ATownThatChangesHandsTakesItsWondersPowerToItsNewPlayer)
{
	const auto wonder = MakeWonder(true);
	_system.Made(wonder, 2.0f);
	_system.TownLeavesPlayer(_town);
	Locator::entitiesRegistry::value().Get<Town>(_town).owner = PlayerNames::PLAYER_TWO;
	_system.TownJoinsPlayer(_town);
	EXPECT_EQ(Most(_one), 1.0f);
	EXPECT_EQ(Most(_two), 3.0f);
}

TEST_F(WonderSystemTest, EachTurnTheTribalPowerIsTheMostPowerAndAtLeastOne)
{
	auto& miracles = Locator::entitiesRegistry::value().Get<Player>(_one).miracles;
	miracles.maxTribalPower.at(k_Celtic) = 0.5f;
	miracles.tribalPower.at(k_Celtic) = 7.0f;
	_system.ProcessTurn();
	EXPECT_EQ(Most(_one), 1.0f);
	EXPECT_EQ(Power(_one), 1.0f);
}

TEST_F(WonderSystemTest, SetPowerChangesOnlyAWonder)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto other = registry.Create();
	_system.SetPower(other, 2.0f);
	EXPECT_FALSE(registry.AllOf<Wonder>(other));
}
