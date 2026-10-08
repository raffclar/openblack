/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// Dead list, the town centre: its ToBeDeleted / DeleteDependants through abodes::OnToBeDeleted.
// DeleteDependants runs twice (its own and the abode part's), so the town's
// centre always ends at null; the totem goes with it. No game data: a hand-made info.dat, abodes without meshes.

#define LOCATOR_IMPLEMENTATIONS

#include <memory>

#include <entt/entity/entity.hpp>
#include <gtest/gtest.h>

#include "ECS/Abodes.h"
#include "ECS/Components/Abode.h"
#include "ECS/Components/TotemStatue.h"
#include "ECS/Components/Town.h"
#include "ECS/Effects/Reactions.h"
#include "ECS/Registry.h"
#include "ECS/ToBeDeleted.h"
#include "Enums.h"
#include "GameClock.h"
#include "InfoConstants.h"
#include "Locator.h"
#include "support/TestServices.h"
#include "support/WorldSystems.h"

using namespace openblack;
using namespace openblack::ecs::components;
namespace abodes = openblack::ecs::abodes;
namespace reactions = openblack::ecs::effects::reactions;

namespace
{
constexpr auto k_Centre = static_cast<AbodeInfo>(0);
constexpr uint32_t k_TownId = 1;

class DeadlistTownCentreTest: public ::testing::Test
{
protected:
	void SetUp() override
	{
		test::EmplaceMapAndVillagerDefaults();
		auto info = std::make_unique<InfoConstants>();
		auto& abode = info->abode.at(static_cast<size_t>(k_Centre));
		abode.abodeType = AbodeType::TownCentre;
		abode.abodeNumber = AbodeNumber::TownCentre;
		abode.tribeType = Tribe::CELTIC;
		abode.thresholdForStopBeingFunctional = 0.75f;
		Locator::infoConstants::reset(info.release());
		Locator::entitiesRegistry::emplace<ecs::Registry>();
		openblack::test::EmplaceWorldSystems();
		auto& registry = Reg();
		_town = registry.Create();
		auto& town = registry.Assign<Town>(_town);
		town.id = k_TownId;
		town.owner = PlayerNames::PLAYER_ONE;
		registry.Context().towns[k_TownId] = _town;
		game_clock::SetTurn(0);
		reactions::Clear();
	}

	void TearDown() override
	{
		reactions::Clear();
		game_clock::SetTurn(0);
		Locator::entitiesRegistry::reset();
		openblack::test::ResetWorldSystems();
		test::ResetMapAndVillagerDefaults();
		Locator::infoConstants::reset();
	}

	static ecs::Registry& Reg() { return Locator::entitiesRegistry::value(); }
	Town& TownData() { return Reg().Get<Town>(_town); }

	/// A built town centre of the town (life 1: functional), without a mesh nor a Transform
	entt::entity MakeCentre()
	{
		auto& registry = Reg();
		const auto e = registry.Create();
		registry.Assign<Abode>(e, AbodeNumber::TownCentre, k_TownId, 0u, 0u).info = k_Centre;
		return e;
	}

	entt::entity _town {entt::null};
};

TEST_F(DeadlistTownCentreTest, DeletingTheTownsCentreLeavesItNullEvenWithAnother)
{
	// 1st pass: the town's centre is this one -> the other functional centre; 2nd pass: not this one -> null
	const auto older = MakeCentre();
	const auto centre = MakeCentre();
	TownData().centre = centre;
	abodes::OnToBeDeleted(centre, false);
	EXPECT_TRUE(TownData().centre == entt::null);
	EXPECT_TRUE(Reg().Valid(older));
}

TEST_F(DeadlistTownCentreTest, DeletingAnotherCentreAlsoClearsIt)
{
	// the town's centre is not this one -> nothing is found -> the centre = null (literal)
	const auto centre = MakeCentre();
	const auto other = MakeCentre();
	TownData().centre = centre;
	abodes::OnToBeDeleted(other, false);
	EXPECT_TRUE(TownData().centre == entt::null);
}

TEST_F(DeadlistTownCentreTest, TheTotemGoesWithItsCentre)
{
	// the totem is deleted now; the icon on top is openblack's own entity, it goes too
	auto& registry = Reg();
	const auto centre = MakeCentre();
	const auto plinth = registry.Create();
	const auto top = registry.Create();
	registry.Assign<TotemStatue>(plinth, centre, top, 0.0f, 0.0f);
	const auto otherCentre = MakeCentre();
	const auto otherPlinth = registry.Create();
	registry.Assign<TotemStatue>(otherPlinth, otherCentre, entt::null, 0.0f, 0.0f);
	abodes::OnToBeDeleted(centre, true);
	EXPECT_FALSE(ecs::IsAvailable(plinth));
	EXPECT_FALSE(ecs::IsAvailable(top));
	EXPECT_TRUE(ecs::IsAvailable(otherPlinth));
}
} // namespace
