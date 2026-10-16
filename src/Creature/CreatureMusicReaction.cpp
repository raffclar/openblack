/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "CreatureMusicReaction.h"

#include <cmath>

#include <algorithm>

#include "Creature/CreatureEmoteAgendas.h"
#include "Creature/CreatureFace.h"
#include "Creature/CreatureLayers.h"

using namespace openblack;
using namespace openblack::creature_music;
using creature_desires::Desire;
using creature_face::Cue;
using creature_mind::Gaze;
using creature_mind::Movement;
using creature_mind::ObjectOrder;
using creature_mind::Step;

namespace
{
/// Each reaction does a number of rounds from the first number up to the first number and the second less one
constexpr uint32_t k_FewestRounds = 8;
constexpr uint32_t k_ExtraRounds = 8;

/// The clips of the reactions
constexpr size_t k_FirstFaceAnimation = 16;
constexpr size_t k_AmazedAnimation = 218;
constexpr size_t k_YawnAnimation = 63;
constexpr size_t k_StretchAnimation = 60;
constexpr size_t k_HappyAnimation = 55;
constexpr size_t k_LikedAnimation69 = 69;
constexpr size_t k_LikedAnimation72 = 72;
constexpr size_t k_SadAnimation = 56;
constexpr size_t k_DislikedAnimation217 = 217;
constexpr size_t k_FrightenedAnimation = 61;
constexpr size_t k_CryAnimation = 219;
constexpr size_t k_RudeGestureAnimation = 222;
constexpr size_t k_AngryAnimation = 53;
/// The static action of each reaction: its start, loop and end are three clips in a row
constexpr size_t k_StaticAnimation = 38;

/// A facial animation is held this long
constexpr float k_FaceSeconds = 5.0f;
/// The creature points at the camera for this long
constexpr float k_PointSeconds = 1.0f;
/// The static action of a new track lasts from the first number of seconds up to the first and second numbers
constexpr float k_NewStaticSeconds = 20.0f;
constexpr float k_NewStaticExtraSeconds = 15.0f;
/// The static action of a liked track lasts up to this long
constexpr float k_LikedStaticSeconds = 60.0f;
/// The dance after a new track, and the dance after a liked track: ten times the mood and five seconds, at most 60
constexpr float k_NewDanceSeconds = 1.0f;
constexpr float k_DanceMoodSeconds = 10.0f;
constexpr float k_DanceExtraSeconds = 5.0f;
constexpr float k_MostDanceSeconds = 60.0f;

/// Turns to face the camera and looks at it, with the face of the round
Step FaceCamera(const Camera& camera, Cue face)
{
	return {.kind = Step::Kind::Move,
	        .movement = {.kind = Movement::Kind::TurnToFace, .point = glm::vec2(camera.position.x, camera.position.z)},
	        .face = face,
	        .gaze = Gaze {.camera = true}};
}

/// Points at where the camera is for a second, and looks at that point
Step PointAtCamera(const Camera& camera, Cue face)
{
	return {.kind = Step::Kind::Object,
	        .order = {.kind = ObjectOrder::Kind::PointAt,
	                  .point = glm::vec2(camera.position.x, camera.position.z),
	                  .pointHeight = camera.position.y,
	                  .seconds = k_PointSeconds},
	        .face = face,
	        .gaze = Gaze {.point = camera.position}};
}

Step Action(size_t animation, Cue face, std::optional<Gaze> gaze = std::nullopt)
{
	return {.kind = Step::Kind::Action, .animation = animation, .face = face, .gaze = gaze};
}

/// An action played as if just woken up
Step SleepyAction(size_t animation, Cue face)
{
	return {.kind = Step::Kind::Action, .animation = animation, .sleepyEyes = true, .face = face};
}

Step Talk(Cue face, std::optional<Gaze> gaze = std::nullopt)
{
	return {.kind = Step::Kind::Overlay, .animation = creature_layers::animations::k_Talk, .face = face, .gaze = gaze};
}

Step StaticAction(float seconds, Cue face, bool sleepy, std::optional<Gaze> gaze)
{
	return {.kind = Step::Kind::Static,
	        .seconds = seconds,
	        .animation = k_StaticAnimation,
	        .sleepyEyes = sleepy,
	        .sequence = {k_StaticAnimation - 1, k_StaticAnimation, k_StaticAnimation + 1},
	        .face = face,
	        .gaze = gaze};
}

Step Dance(float seconds, Cue face, std::optional<Gaze> gaze)
{
	return {.kind = Step::Kind::PracticeDance, .seconds = seconds, .face = face, .gaze = gaze};
}

/// The facial animation that the main step of a reaction holds
Step MainFace(std::optional<Gaze> gaze)
{
	return {.kind = Step::Kind::Face, .seconds = k_FaceSeconds, .animation = k_FirstFaceAnimation, .gaze = gaze, .main = true};
}

/// On the first round, and one round in two after it, the creature first turns to face the camera
void MaybeFaceCamera(std::vector<Step>& agenda, uint32_t round, const creature_mind::Random& random, const Camera& camera,
                     Cue face)
{
	if ((round == 0 || random(2) == 0) && camera.known)
	{
		agenda.push_back(FaceCamera(camera, face));
	}
}
} // namespace

Reaction creature_music::ReactionTo(float mood)
{
	if (mood == 0.0f)
	{
		return Reaction::New;
	}
	return mood > 0.0f ? Reaction::Liked : Reaction::Disliked;
}

Plan creature_music::PlanFor(float mood, uint32_t roll)
{
	if (mood < 0.0f)
	{
		return {.desire = Desire::Sadness, .action = "BeSad"};
	}
	return {.desire = Desire::IdleWithPlayer, .action = roll != 0 ? "PointAtCamera" : "LookAtCamera"};
}

std::vector<Step> creature_music::NewTrack(const creature_mind::Random& random, float alignment, const Camera& camera)
{
	std::vector<Step> agenda;
	const auto rounds = random(k_ExtraRounds) + k_FewestRounds;
	for (uint32_t round = 0; round < rounds; ++round)
	{
		MaybeFaceCamera(agenda, round, random, camera, Cue::Curiosity);
		switch (random(5))
		{
		case 0:
			if (alignment >= 0.0f)
			{
				agenda.push_back(MainFace(std::nullopt));
			}
			else
			{
				agenda.push_back(Action(k_AmazedAnimation, Cue::Curiosity));
			}
			break;
		case 1:
			if (camera.known)
			{
				agenda.push_back(PointAtCamera(camera, Cue::Curiosity));
			}
			break;
		case 2:
			agenda.push_back(Talk(Cue::Curiosity));
			break;
		case 3:
			agenda.push_back(SleepyAction(k_YawnAnimation, Cue::Curiosity));
			break;
		default:
			agenda.push_back(SleepyAction(random(100) > 50 ? k_YawnAnimation : k_StretchAnimation, Cue::Curiosity));
			break;
		}
	}
	const auto seconds = creature_emote::FloatRandom(random, k_NewStaticExtraSeconds) + k_NewStaticSeconds;
	agenda.push_back(StaticAction(seconds, Cue::Curiosity, true, std::nullopt));
	if (camera.known)
	{
		agenda.push_back(PointAtCamera(camera, Cue::Curiosity));
	}
	if (random(100) > 50)
	{
		agenda.push_back(Dance(k_NewDanceSeconds, Cue::Curiosity, std::nullopt));
	}
	return agenda;
}

std::vector<Step> creature_music::LikedTrack(const creature_mind::Random& random, float mood, const Camera& camera)
{
	constexpr std::array<Cue, 3> k_Faces {Cue::Smile, Cue::Amazed, Cue::Compassion};
	const Gaze atCamera {.camera = true};
	std::vector<Step> agenda;
	const auto rounds = random(k_ExtraRounds) + k_FewestRounds;
	for (uint32_t round = 0; round < rounds; ++round)
	{
		const auto face = k_Faces.at(random(3));
		MaybeFaceCamera(agenda, round, random, camera, face);
		switch (random(12))
		{
		case 0:
		case 1:
		case 3:
		case 4:
			agenda.push_back(Action(k_HappyAnimation, face, atCamera));
			break;
		case 2:
			agenda.push_back(Action(k_LikedAnimation69, face, atCamera));
			break;
		case 5:
		case 6:
		case 7:
			agenda.push_back(Action(k_LikedAnimation72, face, atCamera));
			break;
		case 8:
		{
			auto step = MainFace(atCamera);
			step.face = face;
			agenda.push_back(step);
			break;
		}
		case 9:
		{
			auto step = Action(k_AmazedAnimation, face, atCamera);
			step.main = true;
			agenda.push_back(step);
			break;
		}
		default:
			agenda.push_back(Talk(face, atCamera));
			break;
		}
	}
	agenda.push_back(StaticAction(creature_emote::FloatRandom(random, k_LikedStaticSeconds), Cue::Smile, false, atCamera));
	const auto dance = std::min(mood * k_DanceMoodSeconds + k_DanceExtraSeconds, k_MostDanceSeconds);
	agenda.push_back(Dance(dance, Cue::Smile, atCamera));
	return agenda;
}

std::vector<Step> creature_music::DislikedTrack(const creature_mind::Random& random, float alignment, const Camera& camera)
{
	constexpr std::array<Cue, 3> k_Faces {Cue::Anger, Cue::Fear, Cue::Sad};
	std::vector<Step> agenda;
	const auto rounds = random(k_ExtraRounds) + k_FewestRounds;
	for (uint32_t round = 0; round < rounds; ++round)
	{
		const auto face = k_Faces.at(random(3));
		MaybeFaceCamera(agenda, round, random, camera, face);
		switch (random(9))
		{
		case 0:
		case 1:
			agenda.push_back(Action(k_SadAnimation, face));
			break;
		case 2:
			agenda.push_back(Action(k_DislikedAnimation217, face));
			break;
		case 3:
		case 4:
			agenda.push_back(Action(k_FrightenedAnimation, face));
			break;
		default:
			if (alignment >= 0.0f)
			{
				agenda.push_back(Action(k_CryAnimation, face));
			}
			else
			{
				agenda.push_back(Action(random(100) <= 30 ? k_RudeGestureAnimation : k_AngryAnimation, face));
			}
			break;
		}
	}
	return agenda;
}

int32_t creature_music::FollowUpValue(float mood)
{
	return static_cast<int32_t>(std::trunc(mood * k_DanceMoodSeconds));
}

bool creature_music::ShowsHappyAfterLikedTrack(uint32_t roll)
{
	return roll <= 20;
}

bool creature_music::MayDanceWithVillagers(int32_t value)
{
	return value > 8;
}

Dominant creature_music::DominantAfter(Reaction reaction, uint32_t roll)
{
	constexpr float k_NewSeconds = 15.0f;
	constexpr float k_FeltSeconds = 180.0f;
	switch (reaction)
	{
	case Reaction::New:
		return {.desire = Desire::IdleWithPlayer, .seconds = k_NewSeconds};
	case Reaction::Liked:
		return {.desire = Desire::PlayWithPlayer, .seconds = k_FeltSeconds};
	default:
		return {.desire = roll > 50 ? Desire::Sadness : Desire::Fear, .seconds = k_FeltSeconds};
	}
}
