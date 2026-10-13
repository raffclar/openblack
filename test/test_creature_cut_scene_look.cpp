/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cstdint>

#include <optional>
#include <vector>

#include <gtest/gtest.h>

#include "Creature/CreatureLayers.h"
#include "Creature/CreaturePlanActions.h"
#include "Creature/CreatureScriptAgendas.h"

using namespace openblack;
using namespace openblack::creature_mind;
namespace animations = openblack::creature_layers::animations;

namespace
{
uint32_t AlwaysFirst(uint32_t /*range*/)
{
	return 0;
}

uint32_t NeverFirst(uint32_t range)
{
	return range - 1;
}

Senses CutSceneWatching(float distance, float turn)
{
	return {.seconds = 0.1f,
	        .camera =
	            Senses::CameraView {.distance = distance, .groundDistance = distance, .point = {5.0f, 6.0f}, .turn = turn}};
}

/// Just the watching steps of the look, as a plan
std::vector<Step> WatchOnly()
{
	return LookAtCameraInCutScene(std::nullopt, NeverFirst);
}
} // namespace

// Only cinema bars a script brought in make creatures look, and not while they are looking already
TEST(CreatureCutSceneLook, OnlyAScriptsCinemaBarsMakeCreaturesLook)
{
	EXPECT_TRUE(StartsLookAtCameraInCutScene(true, 7, false));
	EXPECT_FALSE(StartsLookAtCameraInCutScene(true, 7, true));
	EXPECT_FALSE(StartsLookAtCameraInCutScene(true, 0, false));
	EXPECT_FALSE(StartsLookAtCameraInCutScene(false, 7, false));
	EXPECT_FALSE(StartsLookAtCameraInCutScene(false, 0, false));
}

// With every chance taken: pointing at the camera, watching, waving, talking, looking confused, talking, watching
TEST(CreatureCutSceneLook, EveryExtraTaken)
{
	const glm::vec3 camera {10.0f, 40.0f, -20.0f};
	const auto agenda = LookAtCameraInCutScene(camera, AlwaysFirst);
	ASSERT_EQ(agenda.size(), 7u);

	EXPECT_EQ(agenda[0].kind, Step::Kind::Object);
	EXPECT_EQ(agenda[0].order.kind, ObjectOrder::Kind::PointAt);
	EXPECT_EQ(agenda[0].order.point, glm::vec2(10.0f, -20.0f));
	EXPECT_EQ(agenda[0].order.pointHeight, 40.0f);
	EXPECT_EQ(agenda[0].order.seconds, k_PointAtCameraSeconds);
	EXPECT_EQ(agenda[0].face, creature_face::Cue::Amazed);

	const auto expectWatch = [](const Step& step) {
		EXPECT_EQ(step.kind, Step::Kind::WatchCamera);
		EXPECT_EQ(step.turns, k_CutSceneWatchTurns);
		EXPECT_EQ(step.face, creature_face::Cue::Curiosity);
	};
	const auto expectTalk = [](const Step& step) {
		EXPECT_EQ(step.kind, Step::Kind::Overlay);
		EXPECT_EQ(step.animation, animations::k_Talk);
		EXPECT_EQ(step.face, creature_face::Cue::Curiosity);
	};
	expectWatch(agenda[1]);
	EXPECT_EQ(agenda[2].kind, Step::Kind::Action);
	EXPECT_EQ(agenda[2].animation, animations::k_FriendlyWave);
	EXPECT_EQ(agenda[2].face, creature_face::Cue::Amazed);
	expectTalk(agenda[3]);
	EXPECT_EQ(agenda[4].kind, Step::Kind::Action);
	EXPECT_EQ(agenda[4].animation, animations::k_Confused);
	EXPECT_EQ(agenda[4].face, creature_face::Cue::Amazed);
	expectTalk(agenda[5]);
	expectWatch(agenda[6]);

	// The head looks at the camera all along
	for (size_t i = 1; i < agenda.size(); ++i)
	{
		ASSERT_TRUE(agenda[i].gaze.has_value()) << i;
		EXPECT_TRUE(agenda[i].gaze->camera) << i;
	}
}

// With no chance taken it only watches the camera twice; with no camera it never points
TEST(CreatureCutSceneLook, NoExtras)
{
	const auto agenda = LookAtCameraInCutScene(glm::vec3(0.0f), NeverFirst);
	ASSERT_EQ(agenda.size(), 2u);
	EXPECT_EQ(agenda[0].kind, Step::Kind::WatchCamera);
	EXPECT_EQ(agenda[1].kind, Step::Kind::WatchCamera);
	EXPECT_EQ(LookAtCameraInCutScene(std::nullopt, AlwaysFirst).size(), 6u);
}

// Each watch lasts 21 turns whatever the camera does: far away or right overhead doesn't end it
TEST(CreatureCutSceneLook, WatchingLastsTwentyOneTurns)
{
	for (const auto distance : {20.0f, 150.0f, 0.05f})
	{
		IdleMind mind;
		Plan(mind, Activity::Told, WatchOnly());
		int turns = 0;
		while (mind.step < 1 && turns < 100)
		{
			static_cast<void>(Think(mind, CutSceneWatching(distance, 0.0f), AlwaysFirst));
			++turns;
		}
		EXPECT_EQ(turns, 21) << distance;
	}
}

// It turns to the camera when more than a quarter turn away, unless the camera is right overhead
TEST(CreatureCutSceneLook, WatchingTurnsToTheCamera)
{
	IdleMind mind;
	Plan(mind, Activity::Told, WatchOnly());
	EXPECT_FALSE(Think(mind, CutSceneWatching(20.0f, 1.5f), AlwaysFirst).move.has_value());
	const auto behind = Think(mind, CutSceneWatching(20.0f, -2.0f), AlwaysFirst);
	ASSERT_TRUE(behind.move.has_value());
	EXPECT_EQ(behind.move->kind, Movement::Kind::TurnToFace);
	EXPECT_EQ(behind.move->point, glm::vec2(5.0f, 6.0f));
	EXPECT_FALSE(Think(mind, CutSceneWatching(0.05f, 3.0f), AlwaysFirst).move.has_value());
}

// Without a player there is no camera to watch: each watch is done at once
TEST(CreatureCutSceneLook, WithoutAPlayerTheWatchingIsDoneAtOnce)
{
	IdleMind mind;
	Plan(mind, Activity::Told, WatchOnly());
	static_cast<void>(Think(mind, {.seconds = 0.1f}, AlwaysFirst));
	EXPECT_EQ(mind.step, 1u);
}

// Talking plays on top of the body and is done once nothing plays there any more
TEST(CreatureCutSceneLook, TalkingWaitsForTheOverlay)
{
	IdleMind mind;
	Plan(mind, Activity::Told,
	     {Step {.kind = Step::Kind::Overlay, .animation = animations::k_Talk},
	      Step {.kind = Step::Kind::Wait, .seconds = 1.0f}});
	const auto start = Think(mind, {.seconds = 0.1f}, AlwaysFirst);
	EXPECT_EQ(start.playOverlay, animations::k_Talk);
	EXPECT_EQ(mind.step, 0u);
	static_cast<void>(Think(mind, {.seconds = 0.1f, .overlayPlaying = true}, AlwaysFirst));
	EXPECT_EQ(mind.step, 0u);
	static_cast<void>(Think(mind, {.seconds = 0.1f}, AlwaysFirst));
	EXPECT_EQ(mind.step, 1u);
}

// The action of the game's table a cut scene forces can be carried out, pointing only with a camera
TEST(CreatureCutSceneLook, TheActionCanBeCarriedOut)
{
	const auto* look = creature_plan_actions::For(k_CutSceneLookAction);
	ASSERT_NE(look, nullptr);
	creature_plan_actions::Situation situation;
	const auto without = creature_plan_actions::Agenda(*look, std::nullopt, {}, situation, AlwaysFirst);
	ASSERT_TRUE(without.has_value());
	EXPECT_EQ(without->size(), 6u);
	situation.eye = glm::vec3(1.0f, 2.0f, 3.0f);
	const auto with = creature_plan_actions::Agenda(*look, std::nullopt, {}, situation, AlwaysFirst);
	ASSERT_TRUE(with.has_value());
	EXPECT_EQ(with->size(), 7u);
}
