/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "CreatureKindAgendas.h"

#include <algorithm>

#include "Creature/CreatureLayers.h"
#include "Creature/CreatureObjectActions.h"
#include "Creature/CreatureThrow.h"

using namespace openblack;
using namespace openblack::creature_mind;

namespace
{
Step PickUp(uint32_t thing)
{
	return {.kind = Step::Kind::Object, .order = {.kind = ObjectOrder::Kind::PickUp, .object = thing}};
}
} // namespace

std::vector<Step> creature_mind::GiveWoodToStore(const std::optional<TreeToTake>& take, uint32_t store, float radius,
                                                 float height)
{
	std::vector<Step> agenda;
	if (take.has_value())
	{
		agenda.push_back({.kind = Step::Kind::Move,
		                  .movement = {.kind = Movement::Kind::ToPoint,
		                               .point = take->point,
		                               .maxDistance = (take->radius + radius) * k_TreeReach}});
		agenda.push_back(PickUp(take->tree));
	}
	agenda.push_back({.kind = Step::Kind::Move,
	                  .movement = {.kind = Movement::Kind::ToThrowPosition, .object = store, .maxDistance = height},
	                  .face = creature_face::Cue::Compassion});
	agenda.push_back({.kind = Step::Kind::Move,
	                  .seconds = k_FaceStoreForWoodSeconds,
	                  .movement = {.kind = Movement::Kind::TurnToFaceObject, .object = store},
	                  .effect = Effect::Completed,
	                  .face = creature_face::Cue::Compassion});
	agenda.push_back({.kind = Step::Kind::Object, .order = {.kind = ObjectOrder::Kind::ThrowInStore, .object = store}});
	return agenda;
}

std::vector<Step> creature_mind::PracticeThrow(std::optional<uint32_t> pickUp, glm::vec2 from, float height)
{
	std::vector<Step> agenda;
	if (pickUp.has_value())
	{
		agenda.push_back(PickUp(*pickUp));
	}
	agenda.push_back({.kind = Step::Kind::Object,
	                  .order = {.kind = ObjectOrder::Kind::Throw, .point = from + glm::vec2(k_PracticeThrowHeights * height)}});
	return agenda;
}

std::vector<Step> creature_mind::ThrowToyAt(uint32_t toy, uint32_t target, glm::vec2 targetPoint, float height,
                                            const Random& random)
{
	const bool happyBefore = random(k_HappyAroundToyThrowLots) == 0;
	const bool happyAfter = random(k_HappyAroundToyThrowLots) == 0;
	std::vector<Step> agenda;
	if (happyBefore)
	{
		agenda.push_back({.kind = Step::Kind::Action, .animation = creature_layers::animations::k_Happy});
	}
	agenda.push_back(PickUp(toy));
	const Gaze watch {.object = target};
	agenda.push_back({.kind = Step::Kind::Move,
	                  .movement = {.kind = Movement::Kind::ToThrowPosition, .object = target, .maxDistance = height},
	                  .face = creature_face::Cue::Anger,
	                  .gaze = watch});
	agenda.push_back({.kind = Step::Kind::Object,
	                  .order = {.kind = ObjectOrder::Kind::Throw, .object = target, .point = targetPoint},
	                  .face = creature_face::Cue::Anger,
	                  .gaze = watch});
	if (happyAfter)
	{
		agenda.push_back({.kind = Step::Kind::Action, .animation = creature_layers::animations::k_Happy});
	}
	return agenda;
}

std::vector<Step> creature_mind::BreakRock(std::optional<uint32_t> pickUp, uint32_t rock, glm::vec2 from, float height)
{
	auto agenda = PracticeThrow(pickUp, from, height);
	agenda.push_back({.kind = Step::Kind::Shatter, .object = rock});
	return agenda;
}

std::vector<Step> creature_mind::StrokeToy(uint32_t toy)
{
	using creature_face::Cue;
	auto pickUp = PickUp(toy);
	pickUp.face = Cue::Compassion;
	return {pickUp,
	        {.kind = Step::Kind::Object,
	         .order = {.kind = ObjectOrder::Kind::Keep, .animation = creature_object_actions::k_StrokeObject},
	         .face = Cue::Compassion,
	         .gaze = Gaze {.object = toy}}};
}

std::vector<Step> creature_mind::TakeToyHome(uint32_t toy, glm::vec2 home, float height, const Random& random,
                                             const std::function<float()>& randomFraction)
{
	using creature_face::Cue;
	auto pickUp = PickUp(toy);
	pickUp.face = Cue::Compassion;
	// Somewhere about home, five either way along each axis
	const float alongX = (randomFraction() * k_ToyHomeSpread) - (k_ToyHomeSpread * 0.5f);
	const float alongZ = (randomFraction() * k_ToyHomeSpread) - (k_ToyHomeSpread * 0.5f);
	const auto drop = random(k_ToyPutDownLots) >= k_ToyTossedBelow ? creature_throw::k_PutDown : creature_throw::k_Discard;
	return {pickUp,
	        {.kind = Step::Kind::Move,
	         .movement = {.kind = Movement::Kind::ToPoint,
	                      .point = home + glm::vec2(alongX, alongZ),
	                      .maxDistance = std::min(height, k_ToyHomeNearest)},
	         .face = Cue::Compassion},
	        {.kind = Step::Kind::Object,
	         .order = {.kind = ObjectOrder::Kind::Discard, .animation = drop},
	         .face = Cue::Compassion}};
}

std::vector<Step> creature_mind::ThrowDie(std::optional<uint32_t> pickUp, uint32_t die, glm::vec2 from, float height,
                                          const Random& random)
{
	std::vector<Step> agenda;
	if (pickUp.has_value())
	{
		agenda.push_back(PickUp(*pickUp));
	}
	agenda.push_back({.kind = Step::Kind::Object,
	                  .order = {.kind = ObjectOrder::Kind::Throw, .point = from + glm::vec2(k_DieThrowHeights * height)}});
	agenda.push_back(
	    {.kind = Step::Kind::WatchFlying, .object = die, .face = creature_face::Cue::Amazed, .gaze = Gaze {.object = die}});
	const bool happy = random(k_DieOutcomeLots) > 0;
	agenda.push_back({.kind = Step::Kind::Action,
	                  .animation = happy ? creature_layers::animations::k_Happy : creature_layers::animations::k_Sad});
	return agenda;
}

std::vector<Step> creature_mind::ThrowToPlayer(uint32_t toy, glm::vec3 hand, bool handFull)
{
	std::vector<Step> agenda {PickUp(toy)};
	if (!handFull)
	{
		agenda.push_back({.kind = Step::Kind::Object,
		                  .order = {.kind = ObjectOrder::Kind::Throw, .point = {hand.x, hand.z}, .pointHeight = hand.y},
		                  .gaze = Gaze {.point = hand}});
	}
	agenda.push_back({.kind = Step::Kind::Action, .animation = creature_layers::animations::k_Happy});
	return agenda;
}

std::vector<Step> creature_mind::HelpRepairHouse(std::optional<uint32_t> pickUp, uint32_t house, uint32_t tree)
{
	std::vector<Step> agenda;
	if (pickUp.has_value())
	{
		agenda.push_back(PickUp(*pickUp));
	}
	agenda.push_back(
	    {.kind = Step::Kind::Move, .movement = {.kind = Movement::Kind::GoNearObject, .object = house, .maxDistance = 0.0f}});
	agenda.push_back({.kind = Step::Kind::Move,
	                  .seconds = k_FaceHouseSeconds,
	                  .movement = {.kind = Movement::Kind::TurnToFaceObject, .object = house},
	                  .gaze = Gaze {.object = house}});
	agenda.push_back(
	    {.kind = Step::Kind::Object, .order = {.kind = ObjectOrder::Kind::Discard, .animation = creature_throw::k_PutDown}});
	agenda.push_back({.kind = Step::Kind::Repair, .object = house, .gaze = Gaze {.object = house}});
	agenda.push_back({.kind = Step::Kind::UseUp, .object = tree});
	return agenda;
}

std::vector<Step> creature_mind::KickBallAround(uint32_t ball, const Random& random)
{
	const Gaze watch {.object = ball};
	const Step goNear {.kind = Step::Kind::Move,
	                   .movement = {.kind = Movement::Kind::GoNearObject, .object = ball, .maxDistance = 0.0f},
	                   .gaze = watch};
	std::vector<Step> agenda;
	uint32_t kicks = 0;
	// The count to reach is drawn afresh after every kick
	do
	{
		agenda.push_back(goNear);
		agenda.push_back({.kind = Step::Kind::Move,
		                  .seconds = k_FaceBallSeconds,
		                  .movement = {.kind = Movement::Kind::TurnToFaceObject, .object = ball},
		                  .gaze = watch});
		agenda.push_back(goNear);
		agenda.push_back(
		    {.kind = Step::Kind::Object, .order = {.kind = ObjectOrder::Kind::Kick, .object = ball}, .gaze = watch});
		if (random(k_PointAfterKickLots) == 0)
		{
			agenda.push_back({.kind = Step::Kind::Object,
			                  .order = {.kind = ObjectOrder::Kind::PointAt, .object = ball, .seconds = k_PointAtBallSeconds},
			                  .gaze = watch});
		}
		else
		{
			const auto turns = k_WaitAfterKickTurns + random(k_WaitAfterKickMoreTurns);
			agenda.push_back({.kind = Step::Kind::Wait, .seconds = static_cast<float>(turns) * k_KickTurnSeconds});
		}
		++kicks;
	} while (kicks < k_LeastKicks + random(k_MoreKicksLots));
	if (random(k_HappyAfterKicksLots) == 0)
	{
		agenda.push_back({.kind = Step::Kind::Action, .animation = creature_layers::animations::k_Happy});
	}
	return agenda;
}

std::vector<Step> creature_mind::GetHigh(std::optional<uint32_t> pickUp, uint32_t eaten)
{
	std::vector<Step> agenda;
	if (pickUp.has_value())
	{
		agenda.push_back(PickUp(*pickUp));
	}
	agenda.push_back(
	    {.kind = Step::Kind::Object, .effect = Effect::Eat, .order = {.kind = ObjectOrder::Kind::Eat, .object = eaten}});
	return agenda;
}

std::vector<Step> creature_mind::BecomeHigh(const std::function<std::optional<HighSeat>()>& findSeat, const Random& random,
                                            const std::function<float(float)>& randomFloat)
{
	using creature_face::Cue;
	const Gaze camera {.camera = true};
	const auto action = [](size_t animation, Cue face = Cue::None) {
		return Step {.kind = Step::Kind::Action, .animation = animation, .face = face, .heldEyes = Eyes::Stoned};
	};
	std::vector<Step> agenda;
	agenda.push_back({.kind = Step::Kind::WatchCamera, .seconds = k_HighWatchSeconds, .face = Cue::Amazed, .gaze = camera});
	// TODO(creature-kind-actions): while talking the game also flutters its eyelids looking at itself, which no
	// creature here does yet
	agenda.push_back({.kind = Step::Kind::Overlay, .animation = k_Talk});
	agenda.push_back(
	    {.kind = Step::Kind::WatchCamera, .seconds = k_HighStaredSeconds, .face = Cue::Smile, .heldEyes = Eyes::Stoned});
	agenda.push_back({.kind = Step::Kind::Wait, .seconds = k_HighWaitSeconds, .face = Cue::Smile, .gaze = camera});
	agenda.push_back(action(k_HighDazed));
	if (random(k_HighChanceLots) == 0)
	{
		agenda.push_back(action(creature_layers::animations::k_Happy));
	}
	agenda.push_back(action(k_HighConfused, Cue::Amazed));
	agenda.push_back({.kind = Step::Kind::Wait, .seconds = k_HighPauseSeconds, .face = Cue::Smile, .gaze = camera});
	if (random(k_HighChanceLots) == 0)
	{
		agenda.push_back(action(k_HighDazedAgain, Cue::Amazed));
	}
	agenda.push_back(action(k_HighConfused, Cue::Amazed));
	if (random(k_HighChanceLots) == 0)
	{
		if (const auto seat = findSeat())
		{
			agenda.push_back(
			    {.kind = Step::Kind::Move,
			     .movement = {.kind = Movement::Kind::ToPoint, .point = seat->place, .maxDistance = seat->radius}});
			agenda.push_back({.kind = Step::Kind::Static,
			                  .seconds = k_HighSitSeconds + randomFloat(k_HighSitExtraSeconds),
			                  .sequence = {creature_layers::animations::k_StartSit, creature_layers::animations::k_Sit,
			                               creature_layers::animations::k_EndSit},
			                  .mirrorAtRandom = true,
			                  .face = Cue::Sad,
			                  .heldEyes = Eyes::Stoned});
		}
		agenda.push_back(action(k_HighSitDazed));
		agenda.push_back(
		    {.kind = Step::Kind::Action, .animation = k_HighFrightened, .face = Cue::Frightened, .heldEyes = Eyes::Wide});
		agenda.push_back(
		    {.kind = Step::Kind::Action, .animation = random(k_HighChanceLots) == 0 ? k_HighSitDazed : k_HighLastDazed});
	}
	agenda.push_back({.kind = Step::Kind::WatchCamera, .seconds = k_HighWatchSeconds, .face = Cue::Amazed, .gaze = camera});
	agenda.push_back({.kind = Step::Kind::Overlay, .animation = k_Talk, .heldEyes = Eyes::Stoned});
	agenda.push_back(action(k_HighDazed));
	return agenda;
}

std::vector<Step> creature_mind::PlantTreeBy(std::optional<uint32_t> tree, uint32_t by)
{
	std::vector<Step> agenda;
	if (tree.has_value())
	{
		agenda.push_back(PickUp(*tree));
	}
	agenda.push_back(
	    {.kind = Step::Kind::Move, .movement = {.kind = Movement::Kind::GoNearObject, .object = by, .maxDistance = 0.0f}});
	agenda.push_back({.kind = Step::Kind::Move, .movement = {.kind = Movement::Kind::TurnToFaceObject, .object = by}});
	agenda.push_back(
	    {.kind = Step::Kind::Object, .order = {.kind = ObjectOrder::Kind::Discard, .animation = creature_throw::k_PutDown}});
	return agenda;
}
