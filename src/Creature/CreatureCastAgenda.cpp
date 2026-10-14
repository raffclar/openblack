/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "CreatureCastAgenda.h"

#include <algorithm>

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

/// The step in which the creature turns to face the object and holds still a moment
Step TurnToFace(uint32_t object)
{
	return {.kind = Step::Kind::Move,
	        .seconds = k_CastSettleSeconds,
	        .movement = {.kind = Movement::Kind::TurnToFaceObject, .object = object}};
}

/// The step in which the creature casts a miracle at the object from a pose and holds it for a time
Step CastFromPose(uint32_t magicType, uint32_t object, const std::array<size_t, 3>& pose, float seconds)
{
	return {.kind = Step::Kind::Cast,
	        .seconds = seconds,
	        .animation = pose[1],
	        .sequence = pose,
	        .cast = {.magicType = magicType, .object = object}};
}

/// The step in which the creature points at the object for a short time, with an amazed face
Step PointAt(uint32_t object)
{
	return {.kind = Step::Kind::Object,
	        .order = {.kind = ObjectOrder::Kind::PointAt, .object = object, .seconds = k_ImpressPointSeconds},
	        .face = creature_face::Cue::Amazed,
	        .gaze = Gaze {.object = object}};
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

std::vector<Step> creature_mind::CastFoodAt(uint32_t magicType, uint32_t gesture, uint32_t object)
{
	std::vector<Step> agenda;
	agenda.push_back({.kind = Step::Kind::Move,
	                  .movement = {.kind = Movement::Kind::GoNearObject, .object = object, .maxDistance = k_FoodCastDistance}});
	agenda.push_back(TurnToFace(object));
	if (gesture != 0)
	{
		agenda.push_back({.kind = Step::Kind::Gesture, .animation = gesture});
	}
	agenda.push_back(CastFromPose(magicType, object, k_ScatterPose, k_CastHoldSeconds));
	return agenda;
}

std::vector<Step> creature_mind::CastShieldAt(uint32_t magicType, uint32_t object)
{
	return {TurnToFace(object), CastFromPose(magicType, object, k_CastPose, k_CastHoldSeconds)};
}

std::vector<Step> creature_mind::CastWaterAt(uint32_t magicType, uint32_t object, float height)
{
	return {
	    {.kind = Step::Kind::Move,
	     .movement = {.kind = Movement::Kind::GoNearObject, .object = object, .maxDistance = 2.0f * height}},
	    TurnToFace(object),
	    CastFromPose(magicType, object, k_ScatterPose, k_CastHoldSeconds),
	};
}

std::vector<Step> creature_mind::CastWaterOnItself(uint32_t magicType, uint32_t self, glm::vec2 clearArea, float height)
{
	return {
	    {.kind = Step::Kind::Move,
	     .movement = {.kind = Movement::Kind::ToPoint, .point = clearArea, .maxDistance = 2.0f * height},
	     .gaze = GoingLook()},
	    CastFromPose(magicType, self, k_ScatterPose, k_CastHoldSeconds),
	    {.kind = Step::Kind::CoolDown},
	};
}

std::vector<Step> creature_mind::CastForestAt(uint32_t magicType, uint32_t object, glm::vec3 place, float height)
{
	auto cast = CastFromPose(magicType, object, k_ScatterPose, k_ForestHoldSeconds);
	cast.cast = {.magicType = magicType, .place = place};
	return {
	    {.kind = Step::Kind::Move,
	     .movement = {.kind = Movement::Kind::GoNearObject, .object = object, .maxDistance = k_ForestCastHeights * height},
	     .gaze = GoingLook()},
	    TurnToFace(object),
	    cast,
	};
}

std::vector<Step> creature_mind::CastToImpress(std::optional<uint32_t> magicType, uint32_t object, float height,
                                               const Random& random)
{
	std::vector<Step> agenda;
	agenda.push_back(
	    {.kind = Step::Kind::Move,
	     .movement = {.kind = Movement::Kind::GoNearObject, .object = object, .maxDistance = k_ImpressCastHeights * height}});
	agenda.push_back({.kind = Step::Kind::Move,
	                  .movement = {.kind = Movement::Kind::GetAwayFromObject, .object = object, .maxDistance = 2.0f * height}});
	const bool pointedFirst = random(k_ImpressPointLots) == 0;
	if (pointedFirst)
	{
		agenda.push_back(PointAt(object));
	}
	if (random(k_ImpressBeckonLots) != 0)
	{
		agenda.push_back({.kind = Step::Kind::Action, .animation = animations::k_Summon});
	}
	agenda.push_back(TurnToFace(object));
	if (magicType.has_value())
	{
		agenda.push_back(CastFromPose(*magicType, object, k_CastPose, k_CastHoldSeconds));
	}
	if (!pointedFirst)
	{
		agenda.push_back(PointAt(object));
	}
	return agenda;
}

std::optional<size_t> creature_mind::PickImpressiveMiracle(const std::array<bool, 4>& known,
                                                           const std::array<uint32_t, 4>& castTurn)
{
	std::optional<size_t> picked;
	for (size_t i = 0; i < known.size(); ++i)
	{
		// Of equal turns, the last miracle in the list wins
		if (known.at(i) && (!picked.has_value() || !(castTurn.at(i) > castTurn.at(*picked))))
		{
			picked = i;
		}
	}
	return picked;
}

bool creature_mind::CanCastToImpress(std::span<const ImpressiveMiracleState> miracles)
{
	return std::ranges::any_of(miracles, [](const ImpressiveMiracleState& miracle) {
		const float share = miracle.seen / miracle.needed;
		return miracle.known && miracle.canPay && share > 0.5f;
	});
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

std::vector<Step> creature_mind::CastOneShotAt(const OneShotCast& cast)
{
	std::vector<Step> agenda;
	if (cast.pickUp.has_value())
	{
		agenda.push_back({.kind = Step::Kind::Object, .order = {.kind = ObjectOrder::Kind::PickUp, .object = *cast.pickUp}});
	}
	agenda.push_back(
	    {.kind = Step::Kind::Move,
	     .movement = {.kind = Movement::Kind::GoNearObject, .object = cast.target, .maxDistance = k_OneShotCastDistance}});
	if (cast.getAway)
	{
		agenda.push_back({.kind = Step::Kind::Move,
		                  .movement = {.kind = Movement::Kind::GetAwayFromObject,
		                               .object = cast.target,
		                               .maxDistance = 2.0f * cast.height}});
	}
	agenda.push_back(TurnToFace(cast.target));
	agenda.push_back({.kind = Step::Kind::UseUpHeld});
	agenda.push_back(CastFromPose(cast.magicType, cast.target, k_CastPose, k_CastHoldSeconds));
	if (cast.used.has_value())
	{
		agenda.push_back({.kind = Step::Kind::UseUp, .object = *cast.used});
	}
	return agenda;
}

creature_mind::FizzleRecord creature_mind::ForgetGoneThing(const FizzleRecord& record, uint32_t gone)
{
	return record.lastThing == gone ? FizzleRecord {} : record;
}
