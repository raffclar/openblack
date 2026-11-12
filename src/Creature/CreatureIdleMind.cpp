/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "CreatureIdleMind.h"

#include <cmath>

#include <algorithm>
#include <iterator>
#include <numbers>

#include <glm/geometric.hpp>

#include "Common/GameRandom.h"
#include "Common/PortableMath.h"
#include "Common/X87Vector.h"
#include "Creature/CreatureDance.h"
#include "Creature/CreatureEmoteAgendas.h"
#include "Creature/CreatureFight.h"
#include "Creature/CreatureLayers.h"
#include "Creature/CreatureObjectActions.h"
#include "Creature/CreatureThrow.h"
#include "Creature/CreatureTownHelp.h"

using namespace openblack;
using namespace openblack::creature_mind;
namespace animations = openblack::creature_layers::animations;

namespace
{
/// A face is due again once its wait is within this much of over, so that the turns' rounding doesn't delay it
constexpr float k_FaceDue = 1e-3f;

/// The movements that the creature's movement says are done or have failed
bool IsSubMove(Movement::Kind kind)
{
	return kind == Movement::Kind::GoNearObject || kind == Movement::Kind::GetAwayFromObject ||
	       kind == Movement::Kind::TurnToFaceObject || kind == Movement::Kind::TurnToFaceAwayFromObject ||
	       kind == Movement::Kind::ToThrowPosition || kind == Movement::Kind::ToThrowAtPoint ||
	       kind == Movement::Kind::TurnToFaceCamera || kind == Movement::Kind::TurnToFaceGiverCamera;
}

/// A sleep and a faint close the eyes only when the body first lies still in the loop
bool EyesShutInLoop(const Step& step)
{
	return step.untilRested || step.faint;
}

/// Whether a step reference names the current step of the current agenda
bool IsStep(const std::optional<IdleMind::StepRef>& ref, const IdleMind& mind)
{
	return ref.has_value() && ref->serial == mind.serial && ref->step == mind.step;
}

/// Whether the last stage of a kind of step has no code, after a stage with code. The agenda stays at such a step until
/// the next turn goes past the empty stage.
bool EndStageEmpty(const Step& step)
{
	const auto stages = creature_sub_states::StagesOf(SubStateOf(step));
	return !stages.end && (stages.start || stages.turn);
}

/// The agenda moves on to its next step
void MoveOn(IdleMind& mind)
{
	mind.movedOn = mind.step;
	++mind.step;
	mind.stepStarted = false;
	mind.stepSeconds = 0.0f;
	mind.sitEnding = false;
	mind.faintLay = false;
	mind.faintTurns = 0;
}

/// The step did its work. A last stage with no code holds the step for one more turn:
/// - the head keeps the look of the step;
/// - when it is the last step, the agenda ends on the next turn of thought.
void FinishStep(IdleMind& mind)
{
	const bool emptyEnd = mind.step < mind.agenda.size() && EndStageEmpty(mind.agenda[mind.step]);
	if (emptyEnd && creature_sub_states::LookHoldsAfterEnd(SubStateOf(mind.agenda[mind.step])))
	{
		mind.endedLook = IdleMind::EndedLook {.serial = mind.serial, .gaze = LookOf(mind.agenda[mind.step])};
	}
	MoveOn(mind);
	if (mind.step < mind.agenda.size())
	{
		return;
	}
	if (emptyEnd)
	{
		mind.endsNextTurn = mind.serial;
	}
	else
	{
		// With its last step done, it stops what it does
		mind.eyesToClear = true;
	}
}

/// The step is not done, as its work is done already. The agenda moves on at once. When it is the last step, the plan
/// ends, but it does not see to its desire.
void SkipStep(IdleMind& mind)
{
	MoveOn(mind);
	if (mind.step >= mind.agenda.size())
	{
		mind.lastStepSkipped = mind.serial;
		mind.eyesToClear = true;
	}
}

/// One turn of a wait step. The count stops while the body does an action, a move or a turn included. The body of a
/// creature in the game also counts while it lies after a faint, but in openblack no wait step comes after a faint.
void CountWaitTurn(IdleMind& mind, const Step& step, const Senses& senses)
{
	if (senses.bodyBusy)
	{
		return;
	}
	++mind.watchTurns;
	if (creature_mind::WaitDone(mind.watchTurns, creature_mind::WaitTurns(step.seconds)))
	{
		FinishStep(mind);
	}
}

/// One turn of watching the camera, the turn it starts on counting as the first: it turns to the camera when it is more
/// than a quarter turn away, and is done once whole seconds of turns more than the step's seconds have gone by, or the
/// camera is right overhead or out of sight, or the player's hand has stayed in one place for more turns than the step's
/// seconds give
void WatchCamera(IdleMind& mind, const Step& step, const Senses& senses, Commands& commands)
{
	if (step.turns.has_value())
	{
		// Watching for a set number of turns: only having no player ends it early
		if (!senses.camera.has_value() || !(senses.seconds > 0.0f))
		{
			FinishStep(mind);
			return;
		}
		if (StartsWatchTurn(*senses.camera, senses.bodyBusy))
		{
			commands.move = Movement {.kind = Movement::Kind::TurnToFace, .point = senses.camera->point};
		}
		const auto turns = std::lround(mind.stepSeconds / senses.seconds) + 1;
		if (turns > static_cast<long>(*step.turns))
		{
			FinishStep(mind);
		}
		return;
	}
	if (!senses.camera.has_value() || senses.camera->groundDistance < k_WatchCameraOverhead || !(senses.seconds > 0.0f))
	{
		FinishStep(mind);
		return;
	}
	if (StartsWatchTurn(*senses.camera, senses.bodyBusy))
	{
		commands.move = Movement {.kind = Movement::Kind::TurnToFace, .point = senses.camera->point};
	}
	const auto turnsPerSecond = std::lround(1.0f / senses.seconds);
	const auto turns = std::lround(mind.stepSeconds / senses.seconds) + 1;
	const auto stillLimit = turnsPerSecond * static_cast<long>(step.seconds);
	if (turns / turnsPerSecond > static_cast<long>(step.seconds) || senses.camera->distance > k_WatchCameraFurthest ||
	    static_cast<long>(senses.camera->handStillTurns) > stillLimit)
	{
		FinishStep(mind);
	}
}

/// A start, loop and end held for some seconds
Step Static(std::array<size_t, 3> sequence, float seconds)
{
	return {.kind = Step::Kind::Static,
	        .seconds = seconds,
	        .animation = sequence[1],
	        .sleepyEyes = false,
	        .movement = {},
	        .sequence = sequence};
}

Step Action(size_t animation, bool sleepyEyes)
{
	return {.kind = Step::Kind::Action, .seconds = 0.0f, .animation = animation, .sleepyEyes = sleepyEyes};
}

/// Every step of an agenda pulls a face for what the creature does or feels, except while its eyes are closed
std::vector<Step> WithFace(std::vector<Step> agenda, creature_face::Cue cue)
{
	for (auto& step : agenda)
	{
		if (!step.closedEyes && step.face == creature_face::Cue::None)
		{
			step.face = cue;
		}
	}
	return agenda;
}

Step Object(ObjectOrder order, Effect effect = Effect::None)
{
	Step step {.kind = Step::Kind::Object};
	step.order = order;
	step.effect = effect;
	return step;
}

Step PickUp(uint32_t object)
{
	return Object({.kind = ObjectOrder::Kind::PickUp, .object = object});
}

/// Picking a thing up. Its head looks at the thing as it walks to it.
Step PickUpLooking(uint32_t object)
{
	auto pickUp = PickUp(object);
	pickUp.gaze = Gaze {.object = object, .whileGoing = true};
	return pickUp;
}

/// The side that a step plays on as it starts: the right side or, one time in two, the left side. The game's random
/// numbers choose it.
bool PickSide(const Random& gameRandom)
{
	return gameRandom(2) == 1;
}

/// The side of an action that the body plays once, as it starts. While the hand holds a thing, the free hand does the
/// action and no draw occurs: a thing in the left hand gives the right side. Else the draw chooses the side.
bool PickIndividualSide(const Random& gameRandom, const Senses& senses)
{
	if (senses.held.has_value())
	{
		return !senses.heldLeft;
	}
	return PickSide(gameRandom);
}

/// The side of a static action or a cast pose as it starts. The draw occurs always, but when a script set real time on
/// the body, the action starts on the right side.
bool PickStaticSide(const Random& gameRandom, bool realTime)
{
	return PickSide(gameRandom) && !realTime;
}

/// Whether the step is a faint. A faint starts at once, also while the body plays an action or moves.
bool IsFaint(const Step& step)
{
	return step.kind == Step::Kind::Static && step.holdLoop;
}
} // namespace

std::string_view creature_mind::Name(Activity activity)
{
	switch (activity)
	{
	case Activity::BeIdle:
		return "Being idle";
	case Activity::Sit:
		return "Sitting";
	case Activity::ShowDesire:
		return "Showing a desire";
	case Activity::HangAround:
		return "Hanging around";
	case Activity::Told:
		return "Doing as told";
	case Activity::Eat:
		return "Eating";
	case Activity::Drink:
		return "Drinking";
	case Activity::Sleep:
		return "Sleeping";
	case Activity::Poo:
		return "Having a poo";
	case Activity::Puke:
		return "Being sick";
	case Activity::Faint:
		return "Out cold";
	case Activity::Examine:
		return "Looking something over";
	case Activity::PlayWithObject:
		return "Throwing something about";
	case Activity::Hurl:
		return "Hurling something";
	case Activity::PutDown:
		return "Putting something down";
	case Activity::Planned:
		return "Acting on a desire";
	case Activity::None:
	default:
		return "Nothing";
	}
}

bool creature_mind::SeesToNeedAtStep(Effect effect, bool planned)
{
	if (planned)
	{
		return false;
	}
	switch (effect)
	{
	case Effect::Eat:
	case Effect::Drink:
	case Effect::Poo:
	case Effect::Fart:
	case Effect::Puke:
	case Effect::Slept:
	case Effect::Examined:
	case Effect::ThrewAbout:
	case Effect::Hurled:
		return true;
	case Effect::None:
	case Effect::CameRound:
	case Effect::Completed:
	case Effect::Observed:
	case Effect::TookFromPile:
	case Effect::TookFromHand:
	case Effect::Inspected:
	case Effect::Stomped:
	case Effect::Revived:
	case Effect::RanAway:
		break;
	}
	return false;
}

std::vector<Step> creature_mind::BeIdle(const Random& random)
{
	std::vector<Step> agenda;
	for (int i = 0; i < k_IdleRepeats; ++i)
	{
		Step wait {.kind = Step::Kind::Wait, .seconds = FloatRand(random, 1.0f) + k_IdleWaitSeconds};
		wait.face = creature_face::Cue::Idle;
		agenda.push_back(wait);
		auto yawn = Action(animations::k_Tired, true);
		yawn.face = creature_face::Cue::Idle;
		agenda.push_back(yawn);
	}
	return agenda;
}

Step creature_mind::SitDown(const std::function<float(float)>& upTo)
{
	// The game adds the ten seconds to the draw
	auto sit =
	    Static({animations::k_StartSit, animations::k_Sit, animations::k_EndSit}, upTo(k_SitExtraSeconds) + k_SitSeconds);
	sit.face = creature_face::Cue::Idle;
	return sit;
}

std::vector<Step> creature_mind::Sleep(const Random& random)
{
	std::vector<Step> agenda;
	if (random(k_YawnBeforeSleepLots) == 0)
	{
		agenda.push_back(Action(animations::k_Tired, true));
	}
	auto sleep = Static({animations::k_StartSleep, animations::k_Sleep, animations::k_EndSleep}, 0.0f);
	sleep.untilRested = true;
	sleep.closedEyes = true;
	sleep.effect = Effect::Slept;
	sleep.main = true;
	agenda.push_back(sleep);
	// Only the dazed look as it wakes pulls a face
	auto dazed = Action(animations::k_Confused, true);
	dazed.face = creature_face::Cue::Grimace;
	agenda.push_back(dazed);
	return agenda;
}

std::vector<Step> creature_mind::Eat(uint32_t food, bool holdingEatable)
{
	std::vector<Step> agenda;
	if (!holdingEatable)
	{
		agenda.push_back(PickUpLooking(food));
	}
	// It looks the food over with a curious face, then eats it
	auto examine = Object({.kind = ObjectOrder::Kind::Keep, .animation = creature_object_actions::k_ExamineObject});
	examine.face = creature_face::Cue::Curiosity;
	examine.gaze = Gaze {.object = food};
	agenda.push_back(examine);
	agenda.push_back(Object({.kind = ObjectOrder::Kind::Eat}, Effect::Eat));
	return agenda;
}

std::vector<Step> creature_mind::EatAlive(uint32_t food, bool holdingEatable, const Random& random)
{
	std::vector<Step> agenda;
	if (!holdingEatable)
	{
		if (random(k_ShowHungerBeforeEatLots) == 0)
		{
			agenda.push_back(Action(animations::k_Hungry, false));
		}
		agenda.push_back(PickUpLooking(food));
	}
	agenda.push_back(Object({.kind = ObjectOrder::Kind::Eat}, Effect::Eat));
	return agenda;
}

std::vector<Step> creature_mind::ExamineByPickingUp(uint32_t object, bool handFull, const Random& random)
{
	std::vector<Step> agenda;
	// It picks the thing up only when its hand is empty
	if (!handFull)
	{
		agenda.push_back(PickUpLooking(object));
	}
	const auto keep = creature_object_actions::k_FirstKeepAnimation +
	                  random(static_cast<uint32_t>(creature_object_actions::k_KeepAnimationCount));
	const auto letGo = random(k_PutDownLots) >= k_TossLots ? ObjectOrder::Kind::PutDown : ObjectOrder::Kind::Discard;
	// Only the look at the thing has a curious face
	auto examine = Object({.kind = ObjectOrder::Kind::Keep, .animation = keep}, Effect::Examined);
	examine.face = creature_face::Cue::Curiosity;
	examine.gaze = Gaze {.object = object};
	examine.main = true;
	agenda.push_back(std::move(examine));
	// After the look at the thing, its eyes stop their sweep, as the dance does. The head still looks about.
	agenda.push_back({.kind = Step::Kind::Dance, .dance = {.kind = DanceOrder::Kind::StopLooking}});
	agenda.push_back(Object({.kind = letGo}));
	// After it releases the thing, it knows the kind of thing
	agenda.push_back({.kind = Step::Kind::Observe, .object = object});
	return agenda;
}

std::vector<Step> creature_mind::ThrowAbout(uint32_t object, const Random& random)
{
	constexpr uint32_t k_Degrees = 360;
	const auto angle = static_cast<float>(random(k_Degrees)) * std::numbers::pi_v<float> / 180.0f;
	const auto distance = k_ThrowAroundDistance + static_cast<float>(random(k_ThrowAroundExtra));
	return WithFace(
	    {PickUp(object), Object({.kind = ObjectOrder::Kind::ThrowNearby,
	                             .point = distance * glm::vec2(portable_math::Cos(angle), portable_math::Sin(angle))},
	                            Effect::ThrewAbout)},
	    creature_face::Cue::Playfulness);
}

std::vector<Step> creature_mind::Hurl(uint32_t object, glm::vec2 target, const Random& random)
{
	std::vector<Step> agenda;
	if (random(k_AngryBeforeHurlLots) == 0)
	{
		agenda.push_back(Action(animations::k_Angry, false));
	}
	agenda.push_back(PickUp(object));
	agenda.push_back(Object({.kind = ObjectOrder::Kind::Throw, .point = target}, Effect::Hurled));
	return WithFace(std::move(agenda), creature_face::Cue::Anger);
}

std::vector<Step> creature_mind::PutDownHeld()
{
	return {Object({.kind = ObjectOrder::Kind::PutDown})};
}

namespace
{
/// Bringing food out of the sea at a fish farm's shoal: walking there, emptying its hand if need be, and the food
/// appearing at its feet to be picked up. Fishing pulls no face of its own.
void Fish(std::vector<Step>& agenda, const creature_mind::FishingTrip& trip)
{
	Step walk {.kind = Step::Kind::Move};
	walk.movement = {
	    .kind = Movement::Kind::ToPoint, .point = trip.shoal, .maxDistance = trip.arriveWithin, .giveUpIfUnreachable = true};
	walk.gaze = GoingLook();
	agenda.push_back(walk);
	agenda.push_back(Object({.kind = ObjectOrder::Kind::FishFromSea}));
}
} // namespace

std::vector<Step> creature_mind::FishAndEat(glm::vec2 shoal, float arriveWithin)
{
	std::vector<Step> agenda;
	Fish(agenda, {.shoal = shoal, .arriveWithin = arriveWithin});
	agenda.push_back(Object({.kind = ObjectOrder::Kind::Eat}, Effect::Eat));
	return agenda;
}

std::vector<Step> creature_mind::GiveFishToStore(const std::optional<FishingTrip>& trip, uint32_t store, float height)
{
	std::vector<Step> agenda;
	if (trip.has_value())
	{
		Fish(agenda, *trip);
	}
	// Its head looks at the store as it goes to throw and as it turns to it
	Step goTo {.kind = Step::Kind::Move, .face = creature_face::Cue::Compassion, .gaze = Gaze {.object = store}};
	goTo.movement = {.kind = Movement::Kind::ToThrowPosition, .object = store, .maxDistance = height};
	agenda.push_back(goTo);
	Step turn {
	    .kind = Step::Kind::Move, .seconds = k_FaceStoreSeconds, .effect = Effect::Completed, .gaze = Gaze {.object = store}};
	turn.movement = {.kind = Movement::Kind::TurnToFaceObject, .object = store};
	agenda.push_back(turn);
	agenda.push_back(Object({.kind = ObjectOrder::Kind::ThrowInStore, .object = store}));
	return agenda;
}

std::vector<Step> creature_mind::TakeFishTo(const std::optional<FishingTrip>& trip, glm::vec2 place, float height)
{
	std::vector<Step> agenda;
	if (trip.has_value())
	{
		Fish(agenda, *trip);
	}
	Step walk {.kind = Step::Kind::Move};
	walk.movement = {.kind = Movement::Kind::ToPoint, .point = place, .maxDistance = height, .giveUpIfUnreachable = true};
	walk.gaze = GoingLook();
	agenda.push_back(walk);
	// It lets go of the food as it lets go of a thing that it brings home, with the animation that puts a thing down
	agenda.push_back(Object({.kind = ObjectOrder::Kind::Discard, .animation = creature_throw::k_PutDown}));
	return agenda;
}

std::vector<Step> creature_mind::Poo(const Random& random, std::optional<PooBy> by)
{
	std::vector<Step> agenda;
	if (random(k_ShowPooLots) == 0)
	{
		agenda.push_back(Action(animations::k_NeedAPoo, false));
	}
	if (by.has_value())
	{
		Step goNear {.kind = Step::Kind::Move};
		goNear.movement = {.kind = Movement::Kind::GoNearObject, .object = by->object, .maxDistance = 0.0f};
		// Going up to itself, the look comes to nothing and it looks about
		if (!by->itself)
		{
			goNear.gaze = Gaze {.object = by->object, .whileGoing = true};
		}
		agenda.push_back(goNear);
		Step turnAway {.kind = Step::Kind::Move};
		turnAway.movement = {.kind = Movement::Kind::TurnToFaceAwayFromObject, .object = by->object};
		agenda.push_back(turnAway);
	}
	auto poo = Static({animations::k_StartPoo, animations::k_Poo, animations::k_EndPoo}, k_PooSeconds);
	poo.effect = Effect::Poo;
	// Only the squat pulls a face
	poo.face = creature_face::Cue::Smile;
	agenda.push_back(poo);
	return agenda;
}

std::vector<Step> creature_mind::Fart(const Random& random, std::optional<PooBy> by)
{
	// The steps are the same as the steps for a poo. Only the result of the squat is different
	auto agenda = Poo(random, by);
	for (auto& step : agenda)
	{
		if (step.effect == Effect::Poo)
		{
			step.effect = Effect::Fart;
		}
	}
	return agenda;
}

std::vector<Step> creature_mind::Puke()
{
	auto puke = Static({animations::k_StartPuke, animations::k_Puke, animations::k_EndPuke}, k_PukeSeconds);
	puke.effect = Effect::Puke;
	return WithFace({puke}, creature_face::Cue::Grimace);
}

std::vector<Step> creature_mind::Faint(float size)
{
	auto faint = Static({animations::k_Faint, animations::k_Faint, animations::k_GetUp},
	                    (k_FaintSecondsPerSize * size) + k_FaintExtraSeconds);
	faint.holdLoop = true;
	faint.closedEyes = true;
	faint.effect = Effect::CameRound;
	faint.faint = true;
	return {faint};
}

std::vector<Step> creature_mind::EatHeld()
{
	return WithFace({Object({.kind = ObjectOrder::Kind::Eat}, Effect::Eat)}, creature_face::Cue::Curiosity);
}

std::vector<Step> creature_mind::TakeFromHand(uint32_t object, bool leftHand)
{
	const Step turn {.kind = Step::Kind::Move, .movement = {.kind = Movement::Kind::TurnToFaceGiverCamera}};
	auto take = Object({.kind = ObjectOrder::Kind::TakeFromHand, .object = object, .mirrored = leftHand}, Effect::TookFromHand);
	std::vector<Step> agenda {turn, take};
	// Its head looks at the player's hand as it turns and as it takes the thing
	for (auto& step : agenda)
	{
		step.gaze = Gaze {.hand = true};
	}
	return WithFace(std::move(agenda), creature_face::Cue::Curiosity);
}

std::vector<Step> creature_mind::ExamineGiven(uint32_t object)
{
	auto examine =
	    Object({.kind = ObjectOrder::Kind::Keep, .animation = creature_object_actions::k_ExamineObject}, Effect::Examined);
	examine.gaze = Gaze {.object = object};
	return WithFace({examine}, creature_face::Cue::Curiosity);
}

std::vector<Step> creature_mind::EatGiven()
{
	return {Object({.kind = ObjectOrder::Kind::Eat}, Effect::Eat)};
}

std::vector<Step> creature_mind::ThrowGiven(glm::vec3 camera)
{
	auto toss =
	    Object({.kind = ObjectOrder::Kind::Throw, .point = {camera.x, camera.z}, .pointHeight = camera.y}, Effect::ThrewAbout);
	toss.gaze = Gaze {.camera = true};
	return {toss};
}

std::vector<Step> creature_mind::Emote(size_t animation)
{
	return {Action(animation, false)};
}

Step creature_mind::TurnToCamera()
{
	return {.kind = Step::Kind::Move, .movement = {.kind = Movement::Kind::TurnToFaceCamera}, .gaze = Gaze {.camera = true}};
}

std::vector<Step> creature_mind::Communicate()
{
	return WithFace({TurnToCamera(), {.kind = Step::Kind::Communicate, .gaze = Gaze {.camera = true}}},
	                creature_face::Cue::AttitudeToPlayer);
}

std::vector<Step> creature_mind::FaceAndEmote(size_t animation)
{
	// Its head looks at the camera as it turns and as it plays the action
	auto emote = Action(animation, false);
	emote.gaze = Gaze {.camera = true};
	return {TurnToCamera(), emote};
}

std::vector<Step> creature_mind::ApproachAndEmote(uint32_t object, size_t animation)
{
	// It goes up to the other with a playful face and its head on it, turns to it with a smile, then plays the action
	const Gaze look {.object = object};
	Step approach {.kind = Step::Kind::Move, .face = creature_face::Cue::Playfulness, .gaze = look};
	approach.movement = {.kind = Movement::Kind::ToObject, .object = object, .maxDistance = k_ApproachDistance};
	Step turn {.kind = Step::Kind::Move, .seconds = k_ApproachFaceSeconds, .face = creature_face::Cue::Smile, .gaze = look};
	turn.movement = {.kind = Movement::Kind::TurnToFaceObject, .object = object};
	return {approach, turn, Action(animation, false)};
}

std::vector<Step> creature_mind::LookAt(glm::vec2 point, float seconds)
{
	Step turn {.kind = Step::Kind::Move};
	turn.movement = {.kind = Movement::Kind::TurnToFace, .point = point};
	return WithFace({turn, {.kind = Step::Kind::Wait, .seconds = seconds}}, creature_face::Cue::Amazed);
}

std::vector<Step> creature_mind::DestroyThing(uint32_t object)
{
	auto stomp = Object({.kind = ObjectOrder::Kind::Destroy, .object = object}, Effect::Stomped);
	stomp.gaze = Gaze {.object = object, .whileGoing = true};
	return WithFace({stomp}, creature_face::Cue::Anger);
}

std::vector<Step> creature_mind::LookAbout(float seconds)
{
	return WithFace({{.kind = Step::Kind::Wait, .seconds = seconds}}, creature_face::Cue::Idle);
}

namespace
{
const Step* CurrentStep(const IdleMind& mind)
{
	return mind.stepStarted && mind.step < mind.agenda.size() ? &mind.agenda[mind.step] : nullptr;
}
} // namespace

bool creature_mind::AgendaOver(const IdleMind& mind)
{
	return mind.step >= mind.agenda.size() && mind.endsNextTurn != mind.serial;
}

bool creature_mind::IsAsleep(const IdleMind& mind)
{
	const auto* step = CurrentStep(mind);
	return step != nullptr && step->kind == Step::Kind::Static && step->untilRested && !mind.sitEnding;
}

bool creature_mind::IsResting(const IdleMind& mind)
{
	const auto* step = CurrentStep(mind);
	return step != nullptr && step->kind == Step::Kind::Static && step->untilBetter && !mind.sitEnding;
}

bool creature_mind::IsUnconscious(const IdleMind& mind)
{
	const auto* step = CurrentStep(mind);
	return step != nullptr && step->kind == Step::Kind::Static && step->holdLoop;
}

std::vector<Step> creature_mind::HangAround(const HangAroundPlaces& places, float turnSeconds, const Random& random)
{
	const glm::vec2 home {places.home.x, places.home.z};
	std::vector<Step> agenda;
	agenda.push_back({.kind = Step::Kind::Move,
	                  .movement = {.kind = Movement::Kind::ToPoint,
	                               .point = home,
	                               .maxDistance = std::min(places.height, k_HangAroundHomeReach)},
	                  .gaze = GoingLook()});
	// The wait counts whole turns
	auto wait = FloatRand(random, k_HangAroundWaitExtraSeconds) + k_HangAroundWaitSeconds;
	if (turnSeconds > 0.0f)
	{
		const auto turnsPerSecond = std::round(1.0f / turnSeconds);
		wait = std::trunc(wait * turnsPerSecond) / turnsPerSecond;
	}
	agenda.push_back({.kind = Step::Kind::Wait, .seconds = wait, .face = creature_face::Cue::Idle});

	const auto lot = places.temple.has_value() ? random(k_HangAroundLots) : 0;
	if (lot == 0)
	{
		auto yawn = Action(animations::k_Tired, true);
		yawn.face = creature_face::Cue::Idle;
		agenda.push_back(yawn);
		return agenda;
	}
	const auto temple = *places.temple;
	const auto sit = [&random]() {
		auto step = Static({animations::k_StartSit, animations::k_Sit, animations::k_EndSit},
		                   FloatRand(random, k_HangAroundSitExtraSeconds) + k_HangAroundSitSeconds);
		return step;
	};
	const auto face = [](glm::vec3 point) {
		return Step {.kind = Step::Kind::Move,
		             .movement = {.kind = Movement::Kind::FacePoint, .point = {point.x, point.z}},
		             .gaze = Gaze {.point = point}};
	};
	if (lot <= k_HangAroundOutLots)
	{
		// The way out from home is away from the temple, and it includes the difference in height. Its length adds the
		// squares of z, x, then y.
		auto away = places.home - temple;
		if (std::abs(away.x) > k_HangAroundSameSpot || std::abs(away.z) > k_HangAroundSameSpot)
		{
			away = x87::Scaled<x87::SumOrder::Zxy>(away);
		}
		else
		{
			away = {1.0f, 0.0f, 0.0f};
		}
		const auto out = places.home + (away * (places.confinementRadius * k_HangAroundOutPart));
		const auto view = places.home + (away * k_HangAroundViewDistance);
		agenda.push_back(
		    {.kind = Step::Kind::Move,
		     .movement = {.kind = Movement::Kind::ToPoint, .point = {out.x, out.z}, .maxDistance = k_HangAroundOutReach},
		     .gaze = GoingLook()});
		agenda.push_back(face(view));
		if (random(2) == 0)
		{
			agenda.push_back(sit());
		}
		else
		{
			agenda.push_back({.kind = Step::Kind::Object,
			                  .order = {.kind = ObjectOrder::Kind::PointAt,
			                            .point = {view.x, view.z},
			                            .pointHeight = view.y,
			                            .seconds = k_HangAroundPointSeconds},
			                  .face = creature_face::Cue::Amazed,
			                  .gaze = Gaze {.point = view}});
		}
	}
	// After the look out, and in the last two lots, it turns to the temple and sits
	agenda.push_back(face(temple));
	agenda.push_back(sit());
	return agenda;
}

void creature_mind::Plan(IdleMind& mind, Activity activity, std::vector<Step> agenda)
{
	mind.activity = activity;
	mind.agenda = std::move(agenda);
	++mind.serial;
	mind.step = 0;
	mind.stepStarted = false;
	mind.stepSeconds = 0.0f;
	mind.sitEnding = false;
	mind.faintLay = false;
	mind.faintTurns = 0;
	mind.wakeWanted = false;
	mind.gaveUp = false;
	mind.stoppedByScript = false;
	mind.stoppedSeen = false;
	mind.usedUpHeld.reset();
	mind.endedLook.reset();
	mind.endsNextTurn.reset();
	mind.lastStepSkipped.reset();
	// Taking up something else, it stops what it did, and its eyes look as a rested creature's eyes do again
	mind.eyesToClear = true;
}

float creature_mind::FloatRand(const Random& random, float most)
{
	if (most == 0.0f || std::isnan(most))
	{
		return 0.0f;
	}
	return game_random::FloatOf(random(game_random::k_FloatRandRange), most);
}

std::optional<int32_t> creature_mind::WaitTurns(float seconds)
{
	if (!std::isfinite(seconds))
	{
		return std::nullopt;
	}
	// The game multiplies in float and then cuts off the fraction
	const float turns = seconds * static_cast<float>(k_WaitTurnsPerSecond);
	return static_cast<int32_t>(turns);
}

bool creature_mind::WaitDone(uint32_t countedTurns, std::optional<int32_t> turns)
{
	if (!turns.has_value())
	{
		return false;
	}
	return static_cast<int64_t>(*turns) - static_cast<int64_t>(countedTurns) <= 0;
}

bool creature_mind::StaticTimeUp(uint32_t countedTurns, float seconds)
{
	constexpr uint32_t k_CountRange = 0x10000;
	const auto turns = WaitTurns(seconds);
	if (!turns.has_value())
	{
		return false;
	}
	const auto count = static_cast<uint32_t>(static_cast<uint16_t>(*turns));
	return countedTurns >= (count == 0 ? k_CountRange : count);
}

bool creature_mind::FaintWaitDone(int32_t countedTurns, float seconds)
{
	const auto turns = WaitTurns(seconds);
	if (!turns.has_value())
	{
		return false;
	}
	return static_cast<int64_t>(*turns) - static_cast<int64_t>(countedTurns) <= 0;
}

bool creature_mind::FaceAgainDue(float& seconds, float turnSeconds)
{
	seconds -= turnSeconds;
	return seconds < k_FaceDue;
}

std::optional<creature_face::Request> creature_mind::PullFace(IdleMind& mind, creature_face::Cue cue, const Senses& senses,
                                                              const Random& random, const Random& gameRandom)
{
	mind.faceSeconds = k_FaceRepeatSeconds;
	auto feelings = senses.feelings;
	feelings.variety = mind.faceVariety;
	// Only the idle face draws its pick
	if (cue == creature_face::Cue::Idle)
	{
		feelings.idlePick = random(creature_face::k_IdleFaceCount);
	}
	const auto face = creature_face::Choose(cue, feelings);
	if (face.has_value())
	{
		mind.faceVariety += gameRandom(k_FaceVarietyStep);
	}
	return face;
}

bool creature_mind::KeepsWatchingFlying(uint32_t turns, uint32_t turnsPerSecond, float limit, bool flying)
{
	return flying && static_cast<float>(turns / turnsPerSecond) < limit;
}

bool creature_mind::WatchedHandEnough(uint32_t turns, uint32_t limitTurns, uint32_t turnsPerSecond)
{
	const auto limit = std::min(static_cast<float>(limitTurns / turnsPerSecond), k_LongestHandWatchSeconds);
	return static_cast<float>(turns / turnsPerSecond) > limit;
}

namespace
{
/// A turn of watching the player's hand. With a player, each turn draws a synced random number that has no effect.
void WatchHand(IdleMind& mind, const Step& step, const Senses& senses, const Random& random, Commands& commands)
{
	++mind.watchTurns;
	if (!senses.hand.has_value())
	{
		FinishStep(mind);
		return;
	}
	static_cast<void>(random(k_HandWatchUnusedDraw));
	if (!(senses.seconds > 0.0f))
	{
		FinishStep(mind);
		return;
	}
	bool turning = senses.moving;
	if (StartsWatchTurn(*senses.hand, senses.bodyBusy))
	{
		commands.move = Movement {.kind = Movement::Kind::TurnToFace, .point = senses.hand->point};
		turning = true;
	}
	const auto turnsPerSecond = static_cast<uint32_t>(std::lround(1.0f / senses.seconds));
	if (!turning && WatchedHandEnough(mind.watchTurns, step.turns.value_or(0), turnsPerSecond))
	{
		FinishStep(mind);
	}
}

/// A turn of watching a flying thing: the limit is picked afresh each turn, before anything else is looked at
/// A turn of following a villager to watch it, from the turn the step starts: it goes on while the villager does what
/// the creature wants to learn, up to the step's whole seconds, then it holds for one to four seconds
void FollowWatching(IdleMind& mind, const Step& step, const Senses& senses, const Random& random, Commands& commands)
{
	const auto turnsPerSecond = std::max(static_cast<uint32_t>(std::lround(1.0f / std::max(senses.seconds, 1e-3f))), 1U);
	if (mind.followHolding)
	{
		if (mind.followHoldTurns > 0)
		{
			--mind.followHoldTurns;
		}
		if (mind.followHoldTurns == 0)
		{
			FinishStep(mind);
		}
		return;
	}
	++mind.watchTurns;
	const auto& followed = senses.followed;
	if (followed.has_value() && followed->interesting && static_cast<float>(mind.watchTurns / turnsPerSecond) <= step.seconds)
	{
		// Too far, it walks up to the villager
		if (followed->distance > step.movement.maxDistance && !senses.moving)
		{
			commands.move = step.movement;
			mind.followMoved = true;
		}
		return;
	}
	const auto pick = static_cast<float>(random(game_random::k_FloatRandRange)) * k_FollowWatchingExtraHoldSeconds *
	                  game_random::k_FloatRandScale;
	mind.followHolding = true;
	mind.followHoldTurns = static_cast<uint32_t>((pick + k_FollowWatchingHoldSeconds) * static_cast<float>(turnsPerSecond));
}

/// A turn of following the player's hand
void FollowHand(IdleMind& mind, const Senses& senses, Commands& commands)
{
	if (!senses.followHand.has_value())
	{
		FinishStep(mind);
		return;
	}
	switch (creature_player_attention::FollowHandTurn(*senses.followHand, mind.followHandTurns))
	{
	case creature_player_attention::FollowHandAction::Done:
		FinishStep(mind);
		break;
	case creature_player_attention::FollowHandAction::Fail:
		mind.step = mind.agenda.size();
		mind.stepStarted = false;
		mind.gaveUp = true;
		break;
	case creature_player_attention::FollowHandAction::WalkToHand:
		commands.move = Movement {.kind = Movement::Kind::ToPoint,
		                          .point = senses.followHand->hand,
		                          .maxDistance = senses.followHand->reach,
		                          .giveUpIfUnreachable = false};
		break;
	case creature_player_attention::FollowHandAction::Continue:
		break;
	}
}

/// A turn of watching the player while it waits for orders
void WatchPlayer(IdleMind& mind, const Senses& senses, Commands& commands)
{
	const auto action =
	    creature_player_attention::WatchPlayerTurn(senses.watchPlayer.value_or(creature_player_attention::WatchPlayerView {}));
	if (action.done)
	{
		FinishStep(mind);
		return;
	}
	if (action.turnTo.has_value())
	{
		commands.stopMoving = true;
		commands.move = Movement {.kind = Movement::Kind::TurnToFace, .point = *action.turnTo};
	}
}

/// Makes the step's object still: the step waits while the object flies, then the object starts to die and the step ends
void RenderImmobile(IdleMind& mind, const Step& step, const Senses& senses, Commands& commands)
{
	if (senses.objectFlying)
	{
		return;
	}
	commands.renderImmobile = step.object;
	FinishStep(mind);
}

void WatchFlying(IdleMind& mind, const Senses& senses, const Random& random)
{
	const auto limit =
	    static_cast<float>(random(game_random::k_FloatRandRange)) * k_WatchFlyingExtraSeconds * game_random::k_FloatRandScale +
	    k_WatchFlyingSeconds;
	++mind.watchTurns;
	if (!(senses.seconds > 0.0f))
	{
		FinishStep(mind);
		return;
	}
	const auto turnsPerSecond = static_cast<uint32_t>(std::lround(1.0f / senses.seconds));
	if (!KeepsWatchingFlying(mind.watchTurns, turnsPerSecond, limit, senses.objectInTheAir))
	{
		FinishStep(mind);
	}
}
} // namespace

HandsNeed creature_mind::HandsNeededFor(const Step& step)
{
	switch (step.kind)
	{
	case Step::Kind::Action:
	case Step::Kind::Static:
	case Step::Kind::Cast:
	case Step::Kind::Douse:
	case Step::Kind::SwapMind:
	case Step::Kind::Communicate:
	case Step::Kind::HealItself:
	case Step::Kind::RaiseTotem:
	case Step::Kind::LowerTotem:
	case Step::Kind::Apply:
		return HandsNeed::Free;
	case Step::Kind::MakeDisciple:
	case Step::Kind::UseUpHeld:
		return HandsNeed::Held;
	case Step::Kind::Object:
		switch (step.order.kind)
		{
		case ObjectOrder::Kind::PickUp:
			return HandsNeed::FreeOrTarget;
		case ObjectOrder::Kind::Catch:
		case ObjectOrder::Kind::FishFromSea:
		case ObjectOrder::Kind::TakeFromPile:
		case ObjectOrder::Kind::TakeFromHand:
			return HandsNeed::Free;
		case ObjectOrder::Kind::Discard:
		case ObjectOrder::Kind::Eat:
		case ObjectOrder::Kind::Keep:
		case ObjectOrder::Kind::Throw:
		case ObjectOrder::Kind::ThrowNearby:
		case ObjectOrder::Kind::ThrowInStore:
			return HandsNeed::Held;
		case ObjectOrder::Kind::PutDown:
		case ObjectOrder::Kind::Destroy:
		case ObjectOrder::Kind::PointAt:
		case ObjectOrder::Kind::Kick:
			break;
		}
		return HandsNeed::Any;
	case Step::Kind::Dance:
		// Only the start of the dance and the dance itself need free hands
		return step.dance.kind == DanceOrder::Kind::Start || step.dance.kind == DanceOrder::Kind::Dance ? HandsNeed::Free
		                                                                                                : HandsNeed::Any;
	// The fight starts with the question to the other creature, which does not need free hands. Each creature lets go
	// of what it holds before its taunt.
	case Step::Kind::Fight:
	case Step::Kind::StopMoving:
	case Step::Kind::FollowWatching:
	case Step::Kind::Kiss:
	case Step::Kind::PracticeDance:
	case Step::Kind::Wait:
	case Step::Kind::Move:
	case Step::Kind::Gesture:
	case Step::Kind::WaitInMap:
	case Step::Kind::CoolDown:
	case Step::Kind::Shatter:
	case Step::Kind::Repair:
	case Step::Kind::UseUp:
	case Step::Kind::WatchCamera:
	case Step::Kind::Observe:
	case Step::Kind::ActOnItself:
	case Step::Kind::Overlay:
	case Step::Kind::Inspect:
	case Step::Kind::WatchFlying:
	case Step::Kind::WatchHand:
	case Step::Kind::Face:
	case Step::Kind::FollowCamera:
	case Step::Kind::Heal:
	case Step::Kind::FollowHand:
	case Step::Kind::WatchPlayer:
	case Step::Kind::RenderImmobile:
	case Step::Kind::RequestPartner:
	case Step::Kind::Order:
	case Step::Kind::Demobilise:
	case Step::Kind::Remobilise:
	case Step::Kind::WaitForStill:
	case Step::Kind::WaitForFree:
	case Step::Kind::WaitUntilFree:
	case Step::Kind::ApplyDesire:
	case Step::Kind::Teleport:
	case Step::Kind::Transported:
	case Step::Kind::StopTeleportControl:
	case Step::Kind::Empty:
		break;
	}
	return HandsNeed::Any;
}

bool creature_mind::LacksClipToStart(const Step& step, std::optional<size_t> communication,
                                     const std::function<bool(size_t)>& hasClip)
{
	switch (step.kind)
	{
	case Step::Kind::Action:
	case Step::Kind::SwapMind:
		return !hasClip(step.animation);
	case Step::Kind::Communicate:
		return communication.has_value() && !hasClip(*communication);
	case Step::Kind::Static:
		// A faint lies down with no test of its clips
		if (step.holdLoop)
		{
			return false;
		}
		[[fallthrough]];
	case Step::Kind::Cast:
		return !std::ranges::all_of(step.sequence, hasClip);
	default:
		return false;
	}
}

bool creature_mind::PutsDownGently(bool heldIsCreature, std::optional<creature_desires::Desire> use)
{
	using creature_desires::Desire;
	return heldIsCreature ||
	       (use.has_value() && (*use == Desire::Impress || *use == Desire::Compassion || *use == Desire::Curiosity));
}

creature_mind::HandsCheck creature_mind::CheckHands(bool holds, bool bodyBusy, bool handsBusy)
{
	if (!holds)
	{
		return HandsCheck::Free;
	}
	// It starts to let go only when its body and hands are free
	return bodyBusy || handsBusy ? HandsCheck::Wait : HandsCheck::LetGo;
}

namespace
{
/// The rest of the agenda is no use
void GiveUpAgenda(IdleMind& mind)
{
	mind.step = mind.agenda.size();
	mind.stepStarted = false;
	mind.gaveUp = true;
	mind.eyesToClear = true;
}

/// Waiting for the dancers to stand still: with no dance there is nothing to wait for
void WaitForDancers(IdleMind& mind, const Senses& senses)
{
	if (!senses.dance.has_value() || !senses.dance->exists)
	{
		GiveUpAgenda(mind);
	}
	else if (!senses.dance->dancersMoving)
	{
		FinishStep(mind);
	}
}

/// A dance step starts: the world carries out its order this turn
void StartDanceStep(IdleMind& mind, const Step& step, const Senses& senses, Commands& commands)
{
	switch (step.dance.kind)
	{
	case DanceOrder::Kind::WaitUntilStill:
		WaitForDancers(mind, senses);
		return;
	case DanceOrder::Kind::StopLooking:
		// The order only stops the sweep of the eyes. The step does nothing else to the creature.
		break;
	case DanceOrder::Kind::Set:
		// The last dance goes first, and the new one comes the turn after
		mind.danceSet = false;
		commands.dance = DanceOrder {.kind = DanceOrder::Kind::Clear};
		return;
	case DanceOrder::Kind::Dance:
		mind.danceTurns = step.dance.count;
		mind.danceEnding = false;
		break;
	default:
		break;
	}
	commands.dance = step.dance;
}

/// A turn of a dance step under way
void DanceStepTurn(IdleMind& mind, const Step& step, const Senses& senses, Commands& commands, const Random& gameRandom)
{
	if (step.dance.kind == DanceOrder::Kind::WaitUntilStill)
	{
		WaitForDancers(mind, senses);
		return;
	}
	if (step.dance.kind == DanceOrder::Kind::Set && !mind.danceSet)
	{
		commands.dance = step.dance;
		mind.danceSet = true;
		return;
	}
	if (step.dance.kind != DanceOrder::Kind::Dance)
	{
		// The world says when it has carried out the order
		return;
	}
	if (mind.danceEnding)
	{
		commands.dance = DanceOrder {.kind = DanceOrder::Kind::Stop};
		FinishStep(mind);
		return;
	}
	if (!senses.dance.has_value() || !senses.dance->inGroup)
	{
		GiveUpAgenda(mind);
		return;
	}
	const auto turn = creature_dance::Turn(*senses.dance, senses.position);
	// A turn already under way is not started again
	if (turn.facePoint.has_value() && !senses.moving)
	{
		commands.move = Movement {.kind = Movement::Kind::TurnToFace, .point = *turn.facePoint};
	}
	// Each turn of a move that plays an action or a pose picks a side, also when the body is busy. A body that plays an
	// action or moves refuses the start.
	if (turn.playOnce.has_value())
	{
		const bool mirrored = PickIndividualSide(gameRandom, senses);
		if (!senses.bodyBusy)
		{
			commands.playOnce = *turn.playOnce;
			commands.mirrored = mirrored;
		}
	}
	if (turn.overlay.has_value() && !senses.overlayPlaying)
	{
		commands.playOverlay = *turn.overlay;
	}
	commands.endStaticLoop = turn.endPose;
	commands.endDance = turn.endDance;
	commands.endPoint = turn.endPoint;
	if (turn.gesture.has_value())
	{
		commands.gesture = *turn.gesture;
		commands.gestureInDance = true;
	}
	// The dance does not start while the creature walks or turns. It starts when the creature stands still.
	if (turn.danceMoveMs.has_value() && !senses.bodyBusy)
	{
		commands.startDance = *turn.danceMoveMs;
	}
	if (turn.pose.has_value())
	{
		const bool mirrored = PickStaticSide(gameRandom, senses.realTime);
		if (!senses.bodyBusy)
		{
			commands.startSequence = std::array {*turn.pose - 1, *turn.pose, *turn.pose + 1};
			commands.mirrored = mirrored;
		}
	}
	if (turn.face.has_value())
	{
		if (const auto face = creature_face::FaceOf(*turn.face))
		{
			commands.face = creature_face::Request {.face = *face, .milliseconds = turn.faceMs};
		}
	}
	if (turn.walkTo.has_value())
	{
		commands.move = Movement {.kind = Movement::Kind::ToPoint,
		                          .point = *turn.walkTo,
		                          .maxDistance = creature_dance::k_NearPlace,
		                          .giveUpIfUnreachable = false};
	}
	--mind.danceTurns;
	if (mind.danceTurns == 0)
	{
		mind.danceEnding = true;
	}
}
} // namespace

void creature_mind::DanceOrderDone(IdleMind& mind, bool done)
{
	if (mind.step >= mind.agenda.size() || mind.agenda[mind.step].kind != Step::Kind::Dance)
	{
		return;
	}
	if (done)
	{
		FinishStep(mind);
	}
	else
	{
		GiveUpAgenda(mind);
	}
}

void creature_mind::CarryDone(IdleMind& mind)
{
	if (mind.step < mind.agenda.size() && mind.agenda[mind.step].kind == Step::Kind::Transported && mind.stepStarted)
	{
		FinishStep(mind);
	}
}

namespace
{
/// A turn of following the camera's point of focus
void FollowCamera(IdleMind& mind, const Step& step, const Senses& senses, Commands& commands)
{
	const float reach = step.movement.maxDistance;
	const auto action = creature_emote::FollowCameraTurn({.focus = senses.cameraFocus,
	                                                      .position = senses.position,
	                                                      .moving = senses.bodyBusy,
	                                                      .focusAhead = senses.cameraFocusAhead},
	                                                     mind.followCameraTurns, reach);
	switch (action)
	{
	case creature_emote::FollowCameraAction::Done:
		FinishStep(mind);
		break;
	case creature_emote::FollowCameraAction::MoveAgain:
		commands.move = Movement {.kind = Movement::Kind::ToPoint,
		                          .point = {senses.cameraFocus->x, senses.cameraFocus->z},
		                          .maxDistance = reach - creature_emote::k_FollowCameraStopShort,
		                          .giveUpIfUnreachable = false};
		break;
	case creature_emote::FollowCameraAction::Continue:
		break;
	}
}
} // namespace

std::optional<Gaze> creature_mind::LookOf(const Step& step)
{
	if (step.gaze.has_value())
	{
		return step.gaze;
	}
	if (step.kind == Step::Kind::Object && step.order.kind == ObjectOrder::Kind::PickUp && step.order.object.has_value())
	{
		return Gaze {.object = step.order.object, .whileGoing = true};
	}
	if (step.kind == Step::Kind::Move && step.movement.kind == Movement::Kind::GoNearObject && step.movement.object.has_value())
	{
		return Gaze {.object = step.movement.object, .whileGoing = true};
	}
	return std::nullopt;
}

namespace
{
using creature_sub_states::SubState;

SubState SubStateOfMove(const Movement& movement)
{
	switch (movement.kind)
	{
	case Movement::Kind::ToPoint:
	case Movement::Kind::Nearby:
		return SubState::MoveToPos;
	case Movement::Kind::ToObject:
	case Movement::Kind::GoNearObject:
		return SubState::GoNearObject;
	case Movement::Kind::Follow:
		return SubState::FollowInterestingObject;
	case Movement::Kind::FleeFrom:
		return SubState::RunAway;
	case Movement::Kind::TurnToFace:
	case Movement::Kind::FacePoint:
		return SubState::TurnToFacePos;
	case Movement::Kind::GetAwayFromObject:
		return SubState::GetAwayFromObject;
	case Movement::Kind::TurnToFaceObject:
		return SubState::TurnToFaceObject;
	case Movement::Kind::TurnToFaceAwayFromObject:
		return SubState::TurnToFaceAwayFromObject;
	case Movement::Kind::ToThrowPosition:
		return SubState::MoveToThrowPos;
	case Movement::Kind::ToThrowAtPoint:
		return SubState::MoveToThrowAtPos;
	case Movement::Kind::FaceDownSlope:
		return SubState::FaceDownSlope;
	case Movement::Kind::TurnToFaceCamera:
		return SubState::TurnToFaceCamera;
	case Movement::Kind::TurnToFaceGiverCamera:
		return SubState::TurnToFaceOtherPlayersCamera;
	}
	return SubState::MoveToPos;
}

SubState SubStateOfObject(const ObjectOrder& order)
{
	switch (order.kind)
	{
	case ObjectOrder::Kind::PickUp:
		return SubState::Pickup;
	case ObjectOrder::Kind::PutDown:
	case ObjectOrder::Kind::Discard:
		return SubState::Discard;
	case ObjectOrder::Kind::Eat:
		return SubState::Eat;
	case ObjectOrder::Kind::Keep:
		return SubState::HeldObjectAction;
	case ObjectOrder::Kind::Throw:
	case ObjectOrder::Kind::ThrowNearby:
		return SubState::Throw;
	case ObjectOrder::Kind::Destroy:
		return SubState::DestroyObject;
	case ObjectOrder::Kind::PointAt:
		return SubState::PointAtPoint;
	case ObjectOrder::Kind::Catch:
		return SubState::Catch;
	case ObjectOrder::Kind::FishFromSea:
		// The fish comes out of the sea, then the creature picks it up: the pick-up is the last part
		return SubState::PickupCreatedObject;
	case ObjectOrder::Kind::ThrowInStore:
		return SubState::ThrowInPile;
	case ObjectOrder::Kind::TakeFromPile:
		return SubState::CreatePickUpThenRemove;
	case ObjectOrder::Kind::TakeFromHand:
		return SubState::PickUpFromHand;
	case ObjectOrder::Kind::Kick:
		return SubState::KickObject;
	}
	return SubState::Pickup;
}

SubState SubStateOfDance(const DanceOrder& order)
{
	switch (order.kind)
	{
	case DanceOrder::Kind::Set:
	case DanceOrder::Kind::Clear:
		return SubState::SetDance;
	case DanceOrder::Kind::Summon:
		return SubState::AddVillagersToDance;
	case DanceOrder::Kind::StopLooking:
		return SubState::StopLooking;
	case DanceOrder::Kind::SendToPlaces:
		return SubState::TellVillagersToGoToStartDancePos;
	case DanceOrder::Kind::WaitUntilStill:
		return SubState::WaitForDancersToStopMoving;
	case DanceOrder::Kind::Start:
		return SubState::StartTheDance;
	case DanceOrder::Kind::TellToDance:
		return SubState::TellVillagersToDance;
	case DanceOrder::Kind::Dance:
	case DanceOrder::Kind::Stop:
		return SubState::Dance;
	}
	return SubState::Dance;
}

SubState SubStateOfStatic(const Step& step)
{
	// A poo or a fart ends with the step that lets it out, after the squat
	if (step.effect == Effect::Poo || step.effect == Effect::Fart)
	{
		return SubState::Poo;
	}
	if (step.faint)
	{
		return SubState::Faint;
	}
	if (step.untilBetter)
	{
		return SubState::RestToGetBetter;
	}
	if (step.untilRested)
	{
		return SubState::Sleep;
	}
	return SubState::StaticAction;
}
} // namespace

creature_sub_states::SubState creature_mind::SubStateOf(const Step& step)
{
	switch (step.kind)
	{
	case Step::Kind::Wait:
		return SubState::Wait;
	case Step::Kind::Action:
		return SubState::IndividualAction;
	case Step::Kind::Static:
		return SubStateOfStatic(step);
	case Step::Kind::Move:
		return SubStateOfMove(step.movement);
	case Step::Kind::Object:
		return SubStateOfObject(step.order);
	case Step::Kind::Cast:
		return step.cast.object.has_value() ? SubState::CastSpellAtObject : SubState::CastSpellAtPos;
	case Step::Kind::Gesture:
		return SubState::Gesture;
	case Step::Kind::WaitInMap:
		return SubState::WaitForObjectToBeInMap;
	case Step::Kind::Douse:
		return SubState::PutOutFire;
	case Step::Kind::CoolDown:
		return SubState::StopBeingOnFire;
	case Step::Kind::Shatter:
		return SubState::ShatterRock;
	case Step::Kind::Repair:
		return SubState::Repair;
	case Step::Kind::UseUp:
		return SubState::DestroyObjectToUse;
	case Step::Kind::UseUpHeld:
		return SubState::HideObjectInHand;
	case Step::Kind::WatchCamera:
		return SubState::LookAtCamera;
	case Step::Kind::Observe:
		return SubState::ObserveObject;
	case Step::Kind::SwapMind:
		return SubState::SwapMindWithOtherCreature;
	case Step::Kind::Overlay:
		return SubState::Talk;
	case Step::Kind::Inspect:
		return SubState::InspectCreature;
	case Step::Kind::WatchFlying:
		return SubState::LookAtFlyingObject;
	case Step::Kind::WatchHand:
		return SubState::LookAtHand;
	case Step::Kind::Communicate:
		return SubState::CommunicateToPlayer;
	case Step::Kind::StopMoving:
		return SubState::StopMoving;
	case Step::Kind::Dance:
		return SubStateOfDance(step.dance);
	case Step::Kind::Face:
		return SubState::FacialAnimation;
	case Step::Kind::FollowCamera:
		return SubState::FollowCamera;
	case Step::Kind::Heal:
		return SubState::Heal;
	case Step::Kind::HealItself:
		return SubState::HealHimself;
	case Step::Kind::FollowWatching:
		return SubState::FollowInterestingObject;
	case Step::Kind::FollowHand:
		return SubState::FollowHand;
	case Step::Kind::WatchPlayer:
		return SubState::WatchPlayerWhileHeHasYourAttention;
	case Step::Kind::ActOnItself:
		return SubState::ClearObjectToActOn;
	case Step::Kind::Fight:
		return SubState::Fight;
	case Step::Kind::RaiseTotem:
		return SubState::RaiseTotem;
	case Step::Kind::LowerTotem:
		return SubState::LowerTotem;
	case Step::Kind::MakeDisciple:
		return SubState::MakeDisciple;
	case Step::Kind::Kiss:
		return step.object.has_value() ? SubState::Kiss : SubState::SingleKiss;
	case Step::Kind::PracticeDance:
		return SubState::PracticeDance;
	case Step::Kind::RenderImmobile:
		return SubState::RenderImmobile;
	case Step::Kind::RequestPartner:
		return SubState::RequestPartner;
	case Step::Kind::Order:
		return SubState::OrderCreature;
	case Step::Kind::Demobilise:
		return SubState::DemobiliseObject;
	case Step::Kind::Remobilise:
		return SubState::RemobiliseObject;
	case Step::Kind::WaitForStill:
		return SubState::WaitForObject;
	case Step::Kind::Apply:
		return step.effect == Effect::Poo || step.effect == Effect::Fart ? SubState::Poo : SubState::Drink;
	case Step::Kind::ApplyDesire:
		return SubState::ApplyDesireToObject;
	case Step::Kind::WaitForFree:
		return SubState::WaitForCreatureToBeFree;
	case Step::Kind::WaitUntilFree:
		return SubState::WaitUntilFree;
	case Step::Kind::Teleport:
		return SubState::DoTeleport;
	case Step::Kind::Transported:
		return SubState::TransportedByTeleport;
	case Step::Kind::StopTeleportControl:
		return SubState::StopControlledByTeleport;
	}
	return SubState::Wait;
}

std::optional<Gaze> creature_mind::TurnLook(const IdleMind& mind)
{
	if (mind.endedLook.has_value() && mind.endedLook->serial == mind.serial)
	{
		return mind.endedLook->gaze;
	}
	if (mind.step < mind.agenda.size())
	{
		return LookOf(mind.agenda[mind.step]);
	}
	return std::nullopt;
}

creature_mind::LookEyes creature_mind::TurnLookEyes(const IdleMind& mind)
{
	// The step that ended with empty last stages is the step before the current one
	const bool held = mind.endedLook.has_value() && mind.endedLook->serial == mind.serial && mind.step > 0;
	const size_t index = held ? mind.step - 1 : mind.step;
	if (index >= mind.agenda.size())
	{
		return {};
	}
	const auto& step = mind.agenda[index];
	const auto eyes = step.heldEyes.value_or(step.sleepyEyes ? Eyes::Sleepy : Eyes::Unchanged);
	return {.eyes = eyes, .flutterEyelids = step.flutterEyelids};
}

bool creature_mind::StartsOnTheWay(const Step& step)
{
	if (step.kind != Step::Kind::Move)
	{
		return false;
	}
	switch (step.movement.kind)
	{
	case Movement::Kind::ToPoint:
	case Movement::Kind::Nearby:
	case Movement::Kind::ToObject:
		return true;
	default:
		return false;
	}
}

namespace
{
/// Whether the step starts while the body is busy, because it does not examine the body. Each of these steps is done
/// on its first turn: it uses up a thing, puts water on a thing, cools, shatters, mends or heals, pulls a face, takes
/// in a thing or acts on itself.
bool StartsWhileBodyIsBusy(const Step& step)
{
	switch (step.kind)
	{
	case Step::Kind::UseUp:
	case Step::Kind::UseUpHeld:
	case Step::Kind::Douse:
	case Step::Kind::CoolDown:
	case Step::Kind::Shatter:
	case Step::Kind::Repair:
	case Step::Kind::Observe:
	case Step::Kind::Face:
	case Step::Kind::Heal:
	case Step::Kind::HealItself:
	case Step::Kind::ActOnItself:
		return true;
	default:
		return false;
	}
}

/// Whether the step starts its clip with a try that the body can refuse. A refused try comes again on each turn. A
/// faint is not refused, and a communication with no clip starts no clip.
bool TriesToStartClip(const Step& step, const Senses& senses)
{
	switch (step.kind)
	{
	case Step::Kind::Action:
	case Step::Kind::Cast:
	case Step::Kind::SwapMind:
		return true;
	case Step::Kind::Static:
		return !step.holdLoop;
	case Step::Kind::Communicate:
		return senses.communication.animation.has_value();
	default:
		return false;
	}
}

/// Whether a step reference is the current step of the agenda
bool IsCurrentStep(const IdleMind& mind, const std::optional<IdleMind::StepRef>& ref)
{
	return ref.has_value() && ref->serial == mind.serial && ref->step == mind.step;
}

/// One try to start a step that the body refuses, because the body is busy or does not have the clip. Each try picks a
/// side first from the game's random numbers: a static action or a cast always, other actions only while the hand
/// holds nothing. Each try to communicate also stops the desire to show its state for a time.
void RefusedStart(IdleMind& mind, const Step& step, const Senses& senses, const Random& gameRandom, Commands& commands)
{
	// Each turn that the step tries to start counts as a turn of the step
	if (!IsCurrentStep(mind, mind.startTried))
	{
		mind.startTried = IdleMind::StepRef {.serial = mind.serial, .step = mind.step};
		mind.startTries = 0;
	}
	++mind.startTries;
	const bool always = step.kind == Step::Kind::Static || step.kind == Step::Kind::Cast;
	if (always || !senses.held.has_value())
	{
		static_cast<void>(gameRandom(2));
	}
	if (step.kind == Step::Kind::Communicate)
	{
		commands.communicated = senses.communication;
		mind.shown = senses.communication.desire;
	}
}
} // namespace

Commands creature_mind::Think(IdleMind& mind, const Senses& senses, const Random& random)
{
	return Think(mind, senses, random, random);
}

namespace
{
/// One turn of thought: the step of the agenda starts, continues or ends
Commands TurnOfThought(IdleMind& mind, const Senses& senses, const Random& random, const Random& gameRandom)
{
	Commands commands;
	mind.movedOn.reset();
	// The agenda goes past the empty last stages of the step that ended, so the look of that step ends
	mind.endedLook.reset();
	// When those stages were the last of the agenda, the agenda ends now, and the creature stops what it does
	if (mind.endsNextTurn == mind.serial)
	{
		mind.eyesToClear = true;
	}
	mind.endsNextTurn.reset();
	if (mind.eyesToClear)
	{
		commands.restEyes = true;
		mind.eyesToClear = false;
	}
	if (mind.step >= mind.agenda.size())
	{
		return commands;
	}
	const auto& step = mind.agenda[mind.step];
	// Each turn, a try to start an action that the body does not have picks a side first
	if (step.retriedClip == RetriedClip::Static || (step.retriedClip == RetriedClip::Individual && !senses.held.has_value()))
	{
		static_cast<void>(gameRandom(2));
	}
	// A step with something to look at has the head on it for as long as it lasts
	commands.gaze = LookOf(step);
	// The creature's moves in a dance with villagers start and stop its poses and its dance themselves
	const bool villageDance = step.kind == Step::Kind::Dance && step.dance.kind == DanceOrder::Kind::Dance;
	// A cast holds the loop of its pose until its time is up
	const bool sitting =
	    ((step.kind == Step::Kind::Static || step.kind == Step::Kind::Cast) && mind.stepStarted) || villageDance;
	const bool dancing = step.kind == Step::Kind::PracticeDance || villageDance;
	// A step pulls its face again every few seconds from its first try to start, also while it waits for the body
	if ((mind.stepStarted || IsStep(mind.facePulled, mind)) && step.face != creature_face::Cue::None)
	{
		if (FaceAgainDue(mind.faceSeconds, senses.seconds))
		{
			commands.face = PullFace(mind, step.face, senses, random, gameRandom);
		}
	}
	// A sit nobody is waiting on any more ends
	if (senses.bodyLooping && !sitting)
	{
		commands.endSit = true;
	}
	// A dance nobody waits on any more stops
	if (senses.bodyDancing && !dancing)
	{
		commands.endDance = true;
	}

	if (!mind.stepStarted)
	{
		const auto need = HandsNeededFor(step);
		const bool handsChecked =
		    mind.handsChecked.has_value() && mind.handsChecked->serial == mind.serial && mind.handsChecked->step == mind.step;
		// The hands that let go keep the body busy
		const bool bodyBusy = senses.bodyBusy || (handsChecked && senses.hands == HandsState::Busy);
		// A step that needs a thing in the hand stops the rest of the agenda when the hand is empty
		if (need == HandsNeed::Held)
		{
			if (!senses.held.has_value())
			{
				GiveUpAgenda(mind);
				return commands;
			}
		}
		// A step that needs empty hands first makes the creature let go of the thing that it holds. The hands are checked
		// only until the let-go starts. From the next turn the step tries to start: the let-go keeps the body busy, but a
		// faint starts at once and cuts it.
		else if (need != HandsNeed::Any && senses.held.has_value() && !handsChecked)
		{
			if (need == HandsNeed::FreeOrTarget && step.order.object == senses.held)
			{
				// It holds the thing that the step picks up already, so it does not do the step
				SkipStep(mind);
				return commands;
			}
			// If it cannot start to let go, it tries again next turn
			if (CheckHands(true, senses.bodyBusy, senses.hands == HandsState::Busy) == HandsCheck::LetGo)
			{
				commands.letGo = need == HandsNeed::Free ? LetGo::AsItChooses : LetGo::PutDown;
				mind.handsChecked = IdleMind::StepRef {.serial = mind.serial, .step = mind.step};
			}
			return commands;
		}
		// Each step pulls the face it is meant to on its first try to start, also when the body is busy and the step
		// waits. The face holds for as long as the face goes with.
		if (!IsStep(mind.facePulled, mind))
		{
			commands.face = PullFace(mind, step.face, senses, random, gameRandom);
			mind.facePulled = IdleMind::StepRef {.serial = mind.serial, .step = mind.step};
		}
		// A toss of a thing that the hand holds but that the creature does not carry fails at once, also while the body
		// is busy. A throw and an eat of such a thing wait for the body first.
		if (step.kind == Step::Kind::Object && step.order.kind == ObjectOrder::Kind::Discard && !senses.heldCarried)
		{
			GiveUpAgenda(mind);
			return commands;
		}
		// A swap of minds waits until no spell is on either creature, bringing those on itself to their end meanwhile
		if (step.kind == Step::Kind::SwapMind && !senses.swap.spellsGone)
		{
			commands.finishSpells = true;
			return commands;
		}
		// Nothing starts while the body still plays an action. A stop starts while the body moves, to stop it, and a
		// faint starts at once. The steps of a teleport also start at once. A walk to a point or to a thing starts
		// while the body only moves: the new walk takes the place of the old one at once. A step that tries to start its
		// clip tries again on each turn. A follow of the camera, a turn to the camera and a step with nothing to do
		// start at once: the body is not examined until the turn after the start. The start of the turn to the camera
		// asks the body to turn, and the body can refuse. The steps that are done on their first turn start at once too.
		const bool teleportStep = step.kind == Step::Kind::Teleport || step.kind == Step::Kind::Transported ||
		                          step.kind == Step::Kind::StopTeleportControl;
		const bool startsAtOnce =
		    step.kind == Step::Kind::FollowCamera || step.kind == Step::Kind::Empty || StartsWhileBodyIsBusy(step) ||
		    (step.kind == Step::Kind::Move && (step.movement.kind == Movement::Kind::TurnToFaceCamera ||
		                                       step.movement.kind == Movement::Kind::TurnToFaceGiverCamera));
		const bool walkOnTheWay =
		    StartsOnTheWay(step) && senses.onlyMoves && !(handsChecked && senses.hands == HandsState::Busy);
		if (step.kind != Step::Kind::Wait && step.kind != Step::Kind::WatchCamera && step.kind != Step::Kind::StopMoving &&
		    step.kind != Step::Kind::PracticeDance && !IsFaint(step) && !teleportStep && !startsAtOnce && !walkOnTheWay &&
		    bodyBusy)
		{
			if (TriesToStartClip(step, senses))
			{
				RefusedStart(mind, step, senses, gameRandom, commands);
			}
			return commands;
		}
		// A dance alone waits while the body is busy. A walk, a turn and an action on a thing make the body busy too.
		if (step.kind == Step::Kind::PracticeDance && bodyBusy)
		{
			return commands;
		}
		// When the body does not have a clip of the step, the step does not start. The creature tries again on each turn
		// until a new plan or its death replaces the agenda.
		if (senses.clipMissing)
		{
			RefusedStart(mind, step, senses, gameRandom, commands);
			return commands;
		}
		mind.stepStarted = true;
		mind.handsChecked.reset();
		mind.movedOn = mind.step;
		mind.stepSeconds = 0.0f;
		mind.faintLay = false;
		mind.faintTurns = 0;
		switch (step.kind)
		{
		case Step::Kind::Action:
			commands.playOnce = step.animation;
			commands.mirrored = PickIndividualSide(gameRandom, senses);
			break;
		case Step::Kind::Static:
			commands.startSequence = step.sequence;
			commands.holdLoop = step.holdLoop;
			commands.fainted = step.faint;
			// The turns that the rest or the sleep tried to start count, and the turn that it starts counts too
			mind.restTurns = (IsCurrentStep(mind, mind.startTried) ? mind.startTries : 0) + 1;
			mind.watchTurns = 0;
			if (IsFaint(step))
			{
				// A faint stops the walk and cuts the action that the body plays. Real time does not change its side.
				commands.mirrored = PickSide(gameRandom);
				commands.stopMoving = true;
				commands.cutBody = true;
			}
			else
			{
				commands.mirrored = PickStaticSide(gameRandom, senses.realTime);
			}
			if (step.closedEyes)
			{
				// The eyes of a sleep or a faint close later, when the body lies still in the loop
				if (!EyesShutInLoop(step))
				{
					commands.eyes = Eyes::Closed;
				}
				// Asleep or out cold, it pulls no face
				commands.face.reset();
				commands.relaxFace = true;
			}
			break;
		case Step::Kind::Move:
			commands.move = step.movement;
			if (step.movement.kind == Movement::Kind::Nearby)
			{
				commands.move->kind = Movement::Kind::ToPoint;
				commands.move->point = senses.position + step.movement.point;
			}
			break;
		case Step::Kind::Gesture:
			commands.gesture = static_cast<uint32_t>(step.animation);
			break;
		case Step::Kind::Cast:
			commands.startSequence = step.sequence;
			commands.keepsPace = true;
			commands.mirrored = PickStaticSide(gameRandom, senses.realTime);
			mind.castDone = false;
			mind.castTurns = 0;
			break;
		case Step::Kind::Object:
			commands.object = step.order;
			if (step.order.kind == ObjectOrder::Kind::ThrowNearby)
			{
				commands.object->kind = ObjectOrder::Kind::Throw;
				commands.object->point = senses.position + step.order.point;
			}
			break;
		case Step::Kind::Douse:
			// The water lands at once, and the step is done
			commands.douse = step.object;
			FinishStep(mind);
			break;
		case Step::Kind::CoolDown:
			commands.coolDown = true;
			FinishStep(mind);
			break;
		case Step::Kind::HealItself:
			commands.healItself = true;
			FinishStep(mind);
			break;
		case Step::Kind::Shatter:
			commands.shatter = step.object;
			FinishStep(mind);
			break;
		case Step::Kind::Repair:
			commands.repair = step.object;
			FinishStep(mind);
			break;
		case Step::Kind::Heal:
			commands.heal = step.object;
			FinishStep(mind);
			break;
		case Step::Kind::Face:
			// The face is held for the step's seconds, and the next step starts on the next turn
			if (const auto face = creature_face::FaceOf(step.animation))
			{
				commands.face =
				    creature_face::Request {.face = *face, .milliseconds = step.seconds * 1000.0f, .cue = step.face};
			}
			commands.effect = step.effect;
			FinishStep(mind);
			break;
		case Step::Kind::FollowCamera:
		{
			// The time is counted in whole turns from the next
			const auto turnsPerSecond = static_cast<float>(std::lround(1.0f / senses.seconds));
			mind.followCameraTurns = static_cast<uint32_t>(step.seconds * turnsPerSecond);
			break;
		}
		case Step::Kind::UseUp:
			commands.useUp = step.object;
			FinishStep(mind);
			break;
		case Step::Kind::UseUpHeld:
			commands.useUpHeld = true;
			FinishStep(mind);
			break;
		case Step::Kind::ActOnItself:
			commands.actOnItself = true;
			FinishStep(mind);
			break;
		case Step::Kind::Observe:
			// Taken in at once
			commands.effect = Effect::Observed;
			commands.effectObject = step.object;
			FinishStep(mind);
			break;
		case Step::Kind::WatchCamera:
			WatchCamera(mind, step, senses, commands);
			break;
		case Step::Kind::SwapMind:
			commands.playOnce = step.animation;
			commands.mirrored = PickIndividualSide(gameRandom, senses);
			commands.startSwapBeam = true;
			break;
		case Step::Kind::Overlay:
			// Nothing starts on top of the body while something already plays there; the step waits for it all the same
			commands.playOverlay = step.animation;
			break;
		case Step::Kind::WatchFlying:
			mind.watchTurns = 0;
			WatchFlying(mind, senses, random);
			break;
		case Step::Kind::WatchHand:
			mind.watchTurns = 0;
			WatchHand(mind, step, senses, random, commands);
			break;
		case Step::Kind::Inspect:
		{
			// The time is picked as the step starts, and counted in whole turns from the next
			const auto turnsPerSecond = static_cast<float>(std::lround(1.0f / senses.seconds));
			const auto extra =
			    static_cast<float>(random(game_random::k_FloatRandRange)) * step.extraSeconds * game_random::k_FloatRandScale;
			mind.inspectTurns = static_cast<uint32_t>((extra + step.seconds) * turnsPerSecond);
			break;
		}
		case Step::Kind::StopMoving:
			commands.stopMoving = true;
			break;
		case Step::Kind::Dance:
			StartDanceStep(mind, step, senses, commands);
			break;
		case Step::Kind::FollowWatching:
			mind.watchTurns = 0;
			mind.followHolding = false;
			mind.followHoldTurns = 0;
			mind.followMoved = false;
			FollowWatching(mind, step, senses, random, commands);
			break;
		case Step::Kind::FollowHand:
			// The time is set as the step starts, and the hand is followed from the next turn
			mind.followHandTurns = creature_player_attention::FollowHandTurns(
			    static_cast<uint32_t>(std::lround(1.0f / std::max(senses.seconds, 1e-3f))));
			break;
		case Step::Kind::WatchPlayer:
			WatchPlayer(mind, senses, commands);
			break;
		case Step::Kind::RenderImmobile:
			RenderImmobile(mind, step, senses, commands);
			break;
		case Step::Kind::Fight:
			commands.startFight = step.object;
			break;
		case Step::Kind::RaiseTotem:
		case Step::Kind::LowerTotem:
			// The totem starts to move on the next turn
			break;
		case Step::Kind::MakeDisciple:
			commands.makeDisciple = step.object;
			commands.effect = step.effect;
			FinishStep(mind);
			break;
		// The steps of a joint action are done as they start. When the other creature refuses to be the partner, the
		// mind's owner gives up the rest of the agenda.
		case Step::Kind::RequestPartner:
			if (step.object.has_value())
			{
				commands.requestPartner = Commands::PartnerRequest {.creature = *step.object, .kind = step.request};
			}
			FinishStep(mind);
			break;
		case Step::Kind::Order:
			if (step.object.has_value() && step.ordered != nullptr)
			{
				commands.order = Commands::PartnerOrder {.partner = *step.object, .step = step.ordered};
			}
			FinishStep(mind);
			break;
		case Step::Kind::Demobilise:
		case Step::Kind::Remobilise:
			if (step.object.has_value())
			{
				commands.demobilise = std::pair {*step.object, step.kind == Step::Kind::Demobilise};
			}
			FinishStep(mind);
			break;
		case Step::Kind::WaitForStill:
			// The step looks at the object on the turn that it starts too
			if (!senses.objectMoving)
			{
				FinishStep(mind);
			}
			break;
		case Step::Kind::WaitForFree:
			// The count of turns starts on the next turn
			mind.watchTurns = 0;
			break;
		case Step::Kind::WaitUntilFree:
			// The step looks at the body on the turn that it starts too
			if (!senses.bodyBusy)
			{
				FinishStep(mind);
			}
			break;
		case Step::Kind::Teleport:
			// The step does not end by itself: a refused jump gives up the agenda, and a jump gives a new plan
			commands.teleport = step.object;
			break;
		case Step::Kind::Transported:
			commands.carry = Commands::Carry::Start;
			break;
		case Step::Kind::StopTeleportControl:
			commands.stopTeleportControl = true;
			FinishStep(mind);
			break;
		case Step::Kind::Empty:
			// The step is done on the next turn
			break;
		case Step::Kind::Apply:
			commands.effect = step.effect;
			FinishStep(mind);
			break;
		case Step::Kind::ApplyDesire:
			// The step does not end by itself: the new plan takes the place of the agenda, and with no plan the agenda
			// stops
			if (step.object.has_value())
			{
				commands.applyDesire = Commands::DesireOn {.desire = step.desire, .thing = *step.object};
			}
			else
			{
				GiveUpAgenda(mind);
			}
			break;
		case Step::Kind::Kiss:
			commands.kiss = Commands::KissOrder {.partner = step.object};
			break;
		case Step::Kind::PracticeDance:
			commands.startDance = creature_dance::k_PracticeMoveMs;
			mind.danceTurns = static_cast<uint32_t>(step.seconds * static_cast<float>(creature_dance::k_TurnsPerSecond));
			break;
		case Step::Kind::Communicate:
			// It shows what it feels now, and it does not want to show its state again for a time
			commands.communicated = senses.communication;
			mind.shown = senses.communication.desire;
			if (senses.communication.animation.has_value())
			{
				commands.playOnce = senses.communication.animation;
				commands.mirrored = PickIndividualSide(gameRandom, senses);
			}
			else
			{
				// With nothing to show, it stops what it does as a failure
				GiveUpAgenda(mind);
			}
			break;
		case Step::Kind::Wait:
			// The wait counts its first turn too
			mind.watchTurns = 0;
			CountWaitTurn(mind, step, senses);
			break;
		case Step::Kind::WaitInMap:
			break;
		}
		return commands;
	}

	mind.stepSeconds += senses.seconds;
	switch (step.kind)
	{
	case Step::Kind::Wait:
		CountWaitTurn(mind, step, senses);
		break;
	case Step::Kind::Communicate:
		if (!senses.bodyBusy)
		{
			FinishStep(mind);
		}
		break;
	case Step::Kind::Action:
		if (!senses.bodyBusy)
		{
			if (step.effect != Effect::None)
			{
				commands.effect = step.effect;
			}
			FinishStep(mind);
		}
		break;
	case Step::Kind::Static:
	{
		// A faint counts its time only from when the creature first lies still. Its wait starts on the turn after that,
		// and the creature gets up on the turn after the wait ends. After that, the wait stops only while the body plays
		// an action that is not the loop of the faint.
		if (step.faint && !mind.sitEnding)
		{
			const auto wait = creature_fight::LyingTurn(
			    creature_fight::FaintCount {.turns = mind.faintTurns, .started = mind.faintLay},
			    {.faintStarted = true, .acts = senses.bodyBusy && !senses.bodyLooping, .lies = senses.bodyLooping},
			    -static_cast<int32_t>(k_FaintWaitTurnsLate));
			mind.faintTurns = wait.turns;
			mind.faintLay = wait.started;
		}
		// The eyes of a sleep or a faint close when the body first lies still in the loop
		if (step.closedEyes && EyesShutInLoop(step) && !mind.sitEnding && senses.bodyLooping && !IsStep(mind.eyesShut, mind))
		{
			commands.eyes = Eyes::Closed;
			mind.eyesShut = IdleMind::StepRef {.serial = mind.serial, .step = mind.step};
		}
		// Sitting rests it a little every turn until it is told to get up
		commands.restSitting = !mind.sitEnding && step.sequence[1] == animations::k_Sit;
		// Each turn of the rest its fire stops, until it starts to get up
		if (step.untilBetter && !mind.sitEnding)
		{
			++mind.restTurns;
			commands.coolDown = true;
		}
		const bool better = senses.better && mind.restTurns > k_FewestRestTurns;
		// A timed pose counts its turns from the turn after it starts. A faint counts its own wait above.
		bool timeUp = false;
		if (!step.untilBetter && !step.untilRested && !mind.sitEnding)
		{
			if (step.faint)
			{
				timeUp = FaintWaitDone(mind.faintTurns, step.seconds);
			}
			else
			{
				++mind.watchTurns;
				timeUp = StaticTimeUp(mind.watchTurns, step.seconds);
			}
		}
		const bool done = mind.wakeWanted || (step.untilBetter ? better : step.untilRested ? senses.rested : timeUp);
		// A body that stands with no action does not end a faint, as after a script moves the creature. The faint ends
		// only when its wait ends.
		const bool faintHolds = step.faint && !mind.sitEnding;
		if (!senses.bodyBusy && !faintHolds)
		{
			if (step.closedEyes)
			{
				commands.eyes = Eyes::Normal;
			}
			// The body played the whole sequence before its time was up: it still takes effect. The rest takes effect once
			// the creature is up.
			if (!mind.sitEnding || step.untilBetter)
			{
				commands.effect = step.effect;
			}
			FinishStep(mind);
		}
		else if (!mind.sitEnding && done)
		{
			commands.endSit = true;
			if (!step.untilBetter)
			{
				commands.effect = step.effect;
			}
			// The eyes of a sleep or a faint open as the get-up starts
			if (step.closedEyes && EyesShutInLoop(step))
			{
				commands.eyes = Eyes::Normal;
			}
			// A sleep does not wait for the get-up. The next step tries to start on the next turn while the get-up plays.
			if (step.untilRested)
			{
				mind.wakeWanted = false;
				FinishStep(mind);
				break;
			}
			mind.sitEnding = true;
			mind.movedOn = mind.step;
			mind.wakeWanted = false;
		}
		break;
	}
	case Step::Kind::Object:
		if (senses.hands == HandsState::Done)
		{
			commands.effect = step.effect;
			if (step.effect == Effect::TookFromHand || step.effect == Effect::Stomped)
			{
				commands.effectObject = step.order.object;
			}
			commands.lastStep = mind.step + 1 == mind.agenda.size();
			FinishStep(mind);
		}
		else if (senses.hands != HandsState::Busy)
		{
			// It couldn't, or was stopped: the rest of the agenda is no use without it
			mind.step = mind.agenda.size();
			mind.stepStarted = false;
			mind.gaveUp = true;
		}
		else if (senses.handsTouched && step.order.kind == ObjectOrder::Kind::PickUp)
		{
			// After the hold, on each turn that the body still plays the pick-up, the head looks at no point
			commands.noLookPoint = true;
		}
		else if (senses.handsTouched && step.order.kind == ObjectOrder::Kind::Destroy && step.order.object.has_value())
		{
			// After the blow, on each turn that the body still plays the stomp, the thing under the foot takes it again
			commands.stomp = Commands::Stomp {.object = *step.order.object, .lastStep = mind.step + 1 == mind.agenda.size()};
		}
		break;
	case Step::Kind::Cast:
		if (!mind.castDone)
		{
			// The miracle is cast as the loop of the pose begins, and held for whole turns from the next. Until the
			// loop begins, the step waits. A pose that ends before its loop keeps the step waiting, as in the game.
			if (senses.bodyLooping)
			{
				commands.cast = step.cast;
				mind.castDone = true;
				const auto turnsPerSecond = static_cast<uint32_t>(std::lround(1.0f / senses.seconds));
				mind.castTurns = static_cast<uint32_t>(step.seconds * static_cast<float>(turnsPerSecond));
			}
		}
		else if (mind.castTurns > 0)
		{
			--mind.castTurns;
		}
		else
		{
			// Its time up, it lets the miracle go and ends the pose, done once the end has played
			if (!mind.sitEnding)
			{
				commands.releaseCast = true;
				commands.endSit = true;
				mind.sitEnding = true;
				mind.movedOn = mind.step;
			}
			if (!senses.bodyBusy)
			{
				FinishStep(mind);
			}
		}
		break;
	case Step::Kind::FollowCamera:
		FollowCamera(mind, step, senses, commands);
		break;
	case Step::Kind::Douse:
	case Step::Kind::CoolDown:
	case Step::Kind::HealItself:
	case Step::Kind::Shatter:
	case Step::Kind::Repair:
	case Step::Kind::Heal:
	case Step::Kind::Face:
	case Step::Kind::UseUp:
	case Step::Kind::UseUpHeld:
	case Step::Kind::Observe:
	case Step::Kind::ActOnItself:
		// Done as it starts
		FinishStep(mind);
		break;
	case Step::Kind::WatchCamera:
		WatchCamera(mind, step, senses, commands);
		break;
	case Step::Kind::Overlay:
		if (!senses.overlayPlaying)
		{
			FinishStep(mind);
		}
		break;
	case Step::Kind::WatchFlying:
		WatchFlying(mind, senses, random);
		break;
	case Step::Kind::WatchHand:
		WatchHand(mind, step, senses, random, commands);
		break;
	case Step::Kind::Inspect:
		if (mind.inspectTurns > 0)
		{
			--mind.inspectTurns;
		}
		if (mind.inspectTurns == 0)
		{
			commands.effect = Effect::Inspected;
			commands.effectObject = step.object;
			FinishStep(mind);
		}
		break;
	case Step::Kind::SwapMind:
		// From the turn after the beam starts it takes on the shape, done once it has, whether or not the action has
		// played out
		if (senses.swap.settled)
		{
			FinishStep(mind);
		}
		else
		{
			commands.easeSwap = true;
		}
		break;
	case Step::Kind::WaitInMap:
		if (!senses.objectInMap.has_value())
		{
			// What it waited for is gone: the rest of the agenda is no use without it
			mind.step = mind.agenda.size();
			mind.stepStarted = false;
			mind.gaveUp = true;
		}
		else if (*senses.objectInMap)
		{
			FinishStep(mind);
		}
		break;
	case Step::Kind::StopMoving:
		// It waits until the body is not busy: its walk slows to a stop, and no action plays. While it waits, a point
		// that the arm holds stops.
		if (!senses.bodyBusy)
		{
			FinishStep(mind);
		}
		else
		{
			commands.stopPointing = true;
		}
		break;
	case Step::Kind::Dance:
		DanceStepTurn(mind, step, senses, commands, gameRandom);
		break;
	case Step::Kind::FollowWatching:
		FollowWatching(mind, step, senses, random, commands);
		break;
	case Step::Kind::FollowHand:
		FollowHand(mind, senses, commands);
		break;
	case Step::Kind::WatchPlayer:
		WatchPlayer(mind, senses, commands);
		break;
	case Step::Kind::RenderImmobile:
		RenderImmobile(mind, step, senses, commands);
		break;
	case Step::Kind::PracticeDance:
		if (mind.danceTurns > 0)
		{
			--mind.danceTurns;
		}
		// The dance alone ends its action, which stops the dance
		if (mind.danceTurns == 0)
		{
			commands.endDance = true;
			FinishStep(mind);
		}
		break;
	case Step::Kind::Fight:
		// The fight has ended. A fight that is the last step of the agenda lets the end of the plan decrease its desire.
		if (!senses.fighting)
		{
			commands.seesToDesire = mind.step + 1 == mind.agenda.size();
			FinishStep(mind);
		}
		break;
	case Step::Kind::RaiseTotem:
	case Step::Kind::LowerTotem:
	{
		if (!senses.totemShare.has_value() || !step.object.has_value())
		{
			GiveUpAgenda(mind);
			break;
		}
		const auto turn = creature_town_help::MoveTotemTurn(*senses.totemShare, step.kind == Step::Kind::RaiseTotem);
		commands.totem = Commands::TotemShare {.totem = *step.object, .share = turn.share};
		if (turn.done)
		{
			commands.effect = step.effect;
			FinishStep(mind);
		}
		break;
	}
	case Step::Kind::MakeDisciple:
	case Step::Kind::RequestPartner:
	case Step::Kind::Order:
	case Step::Kind::Demobilise:
	case Step::Kind::Remobilise:
		FinishStep(mind);
		break;
	case Step::Kind::WaitForStill:
		if (!senses.objectMoving)
		{
			FinishStep(mind);
		}
		break;
	case Step::Kind::WaitUntilFree:
		if (!senses.bodyBusy)
		{
			FinishStep(mind);
		}
		break;
	case Step::Kind::WaitForFree:
		++mind.watchTurns;
		if (mind.watchTurns > step.turns.value_or(0) && !senses.objectBusy)
		{
			FinishStep(mind);
		}
		break;
	case Step::Kind::Apply:
	case Step::Kind::Empty:
		FinishStep(mind);
		break;
	case Step::Kind::ApplyDesire:
	case Step::Kind::Teleport:
	case Step::Kind::StopTeleportControl:
		break;
	case Step::Kind::Transported:
		// The world tells the mind when the carry is done (see CarryDone)
		commands.carry = Commands::Carry::Turn;
		break;
	case Step::Kind::Kiss:
		if (!senses.bodyBusy)
		{
			FinishStep(mind);
		}
		break;
	case Step::Kind::Gesture:
	case Step::Kind::Move:
		if (step.kind == Step::Kind::Gesture || IsSubMove(step.movement.kind))
		{
			if (senses.subMove == SubMove::Done)
			{
				commands.effect = step.effect;
				FinishStep(mind);
			}
			else if (senses.subMove == SubMove::Failed)
			{
				mind.step = mind.agenda.size();
				mind.stepStarted = false;
				mind.gaveUp = true;
			}
			break;
		}
		// Done once it has arrived or given up, or its time is up. A run away tells what it ran from.
		if (!senses.moving)
		{
			if (step.effect == Effect::RanAway)
			{
				commands.effect = Effect::RanAway;
				commands.effectObject = step.gaze.has_value() ? step.gaze->object : std::nullopt;
			}
			FinishStep(mind);
		}
		else if (step.seconds > 0.0f && mind.stepSeconds >= step.seconds)
		{
			commands.stopMoving = true;
			FinishStep(mind);
		}
		break;
	}
	return commands;
}
} // namespace

Commands creature_mind::Think(IdleMind& mind, const Senses& senses, const Random& random, const Random& gameRandom)
{
	auto commands = TurnOfThought(mind, senses, random, gameRandom);
	// An agenda that ended in this turn stops what the creature does in this turn
	if (mind.eyesToClear)
	{
		commands.restEyes = true;
		mind.eyesToClear = false;
	}
	return commands;
}

size_t creature_mind::MainStep(std::span<const Step> agenda)
{
	if (agenda.empty())
	{
		return 0;
	}
	const auto main = std::ranges::find_if(agenda.rbegin(), agenda.rend(),
	                                       [](const Step& step) { return step.main || step.kind == Step::Kind::Cast; });
	return main == agenda.rend() ? agenda.size() - 1 : static_cast<size_t>(std::distance(main, agenda.rend()) - 1);
}

std::optional<uint32_t> creature_mind::InteractingWith(const IdleMind& mind)
{
	if (!mind.stepStarted || mind.step >= mind.agenda.size())
	{
		return std::nullopt;
	}
	const auto& step = mind.agenda[mind.step];
	if (step.kind != Step::Kind::RaiseTotem && step.kind != Step::Kind::LowerTotem)
	{
		return std::nullopt;
	}
	return step.object;
}

bool creature_mind::AtOrPastMainStep(std::span<const Step> agenda, size_t step, bool showingDesire)
{
	if (agenda.empty())
	{
		return false;
	}
	return showingDesire || step >= MainStep(agenda);
}

Step creature_mind::OrderedStep(const Step& ordered, bool toItself)
{
	if (toItself)
	{
		return Object({.kind = ObjectOrder::Kind::PickUp});
	}
	return ordered;
}
