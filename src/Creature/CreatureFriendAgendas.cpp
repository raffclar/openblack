/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "CreatureFriendAgendas.h"

#include <memory>

#include "Creature/CreatureEmoteAgendas.h"
#include "Creature/CreatureLayers.h"

using namespace openblack;
using namespace openblack::creature_mind;
namespace animations = openblack::creature_layers::animations;
using creature_face::Cue;

namespace
{
Step PlayOnce(size_t animation)
{
	return {.kind = Step::Kind::Action, .animation = animation};
}

/// Turning to face a thing, holding still a while once it does
Step FaceThing(uint32_t thing, float seconds, Cue face = Cue::None, std::optional<Gaze> gaze = std::nullopt)
{
	return {.kind = Step::Kind::Move,
	        .seconds = seconds,
	        .movement = {.kind = Movement::Kind::TurnToFaceObject, .object = thing},
	        .face = face,
	        .gaze = gaze};
}

Step PointAt(uint32_t thing, float seconds, Cue face)
{
	return {.kind = Step::Kind::Object,
	        .order = {.kind = ObjectOrder::Kind::PointAt, .object = thing, .seconds = seconds},
	        .face = face,
	        .gaze = Gaze {.object = thing}};
}

/// An order to the partner. The partner does the step with no face of its own and looks about as it does it.
Step Order(uint32_t partner, Step ordered)
{
	ordered.face = Cue::None;
	ordered.gaze.reset();
	return {.kind = Step::Kind::Order, .object = partner, .ordered = std::make_shared<const Step>(std::move(ordered))};
}
} // namespace

std::vector<Step> creature_mind::WaveAtFriend(uint32_t self, uint32_t friendly)
{
	auto wave = PlayOnce(animations::k_FriendlyWave);
	wave.main = true;
	return {
	    {.kind = Step::Kind::RequestPartner,
	     .object = friendly,
	     .face = Cue::Playfulness,
	     .gaze = Gaze {.object = friendly},
	     .request = creature_friendship::RequestKind::Play},
	    {.kind = Step::Kind::Demobilise, .object = friendly},
	    Order(friendly, FaceThing(self, k_FriendFacesWaverSeconds)),
	    FaceThing(friendly, k_FaceFriendToWaveSeconds, Cue::Smile, Gaze {.object = friendly}),
	    Order(friendly, PlayOnce(animations::k_FriendlyWave)),
	    wave,
	    {.kind = Step::Kind::Remobilise, .object = friendly},
	};
}

std::vector<Step> creature_mind::HowlAtFriend(uint32_t friendly)
{
	return {
	    FaceThing(friendly, k_FaceToHowlSeconds, Cue::None, Gaze {.object = friendly}),
	    PlayOnce(k_Howl),
	};
}

std::vector<Step> creature_mind::PineForFriend(uint32_t friendly)
{
	auto pine = PlayOnce(animations::k_Sad);
	pine.main = true;
	return {
	    FaceThing(friendly, k_PineSeconds, Cue::Sad, Gaze {.object = friendly}),
	    PointAt(friendly, k_PineSeconds, Cue::Sad),
	    pine,
	};
}

std::vector<Step> creature_mind::LookForFriend(uint32_t friendly, float height)
{
	auto pine = PlayOnce(animations::k_Sad);
	pine.main = true;
	return {
	    PointAt(friendly, k_PineSeconds, Cue::Sad),
	    pine,
	    {.kind = Step::Kind::Move,
	     .movement = {.kind = Movement::Kind::GoNearObject, .object = friendly, .maxDistance = k_LookForFriendHeights * height},
	     .gaze = Gaze {.object = friendly, .whileGoing = true}},
	};
}

namespace
{
/// The look at a thing, with fluttering eyelids
Step Fluttering(Step step, uint32_t thing)
{
	step.gaze = Gaze {.object = thing};
	step.flutterEyelids = true;
	return step;
}

Step GoTo(glm::vec2 point, float within, std::optional<Gaze> gaze = std::nullopt)
{
	return {.kind = Step::Kind::Move,
	        .movement = {.kind = Movement::Kind::ToPoint, .point = point, .maxDistance = within},
	        .gaze = gaze};
}

Step WaitForStill(uint32_t thing)
{
	return {.kind = Step::Kind::WaitForStill, .object = thing, .gaze = Gaze {.object = thing}};
}

Step AskToBeFriends(uint32_t friendly, Cue face = Cue::None)
{
	return {.kind = Step::Kind::RequestPartner,
	        .object = friendly,
	        .face = face,
	        .gaze = Gaze {.object = friendly},
	        .request = creature_friendship::RequestKind::BeFriends};
}

/// The first steps of the joint actions side by side: it asks the friend, both go to their places, and it waits for the
/// friend to stop
std::vector<Step> MeetAt(const Friends& friends, glm::vec2 selfPlace, glm::vec2 friendPlace)
{
	return {
	    AskToBeFriends(friends.friendly),
	    Order(friends.friendly, GoTo(friendPlace, friends.friendHeight)),
	    GoTo(selfPlace, friends.height, GoingLook(selfPlace)),
	    WaitForStill(friends.friendly),
	};
}

Step SitFor(float seconds)
{
	return {.kind = Step::Kind::Static,
	        .seconds = seconds,
	        .animation = animations::k_Sit,
	        .sequence = {animations::k_StartSit, animations::k_Sit, animations::k_EndSit}};
}
} // namespace

std::pair<glm::vec2, glm::vec2> creature_mind::SideBySide(const Friends& friends)
{
	const auto middle = friends.selfPoint + (friends.friendPoint - friends.selfPoint) * 0.5f;
	return {middle + glm::vec2(friends.height, 0.0f), middle - glm::vec2(friends.friendHeight, 0.0f)};
}

std::pair<glm::vec2, glm::vec2> creature_mind::AboutPlace(glm::vec2 middle, float radius)
{
	const auto offset = glm::vec2(radius * 0.25f, radius * 0.25f);
	return {middle + offset, middle - offset};
}

float creature_mind::SitTogetherRadius(float height, float friendHeight)
{
	return (height + friendHeight) * k_SitTogetherHeights;
}

float creature_mind::SleepTogetherRadius(float height, float friendHeight)
{
	return k_SleepTogetherHeights * (height + friendHeight);
}

std::vector<Step> creature_mind::BeHappyWithFriend(const Friends& friends)
{
	const auto [selfPlace, friendPlace] = SideBySide(friends);
	auto agenda = MeetAt(friends, selfPlace, friendPlace);
	agenda.push_back(Order(friends.friendly, FaceThing(friends.self, k_FaceFriendSeconds)));
	agenda.push_back(Fluttering(FaceThing(friends.friendly, k_FaceFriendSeconds), friends.friendly));
	agenda.push_back(Order(friends.friendly, PlayOnce(animations::k_Happy)));
	agenda.push_back(PlayOnce(animations::k_Happy));
	return agenda;
}

std::vector<Step> creature_mind::PooWithFriend(const Friends& friends, const Random& random)
{
	const auto [selfPlace, friendPlace] = SideBySide(friends);
	auto agenda = MeetAt(friends, selfPlace, friendPlace);
	auto squat = Step {.kind = Step::Kind::Static,
	                   .seconds = k_PooTogetherSeconds,
	                   .animation = animations::k_Poo,
	                   .sequence = {animations::k_StartPoo, animations::k_Poo, animations::k_EndPoo},
	                   .face = Cue::Smile};
	const auto poo = Step {.kind = Step::Kind::Apply, .effect = Effect::Poo, .face = Cue::Smile};
	agenda.push_back(Order(friends.friendly, squat));
	squat.main = true;
	agenda.push_back(squat);
	agenda.push_back(Order(friends.friendly, poo));
	agenda.push_back(poo);
	const float seconds = k_FaceAfterPooSeconds + creature_emote::FloatRandom(random, k_FaceAfterPooExtraSeconds);
	agenda.push_back(FaceThing(friends.friendly, seconds, Cue::Smile, Gaze {.object = friends.friendly}));
	agenda.push_back(PlayOnce(animations::k_Happy));
	return agenda;
}

std::vector<Step> creature_mind::SitWithFriend(const Friends& friends, glm::vec2 middle, float radius, const Random& random)
{
	const auto [selfPlace, friendPlace] = AboutPlace(middle, radius);
	auto agenda = MeetAt(friends, selfPlace, friendPlace);
	agenda.back().flutterEyelids = true;
	agenda.push_back(Order(friends.friendly, FaceThing(friends.self, k_FaceFriendSeconds)));
	agenda.push_back(FaceThing(friends.friendly, k_FaceFriendSeconds, Cue::None, Gaze {.object = friends.friendly}));
	agenda.push_back(
	    Order(friends.friendly, SitFor(k_SitTogetherSeconds + creature_emote::FloatRandom(random, k_SitTogetherExtraSeconds))));
	auto sit = SitFor(k_SitTogetherSeconds + creature_emote::FloatRandom(random, k_SitTogetherExtraSeconds));
	sit.gaze = Gaze {.object = friends.friendly};
	sit.main = true;
	agenda.push_back(sit);
	agenda.push_back(Fluttering(FaceThing(friends.friendly, k_FaceFriendSeconds), friends.friendly));
	agenda.push_back(PlayOnce(animations::k_Happy));
	return agenda;
}

std::vector<Step> creature_mind::SleepWithFriend(const Friends& friends, glm::vec2 middle, float radius, const Random& random)
{
	std::vector<Step> agenda;
	if (random(2) == 0)
	{
		agenda.push_back({.kind = Step::Kind::Action, .animation = animations::k_Tired, .sleepyEyes = true});
	}
	const auto [selfPlace, friendPlace] = AboutPlace(middle, radius);
	const auto meet = MeetAt(friends, selfPlace, friendPlace);
	agenda.insert(agenda.end(), meet.begin(), meet.end());
	agenda.push_back(Order(friends.friendly, FaceThing(friends.self, k_FaceFriendSeconds)));
	agenda.push_back(Fluttering(FaceThing(friends.friendly, k_FaceFriendSeconds), friends.friendly));
	auto sleep = Step {.kind = Step::Kind::Static,
	                   .seconds = k_SleepTogetherSeconds,
	                   .animation = animations::k_Sleep,
	                   .sequence = {animations::k_StartSleep, animations::k_Sleep, animations::k_EndSleep}};
	agenda.push_back(Order(friends.friendly, sleep));
	sleep.main = true;
	agenda.push_back(sleep);
	agenda.push_back(
	    {.kind = Step::Kind::Action, .animation = animations::k_Confused, .sleepyEyes = true, .face = Cue::Grimace});
	return agenda;
}

std::vector<Step> creature_mind::DrinkWithFriend(const Friends& friends, glm::vec2 coast, glm::vec2 sea, const Random& random)
{
	const auto friendPlace = coast + glm::vec2(friends.friendHeight, 0.0f);
	auto agenda = MeetAt(friends, coast, friendPlace);
	const auto faceSea =
	    Step {.kind = Step::Kind::Move, .movement = {.kind = Movement::Kind::TurnToFace, .point = sea}, .gaze = GoingLook(sea)};
	agenda.push_back(Order(friends.friendly, faceSea));
	agenda.push_back(faceSea);
	agenda.push_back(Order(friends.friendly, PlayOnce(animations::k_Drink)));
	auto drinkAction = PlayOnce(animations::k_Drink);
	drinkAction.main = true;
	agenda.push_back(drinkAction);
	const auto drink = Step {.kind = Step::Kind::Apply, .effect = Effect::Drink};
	agenda.push_back(Order(friends.friendly, drink));
	agenda.push_back(drink);
	const float seconds = k_FaceAfterDrinkSeconds + creature_emote::FloatRandom(random, k_FaceAfterDrinkExtraSeconds);
	agenda.push_back(Fluttering(FaceThing(friends.friendly, seconds, Cue::Smile), friends.friendly));
	return agenda;
}

std::vector<Step> creature_mind::TownWithFriend(const Friends& friends, const FriendsTown& town,
                                                creature_desires::Desire desire)
{
	const float distance = k_NearPitTimes * (town.pitRadius + friends.height);
	const auto nearPit = Step {.kind = Step::Kind::Move,
	                           .movement = {.kind = Movement::Kind::GoNearObject, .object = town.pit, .maxDistance = distance},
	                           .gaze = Gaze {.object = town.pit, .whileGoing = true}};
	const auto apply = Step {.kind = Step::Kind::ApplyDesire, .object = town.town, .desire = desire};
	return {
	    AskToBeFriends(friends.friendly, Cue::Playfulness),
	    Fluttering(FaceThing(friends.friendly, k_FaceFriendSeconds), friends.friendly),
	    PlayOnce(animations::k_Happy),
	    {.kind = Step::Kind::Object,
	     .order = {.kind = ObjectOrder::Kind::PointAt,
	               .point = town.townPoint,
	               .pointHeight = 0.0f,
	               .seconds = k_PointAtTownSeconds},
	     .face = Cue::Playfulness,
	     .gaze = Gaze {.point = glm::vec3 {town.townPoint.x, 0.0f, town.townPoint.y}},
	     .main = true},
	    Order(friends.friendly, nearPit),
	    nearPit,
	    Order(friends.friendly, FaceThing(friends.self, k_FaceFriendSeconds)),
	    FaceThing(friends.friendly, k_FaceFriendSeconds),
	    Order(friends.friendly, apply),
	    apply,
	};
}
