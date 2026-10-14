/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "CreaturePlayerAgendas.h"

#include "Creature/CreaturePlayerAttention.h"

using namespace openblack;
using namespace openblack::creature_player_attention;
using creature_mind::Gaze;
using creature_mind::Movement;
using creature_mind::ObjectOrder;
using creature_mind::Step;

namespace
{
/// Turning to face the camera, looking at it
Step FaceCamera(glm::vec2 camera)
{
	return {.kind = Step::Kind::Move,
	        .movement = {.kind = Movement::Kind::TurnToFace, .point = camera},
	        .gaze = Gaze {.camera = true}};
}
} // namespace

std::vector<Step> creature_player_attention::GoToHand(const creature_mind::Random& random)
{
	const Gaze atHand {.hand = true};
	std::vector<Step> agenda {{.kind = Step::Kind::FollowHand, .seconds = k_FollowHandSeconds, .gaze = atHand}};
	if (random(k_HappyAtHandLots) == 0)
	{
		agenda.push_back({.kind = Step::Kind::Action, .animation = k_HappyAnimation, .gaze = atHand});
	}
	return agenda;
}

std::vector<Step> creature_player_attention::PointAtHand(glm::vec2 camera, glm::vec3 hand)
{
	return {
	    FaceCamera(camera),
	    {.kind = Step::Kind::Object,
	     .order = {.kind = ObjectOrder::Kind::PointAt,
	               .point = {hand.x, hand.z},
	               .pointHeight = hand.y,
	               .seconds = k_PointAtHandSeconds},
	     .face = creature_face::Cue::Amazed,
	     .gaze = Gaze {.point = hand}},
	};
}

std::vector<Step> creature_player_attention::ShowHowNice(glm::vec2 camera, float attitudeToPlayer)
{
	const auto animation = attitudeToPlayer > 0.0f ? k_FriendlyWaveAnimation : k_SadAnimation;
	return {FaceCamera(camera), {.kind = Step::Kind::Action, .animation = animation, .gaze = Gaze {.camera = true}}};
}

std::vector<Step> creature_player_attention::PrayToPlayer(glm::vec2 camera)
{
	return {FaceCamera(camera), {.kind = Step::Kind::Action, .animation = k_PrayAnimation}};
}

std::vector<Step> creature_player_attention::ThrowAtCamera(std::optional<uint32_t> pickUp, glm::vec3 camera)
{
	std::vector<Step> agenda;
	if (pickUp.has_value())
	{
		agenda.push_back({.kind = Step::Kind::Object,
		                  .order = {.kind = ObjectOrder::Kind::PickUp, .object = pickUp},
		                  .gaze = Gaze {.object = pickUp}});
	}
	agenda.push_back({.kind = Step::Kind::Object,
	                  .order = {.kind = ObjectOrder::Kind::Throw, .point = {camera.x, camera.z}, .pointHeight = camera.y},
	                  .gaze = Gaze {.camera = true}});
	return agenda;
}

std::vector<Step> creature_player_attention::WatchPlayer()
{
	return {{.kind = Step::Kind::WatchPlayer, .gaze = Gaze {.player = true}}};
}
