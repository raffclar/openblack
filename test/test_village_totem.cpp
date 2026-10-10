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
#include "ECS/Components/SeeThrough.h"
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
constexpr entt::id_type k_Plinth = 3;

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
	void ReactToHandUsingTotem(entt::entity totem, PlayerNames player, glm::vec3 position) override
	{
		reactions.push_back({totem, player, position});
	}
	void EmpathiseWithPlayer(PlayerNames player, glm::vec3 position) override { empathies.push_back({player, position}); }
	void FloatNumber(glm::vec3 position, float value, uint32_t colour) override
	{
		numbers.push_back({position, value, colour});
	}

	struct Reacted
	{
		entt::entity totem;
		PlayerNames player;
		glm::vec3 position;
	};
	struct Empathy
	{
		PlayerNames player;
		glm::vec3 position;
	};
	struct Number
	{
		glm::vec3 position;
		float value;
		uint32_t colour;
	};
	Registry registry;
	std::vector<Reacted> reactions;
	std::vector<Empathy> empathies;
	std::vector<Number> numbers;
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
	registry.Assign<Mesh>(totem, k_Plinth, static_cast<int8_t>(0), static_cast<int8_t>(0));
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

TEST(VillageTotemRules, TheHandClosesOnTheIconAtMostAQuarterOfTheWayIntoItsSideHold)
{
	// The hold's length is halved, whole milliseconds, then halved again by how far the hand closes
	EXPECT_EQ(vt::GripTimeMs(1.0f, 1001), 250u);
	EXPECT_EQ(vt::GripTimeMs(0.5f, 1000), 125u);
	// Cut, not rounded
	EXPECT_EQ(vt::GripTimeMs(0.3f, 1000), 75u);
	EXPECT_EQ(vt::GripTimeMs(0.0f, 1000), 0u);
}

TEST(VillageTotemRules, TheSecondTotemShowsTheOtherShareWhileTheyStandApart)
{
	// Easing, it stands where it has got to; the second stands where it is going
	auto shown = vt::ShownShares(0.25f, 0.5f, false);
	EXPECT_FLOAT_EQ(shown.solid, 0.25f);
	ASSERT_TRUE(shown.ghost.has_value());
	EXPECT_FLOAT_EQ(*shown.ghost, 0.5f);
	// Held, it stands where the hand holds it, the second at the town's share
	shown = vt::ShownShares(0.25f, 0.5f, true);
	EXPECT_FLOAT_EQ(shown.solid, 0.5f);
	EXPECT_FLOAT_EQ(*shown.ghost, 0.25f);
	// Together, there is no second
	EXPECT_FALSE(vt::ShownShares(0.4f, 0.4f, true).ghost.has_value());
	EXPECT_FLOAT_EQ(vt::AsPercentage(0.4f), 40.0f);
}

TEST(VillageTotemSystem, HeldApartFromItsShareASeeThroughSecondTotemShowsTheTownsShare)
{
	auto village = MakeVillage(true);
	auto& registry = village.world->registry;
	auto& system = *village.system;
	system.Update(10.0f);
	EXPECT_TRUE(registry.Get<VillageTotem>(village.totem).ghost == entt::null);
	ASSERT_TRUE(system.Grip(village.totem, PlayerNames::PLAYER_ONE));
	system.Slide(300.0f, 600.0f);
	system.Update(10.0f);
	const auto& totem = registry.Get<VillageTotem>(village.totem);
	ASSERT_TRUE(registry.Valid(totem.ghost));
	ASSERT_TRUE(registry.Valid(totem.ghostIcon));
	// Half see-through, at the town's share of none, with the plinth's and the icon's models
	EXPECT_EQ(registry.Get<SeeThrough>(totem.ghost).alpha, 0x80);
	EXPECT_EQ(registry.Get<SeeThrough>(totem.ghostIcon).alpha, 0x80);
	EXPECT_FLOAT_EQ(registry.Get<Transform>(totem.ghost).position.y, 3.0f);
	EXPECT_FLOAT_EQ(registry.Get<Transform>(totem.ghostIcon).position.y, 3.0f + vt::k_IconAbovePlinth);
	EXPECT_EQ(registry.Get<Mesh>(totem.ghost).id, k_Plinth);
	EXPECT_EQ(registry.Get<Mesh>(totem.ghostIcon).id, k_ApeIcon);
	// Let go, it eases to its share with the second where it is going; once there the second goes
	system.LetGo();
	system.Update(10.0f);
	ASSERT_TRUE(registry.Valid(totem.ghost));
	EXPECT_FLOAT_EQ(registry.Get<Transform>(totem.ghost).position.y, 3.0f + 0.3f * 8.0f);
	system.Update(5200.0f);
	EXPECT_TRUE(totem.ghost == entt::null);
	EXPECT_TRUE(totem.ghostIcon == entt::null);
}

TEST(VillageTotemSystem, AsItMovesItsPlayerSeesTheShareItIsHeldAtButNotJustAfterItsIconIsPutOn)
{
	auto village = MakeVillage(true);
	auto& system = *village.system;
	// Drawn afresh with its icon, it shows nothing
	system.Update(10.0f);
	EXPECT_FALSE(system.TakeShareToolTip(PlayerNames::PLAYER_ONE).has_value());
	// Standing still it shows nothing either
	system.Update(10.0f);
	EXPECT_FALSE(system.TakeShareToolTip(PlayerNames::PLAYER_ONE).has_value());
	ASSERT_TRUE(system.Grip(village.totem, PlayerNames::PLAYER_ONE));
	system.Slide(150.0f, 600.0f);
	system.Update(10.0f);
	const auto tip = system.TakeShareToolTip(PlayerNames::PLAYER_ONE);
	ASSERT_TRUE(tip.has_value());
	EXPECT_EQ(tip->totem, village.totem);
	EXPECT_NEAR(tip->percent, 15.0f, 1e-4f);
	// Taken, it is gone until it moves again; other players see none
	EXPECT_FALSE(system.TakeShareToolTip(PlayerNames::PLAYER_ONE).has_value());
	system.Update(10.0f);
	EXPECT_TRUE(system.TakeShareToolTip(PlayerNames::PLAYER_ONE).has_value());
	EXPECT_FALSE(system.TakeShareToolTip(PlayerNames::PLAYER_TWO).has_value());
}

TEST(VillageTotemSystem, TakingHoldTheLivingReactAndLettingGoRaisedTheCreatureFeelsAndTheShareFloatsUp)
{
	auto village = MakeVillage(true);
	auto& system = *village.system;
	ASSERT_TRUE(system.Grip(village.totem, PlayerNames::PLAYER_ONE));
	ASSERT_EQ(village.world->reactions.size(), 1u);
	EXPECT_EQ(village.world->reactions[0].totem, village.totem);
	EXPECT_EQ(village.world->reactions[0].player, PlayerNames::PLAYER_ONE);
	// At where the totem stands, not where it is risen to
	EXPECT_EQ(village.world->reactions[0].position, glm::vec3(5.0f, 3.0f, 5.0f));
	system.Slide(300.0f, 600.0f);
	system.LetGo();
	ASSERT_EQ(village.world->empathies.size(), 1u);
	EXPECT_EQ(village.world->empathies[0].player, PlayerNames::PLAYER_ONE);
	ASSERT_EQ(village.world->numbers.size(), 1u);
	EXPECT_NEAR(village.world->numbers[0].value, 30.0f, 1e-4f);
	EXPECT_EQ(village.world->numbers[0].colour, 0xFF808080u);
	EXPECT_EQ(village.world->numbers[0].position, glm::vec3(5.0f, 3.0f, 5.0f));
	// Let go at none, no one feels anything, but the number still floats up
	ASSERT_TRUE(system.Grip(village.totem, PlayerNames::PLAYER_ONE));
	system.Slide(-600.0f, 600.0f);
	system.LetGo();
	EXPECT_EQ(village.world->empathies.size(), 1u);
	ASSERT_EQ(village.world->numbers.size(), 2u);
	EXPECT_FLOAT_EQ(village.world->numbers[1].value, 0.0f);
}

TEST(VillageTotemSystem, ItsSecondTotemGoesWithIt)
{
	auto village = MakeVillage(true);
	auto& registry = village.world->registry;
	ASSERT_TRUE(village.system->Grip(village.totem, PlayerNames::PLAYER_ONE));
	village.system->Slide(300.0f, 600.0f);
	village.system->Update(10.0f);
	const auto ghost = registry.Get<VillageTotem>(village.totem).ghost;
	ASSERT_TRUE(registry.Valid(ghost));
	registry.Destroy(registry.Get<VillageTotem>(village.totem).townCentre);
	village.system->Update(10.0f);
	EXPECT_FALSE(registry.Valid(ghost));
}
