/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#define LOCATOR_IMPLEMENTATIONS

#include <cmath>

#include <memory>
#include <numbers>
#include <vector>

#include <glm/glm.hpp>
#include <gtest/gtest.h>

#include "ECS/Components/Abode.h"
#include "ECS/Components/Mesh.h"
#include "ECS/Components/Town.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/VillageTotem.h"
#include "ECS/Registry.h"
#include "ECS/Systems/Implementations/VillageTotemSystem.h"
#include "ECS/VillageTotem.h"

using namespace openblack;
using namespace openblack::ecs;
using namespace openblack::ecs::components;
using namespace openblack::ecs::systems;
namespace vt = openblack::ecs::village_totem;

TEST(VillageTotemEase, FromRestItTakesTheTimeOfTheWholeWayAndArrivesStill)
{
	vt::Ease ease;
	vt::SetShare(ease, 1.0f);
	EXPECT_FLOAT_EQ(ease.duration, 5200.0f);
	// Half way through the time it is past half way: it sets off fast and settles slowly (6u^2 - 8u^3 + 3u^4)
	vt::Step(ease, 2600.0f);
	EXPECT_NEAR(ease.share, 0.6875f, 1e-4f);
	EXPECT_GT(ease.speed, 0.0f);
	vt::Step(ease, 2600.0f);
	EXPECT_FLOAT_EQ(ease.share, 1.0f);
	EXPECT_FLOAT_EQ(ease.speed, 0.0f);
}

TEST(VillageTotemEase, ItSetsOffAsTheMatrixOfTheCurveSays)
{
	// Moving at some speed part of the way, it is sent somewhere new: the curve meets the target at rest with no
	// acceleration, as the game's 3 by 3 solve gives
	vt::Ease ease;
	vt::SetShare(ease, 1.0f);
	vt::Step(ease, 1000.0f);
	const float s0 = ease.share;
	const float v0 = ease.speed;
	vt::SetShare(ease, 0.25f);
	const float t = ease.duration;
	EXPECT_FLOAT_EQ(t, 0.75f * 5200.0f);
	// Rows as the game lays them out, the unknowns the snap, jerk and acceleration
	const glm::dmat3 m(glm::dvec3(std::pow(t, 4) / 24.0, std::pow(t, 3) / 6.0, t * t / 2.0),
	                   glm::dvec3(std::pow(t, 3) / 6.0, t * t / 2.0, t), glm::dvec3(t * t / 2.0, t, 1.0));
	const glm::dvec3 rhs(0.25 - s0 - v0 * t, -v0, 0.0);
	const auto solved = glm::inverse(m) * rhs;
	EXPECT_NEAR(ease.snap, solved.x, std::abs(solved.x) * 1e-3);
	EXPECT_NEAR(ease.jerk, solved.y, std::abs(solved.y) * 1e-3);
	EXPECT_NEAR(ease.acceleration, solved.z, std::abs(solved.z) * 1e-3 + 1e-12);
	vt::Step(ease, t);
	EXPECT_FLOAT_EQ(ease.share, 0.25f);
}

TEST(VillageTotemEase, ANewShareSoNearIsTakenAtOnceAndStillRingsTheBell)
{
	vt::Ease ease;
	vt::SetShare(ease, 0.0f);
	EXPECT_FLOAT_EQ(ease.duration, 0.0f);
	EXPECT_TRUE(vt::TakeArrival(ease));
	EXPECT_FALSE(vt::TakeArrival(ease));
}

TEST(VillageTotemEase, ItArrivesOnceWithinTheBellsReach)
{
	vt::Ease ease;
	vt::SetShare(ease, 1.0f);
	vt::Step(ease, 100.0f);
	EXPECT_FALSE(vt::TakeArrival(ease));
	vt::Step(ease, 5200.0f);
	EXPECT_TRUE(vt::TakeArrival(ease));
	EXPECT_FALSE(vt::TakeArrival(ease));
}

TEST(VillageTotemRules, TheHandSlidesThreeIconHeightsAScreenAndStopsAtTheLand)
{
	EXPECT_FLOAT_EQ(vt::HandSlide(100.0f, 600.0f, 4.0f, 10.0f), 2.0f);
	EXPECT_FLOAT_EQ(vt::HandSlide(-100.0f, 600.0f, 4.0f, 10.0f), -2.0f);
	EXPECT_FLOAT_EQ(vt::HandSlide(-100.0f, 600.0f, 4.0f, 0.5f), -0.5f);
	EXPECT_FLOAT_EQ(vt::SlideShare(0.5f, 2.0f), 0.7f);
	EXPECT_FLOAT_EQ(vt::SlideShare(0.95f, 2.0f), 1.0f);
	EXPECT_FLOAT_EQ(vt::SlideShare(0.05f, -2.0f), 0.0f);
}

TEST(VillageTotemRules, TheHandTipsOverNearTheLandAndClosesOnTheIcon)
{
	const float upright = 7.0f * std::numbers::pi_v<float> / 16.0f;
	EXPECT_FLOAT_EQ(vt::HandTiltAt(10.0f), upright);
	EXPECT_FLOAT_EQ(vt::HandTiltAt(2.5f), upright);
	EXPECT_FLOAT_EQ(vt::HandTiltAt(-0.7f), upright + std::atan(1.0f));
	EXPECT_FLOAT_EQ(vt::GripClosure(0.8f), 0.5f);
	EXPECT_FLOAT_EQ(vt::GripClosure(3.0f), 1.0f);
	EXPECT_FLOAT_EQ(vt::GripY(10.0f, 2.0f), 11.4f);
	EXPECT_FLOAT_EQ(vt::RiseOf(0.5f), 4.0f);
}

namespace
{
constexpr entt::id_type k_HandIcon = 1;
constexpr entt::id_type k_ApeIcon = 2;

class FakeWorld final: public vt::WorldInterface
{
public:
	Registry& Entities() override { return registry; }
	[[nodiscard]] entt::id_type IconMeshFor(PlayerNames player) const override
	{
		return player == PlayerNames::PLAYER_ONE ? k_ApeIcon : k_HandIcon;
	}
	[[nodiscard]] Size SizeOf(entt::entity /*unused*/) const override { return {.radius = 0.8f, .height = 2.0f}; }
	[[nodiscard]] float LandHeightAt(glm::vec2 /*unused*/) const override { return 0.0f; }
	[[nodiscard]] bool TempleBuilt(PlayerNames player) const override { return temple && player == PlayerNames::PLAYER_ONE; }
	[[nodiscard]] bool Built(entt::entity /*unused*/) const override { return true; }
	void SetMovingSound(entt::entity /*unused*/, bool on) override { moving.push_back(on); }
	void RingBell(glm::vec3 /*unused*/) override { ++bells; }

	Registry registry;
	bool temple {true};
	std::vector<bool> moving;
	int bells {0};
};

struct Village
{
	FakeWorld* world;
	std::unique_ptr<VillageTotemSystem> system;
	entt::entity town;
	entt::entity totem;
	entt::entity icon;
};

Village MakeVillage(bool site)
{
	auto world = std::make_unique<FakeWorld>();
	auto* fake = world.get();
	auto& registry = fake->registry;
	const auto town = registry.Create();
	registry.Assign<Town>(town, 7u, PlayerNames::PLAYER_ONE);
	registry.Context().towns[7] = town;
	if (site)
	{
		registry.Get<Town>(town).worshipSite = registry.Create();
	}
	const auto centre = registry.Create();
	registry.Assign<Abode>(centre, AbodeNumber {}, 7u, 0u, 0u);
	const auto totem = registry.Create();
	const auto icon = registry.Create();
	registry.Assign<Transform>(totem, glm::vec3(5.0f, 3.0f, 5.0f), glm::mat3(1.0f), glm::vec3(1.0f));
	registry.Assign<Transform>(icon, glm::vec3(5.0f, 3.0f + vt::k_IconAbovePlinth, 5.0f), glm::mat3(1.0f), glm::vec3(1.0f));
	registry.Assign<Mesh>(icon, k_HandIcon, static_cast<int8_t>(0), static_cast<int8_t>(0));
	registry.Assign<VillageTotem>(totem, VillageTotem {.townCentre = centre, .icon = icon, .restY = 3.0f});
	auto system = std::make_unique<VillageTotemSystem>(std::move(world));
	system->AddToPlayer(totem);
	return {fake, std::move(system), town, totem, icon};
}
} // namespace

TEST(VillageTotemSystem, ItWearsItsPlayersCreatureAndStartsWithNoOneAtWorship)
{
	auto village = MakeVillage(true);
	auto& registry = village.world->registry;
	EXPECT_EQ(registry.Get<Mesh>(village.icon).id, k_ApeIcon);
	EXPECT_FLOAT_EQ(registry.Get<Town>(village.town).worshipShare, 0.0f);
	village.system->Update(10.0f);
	// Set to none from none it arrives at once: the grinding stops and the bell rings
	EXPECT_EQ(village.world->moving, (std::vector {true, false}));
	EXPECT_EQ(village.world->bells, 1);
}

TEST(VillageTotemSystem, TheHandSlidesItAndLettingGoSetsTheTownsShare)
{
	auto village = MakeVillage(true);
	auto& registry = village.world->registry;
	auto& system = *village.system;
	ASSERT_EQ(system.TotemOf(village.icon), village.totem);
	ASSERT_TRUE(system.Grip(village.totem, PlayerNames::PLAYER_ONE));
	// A third of the screen up: two icon heights, a fifth of the town
	system.Slide(200.0f, 600.0f);
	EXPECT_FLOAT_EQ(registry.Get<VillageTotem>(village.totem).held, 0.2f);
	// Held, it stands where the hand holds it, the hand at its icon
	EXPECT_FLOAT_EQ(registry.Get<Transform>(village.totem).position.y, 3.0f + 0.2f * 8.0f);
	const auto hold = system.GetHandHold();
	ASSERT_TRUE(hold.has_value());
	EXPECT_FLOAT_EQ(hold->position.y, 3.0f + 0.2f * 8.0f + vt::k_IconAbovePlinth + 1.4f);
	system.LetGo();
	EXPECT_FLOAT_EQ(registry.Get<Town>(village.town).worshipShare, 0.2f);
	EXPECT_FALSE(system.GetGripped().has_value());
	// Let go, the town's own share eases up from none, over a fifth of the whole way's time: the hand's height was
	// only what the hand held, so the statue drops back and rises again
	const auto& totem = registry.Get<VillageTotem>(village.totem);
	EXPECT_FLOAT_EQ(totem.ease.duration, 0.2f * 5200.0f);
	system.Update(520.0f);
	EXPECT_NEAR(registry.Get<Transform>(village.totem).position.y, 3.0f + 0.2f * 0.6875f * 8.0f, 1e-3f);
	EXPECT_EQ(village.world->bells, 0);
	system.Update(520.0f);
	EXPECT_FLOAT_EQ(registry.Get<Transform>(village.totem).position.y, 3.0f + 0.2f * 8.0f);
	// Arrived, the bell rings once
	EXPECT_EQ(village.world->bells, 1);
}

TEST(VillageTotemSystem, OnlyTheTownsPlayerWithTheirTempleAndAWorshipSiteMayTakeIt)
{
	auto noSite = MakeVillage(false);
	EXPECT_FALSE(noSite.system->Grip(noSite.totem, PlayerNames::PLAYER_ONE));
	auto village = MakeVillage(true);
	EXPECT_FALSE(village.system->Grip(village.totem, PlayerNames::PLAYER_TWO));
	village.world->temple = false;
	EXPECT_FALSE(village.system->Grip(village.totem, PlayerNames::PLAYER_ONE));
	village.world->temple = true;
	EXPECT_TRUE(village.system->Grip(village.totem, PlayerNames::PLAYER_ONE));
}

TEST(VillageTotemSystem, ATownWithNowhereToWorshipKeepsNoOneAtWorship)
{
	auto village = MakeVillage(false);
	village.system->SetTownShare(village.town, 0.6f);
	EXPECT_FLOAT_EQ(village.world->registry.Get<Town>(village.town).worshipShare, 0.0f);
	EXPECT_FLOAT_EQ(village.world->registry.Get<VillageTotem>(village.totem).ease.target, 0.0f);
}

TEST(VillageTotemSystem, ItGoesWithItsTownCentre)
{
	auto village = MakeVillage(true);
	auto& registry = village.world->registry;
	registry.Destroy(registry.Get<VillageTotem>(village.totem).townCentre);
	village.system->Update(10.0f);
	EXPECT_FALSE(registry.Valid(village.totem));
	EXPECT_FALSE(registry.Valid(village.icon));
}
