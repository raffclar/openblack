/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "CreatureEmoteAgendas.h"

#include <cmath>

#include <glm/geometric.hpp>

#include "Common/GameRandom.h"
#include "Creature/CreatureLayers.h"

using namespace openblack;
using namespace openblack::creature_emote;
using creature_face::Cue;
using creature_mind::Effect;
using creature_mind::Eyes;
using creature_mind::Gaze;
using creature_mind::Movement;
using creature_mind::ObjectOrder;
namespace animations = openblack::creature_layers::animations;

namespace
{
Step PlayOnce(size_t animation)
{
	return {.kind = Step::Kind::Action, .animation = animation};
}

Step TurnToCamera(glm::vec2 camera)
{
	return {.kind = Step::Kind::Move,
	        .movement = {.kind = Movement::Kind::TurnToFace, .point = camera},
	        .gaze = Gaze {.camera = true}};
}

/// A frightened start: the eyes go wide and stay so until the creature takes up something else, and the head looks
/// about
Step FrightenedStart()
{
	auto start = PlayOnce(k_FrightenedAnimation);
	start.heldEyes = Eyes::Wide;
	return start;
}
} // namespace

float creature_emote::FloatRandom(const Random& random, float most)
{
	return static_cast<float>(random(game_random::k_FloatRandRange)) * most * game_random::k_FloatRandScale;
}

float creature_emote::WholeTurnSeconds(float seconds, float turnsPerSecond)
{
	return std::trunc(seconds * turnsPerSecond) / turnsPerSecond;
}

std::vector<Step> creature_emote::HowlAtPlayer(glm::vec2 camera)
{
	return {TurnToCamera(camera), PlayOnce(k_HowlAnimation)};
}

std::vector<Step> creature_emote::BeSad(uint32_t self, float turnsPerSecond, const Random& random)
{
	const float wait = k_SadWaitSeconds + FloatRandom(random, k_SadWaitExtraSeconds);
	const Step sad {.kind = Step::Kind::Wait,
	                .seconds = WholeTurnSeconds(wait, turnsPerSecond),
	                .face = Cue::Sad,
	                .gaze = Gaze {.object = self, .bottom = true}};
	return {sad, PlayOnce(random(2) == 0 ? animations::k_Sad : k_CryAnimation)};
}

std::vector<Step> creature_emote::PullSillyFaces(uint32_t villager, float height, const Random& random)
{
	const Gaze look {.object = villager};
	std::vector<Step> agenda {
	    {.kind = Step::Kind::Move,
	     .movement = {.kind = Movement::Kind::GoNearObject, .object = villager, .maxDistance = k_SillyFacesHeights * height},
	     .gaze = Gaze {.object = villager, .whileGoing = true}},
	    {.kind = Step::Kind::Move,
	     .seconds = k_SillyFacesFaceSeconds,
	     .movement = {.kind = Movement::Kind::TurnToFaceObject, .object = villager},
	     .gaze = look},
	};
	if (random(k_SillyFacesPlayLots) > 0)
	{
		agenda.push_back(PlayOnce(animations::k_FeelPlayful));
	}
	for (int i = 0; i < 3; ++i)
	{
		agenda.push_back({.kind = Step::Kind::Face,
		                  .seconds = k_SillyFaceSeconds,
		                  .animation = animations::k_FirstFace + random(k_SillyFaceCount),
		                  .effect = i == 1 ? Effect::Completed : Effect::None,
		                  .gaze = look});
	}
	return agenda;
}

float creature_emote::FollowCameraReach(float height)
{
	return (height + 1.0f) * 2.0f;
}

std::vector<Step> creature_emote::BePathetic(glm::vec2 camera, float height)
{
	return {
	    {.kind = Step::Kind::FollowCamera,
	     .seconds = k_FollowCameraSeconds,
	     .movement = {.maxDistance = FollowCameraReach(height)},
	     .gaze = Gaze {.camera = true}},
	    TurnToCamera(camera),
	    PlayOnce(k_PickMeAnimation),
	};
}

FollowCameraAction creature_emote::FollowCameraTurn(const FollowCameraView& view, uint32_t& turnsLeft, float reach)
{
	if (!view.focus.has_value() || turnsLeft == 0)
	{
		return FollowCameraAction::Done;
	}
	--turnsLeft;
	if (view.moving)
	{
		return view.focusAhead ? FollowCameraAction::Continue : FollowCameraAction::MoveAgain;
	}
	const glm::vec2 focus {view.focus->x, view.focus->z};
	if (glm::distance(view.position, focus) < reach)
	{
		return FollowCameraAction::Done;
	}
	if (*view.focus == glm::vec3(0.0f))
	{
		return FollowCameraAction::Done;
	}
	return FollowCameraAction::MoveAgain;
}

std::vector<Step> creature_emote::RunAway(const RunFrom& from, const Random& random)
{
	std::vector<Step> agenda;
	if (random(k_FrightenedStartLots) == 0)
	{
		agenda.push_back(FrightenedStart());
	}
	Step run {.kind = Step::Kind::Move,
	          .movement = {.kind = Movement::Kind::FleeFrom, .point = from.point, .run = true},
	          .effect = Effect::Completed,
	          .face = Cue::Fear,
	          .gaze = Gaze {.object = from.object, .hand = !from.object.has_value(), .runningAway = true}};
	if (from.object.has_value())
	{
		run.heldEyes = Eyes::Wide;
	}
	agenda.push_back(run);
	if (random(k_FrightenedStartLots) == 0)
	{
		agenda.push_back(FrightenedStart());
	}
	return agenda;
}

bool creature_emote::RunLooksBack(RunLookBack& state, float turnsPerSecond, float backSeconds, float aheadSeconds)
{
	++state.turns;
	if (state.lookingBack)
	{
		if (state.turns > static_cast<uint32_t>(turnsPerSecond * backSeconds))
		{
			state.lookingBack = false;
			state.turns = 0;
		}
		return true;
	}
	if (state.turns > static_cast<uint32_t>(turnsPerSecond * aheadSeconds))
	{
		state.lookingBack = true;
		state.turns = 0;
	}
	return false;
}

std::vector<Step> creature_emote::StrokeAndHeal(uint32_t thing, bool holdingStrokeable)
{
	std::vector<Step> agenda;
	if (!holdingStrokeable)
	{
		agenda.push_back({.kind = Step::Kind::Object,
		                  .order = {.kind = ObjectOrder::Kind::PickUp, .object = thing},
		                  .gaze = Gaze {.object = thing, .whileGoing = true}});
	}
	agenda.push_back({.kind = Step::Kind::Object,
	                  .effect = Effect::Completed,
	                  .order = {.kind = ObjectOrder::Kind::Keep, .animation = k_StrokeAnimation},
	                  .face = Cue::Compassion,
	                  .gaze = Gaze {.object = thing}});
	agenda.push_back({.kind = Step::Kind::Heal, .object = thing, .face = Cue::Compassion});
	agenda.push_back({.kind = Step::Kind::Object,
	                  .order = {.kind = ObjectOrder::Kind::Discard, .animation = k_PutDownAnimation},
	                  .face = Cue::Compassion});
	return agenda;
}

std::vector<Step> creature_emote::LookAtSky(glm::vec3 point, bool evilAtTheMoon, const Random& random)
{
	const Gaze look {.point = point};
	std::vector<Step> agenda {
	    {.kind = Step::Kind::Move, .movement = {.kind = Movement::Kind::TurnToFace, .point = {point.x, point.z}}, .gaze = look},
	};
	const bool points = random(k_PointAtSkyLots) == 0;
	if (points)
	{
		agenda.push_back({.kind = Step::Kind::Object,
		                  .order = {.kind = ObjectOrder::Kind::PointAt,
		                            .point = {point.x, point.z},
		                            .pointHeight = point.y,
		                            .seconds = k_PointAtSkySeconds},
		                  .gaze = look});
	}
	if (evilAtTheMoon)
	{
		// When it points, the action is complete once it has pointed, before it shows its anger
		if (points)
		{
			agenda.back().effect = Effect::Completed;
		}
		auto angry = PlayOnce(k_AngryAnimation);
		angry.face = Cue::Amazed;
		agenda.push_back(angry);
	}
	return agenda;
}

std::vector<Step> creature_emote::SleepAtClearPlace(glm::vec2 place, float radius, const Random& random)
{
	std::vector<Step> agenda;
	if (random(2) == 0)
	{
		agenda.push_back({.kind = Step::Kind::Action, .animation = animations::k_Tired, .sleepyEyes = true});
	}
	agenda.push_back({.kind = Step::Kind::Move,
	                  .movement = {.kind = Movement::Kind::ToPoint, .point = place, .maxDistance = radius},
	                  .gaze = creature_mind::GoingLook()});
	agenda.push_back({.kind = Step::Kind::Static,
	                  .sequence = {animations::k_StartSleep, animations::k_Sleep, animations::k_EndSleep},
	                  .untilRested = true,
	                  .closedEyes = true,
	                  .effect = Effect::Slept});
	agenda.push_back(
	    {.kind = Step::Kind::Action, .animation = animations::k_Confused, .sleepyEyes = true, .face = Cue::Grimace});
	return agenda;
}

std::vector<Step> creature_emote::SitDownBy(uint32_t thing, glm::vec2 place, float radius, const Random& random)
{
	const float sit = k_SitSeconds + FloatRandom(random, k_SitExtraSeconds);
	return {
	    {.kind = Step::Kind::Move,
	     .movement = {.kind = Movement::Kind::ToPoint, .point = place, .maxDistance = radius},
	     .gaze = creature_mind::GoingLook()},
	    {.kind = Step::Kind::Move,
	     .seconds = k_SitFaceSeconds,
	     .movement = {.kind = Movement::Kind::TurnToFaceObject, .object = thing},
	     .gaze = Gaze {.object = thing}},
	    {.kind = Step::Kind::Move, .movement = {.kind = Movement::Kind::FaceDownSlope}},
	    {.kind = Step::Kind::Static,
	     .seconds = sit,
	     .animation = animations::k_Sit,
	     .sequence = {animations::k_StartSit, animations::k_Sit, animations::k_EndSit}},
	};
}

bool creature_emote::NotTooFarFromHome(std::optional<float> homeDistance)
{
	return homeDistance.has_value() && *homeDistance < k_FurthestToSleepAtHome;
}

bool creature_emote::LeashedOrTooFarFromHome(bool leashed, std::optional<float> homeDistance)
{
	return leashed || !homeDistance.has_value() || *homeDistance > k_NearestToSleepAway;
}

bool creature_emote::PlayerNearbyAndFrightening(std::optional<float> handDistance, float attitudeToPlayer)
{
	return handDistance.has_value() && *handDistance < k_FrighteningHandDistance && attitudeToPlayer < 0.0f;
}

bool creature_emote::DoneWithin(uint32_t turnsSinceDone, uint32_t turnsPerSecond, float seconds)
{
	return turnsSinceDone != 0 && static_cast<float>(turnsSinceDone / turnsPerSecond) < seconds;
}

bool creature_emote::ShowedOffRecently(uint32_t lastShowOffTurn, uint32_t turn)
{
	return lastShowOffTurn != 0 && turn - lastShowOffTurn <= k_ShowOffAgainTurns;
}
