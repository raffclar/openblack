/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cmath>

#include <array>
#include <numbers>
#include <optional>
#include <vector>

#include <glm/gtc/matrix_transform.hpp>
#include <glm/mat4x4.hpp>
#include <gtest/gtest.h>

#include "3D/SkeletalAnimation.h"
#include "Creature/CreatureLayers.h"
#include "Creature/CreatureWalkPlanner.h"

using namespace openblack;
using namespace openblack::creature_layers;
using skeletal_animation::Animation;
using skeletal_animation::Matrix;
using skeletal_animation::Pose;

namespace
{
constexpr float k_Tolerance = 1e-4f;
constexpr float k_Pi = std::numbers::pi_v<float>;

void ExpectMatrixNear(const Matrix& a, const Matrix& b)
{
	for (size_t r = 0; r < 3; ++r)
	{
		for (size_t c = 0; c < 3; ++c)
		{
			EXPECT_NEAR(a.at(r).at(c), b.at(r).at(c), k_Tolerance) << "row " << r << " column " << c;
		}
	}
}

/// A skeleton of a root with a bone either side of it, all with no rest rotation
skeletal_animation::Skeleton Symmetric(std::array<glm::mat4, 3>& rest)
{
	rest = {glm::mat4(1.0f), glm::translate(glm::mat4(1.0f), glm::vec3(1.0f, 2.0f, 0.0f)),
	        glm::translate(glm::mat4(1.0f), glm::vec3(-1.0f, 2.0f, 0.0f))};
	const std::array<uint32_t, 3> parents {skeletal_animation::k_NoParent, 0, 0};
	return skeletal_animation::Skeleton::FromRestMatrices(parents, rest);
}

/// A still animation of one keyframe per entry, moving the given bones
Animation Frames(std::vector<uint32_t> rotated, std::vector<std::vector<glm::vec3>> eulers,
                 std::vector<uint32_t> translated = {}, std::vector<std::vector<glm::vec3>> translations = {},
                 uint32_t duration = 1000)
{
	Animation animation {.duration = duration,
	                     .looping = true,
	                     .rotatedJoints = std::move(rotated),
	                     .translatedJoints = std::move(translated),
	                     .frames = {}};
	for (size_t i = 0; i < eulers.size(); ++i)
	{
		animation.frames.push_back(
		    {.eulerAngles = eulers[i], .translations = i < translations.size() ? translations[i] : std::vector<glm::vec3> {}});
	}
	return animation;
}
} // namespace

TEST(CreatureLayers, BiggerCreaturesPlayMoreSlowly)
{
	EXPECT_NEAR(PlaybackRate(0.0f), 1.6f, k_Tolerance);
	EXPECT_NEAR(PlaybackRate(1.0f), 1.175f, k_Tolerance);
	EXPECT_NEAR(PlaybackRate(2.0f), 0.75f, k_Tolerance);
}

// When a script sets real time, the body plays at the usual speed whatever its size
TEST(CreatureLayers, RealTimePlaysAtTheUsualSpeed)
{
	EXPECT_EQ(BodyRate(2.0f, true), 1.0f);
	EXPECT_EQ(BodyRate(0.0f, true), 1.0f);
	EXPECT_EQ(BodyRate(1.0f, false), PlaybackRate(1.0f));
}

// In a game turn, the body's animations move on by whole milliseconds: the turn's milliseconds at the rate of the size,
// cut towards zero
TEST(CreatureLayers, TheStepOfATurnIsWholeMilliseconds)
{
	EXPECT_EQ(TurnStepMs(100, 1.0f, false), 117);
	EXPECT_EQ(TurnStepMs(100, 1.3f, false), 104);
	EXPECT_EQ(TurnStepMs(100, 0.05f, false), 157);
	EXPECT_EQ(TurnStepMs(100, 4.0f, false), -10);
	// A frozen creature lives fewer milliseconds in the turn
	EXPECT_EQ(TurnStepMs(50, 1.0f, false), 58);
	EXPECT_EQ(TurnStepMs(0, 1.0f, false), 0);
	// Real time does not change the milliseconds of the turn
	EXPECT_EQ(TurnStepMs(100, 1.3f, true), 100);
	EXPECT_EQ(TurnStepMs(37, 2.0f, true), 37);
	// The walk's clips and the body's clips move on by the same step
	for (const float size : {0.05f, 0.5f, 0.8f, 1.0f, 1.3f, 1.7f, 2.4f, 3.1f, 4.0f})
	{
		EXPECT_EQ(TurnStepMs(100, size, false), creature_walk_planner::ClipStepMs(100, size)) << size;
	}
}

TEST(CreatureLayers, AnActionPlaysOnceThenTheBodyStands)
{
	const BodyAction stand {};
	EXPECT_FALSE(IsPlaying(stand));
	EXPECT_EQ(CurrentAnimation(stand), animations::k_Stand);

	auto body = PlayOnce(stand, animations::k_Tired, true);
	ASSERT_TRUE(body.has_value());
	EXPECT_TRUE(body->mirrored);
	EXPECT_EQ(CurrentAnimation(*body), animations::k_Tired);
	// Nothing else starts while it plays
	EXPECT_FALSE(PlayOnce(*body, animations::k_Happy, false).has_value());
	EXPECT_FALSE(PlaySequence(*body, 1, 2, 3).has_value());

	auto played = AdvanceBody(*body, 600.0f, 1000);
	EXPECT_TRUE(IsPlaying(played));
	EXPECT_NEAR(played.timeMs, 600.0f, k_Tolerance);
	played = AdvanceBody(played, 400.0f, 1000);
	EXPECT_FALSE(IsPlaying(played));
	EXPECT_FALSE(played.mirrored);
}

TEST(CreatureLayers, TheHandMeasuresABodyActionByTheAnimationThatTheGameKeeps)
{
	// An action played once keeps its own animation, but a kiss keeps the stand
	auto once = PlayOnce({}, animations::k_Tired, false);
	ASSERT_TRUE(once.has_value());
	EXPECT_EQ(ShareAnimation(*once), animations::k_Tired);
	once->kiss = true;
	EXPECT_EQ(ShareAnimation(*once), animations::k_Stand);

	// A sequence keeps its loop in its start, its loop and its end
	auto sequence = PlaySequence({}, 10, 11, 12);
	ASSERT_TRUE(sequence.has_value());
	EXPECT_EQ(CurrentAnimation(*sequence), 10u);
	EXPECT_EQ(ShareAnimation(*sequence), 11u);
	sequence->phase = BodyAction::Phase::End;
	EXPECT_EQ(ShareAnimation(*sequence), 11u);

	// A dance keeps the stand in its start, then the move that plays or played last
	auto dance = PlayDance({}, 1000.0f);
	ASSERT_TRUE(dance.has_value());
	EXPECT_EQ(ShareAnimation(*dance), animations::k_Stand);
	dance->phase = BodyAction::Phase::Loop;
	dance->animations[1] = 150;
	EXPECT_EQ(ShareAnimation(*dance), 150u);
	dance->phase = BodyAction::Phase::End;
	EXPECT_EQ(ShareAnimation(*dance), 150u);

	EXPECT_EQ(ShareAnimation(BodyAction {}), animations::k_Stand);
}

TEST(CreatureLayers, ADanceThatGoesFromItsStartStraightToItsFinishKeepsTheStand)
{
	auto dance = PlayDance({}, 1000.0f);
	ASSERT_TRUE(dance.has_value());
	*dance = EndDance(*dance);
	const auto finish = AdvanceBody(*dance, 1000.0f, 1000);
	ASSERT_EQ(finish.phase, BodyAction::Phase::End);
	EXPECT_FALSE(finish.chooseDanceMove);
	EXPECT_EQ(CurrentAnimation(finish), animations::k_DanceFinish);
	EXPECT_EQ(ShareAnimation(finish), animations::k_Stand);

	// A dance that gets to its moves needs its first move
	const auto moves = AdvanceBody(*PlayDance({}, 1000.0f), 1000.0f, 1000);
	EXPECT_EQ(moves.phase, BodyAction::Phase::Loop);
	EXPECT_TRUE(moves.chooseDanceMove);
}

TEST(CreatureLayers, TheHandReadsTheActionOfTheBodyFirstThenTheMoveOfTheLegs)
{
	// A body at rest that does not move does no action
	EXPECT_FALSE(ShareOf(BodyAction {}, false, std::nullopt).has_value());

	// The legs give their time over the stand
	const auto legs = ShareOf(BodyAction {}, false, 420);
	ASSERT_TRUE(legs.has_value());
	EXPECT_EQ(legs->animation, animations::k_Stand);
	EXPECT_EQ(legs->timeMs, 420.0f);

	// An action of the body comes first, with the animation that the game keeps for it
	auto once = PlayOnce({}, animations::k_Tired, false);
	ASSERT_TRUE(once.has_value());
	once->timeMs = 250.0f;
	const auto action = ShareOf(*once, false, 420);
	ASSERT_TRUE(action.has_value());
	EXPECT_EQ(action->animation, animations::k_Tired);
	EXPECT_EQ(action->timeMs, 250.0f);

	// The confused look of a walk that waits for its route keeps the stand
	auto confused = PlayOnce({}, animations::k_Confused, false);
	ASSERT_TRUE(confused.has_value());
	confused->timeMs = 300.0f;
	const auto look = ShareOf(*confused, true, 420);
	ASSERT_TRUE(look.has_value());
	EXPECT_EQ(look->animation, animations::k_Stand);
	EXPECT_EQ(look->timeMs, 300.0f);
}

TEST(CreatureLayers, AMissingAnimationEndsAtOnce)
{
	const auto body = PlayOnce({}, animations::k_Tired, false);
	ASSERT_TRUE(body.has_value());
	EXPECT_FALSE(IsPlaying(AdvanceBody(*body, 1.0f, std::nullopt)));
}

TEST(CreatureLayers, ASitStartsLoopsUntilToldThenEnds)
{
	auto body = *PlaySequence({}, animations::k_StartSit, animations::k_Sit, animations::k_EndSit);
	EXPECT_EQ(CurrentAnimation(body), animations::k_StartSit);
	body = AdvanceBody(body, 500.0f, 500);
	EXPECT_TRUE(IsLooping(body));
	EXPECT_EQ(CurrentAnimation(body), animations::k_Sit);
	EXPECT_NEAR(body.timeMs, 0.0f, k_Tolerance);
	// The loop wraps round for as long as it isn't told to end
	body = AdvanceBody(body, 700.0f, 400);
	EXPECT_TRUE(IsLooping(body));
	EXPECT_NEAR(body.timeMs, 300.0f, k_Tolerance);
	body = AdvanceBody(EndLoop(body), 10.0f, 400);
	EXPECT_EQ(CurrentAnimation(body), animations::k_EndSit);
	body = AdvanceBody(body, 800.0f, 800);
	EXPECT_FALSE(IsPlaying(body));
}

TEST(CreatureLayers, ACreatureChangingItsPlanLetsAnActionPlayOn)
{
	// Reeling from a slap when it decides to do something else, it reels to the end
	const auto reel = *PlayOnce({}, 167, true);
	auto body = AdvanceBody(reel, 400.0f, 1633);
	const auto stopped = EndLoop(body);
	EXPECT_EQ(stopped.kind, BodyAction::Kind::Once);
	EXPECT_EQ(CurrentAnimation(stopped), 167u);
	EXPECT_TRUE(stopped.mirrored);
	EXPECT_NEAR(stopped.timeMs, 400.0f, k_Tolerance);
	body = AdvanceBody(stopped, 1200.0f, 1633);
	EXPECT_TRUE(IsPlaying(body));
	EXPECT_FALSE(IsPlaying(AdvanceBody(body, 100.0f, 1633)));
}

TEST(CreatureLayers, ACreatureChangingItsPlanGetsUpThroughItsEnd)
{
	// Asleep when it decides to do something else, it wakes up through the end of sleeping rather than snapping upright
	auto body = *PlaySequence({}, animations::k_StartSleep, animations::k_Sleep, animations::k_EndSleep);
	body = AdvanceBody(body, 600.0f, 600);
	ASSERT_TRUE(IsLooping(body));
	body = AdvanceBody(EndLoop(body), 16.0f, 900);
	EXPECT_EQ(CurrentAnimation(body), animations::k_EndSleep);
	body = AdvanceBody(body, 1000.0f, 1000);
	EXPECT_FALSE(IsPlaying(body));
}

TEST(CreatureLayers, ASequenceToldToEndWhileStartingGoesStraightFromItsStartToItsEnd)
{
	auto body = *PlaySequence({}, animations::k_StartSit, animations::k_Sit, animations::k_EndSit);
	body = AdvanceBody(EndLoop(AdvanceBody(body, 200.0f, 500)), 200.0f, 500);
	// The start plays out
	EXPECT_EQ(CurrentAnimation(body), animations::k_StartSit);
	body = AdvanceBody(body, 100.0f, 500);
	EXPECT_EQ(body.phase, BodyAction::Phase::End);
	EXPECT_EQ(CurrentAnimation(body), animations::k_EndSit);
	EXPECT_NEAR(body.timeMs, 0.0f, k_Tolerance);
}

TEST(CreatureLayers, ACreatureLyingWhereItFellGetsUpWhenItsPlanChanges)
{
	auto body = *PlaySequence({}, animations::k_Faint, animations::k_Faint, animations::k_GetUp, true);
	body = AdvanceBody(AdvanceBody(body, 800.0f, 800), 5000.0f, 800);
	ASSERT_TRUE(IsLooping(body));
	body = AdvanceBody(EndLoop(body), 16.0f, 800);
	EXPECT_EQ(CurrentAnimation(body), animations::k_GetUp);
}

TEST(CreatureLayers, AHurriedBodyEndsItsActionAtTwiceTheSpeed)
{
	// A creature that starts to catch ends its squat at two times the usual speed, through the end of the squat
	auto body = *PlaySequence({}, animations::k_StartPoo, animations::k_Poo, animations::k_EndPoo);
	body = AdvanceBody(AdvanceBody(body, 500.0f, 500), 100.0f, 1000);
	ASSERT_TRUE(IsLooping(body));
	body = AdvanceBody(EndLoop(Hurry(body)), 16.0f, 1000);
	EXPECT_EQ(CurrentAnimation(body), animations::k_EndPoo);
	body = AdvanceBody(body, 100.0f, 1000);
	EXPECT_NEAR(body.timeMs, 200.0f, k_Tolerance);
	body = AdvanceBody(body, 400.0f, 1000);
	EXPECT_FALSE(IsPlaying(body));
	// The body stands again and plays the next action at the usual speed
	body = *PlayOnce(body, animations::k_Happy, false);
	body = AdvanceBody(body, 100.0f, 1000);
	EXPECT_NEAR(body.timeMs, 100.0f, k_Tolerance);
}

TEST(CreatureLayers, ABodyThatStandsIsNotHurried)
{
	EXPECT_FALSE(Hurry({}).hurried);
}

TEST(CreatureLayers, AFaintKeepsItsPaceWhenHurried)
{
	// The game ends only the actions and statics rapidly: a faint that lies where it fell keeps its pace
	auto faint = *PlaySequence({}, animations::k_Faint, animations::k_Faint, animations::k_GetUp);
	faint.holdLoop = true;
	EXPECT_FALSE(Hurry(faint).hurried);
}

TEST(CreatureLayers, AFaintStaysAtTheLastFrameFromTheTurnThatItsFallEnds)
{
	// The fall ends in this turn: the held loop shows the last frame at once, never the first frame of the clip
	auto faint = *PlaySequence({}, animations::k_Faint, animations::k_Faint, animations::k_GetUp);
	faint.holdLoop = true;
	faint.timeMs = 1450.0f;
	const auto held = AdvanceBody(faint, 100.0f, 1500);
	EXPECT_EQ(held.phase, BodyAction::Phase::Loop);
	EXPECT_FLOAT_EQ(held.timeMs, 1499.0f);
	EXPECT_FLOAT_EQ(AdvanceBody(held, 100.0f, 1500).timeMs, 1499.0f);
	// A loop that is not held starts its clip again
	auto loop = *PlaySequence({}, animations::k_Faint, animations::k_Faint, animations::k_GetUp);
	loop.timeMs = 1450.0f;
	EXPECT_FLOAT_EQ(AdvanceBody(loop, 100.0f, 1500).timeMs, 0.0f);
}

TEST(CreatureLayers, ABodyTimedByWhatPlaysItStaysWherePut)
{
	// A fight sets its fighter's move at the move's own time each frame; the frame mustn't move it on as well, or the
	// time shown would run ahead by however long each frame took, and go back when a frame takes less
	const BodyAction move {
	    .kind = BodyAction::Kind::Sequence,
	    .phase = BodyAction::Phase::Loop,
	    .animations = {120, 120, 120},
	    .timeMs = 70.0f,
	    .timedByPlayer = true,
	};
	for (const auto milliseconds : {0.0f, 1.0f, 16.0f, 700.0f})
	{
		const auto played = AdvanceBody(move, milliseconds, 500);
		EXPECT_EQ(played.phase, BodyAction::Phase::Loop);
		EXPECT_FLOAT_EQ(played.timeMs, 70.0f);
	}
}

TEST(CreatureLayers, AFaceRunsBackBeforeTheNextStarts)
{
	auto face = PullFace({}, 16, 5000.0f);
	face = AdvanceFace(face, 100.0f, std::nullopt);
	EXPECT_EQ(face.current, 16u);
	face = AdvanceFace(face, 300.0f, 1000);
	EXPECT_NEAR(face.timeMs, 300.0f, k_Tolerance);
	// Another face is pulled: the smile runs back to its start at full speed, then the growl starts
	face = PullFace(face, 18, 5000.0f);
	face = AdvanceFace(face, 200.0f, 1000);
	EXPECT_EQ(face.current, 16u);
	EXPECT_NEAR(face.timeMs, 100.0f, k_Tolerance);
	face = AdvanceFace(face, 200.0f, 1000);
	EXPECT_EQ(face.current, 18u);
	EXPECT_NEAR(face.timeMs, 0.0f, k_Tolerance);
	// Relaxed: it runs back a quarter as fast
	face = AdvanceFace(face, 400.0f, 1000);
	face = RelaxFace(face);
	face = AdvanceFace(face, 400.0f, 1000);
	EXPECT_EQ(face.current, 18u);
	EXPECT_NEAR(face.timeMs, 300.0f, k_Tolerance);
	face = AdvanceFace(face, 1200.0f, 1000);
	EXPECT_FALSE(face.current.has_value());
}

TEST(CreatureLayers, AFaceThatTheRigHasNoClipForIsKeptButItsTimeDoesNotMove)
{
	// The tiger has no clip for the laugh: the laugh becomes the face, but with no clip its time stays at zero
	auto face = PullFace({}, 23, 3000.0f);
	face = AdvanceFace(face, 100.0f, std::nullopt);
	EXPECT_EQ(face.current, 23u);
	for (int turn = 0; turn < 20; ++turn)
	{
		face = AdvanceFace(face, 100.0f, std::nullopt);
		EXPECT_EQ(face.current, 23u);
		EXPECT_FLOAT_EQ(face.timeMs, 0.0f);
	}
	// The next face starts at once, as the face with no clip has no time to run back
	face = PullFace(face, 16, 3000.0f);
	face = AdvanceFace(face, 100.0f, std::nullopt);
	EXPECT_EQ(face.current, 16u);
	EXPECT_FLOAT_EQ(face.timeMs, 0.0f);
	face = AdvanceFace(face, 100.0f, 1000);
	EXPECT_NEAR(face.timeMs, 100.0f, k_Tolerance);
}

TEST(CreatureLayers, AFaceHoldsItsExpressionRatherThanLooping)
{
	auto face = PullFace({}, 20, 3000.0f);
	face = AdvanceFace(face, 10.0f, 250);
	// Played through, it holds its last frame without starting over: the mouth doesn't open and close again
	for (int frame = 0; frame < 100; ++frame)
	{
		face = AdvanceFace(face, 16.0f, 250);
		EXPECT_EQ(face.current, 20u);
		if (frame > 16)
		{
			EXPECT_NEAR(face.timeMs, 250.0f, k_Tolerance);
		}
	}
}

TEST(CreatureLayers, AFaceRelaxesWhenItsTimeIsUp)
{
	auto face = PullFace({}, 21, 1000.0f, creature_face::Cue::Sad);
	EXPECT_EQ(face.cue, creature_face::Cue::Sad);
	face = AdvanceFace(face, 1.0f, 250);
	face = AdvanceFace(face, 500.0f, 250);
	EXPECT_NEAR(face.timeMs, 250.0f, k_Tolerance);
	face = AdvanceFace(face, 600.0f, 250);
	// Let go of after a second, it runs back at a quarter speed, 150 ms of its expression in the last 600 ms
	EXPECT_FALSE(face.wanted.has_value());
	EXPECT_EQ(face.cue, creature_face::Cue::None);
	EXPECT_EQ(face.current, 21u);
	EXPECT_NEAR(face.timeMs, 100.0f, k_Tolerance);
	face = AdvanceFace(face, 200.0f, 250);
	EXPECT_NEAR(face.timeMs, 50.0f, k_Tolerance);
	face = AdvanceFace(face, 1000.0f, 250);
	EXPECT_FALSE(face.current.has_value());
}

TEST(CreatureLayers, AFacePulledWithoutATimeIsHeldForAnHour)
{
	auto face = PullFace({}, 18, 0.0f);
	face = AdvanceFace(face, 30.0f * 60.0f * 1000.0f, 250);
	EXPECT_EQ(face.wanted, 18u);
	face = AdvanceFace(face, 31.0f * 60.0f * 1000.0f, 250);
	EXPECT_FALSE(face.wanted.has_value());
}

TEST(CreatureLayers, PullingTheHeldFaceAgainKeepsHoldingIt)
{
	auto face = PullFace({}, 16, 500.0f);
	face = AdvanceFace(face, 1.0f, 250);
	face = AdvanceFace(face, 400.0f, 250);
	face = PullFace(face, 16, 500.0f);
	face = AdvanceFace(face, 400.0f, 250);
	EXPECT_EQ(face.wanted, 16u);
	EXPECT_EQ(face.current, 16u);
	EXPECT_NEAR(face.timeMs, 250.0f, k_Tolerance);
}

TEST(CreatureLayers, AFaceTheSpeciesLacksStaysAtItsStart)
{
	auto face = PullFace({}, 26, 1000.0f);
	face = AdvanceFace(face, 1.0f, std::nullopt);
	face = AdvanceFace(face, 100.0f, std::nullopt);
	EXPECT_EQ(face.current, 26u);
	EXPECT_NEAR(face.timeMs, 0.0f, k_Tolerance);
}

TEST(CreatureLayers, AGesturePlaysOnce)
{
	auto gesture = PlayGesture({}, animations::k_Yawn);
	ASSERT_TRUE(gesture.has_value());
	EXPECT_FALSE(PlayGesture(*gesture, animations::k_Yawn).has_value());
	auto played = AdvanceGesture(*gesture, 900.0f, 1000);
	EXPECT_EQ(played.animation, animations::k_Yawn);
	played = AdvanceGesture(played, 100.0f, 1000);
	EXPECT_FALSE(played.animation.has_value());
}

TEST(CreatureLayers, TheHeadSpeedsUpSteadily)
{
	const auto first = TurnHead({}, 1.0f, 2.0f, 0.1f, k_YawLimit);
	EXPECT_NEAR(first.velocity, 0.2f, k_Tolerance);
	EXPECT_NEAR(first.angle, 0.02f, k_Tolerance);
	const auto second = TurnHead(first, 1.0f, 2.0f, 0.1f, k_YawLimit);
	EXPECT_NEAR(second.velocity, 0.4f, k_Tolerance);
}

TEST(CreatureLayers, TheHeadSettlesWithoutOvershooting)
{
	LookAxis axis {};
	float largest = 0.0f;
	for (int i = 0; i < 400; ++i)
	{
		axis = TurnHead(axis, -0.8f, 4.0f, 0.02f, k_YawLimit);
		largest = std::max(largest, -axis.angle);
	}
	EXPECT_FLOAT_EQ(axis.angle, -0.8f);
	EXPECT_FLOAT_EQ(axis.velocity, 0.0f);
	EXPECT_LE(largest, 0.8f + k_Tolerance);
}

TEST(CreatureLayers, TheHeadNeverTurnsPastItsLimit)
{
	LookAxis axis {.angle = 1.5f, .velocity = 5.0f};
	axis = TurnHead(axis, 3.0f, 100.0f, 0.1f, k_PitchLimit);
	EXPECT_LE(axis.angle, k_PitchLimit);
}

TEST(CreatureLayers, TheMiddleOfAHeadTurnLooksAhead)
{
	EXPECT_EQ(LookTime(0.0f, k_YawLimit, 1000), 500u);
	EXPECT_EQ(LookTime(k_YawLimit, k_YawLimit, 1000), 1000u);
	EXPECT_EQ(LookTime(-k_YawLimit, k_YawLimit, 1000), 0u);
	EXPECT_EQ(LookTime(k_PitchLimit / 2.0f, k_PitchLimit, 1000), 750u);
}

TEST(CreatureLayers, AnimationNames)
{
	EXPECT_EQ(animations::Name(animations::k_Tired), "tired");
	EXPECT_EQ(animations::Name(animations::k_FriendlyWave), "friendly_wave");
	EXPECT_EQ(animations::Name(animations::k_FirstFace), "smile");
	EXPECT_EQ(animations::Name(animations::k_Yawn), "yawn");
	EXPECT_EQ(animations::Name(animations::k_Sit), "sit");
}

TEST(SkeletalLayers, WeightedPosesSum)
{
	const std::vector<Pose> a {{.rotation = skeletal_animation::k_Identity, .translation = {2.0f, 0.0f, 0.0f}}};
	const std::vector<Pose> b {
	    {.rotation = skeletal_animation::RotationYXZ({0.0f, 0.4f, 0.0f}), .translation = {4.0f, 2.0f, 0.0f}}};
	const std::array<std::vector<Pose>, 2> poses {a, b};
	const std::array<float, 2> weights {0.75f, 0.25f};
	const auto sum = skeletal_animation::WeightedSum(poses, weights);
	ASSERT_EQ(sum.size(), 1u);
	EXPECT_NEAR(sum[0].translation.x, 2.5f, k_Tolerance);
	EXPECT_NEAR(sum[0].translation.y, 0.5f, k_Tolerance);
	ExpectMatrixNear(sum[0].rotation, skeletal_animation::Lerp(a[0].rotation, b[0].rotation, 0.25f));
}

TEST(SkeletalLayers, ALayerAtItsReferenceChangesNothing)
{
	std::array<glm::mat4, 3> rest {};
	const auto skeleton = Symmetric(rest);
	const auto layer = Frames({1}, {{glm::vec3(0.3f, 0.1f, 0.0f)}, {glm::vec3(0.3f, 0.6f, 0.0f)}}, {1},
	                          {{glm::vec3(1.0f, 0.0f, 0.0f)}, {glm::vec3(1.0f, 0.5f, 0.0f)}});
	std::vector<Pose> poses(3, Pose {.rotation = skeletal_animation::RotationYXZ({0.0f, 0.2f, 0.0f}), .translation = {}});
	const auto before = poses;
	skeletal_animation::AddLayer(poses, layer, 0, 0, skeleton);
	ExpectMatrixNear(poses[1].rotation, before[1].rotation);
	EXPECT_NEAR(glm::length(poses[1].translation - before[1].translation), 0.0f, k_Tolerance);
}

TEST(SkeletalLayers, ALayerAddsItsTurnAwayFromTheReference)
{
	std::array<glm::mat4, 3> rest {};
	const auto skeleton = Symmetric(rest);
	// Two keyframes 0.5 apart in y, sampled exactly on the second
	const auto layer = Frames({1}, {{glm::vec3(0.0f, 0.1f, 0.0f)}, {glm::vec3(0.0f, 0.6f, 0.0f)}}, {1},
	                          {{glm::vec3(1.0f, 0.0f, 0.0f)}, {glm::vec3(1.0f, 0.5f, 0.0f)}});
	std::vector<Pose> poses(3, Pose {.rotation = skeletal_animation::RotationYXZ({0.0f, 0.2f, 0.0f}), .translation = {}});
	skeletal_animation::AddLayer(poses, layer, 500, 0, skeleton);
	// Turned on by the layer's half radian: 0.2 + 0.5 about y
	ExpectMatrixNear(poses[1].rotation, skeletal_animation::RotationYXZ({0.0f, 0.7f, 0.0f}));
	EXPECT_NEAR(poses[1].translation.y, 0.5f, k_Tolerance);
	// Bones the layer doesn't move stay as they were
	ExpectMatrixNear(poses[2].rotation, skeletal_animation::RotationYXZ({0.0f, 0.2f, 0.0f}));
}

TEST(SkeletalLayers, MirrorBonesPairAcrossTheBody)
{
	std::array<glm::mat4, 3> rest {};
	[[maybe_unused]] const auto skeleton = Symmetric(rest);
	const auto mirror = skeletal_animation::MirrorJoints(rest);
	ASSERT_EQ(mirror.size(), 3u);
	EXPECT_EQ(mirror[0], 0u);
	EXPECT_EQ(mirror[1], 2u);
	EXPECT_EQ(mirror[2], 1u);
}

TEST(SkeletalLayers, AnUnpairedBoneKeepsItsPlace)
{
	const std::array<glm::mat4, 2> rest {glm::mat4(1.0f), glm::translate(glm::mat4(1.0f), glm::vec3(1.0f, 2.0f, 0.0f))};
	const auto mirror = skeletal_animation::MirrorJoints(rest);
	EXPECT_EQ(mirror[1], 1u);
}

TEST(SkeletalLayers, AMirroredAnimationMovesTheOtherSide)
{
	std::array<glm::mat4, 3> rest {};
	const auto skeleton = Symmetric(rest);
	const auto mirror = skeletal_animation::MirrorJoints(rest);
	const glm::vec3 euler {0.1f, 0.2f, 0.3f};
	const auto stand = Frames({0}, {{glm::vec3(0.0f)}}, {0}, {{glm::vec3(0.0f, 1.0f, 0.0f)}});
	const auto animation = Frames({1}, {{euler}, {euler}}, {0}, {{glm::vec3(2.0f, 1.0f, 3.0f)}, {glm::vec3(2.0f, 1.0f, 3.0f)}});

	const auto plain = skeletal_animation::SampleCycle(animation, stand, 0, skeleton);
	ExpectMatrixNear(plain[1].rotation, skeletal_animation::RotationYXZ(euler));
	ExpectMatrixNear(plain[2].rotation, skeletal_animation::k_Identity);
	EXPECT_NEAR(plain[0].translation.x, 2.0f, k_Tolerance);

	const auto mirrored = skeletal_animation::SampleCycle(animation, stand, 0, skeleton, mirror);
	// The right bone's keyframes move the left, their y and z angles negated
	ExpectMatrixNear(mirrored[2].rotation, skeletal_animation::RotationYXZ({0.1f, -0.2f, -0.3f}));
	ExpectMatrixNear(mirrored[1].rotation, skeletal_animation::k_Identity);
	// The root's sideways movement away from the stand is flipped
	EXPECT_NEAR(mirrored[0].translation.x, -2.0f, k_Tolerance);
	EXPECT_NEAR(mirrored[0].translation.y, 1.0f, k_Tolerance);
	EXPECT_NEAR(mirrored[0].translation.z, 3.0f, k_Tolerance);
}

TEST(CreatureLayers, ADanceStartsThenChoosesItsFirstMoveFromAllSeven)
{
	using creature_layers::BodyAction;
	auto body = creature_layers::PlayDance({}, 800.0f);
	ASSERT_TRUE(body.has_value());
	EXPECT_EQ(creature_layers::CurrentAnimation(*body), creature_layers::animations::k_DanceStart);
	EXPECT_FALSE(creature_layers::PlayDance(*body, 800.0f).has_value());
	auto next = creature_layers::AdvanceBody(*body, 600.0f, 500U);
	EXPECT_EQ(next.phase, BodyAction::Phase::Loop);
	EXPECT_TRUE(next.chooseDanceMove);
	EXPECT_TRUE(next.firstDanceMove);
	uint32_t asked = 0;
	const auto move = creature_layers::ChooseDanceMove(
	    true,
	    [&](uint32_t range) {
		    asked = range;
		    return 6U;
	    },
	    [](size_t) { return true; });
	EXPECT_EQ(asked, 7U);
	EXPECT_EQ(move, std::optional<size_t> {153});
}

TEST(CreatureLayers, ALaterDanceMoveComesFromTheFirstFiveAndTheBodyChoosesAgainForAMissingMove)
{
	std::vector<uint32_t> draws {4, 1};
	size_t at = 0;
	uint32_t asked = 0;
	const auto move = creature_layers::ChooseDanceMove(
	    false,
	    [&](uint32_t range) {
		    asked = range;
		    return draws.at(at++);
	    },
	    [](size_t index) { return index != 151; });
	EXPECT_EQ(asked, 5U);
	EXPECT_EQ(move, std::optional<size_t> {148});
	EXPECT_FALSE(creature_layers::ChooseDanceMove(false, [](uint32_t) { return 0U; }, [](size_t) { return false; }));
}

TEST(CreatureLayers, EachDanceMoveLastsItsRealTimeAndTheLeftOverTimeCarries)
{
	using creature_layers::BodyAction;
	BodyAction body {.kind = BodyAction::Kind::Dance,
	                 .phase = BodyAction::Phase::Loop,
	                 .animations = {145, 148, 146},
	                 .timeMs = 700.0f,
	                 .danceMoveMs = 800.0f};
	// The playback time does not count, only the real time
	auto next = creature_layers::AdvanceBody(body, 1000.0f, 2000U, 150.0f);
	EXPECT_TRUE(next.chooseDanceMove);
	EXPECT_FALSE(next.firstDanceMove);
	EXPECT_FLOAT_EQ(next.timeMs, 50.0f);
	// The animation stretches to the length of the move
	body.timeMs = 400.0f;
	EXPECT_FLOAT_EQ(creature_layers::DanceMoveTime(body, 2000U), 1000.0f);
}

TEST(CreatureLayers, AStopOfTheDancePlaysTheFinishAtOnceOrAfterTheStart)
{
	using creature_layers::BodyAction;
	BodyAction moving {.kind = BodyAction::Kind::Dance, .phase = BodyAction::Phase::Loop, .animations = {145, 150, 146}};
	const auto stopped = creature_layers::EndDance(moving);
	EXPECT_EQ(creature_layers::CurrentAnimation(stopped), creature_layers::animations::k_DanceFinish);
	EXPECT_FALSE(creature_layers::IsDancing(stopped));

	auto starting = *creature_layers::PlayDance({}, 1000.0f);
	starting = creature_layers::EndDance(starting);
	EXPECT_EQ(creature_layers::CurrentAnimation(starting), creature_layers::animations::k_DanceStart);
	const auto after = creature_layers::AdvanceBody(starting, 600.0f, 500U);
	EXPECT_EQ(after.phase, BodyAction::Phase::End);
	EXPECT_FALSE(after.chooseDanceMove);
	// A static pose does not stop for a stop of the dance
	BodyAction sitting {.kind = BodyAction::Kind::Sequence, .phase = BodyAction::Phase::Loop, .animations = {37, 38, 39}};
	EXPECT_FALSE(creature_layers::EndDance(sitting).endWanted);
}

TEST(CreatureLayers, ADroppedFaceShowsNoFaceAtOnceAndWantsNone)
{
	const auto pulled = creature_layers::PullFace({}, 3, 2000.0f, creature_face::Cue::None);
	auto showing = creature_layers::AdvanceFace(pulled, 100.0f, 500U);
	ASSERT_TRUE(showing.current.has_value());
	const auto dropped = creature_layers::DropFace(showing);
	EXPECT_FALSE(dropped.current.has_value());
	EXPECT_FALSE(dropped.wanted.has_value());
	EXPECT_FLOAT_EQ(dropped.timeMs, 0.0f);
	// The time that the face had left stays, but no face comes back
	EXPECT_FLOAT_EQ(dropped.remainingMs, showing.remainingMs);
	const auto later = creature_layers::AdvanceFace(dropped, 100.0f, 500U);
	EXPECT_FALSE(later.current.has_value());
}
