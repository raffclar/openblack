/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "CreatureDance.h"

#include <cmath>

#include <algorithm>
#include <array>
#include <numbers>

#include <glm/geometric.hpp>

#include "3D/MapCoords.h"
#include "Common/GUtilsAngle.h"
#include "Creature/CreatureLocomotion.h"
#include "ECS/DanceMoves.h"

using namespace openblack;
using namespace openblack::creature_dance;
using creature_mind::DanceOrder;
using creature_mind::Gaze;
using creature_mind::Movement;
using creature_mind::Step;

namespace
{
/// The moves of the creature's own group in a dance, by their numbers in the dance files
enum class CreatureMove : uint32_t
{
	FaceCentre = 31,
	FaceAway = 32,
	/// Face the way the dance faces, turned by the move's second argument
	FaceDance = 33,
	/// Dance, each dance move as long as a thousand milliseconds times the rate of the dance
	Dance = 34,
	/// Play an action of the creature's animation set, by the move's first argument
	Action = 35,
	/// Pose: a start, a loop and an end, the loop by the move's first argument
	Pose = 36,
	/// Pull the face of the move's first argument for two seconds
	Face = 38,
	/// Draw the gesture of the move's first argument in the air, then dance
	Gesture = 39,
	/// Face a point 50 metres away along a game angle, the move's second argument
	FaceAngle = 40,
	/// Talk, or play its second action
	Talk = 41,
	SecondAction = 42,
};

/// The point the creature faces along a game angle
constexpr float k_FaceAngleDistance = 50.0f;
/// The creature turns to a game angle only when it faces more than this far away from it (radians)
constexpr float k_FaceAngleWithin = 0.4712389f;
/// The animations that the talk move and the second action move play
constexpr size_t k_TalkAnimation = 205;
constexpr size_t k_SecondAction = 218;
/// The milliseconds of a dance move for each unit of the rate of the dance, and how long the face move holds a face
constexpr float k_DanceMoveMsPerRate = 1000.0f;
constexpr float k_DanceFaceMs = 2000.0f;
/// A full turn in radians, as a float
constexpr float k_FullTurn = 6.2831855f;
/// The music of each tribe's dance: the chants, one after the other with their voices
constexpr std::array<int32_t, 9> k_TribeChants {28, 28, 30, 32, 34, 36, 38, 40, 42};
/// A tribe past the table has this music
constexpr int32_t k_UnknownTribeMusic = 5;
/// The music has voices when more than this many dance
constexpr uint32_t k_DancersForVoices = 8;
/// The share of the town's population that the belief formula keeps above zero
constexpr float k_BeliefShareSmall = 0.001f;

glm::vec2 AlongGameAngle(glm::vec2 from, uint16_t angle)
{
	return from + glm::vec2(gutils::GetXByAngle(angle, k_FaceAngleDistance), gutils::GetZByAngle(angle, k_FaceAngleDistance));
}
} // namespace

std::optional<uint32_t> creature_dance::TurnsBetween(uint32_t type)
{
	switch (type)
	{
	case k_ImpressDanceType:
		return k_TurnsBetweenImpressDances;
	case k_PlayfulDanceType:
		return k_TurnsBetweenPlayfulDances;
	case k_StoryDanceType:
		return k_TurnsBetweenStories;
	default:
		return std::nullopt;
	}
}

bool creature_dance::MayDanceAgain(uint32_t type, uint32_t lastType, uint32_t lastTurn, uint32_t turn)
{
	const auto between = TurnsBetween(type);
	return !between.has_value() || (lastType != type && turn - lastTurn > *between);
}

bool creature_dance::DanceGoesOn(std::optional<uint32_t> startedBy, uint32_t agenda, bool agendaUnderway)
{
	return agendaUnderway && startedBy == agenda;
}

bool creature_dance::DecidesWhenReleased(VillagerStates top)
{
	return top == VillagerStates::InHand || top == VillagerStates::WorshippingCreature ||
	       top == VillagerStates::ControlledByCreature;
}

bool creature_dance::MayDanceToImpress(uint32_t lastType, uint32_t lastTurn, uint32_t turn)
{
	return MayDanceAgain(k_ImpressDanceType, lastType, lastTurn, turn);
}

std::vector<Step> creature_dance::PracticeDance()
{
	return {Step {.kind = Step::Kind::PracticeDance, .seconds = k_PracticeSeconds, .face = creature_face::Cue::Smile}};
}

float creature_dance::AreaWidth(float radius)
{
	return std::max(k_AreaRadiusShare * radius, k_SmallestArea);
}

std::vector<Step> creature_dance::Agenda(const AgendaInput& input)
{
	const auto villager = input.villager;
	const auto order = [](DanceOrder dance) { return Step {.kind = Step::Kind::Dance, .dance = dance}; };
	return {
	    {.kind = Step::Kind::StopMoving},
	    {.kind = Step::Kind::Move,
	     .movement = {.kind = Movement::Kind::GoNearObject, .object = villager, .maxDistance = input.height * k_GoNearHeights},
	     .gaze = Gaze {.object = villager}},
	    {.kind = Step::Kind::Move,
	     .seconds = k_FaceVillagerSeconds,
	     .movement = {.kind = Movement::Kind::TurnToFaceObject, .object = villager},
	     .gaze = Gaze {.object = villager}},
	    {.kind = Step::Kind::Action, .animation = k_SummonAnimation},
	    order({.kind = DanceOrder::Kind::Set, .point = input.place, .type = input.type}),
	    order({.kind = DanceOrder::Kind::Summon, .villager = villager, .count = input.fewestDancers}),
	    order({.kind = DanceOrder::Kind::StopLooking}),
	    // The head no longer turns from here on, so these steps give it nothing to look at
	    {.kind = Step::Kind::Move, .movement = {.kind = Movement::Kind::ToPoint, .point = input.place}},
	    {.kind = Step::Kind::Object,
	     .order = {.kind = creature_mind::ObjectOrder::Kind::PointAt, .point = input.place, .seconds = k_PointSeconds},
	     .face = creature_face::Cue::Smile,
	     .main = true},
	    order({.kind = DanceOrder::Kind::SendToPlaces}),
	    order({.kind = DanceOrder::Kind::WaitUntilStill}),
	    order({.kind = DanceOrder::Kind::Start}),
	    order({.kind = DanceOrder::Kind::TellToDance, .villager = villager}),
	    order({.kind = DanceOrder::Kind::Dance, .count = input.beats * k_TurnsPerBeat}),
	};
}

std::optional<std::vector<uint32_t>> creature_dance::Summon(std::span<const uint32_t> free, uint32_t fewest, uint32_t adults,
                                                            const std::function<bool(uint32_t)>& start)
{
	const auto available = static_cast<uint32_t>(free.size());
	if (available < fewest)
	{
		return std::nullopt;
	}
	std::vector<uint32_t> joined;
	for (const auto villager : free)
	{
		if (!start(villager))
		{
			continue;
		}
		joined.push_back(villager);
		const auto count = static_cast<uint32_t>(joined.size());
		const bool enough = count >= k_MostDancers || (available % 2 == 1 && available - 1 <= count) || adults * 2 / 3 < count;
		if (enough && count % 2 == 0)
		{
			break;
		}
	}
	return joined;
}

float creature_dance::WorshipBelief(float unmodifiedPopulation, uint32_t people, float impressiveSize,
                                    float danceImpressiveness)
{
	const float impression = impressiveSize * danceImpressiveness;
	const float share = (unmodifiedPopulation + k_BeliefShareSmall) / (static_cast<float>(people) + k_BeliefShareSmall);
	return share * impression;
}

DanceTurn creature_dance::Turn(const creature_mind::Senses::DanceView& view, glm::vec2 position)
{
	DanceTurn turn;
	// Before its move, the creature stops what its body did for the other moves: a pose, unless it poses now, and a
	// dance, unless it dances or pulls a face now
	const auto move = static_cast<CreatureMove>(view.action);
	turn.endPose = move != CreatureMove::Pose;
	turn.endDance = move != CreatureMove::Dance && move != CreatureMove::Face;
	turn.endPoint = move != CreatureMove::Gesture && move != CreatureMove::FaceAngle;
	switch (move)
	{
	case CreatureMove::FaceCentre:
	case CreatureMove::FaceAway:
	{
		const auto action = static_cast<CreatureMove>(view.action) == CreatureMove::FaceCentre
		                        ? ecs::dance_moves::Action::FaceCentre
		                        : ecs::dance_moves::Action::FaceAway;
		const auto angle = ecs::dance_moves::Facing({
		    .action = action,
		    .towardsCentre = gutils::GetAngleFromXZ(position, view.groupCentre),
		});
		if (angle.has_value())
		{
			turn.facePoint = AlongGameAngle(position, *angle);
		}
		break;
	}
	case CreatureMove::FaceDance:
	{
		// The game cuts the sum of the two angles to whole radians, then takes a full turn off it once when it is more
		// than a full turn
		auto whole = map_coords::FtoL(view.danceAngle + view.second);
		if (static_cast<float>(static_cast<uint16_t>(whole)) > k_FullTurn)
		{
			whole = map_coords::FtoL(static_cast<float>(static_cast<uint16_t>(whole)) - k_FullTurn);
		}
		const auto angle = gutils::ConvertAngle3DToGame(static_cast<float>(static_cast<uint16_t>(whole)));
		turn.facePoint = AlongGameAngle(position, static_cast<uint16_t>(angle));
		break;
	}
	case CreatureMove::Dance:
		turn.danceMoveMs = static_cast<float>(map_coords::FtoL(view.danceRate * k_DanceMoveMsPerRate));
		break;
	case CreatureMove::Action:
		turn.playOnce = view.first;
		break;
	case CreatureMove::Pose:
		turn.pose = view.first;
		break;
	case CreatureMove::Face:
		turn.face = view.first;
		turn.faceMs = k_DanceFaceMs;
		break;
	case CreatureMove::FaceAngle:
		turn.facePoint = AlongGameAngle(position, static_cast<uint16_t>(map_coords::FtoL(view.second)));
		turn.turnWithin = k_FaceAngleWithin;
		break;
	case CreatureMove::Gesture:
		turn.gesture = view.first;
		break;
	case CreatureMove::Talk:
		turn.overlay = k_TalkAnimation;
		break;
	case CreatureMove::SecondAction:
		turn.playOnce = k_SecondAction;
		break;
	default:
		// TODO(creature-village-dances): the moves that look at a member of a group and that play a set of actions on
		// a group are in no dance of a creature with villagers
		break;
	}
	if (turn.facePoint.has_value())
	{
		const auto offset = *turn.facePoint - position;
		const auto difference = creature_locomotion::HeadingOf(offset).TurnFrom(view.heading);
		if (!(std::abs(difference) > turn.turnWithin))
		{
			turn.facePoint.reset();
		}
	}
	if (view.placeDistance > k_FarFromPlace)
	{
		turn.walkTo = view.place;
	}
	return turn;
}

int32_t creature_dance::MusicType(int32_t tribe, uint32_t dancers)
{
	if (tribe < 0 || static_cast<size_t>(tribe) >= k_TribeChants.size())
	{
		return k_UnknownTribeMusic;
	}
	return k_TribeChants.at(static_cast<size_t>(tribe)) + (dancers > k_DancersForVoices ? 1 : 0);
}
