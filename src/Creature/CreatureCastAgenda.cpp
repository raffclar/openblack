/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "CreatureCastAgenda.h"

#include "Creature/CreatureLayers.h"

using namespace openblack;
using namespace openblack::creature_mind;
namespace animations = openblack::creature_layers::animations;

namespace
{
/// How each kind of casting goes: what it shows first and the faces it pulls, and how near it goes and how far it
/// backs off, for its height
struct Style
{
	size_t emote;
	creature_face::Cue feeling;
	creature_face::Cue facing;
	float goNear;
	float backOff;
};

/// Embarrassed, after a try at a miracle fizzles
constexpr size_t k_Embarrassed = 73;

/// The last steps of every casting: turning to face the object and holding still a moment, drawing the gesture if
/// there is one, and casting from the casting pose
void TurnGestureAndCast(std::vector<Step>& agenda, uint32_t magicType, uint32_t gesture, uint32_t object,
                        creature_face::Cue facing)
{
	agenda.push_back({.kind = Step::Kind::Move,
	                  .seconds = k_CastSettleSeconds,
	                  .movement = {.kind = Movement::Kind::TurnToFaceObject, .object = object},
	                  .face = facing});
	if (gesture != 0)
	{
		agenda.push_back({.kind = Step::Kind::Gesture, .animation = gesture});
	}
	agenda.push_back({.kind = Step::Kind::Cast,
	                  .seconds = k_CastHoldSeconds,
	                  .animation = k_CastPose[1],
	                  .sequence = k_CastPose,
	                  .cast = {.magicType = magicType, .object = object}});
}

Style StyleOf(CastStyle style, float height)
{
	switch (style)
	{
	case CastStyle::Lightning:
		return {.emote = animations::k_Angry,
		        .feeling = creature_face::Cue::Anger,
		        .facing = creature_face::Cue::Anger,
		        .goNear = k_LightningCastDistance,
		        .backOff = height + k_LightningBackOff};
	case CastStyle::Helpful:
		return {.emote = animations::k_FeelingNice,
		        .feeling = creature_face::Cue::Compassion,
		        .facing = creature_face::Cue::Compassion,
		        .goNear = 2.0f * height,
		        .backOff = 2.0f * height};
	case CastStyle::Aggressive:
		return {.emote = animations::k_Angry,
		        .feeling = creature_face::Cue::Anger,
		        .facing = creature_face::Cue::Anger,
		        .goNear = 5.0f * height,
		        .backOff = 2.0f * height};
	case CastStyle::Explosion:
		return {.emote = animations::k_Angry,
		        .feeling = creature_face::Cue::Anger,
		        .facing = creature_face::Cue::Anger,
		        .goNear = k_ExplosionCastDistance,
		        .backOff = 2.0f * height};
	case CastStyle::Playful:
		break;
	}
	// Playful going near, but kind as it turns to face the creature it casts on
	return {.emote = animations::k_FeelPlayful,
	        .feeling = creature_face::Cue::Playfulness,
	        .facing = creature_face::Cue::Compassion,
	        .goNear = 5.0f * height,
	        .backOff = 2.0f * height};
}
} // namespace

std::vector<Step> creature_mind::CastAt(CastStyle style, uint32_t magicType, uint32_t gesture, uint32_t object, float height,
                                        const Random& random)
{
	const auto how = StyleOf(style, height);
	std::vector<Step> agenda;
	if (random(style == CastStyle::Explosion ? k_ExplosionEmoteLots : k_CastEmoteLots) == 0)
	{
		agenda.push_back({.kind = Step::Kind::Action, .animation = how.emote, .face = how.feeling});
	}
	agenda.push_back({.kind = Step::Kind::Move,
	                  .movement = {.kind = Movement::Kind::GoNearObject, .object = object, .maxDistance = how.goNear},
	                  .face = how.feeling});
	agenda.push_back({.kind = Step::Kind::Move,
	                  .movement = {.kind = Movement::Kind::GetAwayFromObject, .object = object, .maxDistance = how.backOff}});
	TurnGestureAndCast(agenda, magicType, gesture, object, how.facing);
	return agenda;
}

std::vector<Step> creature_mind::CastFireballAt(uint32_t magicType, uint32_t gesture, uint32_t object, bool objectLiving,
                                                glm::vec2 clearArea, float height, const Random& random)
{
	std::vector<Step> agenda;
	agenda.push_back({.kind = Step::Kind::Move,
	                  .movement = {.kind = Movement::Kind::ToPoint, .point = clearArea, .maxDistance = 2.0f * height},
	                  .gaze = GoingLook()});
	if (random(k_CastEmoteLots) == 0)
	{
		agenda.push_back({.kind = Step::Kind::Action, .animation = animations::k_Angry, .face = creature_face::Cue::Anger});
	}
	if (objectLiving)
	{
		agenda.push_back(
		    {.kind = Step::Kind::Move,
		     .movement = {.kind = Movement::Kind::GetAwayFromObject, .object = object, .maxDistance = 2.0f * height}});
	}
	TurnGestureAndCast(agenda, magicType, gesture, object, creature_face::Cue::Anger);
	return agenda;
}

creature_mind::FizzleReaction creature_mind::ReactToFizzle(const Random& random, float chance)
{
	FizzleReaction reaction {.agenda = {{.kind = Step::Kind::Action, .animation = k_Embarrassed}}};
	if (random(2) == 0)
	{
		reaction.agenda.push_back({.kind = Step::Kind::Action, .animation = animations::k_Sad});
		reaction.sadness = chance * k_SadnessFromFizzle;
	}
	return reaction;
}
