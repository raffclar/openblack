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
#include <numbers>

#include <glm/geometric.hpp>

#include "Common/GameRandom.h"
#include "Creature/CreatureDance.h"
#include "Creature/CreatureEmoteAgendas.h"
#include "Creature/CreatureLayers.h"
#include "Creature/CreatureObjectActions.h"
#include "Creature/CreatureTownHelp.h"

using namespace openblack;
using namespace openblack::creature_mind;
namespace animations = openblack::creature_layers::animations;

namespace
{
/// Random fractions of a second are drawn in this many steps
constexpr uint32_t k_FractionSteps = 1000;
/// A face is due again once its wait is within this much of over, so that the turns' rounding doesn't delay it
constexpr float k_FaceDue = 1e-3f;
/// The turns between when a fainted creature lies still and when its wait starts, plus the turn between the end of
/// the wait and the start of the get-up
constexpr float k_FaintWaitTurnsLate = 3.0f;

float RandomFraction(const Random& random)
{
	return static_cast<float>(random(k_FractionSteps)) / static_cast<float>(k_FractionSteps);
}

/// The movements that the creature's movement says are done or have failed
bool IsSubMove(Movement::Kind kind)
{
	return kind == Movement::Kind::GoNearObject || kind == Movement::Kind::GetAwayFromObject ||
	       kind == Movement::Kind::TurnToFaceObject || kind == Movement::Kind::TurnToFaceAwayFromObject ||
	       kind == Movement::Kind::ToThrowPosition;
}

void FinishStep(IdleMind& mind)
{
	++mind.step;
	mind.stepStarted = false;
	mind.stepSeconds = 0.0f;
	mind.sitEnding = false;
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

std::vector<Step> creature_mind::BeIdle(const Random& random)
{
	std::vector<Step> agenda;
	for (int i = 0; i < k_IdleRepeats; ++i)
	{
		Step wait {.kind = Step::Kind::Wait, .seconds = k_IdleWaitSeconds + RandomFraction(random)};
		wait.face = creature_face::Cue::Idle;
		agenda.push_back(wait);
		auto yawn = Action(animations::k_Tired, true);
		yawn.face = creature_face::Cue::Idle;
		agenda.push_back(yawn);
	}
	return agenda;
}

Step creature_mind::SitDown(const Random& random)
{
	auto sit = Static({animations::k_StartSit, animations::k_Sit, animations::k_EndSit},
	                  static_cast<float>(k_SitSeconds + random(k_SitExtraSeconds)));
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
	agenda.push_back(sleep);
	agenda.push_back(Action(animations::k_Confused, true));
	return WithFace(std::move(agenda), creature_face::Cue::Grimace);
}

std::vector<Step> creature_mind::Eat(uint32_t food, bool holdingEatable)
{
	std::vector<Step> agenda;
	if (!holdingEatable)
	{
		agenda.push_back(PickUp(food));
	}
	agenda.push_back(Object({.kind = ObjectOrder::Kind::Keep, .animation = creature_object_actions::k_ExamineObject}));
	agenda.push_back(Object({.kind = ObjectOrder::Kind::Eat}, Effect::Eat));
	return WithFace(std::move(agenda), creature_face::Cue::Curiosity);
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
		agenda.push_back(PickUp(food));
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
		agenda.push_back(PickUp(object));
	}
	const auto keep = creature_object_actions::k_FirstKeepAnimation +
	                  random(static_cast<uint32_t>(creature_object_actions::k_KeepAnimationCount));
	const auto letGo = random(k_PutDownLots) >= k_TossLots ? ObjectOrder::Kind::PutDown : ObjectOrder::Kind::Discard;
	agenda.push_back(Object({.kind = ObjectOrder::Kind::Keep, .animation = keep}, Effect::Examined));
	agenda.push_back(Object({.kind = letGo}));
	return WithFace(std::move(agenda), creature_face::Cue::Curiosity);
}

std::vector<Step> creature_mind::ThrowAbout(uint32_t object, const Random& random)
{
	constexpr uint32_t k_Degrees = 360;
	const auto angle = static_cast<float>(random(k_Degrees)) * std::numbers::pi_v<float> / 180.0f;
	const auto distance = k_ThrowAroundDistance + static_cast<float>(random(k_ThrowAroundExtra));
	return WithFace({PickUp(object), Object({.kind = ObjectOrder::Kind::ThrowNearby,
	                                         .point = distance * glm::vec2(std::cos(angle), std::sin(angle))},
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
	Step goTo {.kind = Step::Kind::Move, .face = creature_face::Cue::Compassion};
	goTo.movement = {.kind = Movement::Kind::ToThrowPosition, .object = store, .maxDistance = height};
	agenda.push_back(goTo);
	Step turn {.kind = Step::Kind::Move, .seconds = k_FaceStoreSeconds, .effect = Effect::Completed};
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
	agenda.push_back(Object({.kind = ObjectOrder::Kind::PutDown}));
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
		if (!by->itself)
		{
			goNear.gaze = Gaze {.object = by->object};
		}
		agenda.push_back(goNear);
		Step turnAway {.kind = Step::Kind::Move};
		turnAway.movement = {.kind = Movement::Kind::TurnToFaceAwayFromObject, .object = by->object};
		agenda.push_back(turnAway);
	}
	auto poo = Static({animations::k_StartPoo, animations::k_Poo, animations::k_EndPoo}, k_PooSeconds);
	poo.effect = Effect::Poo;
	agenda.push_back(poo);
	return WithFace(std::move(agenda), creature_face::Cue::Smile);
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
	return {faint};
}

std::vector<Step> creature_mind::EatHeld()
{
	return WithFace({Object({.kind = ObjectOrder::Kind::Eat}, Effect::Eat)}, creature_face::Cue::Curiosity);
}

std::vector<Step> creature_mind::TakeFromHand(uint32_t object, glm::vec2 camera, bool leftHand)
{
	Step turn {.kind = Step::Kind::Move};
	turn.movement = {.kind = Movement::Kind::TurnToFace, .point = camera};
	auto take = Object({.kind = ObjectOrder::Kind::TakeFromHand, .object = object, .mirrored = leftHand}, Effect::TookFromHand);
	std::vector<Step> agenda {turn, take};
	for (auto& step : agenda)
	{
		step.gaze = Gaze {.object = object};
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
	return WithFace({Action(animation, false)}, creature_face::Cue::AttitudeToPlayer);
}

std::vector<Step> creature_mind::Communicate(glm::vec2 camera)
{
	const Gaze gaze {.camera = true};
	Step turn {.kind = Step::Kind::Move, .gaze = gaze};
	turn.movement = {.kind = Movement::Kind::TurnToFace, .point = camera};
	return WithFace({turn, {.kind = Step::Kind::Communicate, .gaze = gaze}}, creature_face::Cue::AttitudeToPlayer);
}

std::vector<Step> creature_mind::FaceAndEmote(glm::vec2 point, size_t animation)
{
	Step turn {.kind = Step::Kind::Move};
	turn.movement = {.kind = Movement::Kind::TurnToFace, .point = point};
	return WithFace({turn, Action(animation, false)}, creature_face::Cue::AttitudeToPlayer);
}

std::vector<Step> creature_mind::ApproachAndEmote(uint32_t object, size_t animation)
{
	Step approach {.kind = Step::Kind::Move};
	approach.movement = {.kind = Movement::Kind::ToObject, .object = object, .maxDistance = k_ApproachDistance};
	return WithFace({approach, Action(animation, false)}, creature_face::Cue::AttitudeToPlayer);
}

std::vector<Step> creature_mind::LookAt(glm::vec2 point, float seconds)
{
	Step turn {.kind = Step::Kind::Move};
	turn.movement = {.kind = Movement::Kind::TurnToFace, .point = point};
	return WithFace({turn, {.kind = Step::Kind::Wait, .seconds = seconds}}, creature_face::Cue::Amazed);
}

std::vector<Step> creature_mind::RunFrom(glm::vec2 point)
{
	Step flee {.kind = Step::Kind::Move};
	flee.movement = {.kind = Movement::Kind::FleeFrom, .point = point, .run = true};
	return WithFace({flee}, creature_face::Cue::Fear);
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

bool creature_mind::IsAsleep(const IdleMind& mind)
{
	const auto* step = CurrentStep(mind);
	return step != nullptr && step->kind == Step::Kind::Static && step->untilRested;
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
	auto wait = k_HangAroundWaitSeconds + (k_HangAroundWaitExtraSeconds * RandomFraction(random));
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
		                   k_HangAroundSitSeconds + (k_HangAroundSitExtraSeconds * RandomFraction(random)));
		step.mirrorAtRandom = true;
		return step;
	};
	const auto face = [](glm::vec3 point) {
		return Step {.kind = Step::Kind::Move,
		             .movement = {.kind = Movement::Kind::FacePoint, .point = {point.x, point.z}},
		             .gaze = Gaze {.point = point}};
	};
	if (lot <= k_HangAroundOutLots)
	{
		// The way out from home is away from the temple, and it includes the difference in height
		auto away = places.home - temple;
		if (std::abs(away.x) > k_HangAroundSameSpot || std::abs(away.z) > k_HangAroundSameSpot)
		{
			away = glm::normalize(away);
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
	mind.wakeWanted = false;
	mind.gaveUp = false;
	mind.usedUpHeld.reset();
	// Taking up something else, its eyes look as usual again
	mind.eyesToClear = mind.eyesHeld;
}

std::optional<creature_face::Request> creature_mind::PullFace(IdleMind& mind, creature_face::Cue cue, const Senses& senses,
                                                              const Random& random)
{
	mind.faceSeconds = k_FaceRepeatSeconds;
	auto feelings = senses.feelings;
	feelings.variety = mind.faceVariety;
	feelings.idlePick = random(creature_face::k_IdleFaceCount);
	const auto face = creature_face::Choose(cue, feelings);
	if (face.has_value())
	{
		mind.faceVariety += random(k_FaceVarietyStep);
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
/// A turn of watching the player's hand
void WatchHand(IdleMind& mind, const Step& step, const Senses& senses, Commands& commands)
{
	++mind.watchTurns;
	if (!senses.hand.has_value() || !(senses.seconds > 0.0f))
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
	case Step::Kind::Fight:
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
		break;
	}
	return HandsNeed::Any;
}

bool creature_mind::PutsDownGently(bool heldIsCreature, std::optional<creature_desires::Desire> use)
{
	using creature_desires::Desire;
	return heldIsCreature ||
	       (use.has_value() && (*use == Desire::Impress || *use == Desire::Compassion || *use == Desire::Curiosity));
}

namespace
{
/// The rest of the agenda is no use
void GiveUpAgenda(IdleMind& mind)
{
	mind.step = mind.agenda.size();
	mind.stepStarted = false;
	mind.gaveUp = true;
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
		// Its head looks about this last turn, then stays still
		commands.lookAbout = true;
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
void DanceStepTurn(IdleMind& mind, const Step& step, const Senses& senses, Commands& commands, const Random& random)
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
	if (turn.playOnce.has_value() && !senses.bodyPlaying)
	{
		commands.playOnce = *turn.playOnce;
		commands.mirrored = random(2) == 1;
	}
	if (turn.overlay.has_value() && !senses.overlayPlaying)
	{
		commands.playOverlay = *turn.overlay;
	}
	commands.endStaticLoop = turn.endPose;
	commands.endDance = turn.endDance;
	if (turn.danceMoveMs.has_value() && !senses.bodyPlaying)
	{
		commands.startDance = *turn.danceMoveMs;
	}
	if (turn.pose.has_value() && !senses.bodyPlaying)
	{
		commands.startSequence = std::array {*turn.pose - 1, *turn.pose, *turn.pose + 1};
		commands.mirrored = random(2) == 1;
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

namespace
{
/// A turn of following the camera's point of focus
void FollowCamera(IdleMind& mind, const Step& step, const Senses& senses, Commands& commands)
{
	const float reach = step.movement.maxDistance;
	const auto action = creature_emote::FollowCameraTurn({.focus = senses.cameraFocus,
	                                                      .position = senses.position,
	                                                      .moving = senses.moving,
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

Commands creature_mind::Think(IdleMind& mind, const Senses& senses, const Random& random)
{
	Commands commands;
	if (mind.step >= mind.agenda.size())
	{
		return commands;
	}
	const auto& step = mind.agenda[mind.step];
	// A step with something to look at has the head on it for as long as it lasts
	commands.gaze = step.gaze;
	// The creature's moves in a dance with villagers start and stop its poses and its dance themselves
	const bool villageDance = step.kind == Step::Kind::Dance && step.dance.kind == DanceOrder::Kind::Dance;
	const bool sitting = (step.kind == Step::Kind::Static && mind.stepStarted) || villageDance;
	const bool dancing = step.kind == Step::Kind::PracticeDance || villageDance;
	// A step that goes on pulls its face again every few seconds
	if (mind.stepStarted && step.face != creature_face::Cue::None)
	{
		mind.faceSeconds -= senses.seconds;
		if (mind.faceSeconds < k_FaceDue)
		{
			commands.face = PullFace(mind, step.face, senses, random);
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
		commands.lookAbout = step.kind == Step::Kind::Wait && !step.gaze.has_value();
		const auto need = HandsNeededFor(step);
		// A step that needs a thing in the hand stops the rest of the agenda when the hand is empty
		if (need == HandsNeed::Held)
		{
			if (!senses.held.has_value())
			{
				GiveUpAgenda(mind);
				return commands;
			}
		}
		// A step that needs empty hands waits while the creature lets go of the thing that it holds
		else if (need != HandsNeed::Any && senses.held.has_value())
		{
			if (need == HandsNeed::FreeOrTarget && step.order.object == senses.held)
			{
				// It holds the thing that the step picks up already, so it does not do the step
				FinishStep(mind);
				return commands;
			}
			// It starts to let go only when its body and hands are free. If it cannot start, it tries again next turn.
			if (!senses.bodyBusy && senses.hands != HandsState::Busy)
			{
				commands.letGo = need == HandsNeed::Free ? LetGo::AsItChooses : LetGo::PutDown;
			}
			return commands;
		}
		// A swap of minds waits until no spell is on either creature, bringing those on itself to their end meanwhile
		if (step.kind == Step::Kind::SwapMind && !senses.swap.spellsGone)
		{
			commands.finishSpells = true;
			return commands;
		}
		// Nothing starts while the body still plays an action. A stop starts while the body moves, to stop it.
		if (step.kind != Step::Kind::Wait && step.kind != Step::Kind::WatchCamera && step.kind != Step::Kind::StopMoving &&
		    step.kind != Step::Kind::PracticeDance && senses.bodyBusy)
		{
			return commands;
		}
		// A dance alone waits only while the body plays an action
		if (step.kind == Step::Kind::PracticeDance && senses.bodyPlaying)
		{
			return commands;
		}
		mind.stepStarted = true;
		mind.stepSeconds = 0.0f;
		// Each step pulls the face it is meant to as it starts, held for as long as the face goes with
		commands.face = PullFace(mind, step.face, senses, random);
		if (step.heldEyes.has_value())
		{
			commands.eyes = *step.heldEyes;
			mind.eyesHeld = true;
			mind.eyesToClear = false;
		}
		else if (mind.eyesToClear)
		{
			commands.eyes = Eyes::Normal;
			mind.eyesHeld = false;
			mind.eyesToClear = false;
		}
		if (step.flutterEyelids)
		{
			// Its eyes go back to their usual blinking once it takes up something else
			commands.blinkIntervalMs = k_FlutterBlinkIntervalMs;
			mind.eyesHeld = true;
		}
		switch (step.kind)
		{
		case Step::Kind::Action:
			commands.playOnce = step.animation;
			commands.mirrored = random(2) == 1;
			if (step.sleepyEyes)
			{
				commands.eyes = Eyes::Sleepy;
			}
			break;
		case Step::Kind::Static:
			commands.startSequence = step.sequence;
			commands.holdLoop = step.holdLoop;
			commands.fainted = step.faint;
			// The turn that the rest starts is its first turn
			mind.restTurns = 1;
			if (step.mirrorAtRandom)
			{
				commands.mirrored = random(2) == 1;
			}
			if (step.closedEyes)
			{
				commands.eyes = Eyes::Closed;
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
			commands.mirrored = random(2) == 1;
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
			WatchHand(mind, step, senses, commands);
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
		case Step::Kind::Kiss:
			commands.playOnce = step.animation;
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
				commands.mirrored = random(2) == 1;
			}
			else
			{
				FinishStep(mind);
			}
			break;
		case Step::Kind::Wait:
		case Step::Kind::WaitInMap:
			break;
		}
		return commands;
	}

	mind.stepSeconds += senses.seconds;
	switch (step.kind)
	{
	case Step::Kind::Wait:
		commands.lookAbout = !step.gaze.has_value();
		if (mind.stepSeconds >= step.seconds)
		{
			FinishStep(mind);
		}
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
			if (step.sleepyEyes)
			{
				commands.eyes = Eyes::Normal;
			}
			if (step.effect != Effect::None)
			{
				commands.effect = step.effect;
			}
			FinishStep(mind);
		}
		break;
	case Step::Kind::Static:
	{
		commands.lookAbout = senses.bodyLooping && !step.closedEyes;
		// A faint counts its time only from when the creature lies still. Its wait starts on the turn after the creature
		// lies still, and the creature gets up on the turn after the wait ends.
		if (step.faint && !senses.bodyLooping && !mind.sitEnding)
		{
			mind.stepSeconds = -k_FaintWaitTurnsLate * senses.seconds;
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
		const bool done = mind.wakeWanted || (step.untilBetter   ? better
		                                      : step.untilRested ? senses.rested
		                                                         : mind.stepSeconds >= step.seconds);
		if (!senses.bodyBusy)
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
			mind.sitEnding = true;
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
		break;
	case Step::Kind::Cast:
		if (!mind.castDone)
		{
			// The miracle is cast as the loop of the pose begins, and held for whole turns from the next
			if (senses.bodyLooping)
			{
				commands.cast = step.cast;
				mind.castDone = true;
				const auto turnsPerSecond = static_cast<uint32_t>(std::lround(1.0f / senses.seconds));
				mind.castTurns = static_cast<uint32_t>(step.seconds * static_cast<float>(turnsPerSecond));
			}
			else if (!senses.bodyBusy)
			{
				// The pose ended before it could cast
				mind.step = mind.agenda.size();
				mind.stepStarted = false;
				mind.gaveUp = true;
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
		WatchHand(mind, step, senses, commands);
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
		if (!senses.bodyPlaying)
		{
			FinishStep(mind);
		}
		break;
	case Step::Kind::Dance:
		DanceStepTurn(mind, step, senses, commands, random);
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
		// The fight has ended
		if (!senses.fighting)
		{
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
		FinishStep(mind);
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
		// Done once it has arrived or given up, or its time is up
		if (!senses.moving)
		{
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
