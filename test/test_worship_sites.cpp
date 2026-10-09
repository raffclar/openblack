/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#define LOCATOR_IMPLEMENTATIONS

#include <array>
#include <memory>
#include <numbers>
#include <optional>
#include <unordered_map>
#include <vector>

#include <glm/glm.hpp>
#include <gtest/gtest.h>

#include "ECS/Components/Mesh.h"
#include "ECS/Components/Physics.h"
#include "ECS/Components/Temple.h"
#include "ECS/Components/Town.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/WorshipSite.h"
#include "ECS/Registry.h"
#include "ECS/Systems/Implementations/WorshipSiteSystem.h"
#include "ECS/WorshipSites.h"

using namespace openblack;
using namespace openblack::ecs;
using namespace openblack::ecs::components;
using namespace openblack::ecs::systems;
namespace ws = openblack::ecs::worship_site;

namespace
{

/// The place point and altar point of the site's model, as the game's model has them
constexpr glm::vec3 k_PlacePoint {18.179f, 0.0f, -39.397f};
constexpr glm::vec3 k_AltarPoint {11.910f, 0.0f, -25.560f};

class FakeWorld final: public ws::WorldInterface
{
public:
	explicit FakeWorld(Registry& registry)
	    : _registry(registry)
	{
	}
	Registry& Entities() override { return _registry; }
	[[nodiscard]] int32_t LandNumber() const override { return landNumber; }
	[[nodiscard]] std::optional<glm::vec3> SitePoint(uint32_t index) const override
	{
		if (index == ws::k_PlacePoint)
		{
			return k_PlacePoint;
		}
		if (index == ws::k_AltarPoint)
		{
			return k_AltarPoint;
		}
		return std::nullopt;
	}
	[[nodiscard]] entt::id_type SiteMesh(entt::entity temple) override
	{
		const auto skinned = templeSkins.find(temple);
		return skinned != templeSkins.end() ? skinned->second : 1;
	}
	[[nodiscard]] entt::id_type AltarMesh(Tribe tribe) const override { return 100 + static_cast<entt::id_type>(tribe); }
	[[nodiscard]] float LandHeightAt(glm::vec2 /*point*/) const override { return 0.0f; }
	[[nodiscard]] uint32_t PopulationOf(entt::entity town) const override
	{
		const auto found = population.find(town);
		return found != population.end() ? found->second : 0;
	}

	int32_t landNumber {2};
	/// The site's model wearing a temple's skin, once the temple has one
	std::unordered_map<entt::entity, entt::id_type> templeSkins;
	std::unordered_map<entt::entity, uint32_t> population;

private:
	Registry& _registry;
};

struct Fixture
{
	Registry registry;
	FakeWorld* world {nullptr};
	std::unique_ptr<WorshipSiteSystem> system;

	Fixture()
	{
		auto fake = std::make_unique<FakeWorld>(registry);
		world = fake.get();
		system = std::make_unique<WorshipSiteSystem>(std::move(fake));
	}

	entt::entity Temple(PlayerNames owner, glm::vec3 position = glm::vec3(0.0f))
	{
		const auto entity = registry.Create();
		registry.Assign<Transform>(entity, position, glm::mat3(1.0f), glm::vec3(1.0f));
		registry.Assign<components::Temple>(entity, owner);
		return entity;
	}

	entt::entity Town(uint32_t id, PlayerNames owner, Tribe tribe, glm::vec3 position, uint32_t people = 10)
	{
		const auto entity = registry.Create();
		auto& town = registry.Assign<components::Town>(entity);
		town.id = id;
		town.owner = owner;
		town.gained = id;
		registry.Assign<Tribe>(entity, tribe);
		registry.Assign<Transform>(entity, position, glm::mat3(1.0f), glm::vec3(1.0f));
		world->population[entity] = people;
		return entity;
	}
};

} // namespace

TEST(WorshipSitePlaces, TheyGoRoundTheTempleASeventhOfATurnApart)
{
	EXPECT_FLOAT_EQ(ws::PlaceFacing(0.5f, 0), 0.5f);
	EXPECT_FLOAT_EQ(ws::PlaceFacing(0.5f, 3), 0.5f + 3.0f * 0.8975979f);
	EXPECT_NEAR(ws::k_PlaceSpacing * 7.0f, 2.0f * std::numbers::pi_v<float>, 1e-5f);
}

TEST(WorshipSitePlaces, APointIsTurnedAboutTheSpotAsTheGameTurnsModels)
{
	// A quarter turn takes a point ahead (+x) to the side (+z)
	const auto turned = ws::TurnedPoint({10.0f, 20.0f}, std::numbers::pi_v<float> / 2.0f, {1.0f, 0.0f, 0.0f});
	EXPECT_NEAR(turned.x, 10.0f, 1e-5f);
	EXPECT_NEAR(turned.y, 21.0f, 1e-5f);
}

TEST(WorshipSitePlaces, TheFreePlaceNearestTheSpotIsChosen)
{
	std::array<bool, ws::k_Places> taken {};
	const glm::vec2 temple {0.0f, 0.0f};
	// Right where place 2's point lies
	const auto atPlaceTwo = ws::TurnedPoint(temple, ws::PlaceFacing(0.0f, 2), k_PlacePoint);
	EXPECT_EQ(ws::NearestFreePlace(taken, temple, k_PlacePoint, atPlaceTwo), 2u);
	taken.at(2) = true;
	const auto next = ws::NearestFreePlace(taken, temple, k_PlacePoint, atPlaceTwo);
	ASSERT_TRUE(next.has_value());
	EXPECT_TRUE(*next == 1u || *next == 3u);
	taken.fill(true);
	EXPECT_FALSE(ws::NearestFreePlace(taken, temple, k_PlacePoint, atPlaceTwo).has_value());
}

TEST(WorshipSitePlaces, ATownMayHaveASiteOffTheFirstLandWithPeopleAndNoScriptStoppingIt)
{
	EXPECT_TRUE(ws::MayHaveSite(2, false, 1));
	EXPECT_FALSE(ws::MayHaveSite(1, false, 10));
	EXPECT_FALSE(ws::MayHaveSite(2, true, 10));
	EXPECT_FALSE(ws::MayHaveSite(2, false, 0));
}

TEST(WorshipSiteSystem, AStandingTempleGivesItsPlayersTownsTheirTribesSitesToBuild)
{
	Fixture f;
	const auto norse = f.Town(0, PlayerNames::PLAYER_ONE, Tribe::NORSE, {0.0f, 0.0f, -100.0f});
	const auto celtic = f.Town(1, PlayerNames::PLAYER_ONE, Tribe::CELTIC, {100.0f, 0.0f, 0.0f});
	const auto otherNorse = f.Town(2, PlayerNames::PLAYER_ONE, Tribe::NORSE, {0.0f, 0.0f, -200.0f});
	const auto neutral = f.Town(3, PlayerNames::NEUTRAL, Tribe::GREEK, {0.0f, 0.0f, 100.0f});
	const auto temple = f.Temple(PlayerNames::PLAYER_ONE);
	f.system->AddTemple(temple, 0.0f, true);

	const auto& worship = f.registry.Get<const CitadelWorship>(temple);
	const auto norseSite = f.registry.Get<const components::Town>(norse).worshipSite;
	ASSERT_TRUE(norseSite != entt::null);
	// One site for each tribe, shared by the tribe's towns
	EXPECT_EQ(f.registry.Get<const components::Town>(otherNorse).worshipSite, norseSite);
	const auto celticSite = f.registry.Get<const components::Town>(celtic).worshipSite;
	ASSERT_TRUE(celticSite != entt::null);
	EXPECT_NE(celticSite, norseSite);
	EXPECT_TRUE(f.registry.Get<const components::Town>(neutral).worshipSite == entt::null);
	EXPECT_EQ(std::ranges::count_if(worship.sites, [](auto site) { return site != entt::null; }), 2);

	const auto& site = f.registry.Get<const WorshipSite>(norseSite);
	EXPECT_EQ(site.tribe, Tribe::NORSE);
	EXPECT_EQ(site.player, PlayerNames::PLAYER_ONE);
	EXPECT_EQ(worship.sites.at(site.place), norseSite);
	// Not built, and both its towns are asked to build it
	EXPECT_FALSE(f.system->IsBuilt(norseSite));
	EXPECT_EQ(site.buildRequests.size(), 2u);
	// It stands at the temple, its altar at the altar point turned to its facing
	EXPECT_EQ(f.registry.Get<const Transform>(norseSite).position, glm::vec3(0.0f));
	const auto altarAt = ws::TurnedPoint({0.0f, 0.0f}, site.facing, k_AltarPoint);
	const auto& altar = f.registry.Get<const Transform>(site.altar).position;
	EXPECT_NEAR(altar.x, altarAt.x, 1e-4f);
	EXPECT_NEAR(altar.z, altarAt.y, 1e-4f);
	// Its altar isn't seen until the site is built
	EXPECT_FALSE(f.registry.AllOf<Mesh>(site.altar));
	EXPECT_EQ(f.registry.Get<const Mesh>(norseSite).id, 1u);
}

TEST(WorshipSiteSystem, ASiteTakesThePlaceNearestItsTribesNearestTown)
{
	Fixture f;
	// The nearest Norse town to the temple stands where place 4 points
	const auto placeFour = ws::TurnedPoint({0.0f, 0.0f}, ws::PlaceFacing(0.0f, 4), k_PlacePoint) * 3.0f;
	f.Town(0, PlayerNames::PLAYER_ONE, Tribe::NORSE, {placeFour.x, 0.0f, placeFour.y});
	const auto temple = f.Temple(PlayerNames::PLAYER_ONE);
	// Turned, the temple's places are still judged as if it faced the first way; the site faces round from the temple
	f.system->AddTemple(temple, 1.0f, true);
	const auto site = f.registry.Get<const CitadelWorship>(temple).sites.at(4);
	ASSERT_TRUE(site != entt::null);
	EXPECT_FLOAT_EQ(f.registry.Get<const WorshipSite>(site).facing, ws::PlaceFacing(1.0f, 4));
}

TEST(WorshipSiteSystem, NoSitesOnTheFirstLandNorForEmptyTownsNorForATempleStillToBeBuilt)
{
	{
		Fixture f;
		f.world->landNumber = 1;
		const auto town = f.Town(0, PlayerNames::PLAYER_ONE, Tribe::NORSE, {0.0f, 0.0f, -50.0f});
		f.system->AddTemple(f.Temple(PlayerNames::PLAYER_ONE), 0.0f, true);
		EXPECT_TRUE(f.registry.Get<const components::Town>(town).worshipSite == entt::null);
	}
	{
		Fixture f;
		const auto town = f.Town(0, PlayerNames::PLAYER_ONE, Tribe::NORSE, {0.0f, 0.0f, -50.0f}, 0);
		f.system->AddTemple(f.Temple(PlayerNames::PLAYER_ONE), 0.0f, true);
		EXPECT_TRUE(f.registry.Get<const components::Town>(town).worshipSite == entt::null);
		// Its first person brings it its site
		f.world->population[town] = 1;
		f.system->PersonJoinedTown(town);
		EXPECT_TRUE(f.registry.Get<const components::Town>(town).worshipSite != entt::null);
	}
	{
		Fixture f;
		const auto town = f.Town(0, PlayerNames::PLAYER_ONE, Tribe::NORSE, {0.0f, 0.0f, -50.0f});
		const auto temple = f.Temple(PlayerNames::PLAYER_ONE);
		f.system->AddTemple(temple, 0.0f, false);
		f.system->LandLaidOut();
		EXPECT_TRUE(f.registry.Get<const components::Town>(town).worshipSite == entt::null);
		f.system->TempleBuilt(temple);
		EXPECT_TRUE(f.registry.Get<const components::Town>(town).worshipSite != entt::null);
	}
}

TEST(WorshipSiteSystem, TheLandScriptsBuiltSiteIsBuiltOnlyWhenItsTownWasAskedToBuildIt)
{
	Fixture f;
	const auto town = f.Town(0, PlayerNames::PLAYER_TWO, Tribe::NORSE, {0.0f, 0.0f, -50.0f});
	const auto temple = f.Temple(PlayerNames::PLAYER_TWO);
	f.system->AddTemple(temple, 0.0f, true);
	const auto site = f.system->MakeBuiltSite(PlayerNames::PLAYER_TWO, Tribe::NORSE);
	ASSERT_TRUE(site != entt::null);
	EXPECT_EQ(site, f.registry.Get<const components::Town>(town).worshipSite);
	EXPECT_TRUE(f.system->IsBuilt(site));
	EXPECT_TRUE(f.registry.Get<const WorshipSite>(site).buildRequests.empty());
	// Built, it no longer goes up, and its tribe's altar is seen
	EXPECT_FALSE(f.registry.AllOf<BuildProgress>(site));
	EXPECT_EQ(f.registry.Get<const Mesh>(f.registry.Get<const WorshipSite>(site).altar).id,
	          100u + static_cast<uint32_t>(Tribe::NORSE));

	// A tribe the player has no town of: built, but not handed back
	EXPECT_TRUE(f.system->MakeBuiltSite(PlayerNames::PLAYER_TWO, Tribe::GREEK) == entt::null);
	const auto& sites = f.registry.Get<const CitadelWorship>(temple).sites;
	const auto greek = std::ranges::find_if(sites, [&f](auto entity) {
		return entity != entt::null && f.registry.Get<const WorshipSite>(entity).tribe == Tribe::GREEK;
	});
	ASSERT_NE(greek, sites.end());
	EXPECT_TRUE(f.system->IsBuilt(*greek));

	// Without a temple, nothing
	EXPECT_TRUE(f.system->MakeBuiltSite(PlayerNames::PLAYER_THREE, Tribe::NORSE) == entt::null);
}

TEST(WorshipSiteSystem, ScriptsStopAndAllowSitesForATempleOrATown)
{
	Fixture f;
	const auto town = f.Town(0, PlayerNames::PLAYER_ONE, Tribe::NORSE, {0.0f, 0.0f, -50.0f});
	const auto temple = f.Temple(PlayerNames::PLAYER_ONE);
	f.system->AddTemple(temple, 0.0f, false);
	f.system->SetCanHaveSites(temple, false);
	f.system->TempleBuilt(temple);
	EXPECT_TRUE(f.registry.Get<const components::Town>(town).worshipSite == entt::null);
	f.system->SetCanHaveSites(temple, true);
	const auto site = f.registry.Get<const components::Town>(town).worshipSite;
	ASSERT_TRUE(site != entt::null);
	EXPECT_EQ(f.registry.Get<const WorshipSite>(site).buildRequests.size(), 1u);

	const auto celtic = f.Town(1, PlayerNames::PLAYER_ONE, Tribe::CELTIC, {50.0f, 0.0f, 0.0f});
	f.system->SetCanHaveSites(celtic, false);
	f.system->LandLaidOut();
	EXPECT_TRUE(f.registry.Get<const components::Town>(celtic).worshipSite == entt::null);
	f.system->SetCanHaveSites(celtic, true);
	EXPECT_TRUE(f.registry.Get<const components::Town>(celtic).worshipSite != entt::null);
}

TEST(WorshipSiteSystem, BuildingUpASiteFinishesItAtTheWhole)
{
	Fixture f;
	f.Town(0, PlayerNames::PLAYER_ONE, Tribe::NORSE, {0.0f, 0.0f, -50.0f});
	const auto temple = f.Temple(PlayerNames::PLAYER_ONE);
	f.system->AddTemple(temple, 0.0f, true);
	const auto site = f.registry.Get<const CitadelWorship>(temple).sites.at(
	    f.registry
	        .Get<const WorshipSite>(*std::ranges::find_if(f.registry.Get<const CitadelWorship>(temple).sites,
	                                                      [](auto entity) { return entity != entt::null; }))
	        .place);
	f.system->BuildBy(site, -0.5f);
	EXPECT_FLOAT_EQ(f.registry.Get<const BuildProgress>(site).built, 0.0f);
	f.system->BuildBy(site, 0.6f);
	EXPECT_FALSE(f.system->IsBuilt(site));
	EXPECT_FALSE(f.registry.Get<const WorshipSite>(site).buildRequests.empty());
	f.system->BuildBy(site, 0.6f);
	EXPECT_TRUE(f.system->IsBuilt(site));
	EXPECT_TRUE(f.registry.Get<const WorshipSite>(site).buildRequests.empty());
}

TEST(WorshipSiteSystem, ASiteWearsItsTemplesSkinOnceTheTempleHasOne)
{
	Fixture f;
	f.Town(0, PlayerNames::PLAYER_ONE, Tribe::NORSE, {0.0f, 0.0f, -50.0f});
	const auto temple = f.Temple(PlayerNames::PLAYER_ONE);
	f.system->AddTemple(temple, 0.0f, true);
	const auto site = *std::ranges::find_if(f.registry.Get<const CitadelWorship>(temple).sites,
	                                        [](auto entity) { return entity != entt::null; });
	f.system->UpdateTurn();
	EXPECT_EQ(f.registry.Get<const Mesh>(site).id, 1u);
	f.world->templeSkins[temple] = 7;
	f.system->UpdateTurn();
	EXPECT_EQ(f.registry.Get<const Mesh>(site).id, 7u);
}
