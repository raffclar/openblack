/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#define LOCATOR_IMPLEMENTATIONS

#include <algorithm>
#include <array>
#include <memory>
#include <numbers>
#include <optional>
#include <unordered_map>
#include <utility>
#include <vector>

#include <DanceFile.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <gtest/gtest.h>

#include "ECS/Components/Dance.h"
#include "ECS/Components/FishFarm.h"
#include "ECS/Components/Footpath.h"
#include "ECS/Components/Mesh.h"
#include "ECS/Components/Physics.h"
#include "ECS/Components/Player.h"
#include "ECS/Components/PlayerStatistics.h"
#include "ECS/Components/StoragePit.h"
#include "ECS/Components/Temple.h"
#include "ECS/Components/Town.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/WorshipChants.h"
#include "ECS/Components/WorshipSite.h"
#include "ECS/Dances.h"
#include "ECS/Registry.h"
#include "ECS/ScriptFind.h"
#include "ECS/Systems/Implementations/ObjectMeasures.h"
#include "ECS/Systems/Implementations/WorshipSiteSystem.h"
#include "ECS/WorshipSites.h"

using namespace openblack;
using namespace openblack::ecs;
using namespace openblack::ecs::components;
using namespace openblack::ecs::systems;
namespace ws = openblack::ecs::worship_site;

namespace
{

/// The gate point and altar point of the site's model, as the game's model has them
constexpr glm::vec3 k_GatePoint {18.179f, 0.0f, -39.397f};
constexpr glm::vec3 k_AltarPoint {11.910f, 0.0f, -25.560f};
/// The point of the temple's model that the places are judged by, as the game's models have it
constexpr glm::vec3 k_TemplePoint {-2.9353f, -0.2926f, -23.9176f};

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
		if (index == ws::k_GatePoint)
		{
			return k_GatePoint;
		}
		if (index == ws::k_AltarPoint)
		{
			return k_AltarPoint;
		}
		return std::nullopt;
	}
	[[nodiscard]] std::optional<glm::vec3> TemplePoint(entt::entity /*temple*/, uint32_t index) const override
	{
		return index == ws::k_TemplePlacePoint ? std::optional(k_TemplePoint) : std::nullopt;
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

	[[nodiscard]] magic::WorshipBatteryRules ChantRules(Tribe /*tribe*/, PlayerNames /*player*/) const override
	{
		return chantRules;
	}

	entt::entity MakeDance(DanceInfo dance, glm::vec3 position, entt::entity site) override
	{
		// Every dance file read here loops twice and the dances start by themselves
		auto file = std::make_shared<dance::DanceFile>();
		file->loops = 2;
		return ecs::dances::Create(
		    _registry,
		    {.type = dance, .autostart = true, .place = map_coords::FromMetres({position.x, position.z}), .owner = site},
		    std::move(file));
	}
	[[nodiscard]] uint32_t Turn() const override { return turn; }
	entt::entity MakeFoodPot(glm::vec3 position, float yAngle) override
	{
		const auto pot = _registry.Create();
		_registry.Assign<Transform>(pot, position, glm::mat3(1.0f), glm::vec3(1.0f));
		potAngles[pot] = yAngle;
		return pot;
	}

	void OpenBuildingSite(entt::entity site, entt::entity town, float desire) override
	{
		openedSites.push_back({site, town});
		siteDesires[site] = desire;
	}
	void CloseBuildingSite(entt::entity site) override { closedSites.push_back(site); }
	void CloseTownsBuildingSite(entt::entity site, entt::entity town) override { closedTownSites.push_back({site, town}); }
	void RemoveSpellIcons(entt::entity site) override
	{
		// Whether the site's altar still stood as its icons went
		const auto altar = _registry.Get<const WorshipSite>(site).altar;
		removedIcons.emplace_back(site, _registry.Valid(altar));
	}
	void RemoveDance(entt::entity dance) override
	{
		// The towns that still had a site as its dance went
		std::vector<entt::entity> withSites;
		_registry.Each<const components::Town>([&withSites](entt::entity town, const components::Town& component) {
			if (component.worshipSite != entt::null)
			{
				withSites.push_back(town);
			}
		});
		removedDances.emplace_back(dance, std::move(withSites));
		_registry.Destroy(dance);
	}
	void ArtefactsLeaveSite(entt::entity site) override { artefactsLeft.push_back(site); }
	entt::entity MakeNeedFlag(entt::entity site, uint8_t need) override
	{
		if (!makesFlags)
		{
			return entt::null;
		}
		const auto flag = _registry.Create();
		_registry.Assign<WorshipSiteFlag>(flag, WorshipSiteFlag {.site = site, .need = need});
		return flag;
	}
	uint32_t StartMotes() override { return ++motesStarted; }
	void AddMote(uint32_t effect, glm::vec3 from, glm::vec3 to, uint32_t rgb) override
	{
		motes.push_back({.effect = effect, .from = from, .to = to, .rgb = rgb});
	}
	bool StepMotes(uint32_t effect) override
	{
		motesStepped.push_back(effect);
		return motesRun;
	}
	void RemoveMotes(uint32_t effect) override { motesRemoved.push_back(effect); }
	void Remove(entt::entity object) override
	{
		removed.push_back(object);
		_registry.Destroy(object);
	}

	int32_t landNumber {2};
	uint32_t turn {0};
	/// It makes the need flags that the sites ask for
	bool makesFlags {false};
	/// The motes' effects: how many were started, each mote given, each step and whether the effect runs on after it,
	/// and the effects removed
	struct Mote
	{
		uint32_t effect;
		glm::vec3 from;
		glm::vec3 to;
		uint32_t rgb;
	};
	uint32_t motesStarted {0};
	std::vector<Mote> motes;
	std::vector<uint32_t> motesStepped;
	bool motesRun {true};
	std::vector<uint32_t> motesRemoved;
	/// The building sites opened for a site and a town asked to build it, in turn, and the sites closed
	std::vector<std::pair<entt::entity, entt::entity>> openedSites;
	std::unordered_map<entt::entity, float> siteDesires;
	std::vector<entt::entity> closedSites;
	std::vector<std::pair<entt::entity, entt::entity>> closedTownSites;
	/// The dances removed, each with the towns that still had a site as it went
	std::vector<std::pair<entt::entity, std::vector<entt::entity>>> removedDances;
	/// The sites whose icons went, and whether the site's altar still stood then
	std::vector<std::pair<entt::entity, bool>> removedIcons;
	std::vector<entt::entity> artefactsLeft;
	std::vector<entt::entity> removed;
	std::unordered_map<entt::entity, float> potAngles;
	magic::WorshipBatteryRules chantRules {
	    .chantsPerVillager = 2.0f, .chantsToFillBattery = 100.0f, .eachVillagerAddToFillBattery = 10.0f};
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

TEST(WorshipSitePlaces, ThePourIsJudgedFromTheSiteMiddleWellOutFromWhereItIsSetDown)
{
	// Unturned, the middle is the model's own middle point from where the site is set down
	const auto unturned = ws::CentreOnLand({100.0f, 5.0f, 200.0f}, glm::mat3(1.0f), 1.0f);
	EXPECT_NEAR(unturned.x, 112.55f, 1e-4f);
	EXPECT_NEAR(unturned.y, 173.9f, 1e-4f);
	// Turned as the site is drawn (a negative turn about the up axis by its facing), it matches the places' turning
	const float facing = 1.1f;
	const auto rotation = glm::mat3(glm::rotate(glm::mat4(1.0f), -facing, glm::vec3(0.0f, 1.0f, 0.0f)));
	const auto turned = ws::CentreOnLand({10.0f, 0.0f, 20.0f}, rotation, 1.0f);
	const auto expected = ws::TurnedPoint({10.0f, 20.0f}, facing, ws::k_CentrePoint);
	EXPECT_NEAR(turned.x, expected.x, 1e-4f);
	EXPECT_NEAR(turned.y, expected.y, 1e-4f);
	// A scaled site has its middle scaled out with it
	const auto doubled = ws::CentreOnLand({0.0f, 0.0f, 0.0f}, glm::mat3(1.0f), 2.0f);
	EXPECT_NEAR(doubled.x, 25.1f, 1e-4f);
	EXPECT_NEAR(doubled.y, -52.2f, 1e-4f);
}

TEST(WorshipSitePlaces, AVillagerAtTheGateTouchesTheSiteMeasuredFromItsMiddle)
{
	// The gate is about 14.4 metres from the middle, inside the reach of the site and the radius of the villager
	const auto centre = ws::CentreOnLand({0.0f, 0.0f, 0.0f}, glm::mat3(1.0f), 1.0f);
	EXPECT_LE(ws::EdgeGap(centre, {k_GatePoint.x, k_GatePoint.z}, 0.5f), 0.0f);
	// Far from the site, the gap is the distance from the middle less the reach and the radius
	EXPECT_NEAR(ws::EdgeGap(centre, centre + glm::vec2(30.0f, 0.0f), 1.0f), 15.0f, 0.05f);
}

TEST(WorshipSitePlaces, TheDistanceFromASiteIsFromItsMiddleNotFromWhereItStands)
{
	Registry registry;
	// A site stands at the temple, with its model far out from there
	const auto site = registry.Create();
	registry.Assign<Transform>(site, glm::vec3(1000.0f, 0.0f, 1000.0f), glm::mat3(1.0f), glm::vec3(1.0f));
	registry.Assign<WorshipSite>(site);
	const auto villager = registry.Create();
	registry.Assign<Transform>(villager, glm::vec3(1000.0f + k_GatePoint.x, 0.0f, 1000.0f + k_GatePoint.z), glm::mat3(1.0f),
	                           glm::vec3(1.0f));
	// A villager without a model has no radius: it stands less than half a metre from the reach of the site
	const auto centre = ws::CentreOnLand({1000.0f, 0.0f, 1000.0f}, glm::mat3(1.0f), 1.0f);
	const auto gap = object_measures::DistanceFromObject(registry, site, villager);
	EXPECT_EQ(gap, ws::EdgeGap(centre, {1000.0f + k_GatePoint.x, 1000.0f + k_GatePoint.z}, 0.0f));
	EXPECT_LT(gap, 0.5f);
	// A thing of another kind measures from where it stands, about 43 metres away
	const auto other = registry.Create();
	registry.Assign<Transform>(other, glm::vec3(1000.0f, 0.0f, 1000.0f), glm::mat3(1.0f), glm::vec3(1.0f));
	EXPECT_NEAR(object_measures::DistanceFromObject(registry, other, villager), glm::length(glm::vec2(18.179f, 39.397f)),
	            0.05f);
}

TEST(WorshipSitePlaces, AFishFarmMeasuresWithItsFixedRadiusWhateverItsModel)
{
	Registry registry;
	const auto farm = registry.Create();
	registry.Assign<Transform>(farm, glm::vec3(0.0f), glm::mat3(1.0f), glm::vec3(3.0f));
	registry.Assign<FishFarm>(farm);
	EXPECT_EQ(object_measures::TwoDRadius(registry, farm), 5.0f);
	const auto villager = registry.Create();
	registry.Assign<Transform>(villager, glm::vec3(20.0f, 0.0f, 0.0f), glm::mat3(1.0f), glm::vec3(1.0f));
	EXPECT_NEAR(object_measures::DistanceFromObject(registry, farm, villager), 15.0f, 0.05f);
}

TEST(WorshipSitePlaces, EachPlaceIsJudgedByTheTemplesPointTurnedToThePlace)
{
	// The share of a turn of the place is added to the temple's own turn, and the point is scaled with the temple
	const glm::vec2 temple {100.0f, 200.0f};
	for (uint32_t place = 0; place < ws::k_Places; ++place)
	{
		const auto judged = ws::PlaceJudgePoint(temple, 1.0f, 2.0f, place, k_TemplePoint);
		const auto expected = ws::TurnedPoint(temple, ws::PlaceFacing(1.0f, place), k_TemplePoint * 2.0f);
		EXPECT_NEAR(judged.x, expected.x, 1e-3f);
		EXPECT_NEAR(judged.y, expected.y, 1e-3f);
	}
	// A temple with no turn has its first place judged by the point itself
	const auto first = ws::PlaceJudgePoint(temple, 0.0f, 1.0f, 0, k_TemplePoint);
	EXPECT_EQ(first, temple + glm::vec2(k_TemplePoint.x, k_TemplePoint.z));
}

TEST(WorshipSitePlaces, TheFreePlaceNearestTheSpotIsChosen)
{
	std::array<bool, ws::k_Places> taken {};
	const glm::vec2 temple {0.0f, 0.0f};
	constexpr float k_Facing = 2.0f;
	// Far out past where place 2's point lies
	const auto atPlaceTwo = (ws::PlaceJudgePoint(temple, k_Facing, 1.0f, 2, k_TemplePoint) - temple) * 10.0f;
	EXPECT_EQ(ws::NearestFreePlace(taken, temple, k_Facing, 1.0f, k_TemplePoint, atPlaceTwo), 2u);
	taken.at(2) = true;
	const auto next = ws::NearestFreePlace(taken, temple, k_Facing, 1.0f, k_TemplePoint, atPlaceTwo);
	ASSERT_TRUE(next.has_value());
	EXPECT_TRUE(*next == 1u || *next == 3u);
	taken.fill(true);
	EXPECT_FALSE(ws::NearestFreePlace(taken, temple, k_Facing, 1.0f, k_TemplePoint, atPlaceTwo).has_value());
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

// The temples of all the players from one walk are the temples that TempleOf gives for each player: the first temple
// made for the player, none for a player without one or with a temple that has no worship yet
TEST(WorshipSiteSystem, TheTemplesOfThePlayersAreTheTempleOfEachPlayer)
{
	Fixture f;
	const auto first = f.Temple(PlayerNames::PLAYER_TWO);
	const auto second = f.Temple(PlayerNames::PLAYER_TWO);
	const auto other = f.Temple(PlayerNames::PLAYER_ONE);
	f.Temple(PlayerNames::PLAYER_THREE);
	// The temples come into the worship in an order that is not the order that they were made
	f.system->AddTemple(second, 0.0f, true);
	f.system->AddTemple(other, 0.0f, true);
	f.system->AddTemple(first, 0.0f, true);
	constexpr size_t k_Count = 8;
	const auto temples = f.system->TemplesOfPlayers(k_Count);
	ASSERT_EQ(temples.size(), k_Count);
	for (size_t player = 0; player < k_Count; ++player)
	{
		EXPECT_EQ(temples[player], f.system->TempleOf(static_cast<PlayerNames>(player)));
	}
	EXPECT_EQ(temples[static_cast<size_t>(PlayerNames::PLAYER_TWO)], first);
	EXPECT_EQ(temples[static_cast<size_t>(PlayerNames::PLAYER_ONE)], other);
	EXPECT_TRUE(temples[static_cast<size_t>(PlayerNames::PLAYER_THREE)] == entt::null);
	// A shorter list leaves out the later players
	EXPECT_EQ(f.system->TemplesOfPlayers(1).size(), 1u);
}

TEST(WorshipSiteSystem, ASiteTakesThePlaceNearestItsTribesNearestTown)
{
	Fixture f;
	// The nearest Norse town to the temple stands far out past where place 4 is judged, with the temple turned
	const auto placeFour = ws::PlaceJudgePoint({0.0f, 0.0f}, 1.0f, 1.0f, 4, k_TemplePoint) * 10.0f;
	f.Town(0, PlayerNames::PLAYER_ONE, Tribe::NORSE, {placeFour.x, 0.0f, placeFour.y});
	const auto temple = f.Temple(PlayerNames::PLAYER_ONE);
	f.system->AddTemple(temple, 1.0f, true);
	const auto site = f.registry.Get<const CitadelWorship>(temple).sites.at(4);
	ASSERT_TRUE(site != entt::null);
	EXPECT_FLOAT_EQ(f.registry.Get<const WorshipSite>(site).facing, ws::PlaceFacing(1.0f, 4));
}

TEST(WorshipSiteSystem, OnTheSecondLandTheNorseSiteTakesTheFirstPlaceAndTheIndianSiteTheNext)
{
	// The player's temple, its Norse town, and the Indian towns of the second land
	Fixture f;
	f.Town(0, PlayerNames::PLAYER_ONE, Tribe::NORSE, {3231.53f, 0.0f, 3396.45f});
	const auto won = f.Town(10, PlayerNames::PLAYER_ONE, Tribe::INDIAN, {2761.49f, 0.0f, 3102.94f});
	f.Town(12, PlayerNames::NEUTRAL, Tribe::INDIAN, {2862.58f, 0.0f, 3448.27f});
	const glm::vec2 templeAt {3361.26f, 3241.85f};
	const auto temple = f.Temple(PlayerNames::PLAYER_ONE, {templeAt.x, 0.0f, templeAt.y});
	f.system->AddTemple(temple, 4.254f, true);

	const auto& sites = f.registry.Get<const CitadelWorship>(temple).sites;
	ASSERT_TRUE(sites.at(0) != entt::null);
	ASSERT_TRUE(sites.at(1) != entt::null);
	EXPECT_EQ(f.registry.Get<const WorshipSite>(sites.at(0)).tribe, Tribe::NORSE);
	// The Indian site is judged from the Indian town nearest the temple, of any player. The first place is the nearest,
	// but the Norse site has it.
	const auto& indian = f.registry.Get<const WorshipSite>(sites.at(1));
	EXPECT_EQ(indian.tribe, Tribe::INDIAN);
	EXPECT_EQ(f.registry.Get<const components::Town>(won).worshipSite, sites.at(1));
	// Its altar stands where the scripts of the second land look for the site's altar
	const auto& altar = f.registry.Get<const Transform>(indian.altar).position;
	EXPECT_NEAR(altar.x, 3343.191f, 0.01f);
	EXPECT_NEAR(altar.z, 3220.2014f, 0.01f);
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

TEST(WorshipSiteSystem, TheSecondLandLessonFindsTheNorseSiteNearTheTempleAfterItAllowsTheTown)
{
	Fixture f;
	// The town and the temple of the player on the second land, as the land puts them
	const auto town = f.Town(0, PlayerNames::PLAYER_ONE, Tribe::NORSE, {-129.73f, 0.0f, 154.6f});
	const auto temple = f.Temple(PlayerNames::PLAYER_ONE);
	f.system->AddTemple(temple, 4.254f, false);

	// The lesson stops the temple from sites before its builders finish it
	f.system->SetCanHaveSites(temple, false);
	f.system->TempleBuilt(temple);
	ASSERT_TRUE(f.registry.Get<const components::Town>(town).worshipSite == entt::null);

	// The film allows the town and then the temple: the Norse site stands from then on
	f.system->SetCanHaveSites(town, true);
	const auto site = f.registry.Get<const components::Town>(town).worshipSite;
	ASSERT_TRUE(site != entt::null);
	f.system->SetCanHaveSites(temple, true);
	EXPECT_EQ(f.registry.Get<const WorshipSite>(site).buildRequests.size(), 1u);

	// The search for type 19 and subtype 7 finds it from its altar, within 50 metres of the temple
	const auto kind = script_find::KindOf(f.registry, site);
	ASSERT_TRUE(kind.has_value());
	EXPECT_TRUE(script_find::Matches(*kind, script::ObjectType::WorshipSite, 7));
	const auto measured = script_find::MeasuredFrom(f.registry, site);
	ASSERT_TRUE(measured == f.registry.Get<const WorshipSite>(site).altar);
	const auto altarAt = f.registry.Get<const Transform>(measured).position;
	EXPECT_LT(glm::length(glm::vec2(altarAt.x, altarAt.z)), 50.0f);
}

TEST(WorshipSiteSystem, TheDebugToolLetsATownHaveItsSiteGoingUpAsTheScriptsDo)
{
	Fixture f;
	f.world->landNumber = 1;
	const auto town = f.Town(0, PlayerNames::PLAYER_ONE, Tribe::NORSE, {0.0f, 0.0f, -50.0f});
	const auto temple = f.Temple(PlayerNames::PLAYER_ONE);
	f.system->AddTemple(temple, 0.0f, true);
	ASSERT_TRUE(f.registry.Get<const components::Town>(town).worshipSite == entt::null);

	// Kept to the land's rule, the first land gives no site
	EXPECT_TRUE(f.system->DebugAllowSite(town, false) == entt::null);
	// Let past it, the town's site goes up and the town is asked to build it
	const auto site = f.system->DebugAllowSite(town, true);
	ASSERT_TRUE(site != entt::null);
	EXPECT_EQ(site, f.registry.Get<const components::Town>(town).worshipSite);
	EXPECT_FALSE(f.system->IsBuilt(site));
	EXPECT_EQ(f.registry.Get<const WorshipSite>(site).buildRequests.size(), 1u);
	EXPECT_EQ(f.world->openedSites.back(), std::make_pair(site, town));

	// The land's rule holds again for everything else
	const auto other = f.Town(1, PlayerNames::PLAYER_ONE, Tribe::CELTIC, {50.0f, 0.0f, 0.0f});
	f.system->LandLaidOut();
	EXPECT_TRUE(f.registry.Get<const components::Town>(other).worshipSite == entt::null);
	// A town with no people has none even then
	const auto empty = f.Town(2, PlayerNames::PLAYER_ONE, Tribe::GREEK, {-50.0f, 0.0f, 0.0f}, 0);
	EXPECT_TRUE(f.system->DebugAllowSite(empty, true) == entt::null);
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

TEST(WorshipSiteSystem, AScriptSetsHowMuchOfASiteIsBuiltAndAtTheWholeItStandsBuilt)
{
	Fixture f;
	f.Town(0, PlayerNames::PLAYER_ONE, Tribe::NORSE, {0.0f, 0.0f, -50.0f});
	const auto temple = f.Temple(PlayerNames::PLAYER_ONE);
	f.system->AddTemple(temple, 0.0f, true);
	const auto site = *std::ranges::find_if(f.registry.Get<const CitadelWorship>(temple).sites,
	                                        [](auto entity) { return entity != entt::null; });
	f.system->SetBuilt(site, -0.5f);
	EXPECT_FLOAT_EQ(f.registry.Get<const BuildProgress>(site).built, 0.0f);
	f.system->SetBuilt(site, 0.7f);
	EXPECT_FLOAT_EQ(f.registry.Get<const BuildProgress>(site).built, 0.7f);
	EXPECT_FALSE(f.system->IsBuilt(site));
	EXPECT_TRUE(f.world->closedSites.empty());
	f.system->SetBuilt(site, 1.0f);
	EXPECT_TRUE(f.system->IsBuilt(site));
	EXPECT_FALSE(f.registry.AllOf<BuildProgress>(site));
	EXPECT_TRUE(f.registry.Get<const WorshipSite>(site).buildRequests.empty());
	ASSERT_EQ(f.world->closedSites.size(), 1u);
	EXPECT_EQ(f.world->closedSites.front(), site);
	// A built site set under the whole stands part built again
	f.system->SetBuilt(site, 0.25f);
	EXPECT_FALSE(f.system->IsBuilt(site));
	EXPECT_FLOAT_EQ(f.registry.Get<const BuildProgress>(site).built, 0.25f);
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

namespace
{
/// A player's standing temple with a built site for one Norse town; the site
std::pair<entt::entity, entt::entity> BuiltSite(Fixture& f, PlayerNames owner)
{
	f.Town(static_cast<uint32_t>(owner), owner, Tribe::NORSE, {0.0f, 0.0f, -50.0f});
	const auto temple = f.Temple(owner);
	f.system->AddTemple(temple, 0.0f, true);
	const auto site = *std::ranges::find_if(f.registry.Get<const CitadelWorship>(temple).sites,
	                                        [](auto entity) { return entity != entt::null; });
	f.system->BuildBy(site, 1.0f);
	return {temple, site};
}
} // namespace

TEST(WorshipSiteChants, ASitesDancersChantIntoItEachTurn)
{
	Fixture f;
	const auto [temple, site] = BuiltSite(f, PlayerNames::PLAYER_ONE);
	f.system->SetDancers(site, 4);
	EXPECT_EQ(f.system->Dancers(site), 4u);
	f.system->ProcessChants();
	const auto& chants = f.registry.Get<const WorshipChants>(site);
	// Nothing drawn and an empty battery: the dance goes at the boost of an empty battery, 0.5, and the four dancers
	// chant 4 x 2 x 0.5 into the battery; what can be drawn next turn is the battery and their full chanting
	EXPECT_FLOAT_EQ(chants.danceIntensity, 0.5f);
	EXPECT_FLOAT_EQ(chants.battery, 4.0f);
	EXPECT_FLOAT_EQ(chants.available, 12.0f);
	EXPECT_FLOAT_EQ(f.system->ChantsAvailable(site), 12.0f);
}

TEST(WorshipSiteChants, WhatIsDrawnCountsInThePlayersStatistics)
{
	Fixture f;
	const auto player = f.registry.Create();
	f.registry.Assign<Player>(player, Player {.name = PlayerNames::PLAYER_ONE});
	f.registry.Assign<PlayerStatistics>(player);
	const auto [temple, site] = BuiltSite(f, PlayerNames::PLAYER_ONE);
	f.system->SetDancers(site, 4);
	f.system->ProcessChants();
	EXPECT_FLOAT_EQ(f.system->UseChants(site, 5.0f), 5.0f);
	// Asked more than is left, the site gives what it has
	EXPECT_FLOAT_EQ(f.system->UseChants(site, 50.0f), 7.0f);
	EXPECT_FLOAT_EQ(f.system->ChantsAvailable(site), 0.0f);
	EXPECT_FLOAT_EQ(f.registry.Get<const PlayerStatistics>(player).chantsUsed, 12.0f);
	// A dropped seed's charge goes back into the battery
	f.system->ReturnChants(site, 3.0f);
	EXPECT_FLOAT_EQ(f.registry.Get<const WorshipChants>(site).battery, 7.0f);
}

// A script sets the battery of a site. What the site can give this turn stays the same until the turn of the site ends.
TEST(WorshipSiteChants, AScriptSetsTheBatteryButNotWhatTheTurnCanGive)
{
	Fixture f;
	const auto [temple, site] = BuiltSite(f, PlayerNames::PLAYER_ONE);
	f.system->SetDancers(site, 4);
	f.system->ProcessChants();
	const float available = f.system->ChantsAvailable(site);
	f.system->SetStoredChants(site, 123.5f);
	EXPECT_EQ(f.system->StoredChants(site), 123.5f);
	EXPECT_EQ(f.registry.Get<const WorshipChants>(site).battery, 123.5f);
	EXPECT_EQ(f.system->ChantsAvailable(site), available);
	// A thing that is not a site has no battery
	f.system->SetStoredChants(temple, 5.0f);
	EXPECT_EQ(f.system->StoredChants(temple), 0.0f);
	EXPECT_EQ(f.system->StoredChants(entt::null), 0.0f);
}

TEST(WorshipSiteChants, CheatsGiveWithoutDrawing)
{
	Fixture f;
	const auto [temple, site] = BuiltSite(f, PlayerNames::PLAYER_ONE);
	auto& chants = f.registry.Get<WorshipChants>(site);
	chants.infinite = true;
	chants.freeMaintenance = true;
	EXPECT_FLOAT_EQ(f.system->UseCreateChants(site, 40.0f), 40.0f);
	EXPECT_FLOAT_EQ(f.system->MaintainSpell(site, 30.0f), 30.0f);
	EXPECT_FLOAT_EQ(f.registry.Get<const WorshipChants>(site).used, 0.0f);
}

TEST(WorshipSiteChants, VirtualInfluenceTakesOnlyThisTurnsChantingSharedAmongHands)
{
	Fixture f;
	const auto [temple, site] = BuiltSite(f, PlayerNames::PLAYER_ONE);
	f.system->SetDancers(site, 4);
	f.system->ProcessChants();
	// 12 can be drawn, but only the dancers' 8 of chanting this turn is offered, split between two hands
	EXPECT_FLOAT_EQ(f.system->TakeChantsForVirtualInfluence(PlayerNames::PLAYER_ONE, 2), 4.0f);
	EXPECT_FLOAT_EQ(f.registry.Get<const WorshipChants>(site).used, 4.0f);
	EXPECT_FLOAT_EQ(f.system->TakeChantsForVirtualInfluence(PlayerNames::PLAYER_TWO, 1), 0.0f);
}

TEST(WorshipSiteSystem, ANewSiteHasItsPlacesDanceSetGoingAndAnEmptyFoodPot)
{
	Fixture f;
	const auto [temple, site] = BuiltSite(f, PlayerNames::PLAYER_ONE);
	const auto& component = f.registry.Get<const WorshipSite>(site);
	ASSERT_TRUE(component.dance != entt::null);
	const auto& dance = f.registry.Get<const Dance>(component.dance);
	EXPECT_EQ(static_cast<int>(dance.type), static_cast<int>(DanceInfo::CitadelDance_1) + component.place);
	EXPECT_TRUE(dance.owner == site);
	// Its loop is as long as its dance file says
	EXPECT_EQ(dance.loopLength, 2u);
	// Made at a quarter speed, then set going at half: danced at twice its keyed speed
	EXPECT_EQ(dance.state, Dance::State::Dancing);
	EXPECT_FLOAT_EQ(dance.speed, 0.5f);
	EXPECT_FLOAT_EQ(dance.dancingRate, 2.0f);
	// Centred on the altar's point, as the altar is
	const auto& altarAt = f.registry.Get<const Transform>(component.altar).position;
	const auto& danceAt = f.registry.Get<const Transform>(component.dance).position;
	EXPECT_NEAR(danceAt.x, altarAt.x, 1e-4f);
	EXPECT_NEAR(danceAt.z, altarAt.z, 1e-4f);
	// The food pot stands at its own spot of the model, turned a further 1.5 radians
	ASSERT_TRUE(component.foodPot != entt::null);
	const auto expected = ws::TurnedPoint({0.0f, 0.0f}, component.facing, ws::k_FoodPotPoint);
	const auto& potAt = f.registry.Get<const Transform>(component.foodPot).position;
	EXPECT_NEAR(potAt.x, expected.x, 1e-4f);
	EXPECT_NEAR(potAt.z, expected.y, 1e-4f);
	EXPECT_FLOAT_EQ(f.world->potAngles.at(component.foodPot), component.facing + 1.5f);
}

TEST(WorshipSiteSystem, TheDanceFollowsTheChantingAndItsClockRunsWhileDanced)
{
	Fixture f;
	const auto [temple, site] = BuiltSite(f, PlayerNames::PLAYER_ONE);
	const auto danceEntity = f.registry.Get<const WorshipSite>(site).dance;
	// With no dancers to chant, the game counts all their chanting as drawn: the dance is set going flat out
	f.system->ProcessChants();
	EXPECT_FLOAT_EQ(f.registry.Get<const Dance>(danceEntity).speed, 1.0f);
	// It goes on in the dances' turn, as every dance does
	ecs::dances::TurnContext context {.turn = 0, .available = [](entt::entity) { return true; }, .finished = {}};
	ecs::dances::ProcessTurn(f.registry, danceEntity, context);
	EXPECT_FLOAT_EQ(f.registry.Get<const Dance>(danceEntity).clock, 0.0f);
	f.system->SetDancers(site, 3);
	EXPECT_EQ(f.registry.Get<const Dance>(danceEntity).dancers, 3u);
	context.turn = 5;
	ecs::dances::ProcessTurn(f.registry, danceEntity, context);
	EXPECT_FLOAT_EQ(f.registry.Get<const Dance>(danceEntity).clock, 1.0f);
}

TEST(WorshipSiteSystem, ATownWithAStoragePitIsLinkedToItsSiteByAFootpath)
{
	Fixture f;
	const auto town = f.Town(0, PlayerNames::PLAYER_ONE, Tribe::NORSE, {0.0f, 0.0f, -50.0f});
	const auto pit = f.registry.Create();
	f.registry.Assign<StoragePit>(pit);
	f.registry.Get<components::Town>(town).abodes.push_back(pit);
	f.Town(1, PlayerNames::PLAYER_ONE, Tribe::CELTIC, {50.0f, 0.0f, 0.0f});
	const auto temple = f.Temple(PlayerNames::PLAYER_ONE);
	f.system->AddTemple(temple, 0.0f, true);
	const auto norse = f.registry.Get<const components::Town>(town).worshipSite;
	ASSERT_TRUE(norse != entt::null);
	ASSERT_TRUE(f.registry.AllOf<FootpathLink>(norse));
	// Its end is the site's gate
	const auto gate = ws::TurnedPoint({0.0f, 0.0f}, f.registry.Get<const WorshipSite>(norse).facing, k_GatePoint);
	EXPECT_NEAR(f.registry.Get<const FootpathLink>(norse).position.x, gate.x, 1e-4f);
	// The Celtic town has no pit, so its site has no link
	for (const auto site : f.registry.Get<const CitadelWorship>(temple).sites)
	{
		if (site != entt::null && site != norse)
		{
			EXPECT_FALSE(f.registry.AllOf<FootpathLink>(site));
		}
	}
}

TEST(WorshipSiteSystem, ATownAskedToBuildItsSiteHasItAsABuildingSiteUntilItIsBuilt)
{
	Fixture f;
	const auto town = f.Town(0, PlayerNames::PLAYER_ONE, Tribe::NORSE, {0.0f, 0.0f, -50.0f});
	const auto temple = f.Temple(PlayerNames::PLAYER_ONE);
	f.system->AddTemple(temple, 0.0f, false);
	f.system->TempleBuilt(temple);
	const auto site = f.registry.Get<const components::Town>(town).worshipSite;
	ASSERT_TRUE(site != entt::null);
	// Given the site, the town is asked to build it; the standing temple then asks again with its boost
	ASSERT_EQ(f.world->openedSites.size(), 2u);
	EXPECT_EQ(f.world->openedSites.front(), std::make_pair(site, town));
	EXPECT_EQ(f.world->openedSites.back(), std::make_pair(site, town));
	EXPECT_FLOAT_EQ(f.world->siteDesires.at(site), 0.0f);
	EXPECT_TRUE(f.world->closedSites.empty());

	// Built whole, its building site goes
	f.system->BuildBy(site, 1.0f);
	ASSERT_EQ(f.world->closedSites.size(), 1u);
	EXPECT_EQ(f.world->closedSites.front(), site);
	// Built, no town is asked again
	f.system->SetCanHaveSites(temple, true);
	EXPECT_EQ(f.world->openedSites.size(), 2u);
}

TEST(WorshipSiteSystem, ATownLeavingItsPlayerClosesOnlyItsOwnBuildingSiteAtTheSiteGoingUp)
{
	Fixture f;
	const auto first = f.Town(0, PlayerNames::PLAYER_ONE, Tribe::NORSE, {0.0f, 0.0f, -50.0f});
	const auto second = f.Town(1, PlayerNames::PLAYER_ONE, Tribe::NORSE, {0.0f, 0.0f, -80.0f});
	const auto temple = f.Temple(PlayerNames::PLAYER_ONE);
	f.system->AddTemple(temple, 0.0f, true);
	const auto site = f.registry.Get<const components::Town>(first).worshipSite;
	ASSERT_TRUE(site != entt::null);
	EXPECT_EQ(site, f.registry.Get<const components::Town>(second).worshipSite);
	// Both towns of the tribe are asked to build it, each with a building site of its own there
	const auto opened = [&f, site](entt::entity town) {
		return std::ranges::find(f.world->openedSites, std::make_pair(site, town)) != f.world->openedSites.end();
	};
	EXPECT_TRUE(opened(first));
	EXPECT_TRUE(opened(second));
	EXPECT_EQ(f.registry.Get<const WorshipSite>(site).buildRequests.size(), 2u);

	f.registry.Get<components::Town>(first).owner = PlayerNames::NEUTRAL;
	f.system->TownChangedHands(first);
	ASSERT_EQ(f.world->closedTownSites.size(), 1u);
	EXPECT_EQ(f.world->closedTownSites.front(), std::make_pair(site, first));
	EXPECT_TRUE(f.world->closedSites.empty());
	// The other town still builds it
	const auto& requests = f.registry.Get<const WorshipSite>(site).buildRequests;
	ASSERT_EQ(requests.size(), 1u);
	EXPECT_EQ(requests.front().town, second);

	f.registry.Get<components::Town>(second).owner = PlayerNames::NEUTRAL;
	f.system->TownChangedHands(second);
	ASSERT_EQ(f.world->closedTownSites.size(), 2u);
	EXPECT_EQ(f.world->closedTownSites.back(), std::make_pair(site, second));
}

TEST(WorshipSiteSystem, ATempleLosesItsSitesNewestFirstWithAllOfEachAndItsTownsKeepTheirShare)
{
	Fixture f;
	const auto norseTown = f.Town(0, PlayerNames::PLAYER_ONE, Tribe::NORSE, {0.0f, 0.0f, -100.0f});
	const auto temple = f.Temple(PlayerNames::PLAYER_ONE);
	f.system->AddTemple(temple, 0.0f, true);
	// A second site, made later for a town that joins the player
	const auto celticTown = f.Town(1, PlayerNames::PLAYER_ONE, Tribe::CELTIC, {100.0f, 0.0f, 0.0f}, 1);
	f.system->PersonJoinedTown(celticTown);
	const auto norse = f.registry.Get<const components::Town>(norseTown).worshipSite;
	const auto celtic = f.registry.Get<const components::Town>(celticTown).worshipSite;
	ASSERT_TRUE(norse != entt::null);
	ASSERT_TRUE(celtic != entt::null);
	f.registry.Get<components::Town>(norseTown).worshipShare = 0.4f;
	const auto norseSite = f.registry.Get<const WorshipSite>(norse);
	const auto celticSite = f.registry.Get<const WorshipSite>(celtic);
	EXPECT_EQ(norseSite.made, 0u);
	EXPECT_EQ(celticSite.made, 1u);

	f.system->RemoveSites(temple);

	// The newest first: its altar, its dance, its pot and the site itself, then the older one's
	EXPECT_EQ(f.world->removed, (std::vector<entt::entity> {celticSite.altar, celticSite.foodPot, celtic, norseSite.altar,
	                                                        norseSite.foodPot, norse}));
	// Each dance goes while the towns still have their sites, so its dancers are counted out of its worship
	ASSERT_EQ(f.world->removedDances.size(), 2u);
	EXPECT_EQ(f.world->removedDances[0].first, celticSite.dance);
	EXPECT_EQ(f.world->removedDances[0].second.size(), 2u);
	EXPECT_EQ(f.world->removedDances[1].first, norseSite.dance);
	EXPECT_EQ(f.world->removedDances[1].second, std::vector<entt::entity> {norseTown});
	EXPECT_EQ(f.world->artefactsLeft, (std::vector<entt::entity> {celtic, norse}));
	// Each site's icons go first of all, before its altar
	EXPECT_EQ(f.world->removedIcons, (std::vector<std::pair<entt::entity, bool>> {{celtic, true}, {norse, true}}));
	// Both stop being building sites, as neither was finished
	EXPECT_EQ(std::ranges::count(f.world->closedSites, celtic), 1);
	EXPECT_EQ(std::ranges::count(f.world->closedSites, norse), 1);
	// The towns are left without a site, keeping their share, and the temple's places are free
	EXPECT_TRUE(f.registry.Get<const components::Town>(norseTown).worshipSite == entt::null);
	EXPECT_TRUE(f.registry.Get<const components::Town>(celticTown).worshipSite == entt::null);
	EXPECT_FLOAT_EQ(f.registry.Get<const components::Town>(norseTown).worshipShare, 0.4f);
	const auto& worship = f.registry.Get<const CitadelWorship>(temple);
	EXPECT_TRUE(std::ranges::all_of(worship.sites, [](entt::entity site) { return site == entt::null; }));
	// A site made afterwards is numbered on from the last
	f.system->SetCanHaveSites(temple, true);
	const auto again = f.registry.Get<const components::Town>(norseTown).worshipSite;
	ASSERT_TRUE(again != entt::null);
	EXPECT_EQ(f.registry.Get<const WorshipSite>(again).made, 2u);
}

TEST(WorshipSiteSystem, ATempleWithoutSitesLosesNothing)
{
	Fixture f;
	const auto temple = f.Temple(PlayerNames::PLAYER_ONE);
	f.system->RemoveSites(temple);
	f.system->AddTemple(temple, 0.0f, true);
	f.system->RemoveSites(temple);
	EXPECT_TRUE(f.world->removed.empty());
	EXPECT_TRUE(f.world->removedDances.empty());
}

TEST(WorshipSiteNeeds, EachTurnWorksOutTheNeedsAndGivesTheSiteItsThreeFlags)
{
	Fixture f;
	f.world->makesFlags = true;
	const auto [temple, site] = BuiltSite(f, PlayerNames::PLAYER_ONE);
	// Three worship and one of them dances: two hide, a hundredth each
	for (int i = 0; i < 3; ++i)
	{
		f.system->AddWorshipper(site, f.registry.Create());
	}
	f.system->SetDancers(site, 1);
	f.system->UpdateTurn();
	const auto& component = f.registry.Get<const WorshipSite>(site);
	// No dancer wants food or rest, as none is a villager in the dance
	EXPECT_FLOAT_EQ(component.needs.at(0), 0.0f);
	EXPECT_FLOAT_EQ(component.needs.at(1), 0.0f);
	EXPECT_FLOAT_EQ(component.needs.at(2), 0.02f);
	for (uint8_t need = 0; need < 3; ++need)
	{
		const auto flag = component.needFlags.at(need);
		ASSERT_TRUE(f.registry.Valid(flag));
		EXPECT_TRUE(f.registry.Get<const WorshipSiteFlag>(flag).site == site);
		EXPECT_EQ(f.registry.Get<const WorshipSiteFlag>(flag).need, need);
	}
	// The next turn keeps the same flags
	const auto flags = component.needFlags;
	f.system->UpdateTurn();
	EXPECT_EQ(f.registry.Get<const WorshipSite>(site).needFlags, flags);
}

TEST(WorshipSiteNeeds, ASiteGoesWithItsFlagsAndItsMotes)
{
	Fixture f;
	f.world->makesFlags = true;
	const auto [temple, site] = BuiltSite(f, PlayerNames::PLAYER_ONE);
	f.system->UpdateTurn();
	f.system->AddMote(site, glm::vec3(0.0f), glm::vec3(10.0f, 0.0f, 0.0f));
	const auto flags = f.registry.Get<const WorshipSite>(site).needFlags;
	f.system->RemoveSites(temple);
	for (const auto flag : flags)
	{
		EXPECT_EQ(std::ranges::count(f.world->removed, flag), 1);
	}
	EXPECT_EQ(f.world->motesRemoved, std::vector<uint32_t> {1u});
}

TEST(WorshipSiteMotes, TheFirstMoteStartsTheEffectAndTheSiteTurnsEndIt)
{
	Fixture f;
	const auto [temple, site] = BuiltSite(f, PlayerNames::PLAYER_TWO);
	const glm::vec3 icon {1.0f, 2.0f, 3.0f};
	const glm::vec3 miracle {40.0f, 0.0f, 3.0f};
	f.system->AddMote(site, icon, miracle);
	f.system->AddMote(site, icon, miracle);
	// One effect for both, each mote in the colour of the site's player
	EXPECT_EQ(f.world->motesStarted, 1u);
	ASSERT_EQ(f.world->motes.size(), 2u);
	EXPECT_EQ(f.world->motes[0].effect, 1u);
	EXPECT_EQ(f.world->motes[1].effect, 1u);
	EXPECT_EQ(f.world->motes[0].from, icon);
	EXPECT_EQ(f.world->motes[0].to, miracle);
	EXPECT_EQ(f.world->motes[0].rgb, Player::k_Colours.at(1) & 0xFFFFFFu);
	// The site's turn moves them on, and once the effect has ended the next mote starts another
	f.system->UpdateTurn();
	EXPECT_EQ(f.world->motesStepped, std::vector<uint32_t> {1u});
	f.world->motesRun = false;
	f.system->UpdateTurn();
	EXPECT_EQ(f.registry.Get<const WorshipSite>(site).manaPath, 0u);
	f.system->UpdateTurn();
	EXPECT_EQ(f.world->motesStepped.size(), 2u);
	f.system->AddMote(site, icon, miracle);
	EXPECT_EQ(f.world->motesStarted, 2u);
	EXPECT_EQ(f.world->motes.back().effect, 2u);
}
