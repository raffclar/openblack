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
	/// Play an action of the creature's animation set, by the move's first argument
	Action = 35,
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
/// A full turn in radians, as a float
constexpr float k_FullTurn = 6.2831855f;
/// The music of each tribe's dance: the chants, one after the other with their voices
constexpr std::array<int32_t, 9> k_TribeChants {28, 28, 30, 32, 34, 36, 38, 40, 42};
/// A tribe past the table has this music
constexpr int32_t k_UnknownTribeMusic = 5;
/// The music has voices when more than this many dance
constexpr uint32_t k_DancersForVoices = 8;
/// The share of the town's population that the belief formula keeps above zero
constexpr double k_BeliefShareSmall = 0.001;

glm::vec2 AlongGameAngle(glm::vec2 from, uint16_t angle)
{
	return from + glm::vec2(gutils::GetXByAngle(angle, k_FaceAngleDistance), gutils::GetZByAngle(angle, k_FaceAngleDistance));
}
} // namespace

bool creature_dance::MayDanceToImpress(uint32_t lastType, uint32_t lastTurn, uint32_t turn)
{
	return lastType != k_ImpressDanceType && turn - lastTurn > k_TurnsBetweenImpressDances;
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
	     .face = creature_face::Cue::Smile},
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
	// The game works the sum out at a higher precision and keeps a float at the end
	const double impression = static_cast<double>(impressiveSize) * static_cast<double>(danceImpressiveness);
	const double share =
	    (static_cast<double>(unmodifiedPopulation) + k_BeliefShareSmall) / (static_cast<double>(people) + k_BeliefShareSmall);
	return static_cast<float>(share * impression);
}

DanceTurn creature_dance::Turn(const creature_mind::Senses::DanceView& view, glm::vec2 position)
{
	DanceTurn turn;
	switch (static_cast<CreatureMove>(view.action))
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
	case CreatureMove::Action:
		turn.playOnce = view.first;
		break;
	case CreatureMove::FaceAngle:
		turn.facePoint = AlongGameAngle(position, static_cast<uint16_t>(map_coords::FtoL(view.second)));
		turn.turnWithin = k_FaceAngleWithin;
		break;
	case CreatureMove::Talk:
		turn.overlay = k_TalkAnimation;
		break;
	case CreatureMove::SecondAction:
		turn.playOnce = k_SecondAction;
		break;
	default:
		// TODO(creature-dance-with-villagers): the other moves of a creature's group (animation speed, look at a
		// group, a face, a set of actions on a group) are in no dance a creature dances with villagers to impress
		break;
	}
	if (turn.facePoint.has_value())
	{
		const auto offset = *turn.facePoint - position;
		const auto difference = creature_locomotion::WrapAngle(creature_locomotion::HeadingOf(offset) - view.heading);
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
