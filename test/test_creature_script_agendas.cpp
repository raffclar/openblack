/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <optional>

#include <gtest/gtest.h>

#include "Creature/CreaturePlanActions.h"
#include "Creature/CreatureScriptAgendas.h"

using namespace openblack;
using namespace openblack::creature_mind;

namespace
{
uint32_t NoRandom(uint32_t /*range*/)
{
	return 0;
}

Senses Free()
{
	return {.seconds = 0.1f};
}
} // namespace

// "force CreatureCow CREATURE_LOOK_FOREVER CowLookPos": it turns to the thing, holding still two seconds, then watches it
// for a day
TEST(CreatureScriptAgendas, LookingForeverTurnsToTheThingAndWatchesItForADay)
{
	const auto agenda = LookForever(7);
	ASSERT_EQ(agenda.size(), 2);
	EXPECT_EQ(agenda[0].movement.kind, Movement::Kind::TurnToFaceObject);
	EXPECT_EQ(agenda[0].movement.object, 7u);
	EXPECT_FLOAT_EQ(agenda[0].seconds, 2.0f);
	EXPECT_EQ(agenda[1].kind, Step::Kind::Wait);
	EXPECT_FLOAT_EQ(agenda[1].seconds, 86400.0f);
	ASSERT_TRUE(agenda[1].gaze.has_value());
	EXPECT_EQ(agenda[1].gaze->object, 7u);
	EXPECT_FALSE(agenda[1].gaze->bottom);
}

// Looking without going up to it, it watches the thing's foot for 1.1 seconds and up to 1.2 more
TEST(CreatureScriptAgendas, LookingWithoutApproachingWatchesTheFootAWhile)
{
	const auto shortest = LookButDontApproach(3, 0.0f);
	const auto longest = LookButDontApproach(3, 1.0f);
	ASSERT_EQ(shortest.size(), 2);
	EXPECT_EQ(shortest[0].movement.kind, Movement::Kind::TurnToFaceObject);
	EXPECT_FLOAT_EQ(shortest[1].seconds, 1.1f);
	EXPECT_FLOAT_EQ(longest[1].seconds, 2.3f);
	ASSERT_TRUE(shortest[1].gaze.has_value());
	EXPECT_TRUE(shortest[1].gaze->bottom);
}

// The glade's creatures have no player: looking at the camera is over at once; a player's creature watches it five
// seconds
TEST(CreatureScriptAgendas, OnlyACreatureWithAPlayerLooksAtTheCamera)
{
	const auto none = LookAtCamera(false);
	ASSERT_EQ(none.size(), 1);
	EXPECT_FLOAT_EQ(none[0].seconds, 0.0f);
	EXPECT_FALSE(none[0].gaze.has_value());
	const auto owned = LookAtCamera(true);
	ASSERT_EQ(owned.size(), 1);
	EXPECT_FLOAT_EQ(owned[0].seconds, 5.0f);
	ASSERT_TRUE(owned[0].gaze.has_value());
	EXPECT_TRUE(owned[0].gaze->camera);
	EXPECT_EQ(owned[0].face, creature_face::Cue::Curiosity);
	EXPECT_EQ(owned[0].kind, Step::Kind::WatchCamera);
}

namespace
{
Senses Watching(float distance, float turn)
{
	return {.seconds = 0.1f,
	        .choosesNext = false,
	        .camera =
	            Senses::CameraView {.distance = distance, .groundDistance = distance, .point = {5.0f, 6.0f}, .turn = turn}};
}
} // namespace

// "force MyCreature CREATURE_LOOK_AT_CAMERA MyCreature": it watches the camera until whole seconds of turns pass five,
// the turn it starts on counting as the first: sixty turns at ten a second
TEST(CreatureScriptAgendas, WatchingTheCameraLastsSixtyTurns)
{
	IdleMind mind;
	Plan(mind, Activity::Told, LookAtCamera(true));
	int turns = 0;
	while (mind.step < mind.agenda.size() && turns < 100)
	{
		static_cast<void>(Think(mind, Watching(20.0f, 0.0f), NoRandom));
		++turns;
	}
	EXPECT_EQ(turns, 60);
}

// More than a quarter turn away from the camera it turns to face it, every turn it is still that far round
TEST(CreatureScriptAgendas, WatchingTheCameraTurnsToItPastAQuarterTurn)
{
	IdleMind mind;
	Plan(mind, Activity::Told, LookAtCamera(true));
	EXPECT_FALSE(Think(mind, Watching(20.0f, 1.5f), NoRandom).move.has_value());
	const auto behind = Think(mind, Watching(20.0f, -2.0f), NoRandom);
	ASSERT_TRUE(behind.move.has_value());
	EXPECT_EQ(behind.move->kind, Movement::Kind::TurnToFace);
	EXPECT_EQ(behind.move->point, glm::vec2(5.0f, 6.0f));
	EXPECT_TRUE(Think(mind, Watching(20.0f, 3.0f), NoRandom).move.has_value());
}

// The camera more than 100 away or right overhead ends the look, as does the camera going away (no player)
TEST(CreatureScriptAgendas, WatchingTheCameraEndsWhenItIsOutOfReach)
{
	for (const auto& senses : {Watching(100.5f, 0.0f), Watching(0.05f, 0.0f), Senses {.seconds = 0.1f, .choosesNext = false}})
	{
		IdleMind mind;
		Plan(mind, Activity::Told, LookAtCamera(true));
		static_cast<void>(Think(mind, senses, NoRandom));
		EXPECT_EQ(mind.step, 1u);
	}
	IdleMind mind;
	Plan(mind, Activity::Told, LookAtCamera(true));
	static_cast<void>(Think(mind, Watching(100.0f, 0.0f), NoRandom));
	EXPECT_EQ(mind.step, 0u);
}

// Pointing at a thing lasts five seconds and follows it; at the camera, a second at the camera's height, turning first
// only with a player
TEST(CreatureScriptAgendas, PointingAtAThingOrTheCamera)
{
	const auto thing = PointAtThing(9);
	ASSERT_EQ(thing.size(), 1);
	EXPECT_EQ(thing[0].order.kind, ObjectOrder::Kind::PointAt);
	EXPECT_EQ(thing[0].order.object, 9u);
	EXPECT_EQ(thing[0].order.seconds, 5.0f);
	EXPECT_EQ(thing[0].face, creature_face::Cue::Amazed);

	const glm::vec3 camera {10.0f, 40.0f, 20.0f};
	const auto unowned = PointAtCamera(false, camera);
	ASSERT_EQ(unowned.size(), 1);
	EXPECT_EQ(unowned[0].order.kind, ObjectOrder::Kind::PointAt);
	EXPECT_EQ(unowned[0].order.point, glm::vec2(10.0f, 20.0f));
	EXPECT_EQ(unowned[0].order.pointHeight, 40.0f);
	EXPECT_EQ(unowned[0].order.seconds, 1.0f);
	const auto owned = PointAtCamera(true, camera);
	ASSERT_EQ(owned.size(), 2);
	EXPECT_EQ(owned[0].movement.kind, Movement::Kind::TurnToFace);
}

// "force CreatureTiger CREATURE_SIT TigerBasePos": walks there, faces the place, then down the slope, and sits
TEST(CreatureScriptAgendas, SittingWhereAScriptSays)
{
	const auto agenda = SitDownAt(4, {100.0f, 200.0f}, 3.0f, NoRandom);
	ASSERT_EQ(agenda.size(), 4);
	EXPECT_EQ(agenda[0].movement.kind, Movement::Kind::ToPoint);
	EXPECT_EQ(agenda[0].movement.point, glm::vec2(100.0f, 200.0f));
	EXPECT_FLOAT_EQ(agenda[0].movement.maxDistance, 3.0f);
	EXPECT_EQ(agenda[1].movement.kind, Movement::Kind::TurnToFaceObject);
	EXPECT_FLOAT_EQ(agenda[1].seconds, 0.1f);
	EXPECT_EQ(agenda[2].movement.kind, Movement::Kind::FaceDownSlope);
	EXPECT_EQ(agenda[3].kind, Step::Kind::Static);
	EXPECT_FLOAT_EQ(agenda[3].seconds, 10.0f);
}

// The sit goes to the place only for a creature a script controls and given a place
TEST(CreatureScriptAgendas, TheSitActionGoesToThePlaceOnlyUnderAScript)
{
	const auto* sit = creature_plan_actions::For("SitDown");
	ASSERT_NE(sit, nullptr);
	creature_plan_actions::Situation situation {.radius = 2.0f};
	EXPECT_EQ(creature_plan_actions::Agenda(*sit, 4u, {1.0f, 1.0f}, situation, NoRandom)->size(), 1);
	situation.controlledByScript = true;
	EXPECT_EQ(creature_plan_actions::Agenda(*sit, 4u, {1.0f, 1.0f}, situation, NoRandom)->size(), 4);

	const auto* point = creature_plan_actions::For("PointAtCamera");
	ASSERT_NE(point, nullptr);
	EXPECT_FALSE(creature_plan_actions::Agenda(*point, std::nullopt, {}, situation, NoRandom).has_value());
	situation.eye = glm::vec3(0.0f, 30.0f, 0.0f);
	EXPECT_TRUE(creature_plan_actions::Agenda(*point, std::nullopt, {}, situation, NoRandom).has_value());
	for (const auto* name : {"LookForever", "LookButDontApproach", "LookAtCamera", "PointAtObject"})
	{
		EXPECT_NE(creature_plan_actions::For(name), nullptr) << name;
	}
}

// "move CreatureCow position to [CowBasePos]": a sixth of the way first (at least its height, at most 0.7 of it and
// 22), then within its height; with a distance, within it twice
TEST(CreatureScriptAgendas, AScriptSendsACreatureInTwoGoes)
{
	const glm::vec3 to {600.0f, 0.0f, 0.0f};
	const auto far = MoveForScript({0.0f, 0.0f}, to, 15.0f, 0.0f);
	ASSERT_EQ(far.size(), 2);
	EXPECT_FLOAT_EQ(far[0].movement.maxDistance, (0.7f * 15.0f) + 22.0f);
	EXPECT_FLOAT_EQ(far[1].movement.maxDistance, 15.0f);
	EXPECT_EQ(far[0].movement.point, glm::vec2(600.0f, 0.0f));
	ASSERT_TRUE(far[0].gaze.has_value());

	const auto middling = MoveForScript({0.0f, 0.0f}, {120.0f, 0.0f, 0.0f}, 15.0f, 0.0f);
	EXPECT_FLOAT_EQ(middling[0].movement.maxDistance, 20.0f);
	const auto near = MoveForScript({0.0f, 0.0f}, {30.0f, 0.0f, 0.0f}, 15.0f, 0.0f);
	EXPECT_FLOAT_EQ(near[0].movement.maxDistance, 15.0f);
	const auto given = MoveForScript({0.0f, 0.0f}, to, 15.0f, 4.0f);
	EXPECT_FLOAT_EQ(given[0].movement.maxDistance, 4.0f);
	EXPECT_FLOAT_EQ(given[1].movement.maxDistance, 4.0f);
}

// A script's focus: it turns only when more than an eighth of a half turn off, and not on top of the point
TEST(CreatureScriptAgendas, FacingAScriptsPointTurnsOnlyWhenWellOff)
{
	EXPECT_FALSE(NeedsToTurnToFace(50.0f, 0.3f));
	EXPECT_FALSE(NeedsToTurnToFace(50.0f, -0.39f));
	EXPECT_TRUE(NeedsToTurnToFace(50.0f, 0.4f));
	EXPECT_TRUE(NeedsToTurnToFace(50.0f, -3.0f));
	EXPECT_FALSE(NeedsToTurnToFace(0.05f, 3.0f));
	const auto face = FaceForScript({1.0f, 2.0f, 3.0f});
	ASSERT_EQ(face.size(), 1);
	EXPECT_EQ(face[0].movement.kind, Movement::Kind::FacePoint);
	EXPECT_EQ(face[0].movement.point, glm::vec2(1.0f, 3.0f));
	// Facing a thing, it turns to it and holds still two seconds
	const auto faceThing = FaceThingForScript(7);
	ASSERT_EQ(faceThing.size(), 1);
	EXPECT_EQ(faceThing[0].movement.kind, Movement::Kind::TurnToFaceObject);
	EXPECT_EQ(faceThing[0].movement.object, 7u);
	EXPECT_FLOAT_EQ(faceThing[0].seconds, 2.0f);
}

// Down the slope: the lowest of eight points ten away, the first when several are as low, the map's corner when none
// is on the land
TEST(CreatureScriptAgendas, DownTheSlopeIsTheLowestGroundAbout)
{
	const auto lowest = DownSlope({100.0f, 100.0f}, [](glm::vec2 at) -> std::optional<float> { return at.x + at.y; });
	EXPECT_EQ(lowest, glm::vec2(90.0f, 90.0f));
	const auto flat = DownSlope({100.0f, 100.0f}, [](glm::vec2) -> std::optional<float> { return 5.0f; });
	EXPECT_EQ(flat, glm::vec2(110.0f, 100.0f));
	const auto off = DownSlope({100.0f, 100.0f}, [](glm::vec2) -> std::optional<float> { return std::nullopt; });
	EXPECT_EQ(off, glm::vec2(0.0f, 0.0f));
}

// Under a script's control the mind chooses nothing for itself when its agenda is over; a step with something to look
// at says so instead of looking about
TEST(CreatureScriptAgendas, UnderAScriptTheMindChoosesNothingAndLooksWhereItIsTold)
{
	IdleMind mind;
	auto senses = Free();
	senses.choosesNext = false;
	static_cast<void>(Think(mind, senses, NoRandom));
	EXPECT_TRUE(mind.agenda.empty());
	senses.choosesNext = true;
	static_cast<void>(Think(mind, senses, NoRandom));
	EXPECT_FALSE(mind.agenda.empty());

	IdleMind told;
	Plan(told, Activity::Told, LookForever(5));
	told.step = 1;
	const auto commands = Think(told, Free(), NoRandom);
	ASSERT_TRUE(commands.gaze.has_value());
	EXPECT_EQ(commands.gaze->object, 5u);
	EXPECT_FALSE(commands.lookAbout);
}
