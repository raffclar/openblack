/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cstdint>

#include <bit>
#include <memory>
#include <string>
#include <vector>

#include <DanceFile.h>
#include <gtest/gtest.h>

#include "3D/AllMeshes.h"
#include "ECS/Components/Dance.h"
#include "ECS/DanceMoves.h"
#include "ECS/DanceRules.h"
#include "ECS/DanceShapes.h"
#include "ECS/Dances.h"
#include "ECS/Registry.h"

using namespace openblack;
using namespace openblack::ecs;
using openblack::ecs::components::Dance;
using openblack::ecs::components::DanceGroup;
namespace rules = openblack::ecs::dance_rules;
namespace moves = openblack::ecs::dance_moves;
namespace shapes = openblack::ecs::dance_shapes;

namespace
{
void Put(std::vector<uint8_t>& bytes, uint32_t value)
{
	for (int i = 0; i < 4; ++i)
	{
		bytes.push_back(static_cast<uint8_t>(value >> (8 * i)));
	}
}

void PutAction(std::vector<uint8_t>& bytes, std::vector<uint32_t> groups, uint32_t type, uint32_t first, uint32_t second)
{
	Put(bytes, static_cast<uint32_t>(groups.size()));
	for (const auto group : groups)
	{
		Put(bytes, group);
	}
	Put(bytes, type);
	Put(bytes, first);
	Put(bytes, second);
	for (int i = 2; i < 14; ++i)
	{
		Put(bytes, 0);
	}
}

/// A dance of two groups named M and W, sharing their dancers half and half from its first key frame
std::vector<uint8_t> TwoGroupDance()
{
	std::vector<uint8_t> bytes;
	Put(bytes, 1); // version
	Put(bytes, 2); // key frames
	Put(bytes, std::bit_cast<uint32_t>(0.0f));
	Put(bytes, 1);
	Put(bytes, 2); // actions
	PutAction(bytes, {0}, 6, 0, 50);
	PutAction(bytes, {1}, 6, 0, 50);
	Put(bytes, std::bit_cast<uint32_t>(20.0f));
	Put(bytes, 0);
	Put(bytes, 1);
	PutAction(bytes, {1}, 16, 2, 0);
	Put(bytes, std::bit_cast<uint32_t>(0.0f)); // clock
	Put(bytes, 2);                             // groups
	Put(bytes, 14);
	Put(bytes, 0);
	Put(bytes, 1); // loops
	for (const char name : {'M', 'W'})
	{
		Put(bytes, 1);
		bytes.push_back(static_cast<uint8_t>(name));
	}
	return bytes;
}

entt::entity Entity(uint32_t value)
{
	return static_cast<entt::entity>(value);
}
} // namespace

TEST(DanceRules, TheRateGoesInStepsOfFourTenths)
{
	EXPECT_FLOAT_EQ(rules::RateForSpeed(0.5f), 2.0f);
	EXPECT_FLOAT_EQ(rules::RateForSpeed(0.25f), 0.8f);
	EXPECT_FLOAT_EQ(rules::RateForSpeed(0.99f), 3.6f);
	EXPECT_FLOAT_EQ(rules::RateForSpeed(1.0f), 4.0f);
}

TEST(DanceRules, AWorshipSitesDanceIsDancedWhileItsDancersChant)
{
	Dance dance;
	// As it is made: at a quarter speed, stopped
	rules::SetSpeed(dance, rules::k_MadeSpeed);
	EXPECT_EQ(dance.state, Dance::State::Stopped);
	EXPECT_FLOAT_EQ(dance.rate, 0.8f);
	EXPECT_FLOAT_EQ(dance.dancingRate, 1.0f);
	// Then set going at half speed: danced, starting over at the new rate
	dance.clock = 12.0f;
	rules::SetWorshipSpeed(dance, 0.5f);
	EXPECT_EQ(dance.state, Dance::State::Dancing);
	EXPECT_FLOAT_EQ(dance.dancingRate, 2.0f);
	EXPECT_FLOAT_EQ(dance.clock, 0.0f);
	// The same rate again goes on where it was
	dance.clock = 5.0f;
	rules::SetWorshipSpeed(dance, 0.55f);
	EXPECT_FLOAT_EQ(dance.clock, 5.0f);
	rules::SetWorshipSpeed(dance, 0.0f);
	EXPECT_EQ(dance.state, Dance::State::Stopped);
}

TEST(DanceRules, ADanceStartsOnceMoreThanHalfOfThoseOnTheirWayHaveComeOrAfterTheLongestWait)
{
	Dance dance;
	EXPECT_FALSE(rules::HasProperlyStarted(dance, 100));
	EXPECT_FALSE(dance.firstDancerTurn.has_value());
	dance.dancers = 2;
	dance.onTheirWay = 4;
	EXPECT_FALSE(rules::HasProperlyStarted(dance, 100));
	EXPECT_EQ(dance.firstDancerTurn, 100u);
	dance.dancers = 3;
	EXPECT_TRUE(rules::HasProperlyStarted(dance, 101));
	dance.dancers = 1;
	EXPECT_FALSE(rules::HasProperlyStarted(dance, 100 + 899));
	EXPECT_TRUE(rules::HasProperlyStarted(dance, 100 + 900));
}

TEST(DanceRules, AScriptsDanceStartsOnceItHasADancer)
{
	// None are ever on their way to a script's dance, so its first dancer will do
	Dance dance;
	EXPECT_FALSE(rules::HasProperlyStarted(dance, 100));
	dance.dancers = 1;
	EXPECT_TRUE(rules::HasProperlyStarted(dance, 120));
	EXPECT_EQ(dance.firstDancerTurn, 120u);
}

TEST(DanceRules, TheClockRunsWhileItIsDancedAndStartsOverAfterItsLoop)
{
	Dance dance {.autostart = true, .loopLength = 1};
	EXPECT_EQ(rules::LoopTurns(dance), 600u);
	dance.dancers = 1;
	rules::ProcessTurn(dance, 50);
	EXPECT_EQ(dance.state, Dance::State::Dancing);
	EXPECT_EQ(dance.startTurn, 50u);
	EXPECT_FLOAT_EQ(dance.clock, 1.0f);
	dance.clock = 598.0f;
	rules::ProcessTurn(dance, 51);
	EXPECT_FLOAT_EQ(dance.clock, 599.0f);
	rules::ProcessTurn(dance, 52);
	EXPECT_FLOAT_EQ(dance.clock, 0.0f);
	// No dancers, no clock
	dance.dancers = 0;
	rules::ProcessTurn(dance, 53);
	EXPECT_FLOAT_EQ(dance.clock, 0.0f);
	// One that doesn't start by itself waits to be started
	Dance waiting {.loopLength = 1, .dancers = 5};
	rules::ProcessTurn(waiting, 10);
	EXPECT_EQ(waiting.state, Dance::State::Stopped);
}

TEST(DanceRules, KeyFramesComeByTheHalfSecondAndTheClockLoops)
{
	EXPECT_TRUE(rules::KeyFrameDue(20.0f, 24.0f));
	EXPECT_FALSE(rules::KeyFrameDue(20.0f, 25.0f));
	EXPECT_FLOAT_EQ(rules::NextClock(5.0f, 1), 6.0f);
	EXPECT_FLOAT_EQ(rules::NextClock(599.0f, 1), 0.0f);
	EXPECT_FLOAT_EQ(rules::NextClock(599.0f, 2), 600.0f);
}

TEST(DanceRules, SharedGroupsTakeNewcomersInTurnByTheirShares)
{
	dance::DanceFile file;
	ASSERT_EQ(file.Open(TwoGroupDance()), dance::DanceResult::Success);
	Dance dance;
	dance.groups.all.resize(2);
	rules::ApplyKeyFramesUpTo(dance, file, 0.0f);
	// Half and half: one each in a round of two
	EXPECT_EQ(dance.groups.shared, (std::vector<std::size_t> {0, 1}));
	EXPECT_EQ(dance.groups.all[0].weight, 1u);
	EXPECT_EQ(dance.groups.roundLength, 2u);
	EXPECT_EQ(rules::AddDancer(dance, Entity(1), 0, DanceGroup::k_Men), 0u);
	EXPECT_EQ(rules::AddDancer(dance, Entity(2), 0, DanceGroup::k_Women), 1u);
	EXPECT_EQ(rules::AddDancer(dance, Entity(3), 0, DanceGroup::k_Men), 0u);
	EXPECT_EQ(dance.dancers, 3u);
	// A dancer of another kind finds no group
	EXPECT_FALSE(rules::AddDancer(dance, Entity(4), 1, DanceGroup::k_Men).has_value());
}

TEST(DanceRules, AGroupTakingOneSexPassesItsTurnOnToTheNext)
{
	Dance dance;
	auto& groups = dance.groups;
	groups.all.resize(2);
	groups.all[0].sexes = DanceGroup::k_Women;
	groups.shared = {0, 1};
	groups.all[0].quota = 50;
	groups.all[1].quota = 50;
	rules::SetWeights(groups);
	// A man on the women's turn goes to the second group, and the round moves on
	EXPECT_EQ(rules::AddDancer(dance, Entity(1), 0, DanceGroup::k_Men), 1u);
	EXPECT_EQ(groups.round, 1);
}

TEST(DanceRules, GroupsWithAFixedNumberAreFilledFirst)
{
	Dance dance;
	auto& groups = dance.groups;
	groups.all.resize(2);
	groups.all[0].limited = true;
	groups.all[0].quota = 1;
	groups.limited = {0};
	groups.all[1].quota = 100;
	groups.shared = {1};
	rules::SetWeights(groups);
	EXPECT_EQ(rules::AddDancer(dance, Entity(1), 0, DanceGroup::k_Men), 0u);
	EXPECT_EQ(rules::AddDancer(dance, Entity(2), 0, DanceGroup::k_Men), 1u);
	rules::RemoveDancer(dance, 0, Entity(1));
	EXPECT_EQ(groups.all[0].limitedDancers, 0u);
	EXPECT_EQ(dance.dancers, 1u);
	EXPECT_EQ(rules::AddDancer(dance, Entity(3), 0, DanceGroup::k_Men), 0u);
	EXPECT_EQ(rules::FirstDancer(groups, Entity(3)), Entity(2));
}

TEST(Dances, ADanceIsMadeStoppedAtAQuarterSpeedWithItsFilesGroups)
{
	Registry registry;
	auto file = std::make_shared<dance::DanceFile>();
	ASSERT_EQ(file->Open(TwoGroupDance()), dance::DanceResult::Success);
	const auto entity = dances::Create(registry, {.type = DanceInfo::NewDanceAroundPerson, .autostart = true}, file);
	const auto& dance = registry.Get<const Dance>(entity);
	EXPECT_EQ(dance.state, Dance::State::Stopped);
	EXPECT_FLOAT_EQ(dance.speed, 0.25f);
	EXPECT_FLOAT_EQ(dance.rate, 0.8f);
	EXPECT_EQ(dance.loopLength, 1u);
	ASSERT_EQ(dance.groups.all.size(), 2u);
	EXPECT_EQ(dance.groups.all[0].name, "M");
	EXPECT_EQ(dance.groups.all[1].name, "W");
}

TEST(Dances, ADanceStopsForATurnWhenItsTimeIsUp)
{
	Registry registry;
	auto file = std::make_shared<dance::DanceFile>();
	ASSERT_EQ(file->Open(TwoGroupDance()), dance::DanceResult::Success);
	const auto entity = dances::Create(
	    registry, {.type = DanceInfo::NewDanceAroundPerson, .autostart = true, .duration = 30, .madeByScript = true}, file);
	const auto dancer = registry.Create();
	ASSERT_TRUE(dances::AddDancer(registry, entity, dancer, DanceGroup::k_Men));
	EXPECT_EQ(dances::DanceOf(registry, dancer), entity);
	std::vector<entt::entity> finished;
	dances::TurnContext context {.turn = 1,
	                             .available = [](entt::entity) { return true; },
	                             .finished = [&finished](entt::entity e) { finished.push_back(e); }};
	dances::ProcessTurn(registry, entity, context);
	const auto& dance = registry.Get<Dance>(entity);
	EXPECT_EQ(dance.state, Dance::State::Dancing);
	EXPECT_FLOAT_EQ(dance.clock, 1.0f);
	// 31 turns on it stops, and starts again the turn after
	context.turn = 32;
	dances::ProcessTurn(registry, entity, context);
	EXPECT_EQ(registry.Get<Dance>(entity).state, Dance::State::Stopped);
	context.turn = 33;
	dances::ProcessTurn(registry, entity, context);
	EXPECT_EQ(registry.Get<Dance>(entity).state, Dance::State::Dancing);
	// Its dancers are told when it goes
	dances::Destroy(registry, entity, context.finished);
	EXPECT_EQ(finished, std::vector<entt::entity> {dancer});
	EXPECT_FALSE(registry.AllOf<components::Dancer>(dancer));
}

TEST(Dances, ADanceGoesWhenWhatItIsDancedForGoes)
{
	Registry registry;
	const auto owner = registry.Create();
	const auto entity = dances::Create(registry, {.type = DanceInfo::CitadelDance_1, .owner = owner}, nullptr);
	dances::TurnContext context {.turn = 1, .available = [](entt::entity) { return false; }, .finished = {}};
	dances::ProcessTurn(registry, entity, context);
	EXPECT_FALSE(registry.Valid(entity));
}

TEST(DanceShapes, TheShapesAreWorkedOutAsTheGameDoes)
{
	const auto circle = shapes::Build(0);
	EXPECT_EQ(circle[0], glm::ivec2(65535, 0));
	EXPECT_EQ(circle[1], glm::ivec2(65515, 1608));
	EXPECT_EQ(circle[64], glm::ivec2(0, 65535));
	EXPECT_EQ(circle[100], glm::ivec2(-50659, 41574));
	const auto wavy = shapes::Build(1);
	EXPECT_EQ(wavy[5], glm::ivec2(70501, 8695));
	EXPECT_EQ(wavy[37], glm::ivec2(36585, 46879));
	const auto spiral = shapes::Build(2);
	EXPECT_EQ(spiral[10], glm::ivec2(1787, 1619));
	EXPECT_EQ(spiral[255], glm::ivec2(61343, -4522));
	const auto octagon = shapes::Build(10);
	EXPECT_EQ(octagon[0], glm::ivec2(65535, 0));
	EXPECT_EQ(octagon[33], glm::ivec2(44891, 46939));
	EXPECT_EQ(octagon[100], glm::ivec2(-48739, 40547));
	EXPECT_EQ(octagon[255], glm::ivec2(64935, -1448));
	// The square's sides and the wave's first half stop after their first points
	const auto square = shapes::Build(4);
	EXPECT_EQ(square[0], glm::ivec2(-0xFFFF, -0xFFE0));
	EXPECT_EQ(square[1], glm::ivec2(0, 0));
	EXPECT_EQ(square[192], glm::ivec2(0xFFE0, -0xFFFF));
	const auto wave = shapes::Build(3);
	EXPECT_EQ(wave[0], glm::ivec2(0, -8160));
	EXPECT_EQ(wave[1], glm::ivec2(0, 0));
	EXPECT_EQ(wave[128].y, -8160);
	EXPECT_EQ(shapes::Build(5)[3], glm::ivec2(0, 3 * 512 - 65536));
	EXPECT_EQ(shapes::Build(6)[3], glm::ivec2(3 * 512 - 65536, 0));
	EXPECT_EQ(shapes::Build(11)[2], glm::ivec2(2 * 224 + 0x2000, 0));
	EXPECT_EQ(std::string(shapes::FileName(19)), "Arc");
}

TEST(DanceMoves, APointTurnsThroughTheGamesSineTable)
{
	// Unturned, it still goes through the table's 2048 steps
	const auto same = moves::RotatePointByAngle({3.0f, 0.0f}, 0.0f);
	EXPECT_FLOAT_EQ(same.x, 3.0f);
	EXPECT_FLOAT_EQ(same.y, 0.0f);
	const auto quarter = moves::RotatePointByAngle({0.0f, 2.0f}, 0.0f);
	EXPECT_NEAR(quarter.x, 0.0f, 1e-6f);
	EXPECT_FLOAT_EQ(quarter.y, 2.0f);
	const auto half = moves::RotatePointByAngle({1.0f, 0.0f}, 3.1415927f);
	EXPECT_FLOAT_EQ(half.x, -1.0f);
}

TEST(DanceMoves, AMoveSharesTheDancesRateOverTheBeatsLeft)
{
	components::DanceGroup group;
	group.radius = 10.0f;
	moves::StartMove(group, {.action = 11, .first = std::bit_cast<uint32_t>(20.0f), .second = 0, .end = 10}, 0, 0.8f);
	EXPECT_FLOAT_EQ(group.radiusRate, 0.8f);
	moves::ProcessGroup(group, 0);
	EXPECT_FLOAT_EQ(group.radius, 10.8f);
	EXPECT_TRUE(group.moved);
	// At its end beat the move stops
	moves::ProcessGroup(group, 10);
	EXPECT_EQ(group.move.action, 0u);
	moves::ProcessGroup(group, 11);
	EXPECT_FALSE(group.moved);
	EXPECT_FLOAT_EQ(group.radius, 11.6f);

	// A spin goes round 256 over the move, and starts round again
	components::DanceGroup spinning;
	moves::StartMove(spinning, {.action = 1, .first = 0, .second = 0, .end = 100}, 0, 1.0f);
	EXPECT_FLOAT_EQ(spinning.spinRate, 2.56f);
	spinning.spin = 255.0f;
	moves::ProcessGroup(spinning, 1);
	EXPECT_NEAR(spinning.spin, 1.56f, 1e-5f);
	// Backwards it comes round the other way
	spinning.move.first = 1;
	moves::ProcessGroup(spinning, 2);
	EXPECT_NEAR(spinning.spin, 255.0f, 1e-4f);
}

TEST(DanceMoves, ADancersPlaceIsRoundItsGroupsShape)
{
	Dance dance;
	dance.place = map_coords::FromMetres({1000.0f, 2000.0f});
	dance.groups.all.resize(1);
	auto& group = dance.groups.all[0];
	group.radius = 10.0f;
	group.dancers = {Entity(1), Entity(2)};
	// Half the radius out, the first along x and the second on the far side
	const auto first = map_coords::ToMetres(moves::SlotPosition(dance, 0, 0, moves::Shapes()));
	EXPECT_NEAR(first.x, 1005.0f, 0.01f);
	EXPECT_NEAR(first.y, 2000.0f, 0.01f);
	const auto second = map_coords::ToMetres(moves::SlotPosition(dance, 0, 1, moves::Shapes()));
	EXPECT_NEAR(second.x, 995.0f, 0.01f);
	EXPECT_NEAR(second.y, 2000.0f, 0.01f);
	// Moved off, the group's centre moves by whole metres
	group.offset = {2.7f, -1.2f};
	const auto centre = map_coords::ToMetres(moves::GroupCentre(dance, 0));
	EXPECT_NEAR(centre.x, 1002.0f, 0.01f);
	EXPECT_NEAR(centre.y, 1999.0f, 0.01f);
}

TEST(DanceMoves, DancersFaceAsTheirGroupsMoveHasThem)
{
	EXPECT_EQ(moves::Facing({.action = moves::Action::FaceCentre, .towardsCentre = 300}), 300);
	EXPECT_EQ(moves::Facing({.action = moves::Action::FaceAway, .towardsCentre = 300}), 1324);
	EXPECT_EQ(moves::Facing({.action = moves::Action::FaceAway, .towardsCentre = 1500}), 476);
	EXPECT_EQ(moves::Facing({.action = moves::Action::TurnOnTheSpot, .facing = 2000}), 80);
	EXPECT_FALSE(moves::Facing({.action = moves::Action::Turn, .distance = 1.0f}).has_value());
	EXPECT_FALSE(moves::Facing({.action = moves::Action::Clip}).has_value());
}

TEST(DanceMoves, ADancersClipFollowsItsGroupsMove)
{
	const auto never = [](uint32_t) -> uint32_t { return 0; };
	EXPECT_EQ(moves::DanceClip({}, never), static_cast<int32_t>(AnimId::PStand));
	EXPECT_EQ(moves::DanceClip({.inGroup = true, .stopped = true}, never), 0x171);
	// The clip table: a man's DanceA, a woman's Pray
	EXPECT_EQ(moves::DanceClip({.inGroup = true, .action = 10, .first = 0}, never), static_cast<int32_t>(AnimId::PMDanceA));
	EXPECT_EQ(moves::DanceClip({.inGroup = true, .action = 10, .first = 22, .female = true}, never),
	          static_cast<int32_t>(AnimId::PPray));
	// One of three dances, drawn
	EXPECT_EQ(moves::DanceClip({.inGroup = true, .action = 4, .female = true}, [](uint32_t) -> uint32_t { return 2; }),
	          static_cast<int32_t>(AnimId::PFDanceC));
	// Growing, they walk
	EXPECT_EQ(moves::DanceClip({.inGroup = true, .action = 11, .walkClip = 7}, never), 7);
	EXPECT_EQ(moves::DanceClip({.inGroup = true, .action = 1, .rate = 0.0f, .walkClip = 7}, never),
	          static_cast<int32_t>(AnimId::PStand));
}

TEST(Dances, AKeyFramesMoveSetsItsGroupGoingAndItsDancersClipsAgain)
{
	Registry registry;
	auto file = std::make_shared<dance::DanceFile>();
	ASSERT_EQ(file->Open(TwoGroupDance()), dance::DanceResult::Success);
	dance::DanceAction grow;
	grow.groups = {0};
	grow.type = 3;
	grow.arguments[4] = 11;
	grow.arguments[5] = std::bit_cast<uint32_t>(15.0f);
	grow.arguments[7] = 100;
	file->keyFrames.at(1).actions.push_back(grow);
	const auto entity = dances::Create(registry, {.type = DanceInfo::NewDanceAroundPerson, .autostart = true}, file);
	const auto dancer = registry.Create();
	ASSERT_TRUE(dances::AddDancer(registry, entity, dancer, DanceGroup::k_Men));
	auto& dance = registry.Get<Dance>(entity);
	dance.clock = 20.0f;
	std::vector<entt::entity> playedAgain;
	dances::TurnContext context {.turn = 1,
	                             .available = [](entt::entity) { return true; },
	                             .finished = {},
	                             .playClipAgain = [&playedAgain](entt::entity e) { playedAgain.push_back(e); }};
	dances::ProcessTurn(registry, entity, context);
	EXPECT_EQ(playedAgain, std::vector<entt::entity> {dancer});
	const auto& group = registry.Get<const Dance>(entity).groups.all[0];
	// At a quarter speed (rate 0.8) over the 80 beats left: (15 - 20) * 0.01 a turn
	EXPECT_FLOAT_EQ(group.radiusRate, -0.05f);
	EXPECT_FLOAT_EQ(group.radius, 19.95f);
}
