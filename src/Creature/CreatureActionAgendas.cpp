/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "CreatureActionAgendas.h"

#include <algorithm>
#include <array>
#include <iterator>
#include <utility>

#include "Creature/CreatureLayers.h"
#include "Creature/CreatureStomp.h"
#include "Creature/CreatureThrow.h"

using namespace openblack;
using namespace openblack::creature_mind;
namespace animations = openblack::creature_layers::animations;

namespace
{
Step PlayOnce(size_t animation, bool sleepyEyes = false, creature_face::Cue face = creature_face::Cue::None)
{
	return {.kind = Step::Kind::Action, .animation = animation, .sleepyEyes = sleepyEyes, .face = face};
}

Step GoNear(uint32_t thing, float distance, creature_face::Cue face = creature_face::Cue::None,
            std::optional<Gaze> gaze = std::nullopt)
{
	return {.kind = Step::Kind::Move,
	        .movement = {.kind = Movement::Kind::GoNearObject, .object = thing, .maxDistance = distance},
	        .face = face,
	        .gaze = gaze};
}

/// Turning to face a thing, holding still a while once it does
Step FaceThing(uint32_t thing, float seconds, creature_face::Cue face = creature_face::Cue::None,
               std::optional<Gaze> gaze = std::nullopt)
{
	return {.kind = Step::Kind::Move,
	        .seconds = seconds,
	        .movement = {.kind = Movement::Kind::TurnToFaceObject, .object = thing},
	        .face = face,
	        .gaze = gaze};
}

} // namespace

std::vector<Step> creature_mind::HurlAt(uint32_t thrown, uint32_t target, glm::vec2 targetPoint, float height, bool handFull,
                                        const Random& random)
{
	std::vector<Step> agenda;
	if (!handFull)
	{
		if (random(k_AngryBeforeHurlingLots) == 0)
		{
			agenda.push_back(PlayOnce(animations::k_Angry, false, creature_face::Cue::Amazed));
		}
		agenda.push_back({.kind = Step::Kind::Object, .order = {.kind = ObjectOrder::Kind::PickUp, .object = thrown}});
	}
	const Gaze watch {.object = target};
	agenda.push_back({.kind = Step::Kind::Move,
	                  .movement = {.kind = Movement::Kind::ToThrowPosition, .object = target, .maxDistance = height},
	                  .face = creature_face::Cue::Anger,
	                  .gaze = watch});
	agenda.push_back({.kind = Step::Kind::Object,
	                  .order = {.kind = ObjectOrder::Kind::Throw, .object = target, .point = targetPoint},
	                  .face = creature_face::Cue::Anger,
	                  .gaze = watch});
	return agenda;
}

std::vector<Step> creature_mind::Stomp(uint32_t thing, const Random& random)
{
	std::vector<Step> agenda;
	if (random(creature_stomp::k_AngryBeforeStompLots) == 0)
	{
		agenda.push_back(PlayOnce(animations::k_Angry, false, creature_face::Cue::Anger));
	}
	std::ranges::move(DestroyThing(thing), std::back_inserter(agenda));
	return agenda;
}

std::vector<Step> creature_mind::StompAndEat(uint32_t thing, bool holdingEatable)
{
	std::vector<Step> agenda;
	if (!holdingEatable)
	{
		std::ranges::move(DestroyThing(thing), std::back_inserter(agenda));
		agenda.push_back({.kind = Step::Kind::Object,
		                  .order = {.kind = ObjectOrder::Kind::PickUp, .object = thing},
		                  .gaze = Gaze {.object = thing, .whileGoing = true}});
	}
	agenda.push_back({.kind = Step::Kind::Object, .effect = Effect::Eat, .order = {.kind = ObjectOrder::Kind::Eat}});
	return agenda;
}

std::vector<Step> creature_mind::StoneAndEat(uint32_t rock, uint32_t thing, float height, bool holdingRock)
{
	std::vector<Step> agenda;
	if (!holdingRock)
	{
		agenda.push_back({.kind = Step::Kind::Object,
		                  .order = {.kind = ObjectOrder::Kind::PickUp, .object = rock},
		                  .gaze = Gaze {.object = rock, .whileGoing = true}});
	}
	agenda.push_back({.kind = Step::Kind::Move,
	                  .movement = {.kind = Movement::Kind::ToThrowPosition, .object = thing, .maxDistance = height},
	                  .face = creature_face::Cue::Anger,
	                  .gaze = Gaze {.object = thing}});
	// The creature aims the throw at the rock in its own hand, not at the thing to eat. This is a quirk of the game. The
	// rock is too near to throw at, so the throw fails and the creature stops the action with the rock in its hand.
	agenda.push_back({.kind = Step::Kind::Object,
	                  .order = {.kind = ObjectOrder::Kind::Throw, .object = rock, .atObject = true},
	                  .face = creature_face::Cue::Anger,
	                  .gaze = Gaze {.object = rock}});
	agenda.push_back({.kind = Step::Kind::Object,
	                  .order = {.kind = ObjectOrder::Kind::PickUp, .object = thing},
	                  .gaze = Gaze {.object = thing, .whileGoing = true}});
	agenda.push_back({.kind = Step::Kind::Object, .effect = Effect::Eat, .order = {.kind = ObjectOrder::Kind::Eat}});
	return agenda;
}

std::vector<Step> creature_mind::ShowCreatureYouHateHim(uint32_t other, float height, float otherHeight, const Random& random)
{
	const Gaze watch {.object = other};
	std::vector<Step> agenda {
	    GoNear(other, std::max(height * k_HateDistanceHeights, otherHeight * k_HateDistanceHeights), creature_face::Cue::Anger,
	           Gaze {.object = other, .whileGoing = true}),
	    FaceThing(other, k_HateFaceSeconds, creature_face::Cue::Anger, watch),
	};
	if (random(k_HateLots) == 0)
	{
		agenda.push_back({.kind = Step::Kind::Object,
		                  .order = {.kind = ObjectOrder::Kind::PointAt, .object = other, .seconds = k_HatePointSeconds},
		                  .face = creature_face::Cue::Grimace,
		                  .gaze = watch});
	}
	agenda.push_back(PlayOnce(random(k_HateLots) != 0 ? k_HateTaunt : animations::k_Taunt));
	if (random(k_HateLots) == 0)
	{
		agenda.push_back(PlayOnce(animations::k_Angry));
	}
	return agenda;
}

std::vector<Step> creature_mind::KickThing(uint32_t thing)
{
	const Gaze watch {.object = thing};
	return {
	    GoNear(thing, 0.0f, creature_face::Cue::None, Gaze {.object = thing, .whileGoing = true}),
	    FaceThing(thing, k_FaceToKickSeconds, creature_face::Cue::None, watch),
	    {.kind = Step::Kind::Object, .order = {.kind = ObjectOrder::Kind::Kick, .object = thing}, .gaze = watch},
	};
}

std::vector<Step> creature_mind::PooAtHome(glm::vec2 home, float height, uint32_t self, const Random& random)
{
	std::vector<Step> agenda {
	    {.kind = Step::Kind::Move,
	     .movement = {.kind = Movement::Kind::ToPoint, .point = home, .maxDistance = k_PooAtHomeHeights * height},
	     .gaze = GoingLook(home)}};
	std::ranges::move(Poo(random, PooBy {.object = self, .itself = true}), std::back_inserter(agenda));
	return agenda;
}

std::vector<Step> creature_mind::SleepAtHome(glm::vec2 home, float height, const Random& random)
{
	std::vector<Step> agenda;
	if (random(k_YawnBeforeSleepingLots) == 0)
	{
		agenda.push_back(PlayOnce(animations::k_Tired, true));
	}
	agenda.push_back({.kind = Step::Kind::Move,
	                  .movement = {.kind = Movement::Kind::ToPoint,
	                               .point = home,
	                               .maxDistance = std::min(k_FurthestFromHomeToSleep, height)},
	                  .gaze = GoingLook()});
	agenda.push_back({.kind = Step::Kind::Move, .movement = {.kind = Movement::Kind::FaceDownSlope}});
	agenda.push_back({.kind = Step::Kind::Static,
	                  .sequence = {animations::k_StartSleep, animations::k_Sleep, animations::k_EndSleep},
	                  .untilRested = true,
	                  .closedEyes = true,
	                  .effect = Effect::Slept,
	                  .main = true});
	agenda.push_back(PlayOnce(animations::k_Confused, true, creature_face::Cue::Grimace));
	return agenda;
}

std::vector<Step> creature_mind::SleepByObject(glm::vec2 place, float radius, const Random& random)
{
	std::vector<Step> agenda;
	agenda.push_back({.kind = Step::Kind::Move,
	                  .movement = {.kind = Movement::Kind::ToPoint, .point = place, .maxDistance = radius},
	                  .gaze = GoingLook()});
	if (random(k_YawnBeforeSleepingLots) == 0)
	{
		agenda.push_back(PlayOnce(animations::k_Tired, true));
	}
	agenda.push_back({.kind = Step::Kind::Move, .movement = {.kind = Movement::Kind::FaceDownSlope}});
	agenda.push_back({.kind = Step::Kind::Static,
	                  .sequence = {animations::k_StartSleep, animations::k_Sleep, animations::k_EndSleep},
	                  .untilRested = true,
	                  .closedEyes = true,
	                  .effect = Effect::Slept,
	                  .main = true});
	agenda.push_back(PlayOnce(animations::k_Confused, true, creature_face::Cue::Grimace));
	return agenda;
}

std::vector<Step> creature_mind::EatFromPile(uint32_t pile, glm::vec2 pilePoint, float pileRadius, float radius)
{
	return {
	    {.kind = Step::Kind::Move,
	     .movement = {.kind = Movement::Kind::ToPoint,
	                  .point = pilePoint,
	                  .maxDistance = (pileRadius + radius) * k_EatFromPileReach}},
	    FaceThing(pile, k_FacePileSeconds),
	    {.kind = Step::Kind::Object,
	     .effect = Effect::TookFromPile,
	     .order = {.kind = ObjectOrder::Kind::TakeFromPile, .object = pile}},
	    {.kind = Step::Kind::Object, .effect = Effect::Eat, .order = {.kind = ObjectOrder::Kind::Eat}},
	};
}

namespace
{
/// The two steps that take a handful from a thing and eat it
std::array<Step, 2> TakeHandfulAndEat(uint32_t thing, ObjectOrder::Handful handful)
{
	return {
	    Step {.kind = Step::Kind::Object,
	          .effect = Effect::TookFromPile,
	          .order = {.kind = ObjectOrder::Kind::TakeFromPile, .handful = handful, .object = thing}},
	    Step {.kind = Step::Kind::Object, .effect = Effect::Eat, .order = {.kind = ObjectOrder::Kind::Eat}},
	};
}

/// The two steps that walk to the point of a thing, then turn the creature to the thing for a tenth of a second
std::vector<Step> GoToFace(uint32_t thing, glm::vec2 point, float within)
{
	return {
	    {.kind = Step::Kind::Move, .movement = {.kind = Movement::Kind::ToPoint, .point = point, .maxDistance = within}},
	    FaceThing(thing, k_FacePileSeconds),
	};
}
} // namespace

std::vector<Step> creature_mind::EatFromStoragePit(uint32_t pile, glm::vec2 pilePoint, float height)
{
	auto agenda = GoToFace(pile, pilePoint, height * k_EatFromStoragePitHeights);
	agenda.push_back({.kind = Step::Kind::ActOnItself});
	std::ranges::copy(TakeHandfulAndEat(pile, ObjectOrder::Handful::Wheat), std::back_inserter(agenda));
	return agenda;
}

std::vector<Step> creature_mind::EatFromField(uint32_t field, glm::vec2 fieldPoint, float height)
{
	auto agenda = GoToFace(field, fieldPoint, height);
	agenda.push_back({.kind = Step::Kind::ActOnItself});
	std::ranges::copy(TakeHandfulAndEat(field, ObjectOrder::Handful::Wheat), std::back_inserter(agenda));
	return agenda;
}

std::vector<Step> creature_mind::EatFromTree(uint32_t tree, glm::vec2 treePoint, float treeRadius, float radius)
{
	auto agenda = GoToFace(tree, treePoint, (treeRadius + radius) * k_EatFromPileReach);
	std::ranges::copy(TakeHandfulAndEat(tree, ObjectOrder::Handful::Fruit), std::back_inserter(agenda));
	return agenda;
}

std::vector<Step> creature_mind::ExamineByLooking(uint32_t thing, float height, float thingHeight)
{
	return {
	    GoNear(thing, std::max(height * k_ExamineHeights, thingHeight)),
	    FaceThing(thing, k_ExamineFaceSeconds, creature_face::Cue::None, Gaze {.object = thing}),
	    {.kind = Step::Kind::Wait, .seconds = k_ExamineWatchSeconds, .gaze = Gaze {.object = thing, .bottom = true}},
	    {.kind = Step::Kind::Observe, .object = thing},
	};
}

std::vector<Step> creature_mind::ExamineByFollowing(uint32_t villager)
{
	Step follow {.kind = Step::Kind::FollowWatching,
	             .seconds = k_FollowWatchingSeconds,
	             .object = villager,
	             .face = creature_face::Cue::Amazed,
	             .gaze = Gaze {.object = villager}};
	follow.movement = {.kind = Movement::Kind::ToObject, .object = villager, .maxDistance = k_FollowWatchingDistance};
	return {follow, {.kind = Step::Kind::Action, .animation = animations::k_Confused, .face = creature_face::Cue::Amazed}};
}

std::vector<Step> creature_mind::InspectCreature(uint32_t other, float height)
{
	// TODO(creature-reactions-2): each turn of the watch the game also weighs which of its desires it would act on with
	// the other creature, and which the other would act on with it, and on finding one drops the watch to act on it then
	// (anger, and fear unless anger is the stronger, settled first); the planner can't yet weigh a desire for one given
	// thing, so the watch always runs its time
	return {
	    GoNear(other, height * k_InspectHeights),
	    FaceThing(other, k_InspectFaceSeconds, creature_face::Cue::Curiosity),
	    {.kind = Step::Kind::Inspect,
	     .seconds = k_InspectSeconds,
	     .extraSeconds = k_InspectExtraSeconds,
	     .object = other,
	     .face = creature_face::Cue::Curiosity,
	     .gaze = Gaze {.object = other}},
	};
}

std::vector<Step> creature_mind::LookAtFlyingObject(uint32_t thing, const Random& random)
{
	const Gaze follow {.object = thing, .itself = true};
	std::vector<Step> agenda;
	if (random(k_PointAtFlyingLots) == 0)
	{
		agenda.push_back({.kind = Step::Kind::Object,
		                  .order = {.kind = ObjectOrder::Kind::PointAt, .object = thing, .seconds = k_PointAtFlyingSeconds},
		                  .face = creature_face::Cue::Amazed,
		                  .gaze = follow});
	}
	agenda.push_back({.kind = Step::Kind::WatchFlying, .object = thing, .face = creature_face::Cue::Amazed, .gaze = follow});
	return agenda;
}

std::vector<Step> creature_mind::LookAtHand()
{
	return {{.kind = Step::Kind::WatchHand,
	         .turns = k_LookAtHandTurns,
	         .face = creature_face::Cue::Curiosity,
	         .gaze = Gaze {.hand = true}}};
}

std::vector<Step> creature_mind::SmileAt(uint32_t friendly, float height, float chance, const Random& random)
{
	const Gaze look {.object = friendly};
	// It flutters its eyelids at the friend as it goes near and as it faces it
	std::vector<Step> agenda {
	    GoNear(friendly, k_SmileHeights * height, creature_face::Cue::Smile, look),
	    FaceThing(friendly, k_SmileSeconds + (chance * k_SmileExtraSeconds), creature_face::Cue::Smile, look),
	};
	for (auto& step : agenda)
	{
		step.flutterEyelids = true;
	}
	// The smile as it faces the friend is the main step
	agenda.back().main = true;
	if (random(k_HappyAfterSmilingLots) == 0)
	{
		agenda.push_back(PlayOnce(animations::k_Happy));
	}
	return agenda;
}

std::vector<Step> creature_mind::WaveAt(uint32_t thing, float height)
{
	const Gaze look {.object = thing};
	auto wave = PlayOnce(animations::k_FriendlyWave);
	wave.gaze = look;
	return {
	    GoNear(thing, k_WaveHeights * height, creature_face::Cue::Smile),
	    FaceThing(thing, k_WaveFaceSeconds, creature_face::Cue::Smile, look),
	    wave,
	};
}

std::vector<Step> creature_mind::ShowImpressiveAnimation(std::optional<uint32_t> thing, float height, const Random& random)
{
	std::vector<Step> steps;
	if (thing.has_value())
	{
		steps.push_back(GoNear(*thing, k_ShowOffHeights * height, creature_face::Cue::AttitudeToPlayer));
		steps.push_back(FaceThing(*thing, k_ShowOffFaceSeconds, creature_face::Cue::AttitudeToPlayer, Gaze {.object = thing}));
	}
	steps.push_back(PlayOnce(k_LookAtMeAnimation, false, creature_face::Cue::AttitudeToPlayer));
	steps.push_back(
	    PlayOnce(random(2) == 0 ? animations::k_Impress : animations::k_Summon, false, creature_face::Cue::AttitudeToPlayer));
	return steps;
}

std::vector<Step> creature_mind::BeFrightenedOnTheSpot()
{
	// Its eyes go wide as it shows its fright
	auto fright = PlayOnce(animations::k_Frightened);
	fright.heldEyes = Eyes::Wide;
	return {
	    fright,
	    {.kind = Step::Kind::Wait, .seconds = k_FrightenedSeconds, .face = creature_face::Cue::Frightened},
	    fright,
	};
}

std::vector<Step> creature_mind::PutDown()
{
	return {
	    {.kind = Step::Kind::Object, .order = {.kind = ObjectOrder::Kind::Discard, .animation = creature_throw::k_PutDown}}};
}

std::vector<Step> creature_mind::DeadForever()
{
	return {{.kind = Step::Kind::Static,
	         .seconds = k_DeadForeverSeconds,
	         .sequence = {animations::k_Faint, animations::k_Faint, animations::k_GetUp},
	         .holdLoop = true,
	         .closedEyes = true}};
}

std::vector<Step> creature_mind::PointOutHighlight(uint32_t highlight, float cameraDistance)
{
	// TODO(creature-do-action-2): looking at the camera is only the look here, as for the scripts' look at the camera
	const auto lookAtCamera = [](float seconds) {
		return Step {
		    .kind = Step::Kind::Wait, .seconds = seconds, .face = creature_face::Cue::Curiosity, .gaze = Gaze {.camera = true}};
	};
	std::vector<Step> agenda;
	if (cameraDistance > k_CallCameraDistance)
	{
		agenda.push_back(lookAtCamera(k_CallCameraSeconds));
		agenda.push_back(PlayOnce(k_LookAtMeAnimation));
	}
	agenda.push_back({.kind = Step::Kind::Object,
	                  .order = {.kind = ObjectOrder::Kind::PointAt, .object = highlight, .seconds = k_PointOutSeconds},
	                  .face = creature_face::Cue::Amazed,
	                  .gaze = Gaze {.object = highlight}});
	agenda.push_back(lookAtCamera(k_AfterPointingOutSeconds));
	return agenda;
}

std::optional<NoticeKind> creature_mind::NoticeKindOf(std::string_view action)
{
	constexpr std::array<std::pair<std::string_view, NoticeKind>, 5> k_Notices {{
	    {"NoticeHelpfulAction", NoticeKind::Helpful},
	    {"NoticeAggressiveAction", NoticeKind::Aggressive},
	    {"NoticeAction", NoticeKind::Plain},
	    {"NoticeStealingAction", NoticeKind::Stealing},
	    {"NoticePlayfulAction", NoticeKind::Playful},
	}};
	const auto found = std::ranges::find(k_Notices, action, &std::pair<std::string_view, NoticeKind>::first);
	return found != k_Notices.end() ? std::optional(found->second) : std::nullopt;
}

std::vector<Step> creature_mind::Notice(NoticeKind kind, uint32_t thing, bool handFull, bool hasPlayer, glm::vec3 camera,
                                        const Random& random)
{
	using creature_face::Cue;
	const Gaze atCamera {.camera = true};
	const auto watch = [&atCamera](Cue face) {
		return Step {
		    .kind = Step::Kind::WatchCamera, .seconds = k_NoticeWatchSeconds, .face = face, .gaze = atCamera, .main = true};
	};
	std::vector<Step> agenda;
	if (!handFull)
	{
		const auto seconds = kind == NoticeKind::Aggressive ? k_NoticeAggressivePointSeconds : k_NoticePointSeconds;
		agenda.push_back({.kind = Step::Kind::Object,
		                  .order = {.kind = ObjectOrder::Kind::PointAt, .object = thing, .seconds = seconds},
		                  .face = Cue::Amazed,
		                  .gaze = Gaze {.object = thing, .itself = true}});
	}
	// A creature without a player has no camera of its own to turn to
	if (hasPlayer)
	{
		agenda.push_back({.kind = Step::Kind::Move,
		                  .movement = {.kind = Movement::Kind::TurnToFace, .point = {camera.x, camera.z}},
		                  .gaze = atCamera});
	}
	switch (kind)
	{
	case NoticeKind::Helpful:
		agenda.push_back(watch(Cue::Curiosity));
		agenda.push_back(watch(Cue::Compassion));
		agenda.push_back(PlayOnce(random(k_NoticeHelpfulLots) == 0 ? animations::k_Happy : k_AhaAnimation));
		break;
	case NoticeKind::Aggressive:
		agenda.push_back(watch(Cue::Anger));
		if (random(k_NoticeLaughLots) == 0)
		{
			agenda.push_back(PlayOnce(k_LaughAnimation));
		}
		agenda.push_back(PlayOnce(animations::k_Angry));
		break;
	case NoticeKind::Plain:
	case NoticeKind::Stealing:
	case NoticeKind::Playful:
	{
		agenda.push_back(watch(Cue::Curiosity));
		agenda.push_back(watch(Cue::Curiosity));
		const auto animation = kind == NoticeKind::Plain      ? k_AhaAnimation
		                       : kind == NoticeKind::Stealing ? animations::k_Scratch
		                                                      : animations::k_Happy;
		agenda.push_back(PlayOnce(animation));
		break;
	}
	}
	return agenda;
}

float creature_mind::CuriosityShare(bool observedKind, bool doingSomethingInteresting)
{
	return observedKind && !doingSomethingInteresting ? k_ObservedCuriosityShare : 1.0f;
}

bool creature_mind::DoingSomethingInteresting(uint32_t taughtSkill, const std::vector<bool>& skillsKnown)
{
	if (taughtSkill == k_NoSkillTaught)
	{
		return false;
	}
	return taughtSkill >= skillsKnown.size() || !skillsKnown[taughtSkill];
}
