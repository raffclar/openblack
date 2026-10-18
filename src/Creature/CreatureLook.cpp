/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "CreatureLook.h"

#include <cmath>

#include <algorithm>
#include <array>
#include <numbers>

#include <glm/ext/vector_int2.hpp>
#include <glm/geometric.hpp>
#include <glm/vec2.hpp>

#include "3D/MapCoords.h"
#include "Common/GUtilsAngle.h"
#include "Common/GUtilsDistance.h"
#include "Creature/CreatureObjectBeliefs.h"

using namespace openblack;
using namespace openblack::creature_look;

namespace
{
constexpr std::array<float, 11> k_Interest {0.9f, 0.9f, 0.85f, 0.7f, 0.5f, 0.4f, 0.3f, 0.25f, 0.9f, 0.4f, 0.0f};
/// The land the creature looks over is a square of map cells, this many metres across each
constexpr float k_CellMetres = 10.0f;
/// The square is the odd number of cells nearest below the square root of this much per size, and this much more
constexpr float k_CellsSquaredPerSize = 60.0f;
constexpr float k_CellsSquaredAtSizeZero = 160.0f;
/// A full turn in game angles
constexpr int32_t k_Circle = 0x800;
/// How far ahead the creature looks when nothing catches its eye
constexpr float k_AheadMetres = 50.0f;
} // namespace

float creature_look::InterestOf(Interest kind)
{
	return k_Interest.at(static_cast<size_t>(kind));
}

float creature_look::LookRange(float size)
{
	const auto root = std::sqrt(std::max((k_CellsSquaredPerSize * size) + k_CellsSquaredAtSizeZero, 0.0f));
	const auto halfCells = static_cast<uint32_t>(root) / 2;
	return k_CellMetres * static_cast<float>((halfCells * 2) + 1);
}

float creature_look::DistanceFactor(float distance, float range)
{
	if (range <= 0.0f)
	{
		return 0.0f;
	}
	return distance < range * 0.5f ? 1.0f : 1.0f - (distance / range);
}

bool creature_look::InView(uint16_t looking, uint16_t facing, uint16_t direction, bool sameCell)
{
	if (sameCell)
	{
		return true;
	}
	const auto apart = std::abs(static_cast<int32_t>(facing) - static_cast<int32_t>(direction));
	return creature_object_beliefs::CanSee(looking, direction, false) && apart < k_Circle / k_BodyViewParts;
}

float creature_look::WeightOf(const Weights& weights, Sight sight)
{
	switch (sight)
	{
	case Sight::Mountains:
		return weights.mountains;
	case Sight::Water:
		return weights.water;
	case Sight::Thing:
	default:
		return weights.things;
	}
}

bool creature_look::Watches(const Target& target)
{
	return target.id.has_value() || target.place.has_value();
}

float creature_look::Hold(const Target& target, float distance, float range, float weight)
{
	if (!Watches(target))
	{
		return 0.0f;
	}
	return weight * target.interest * DistanceFactor(std::clamp(distance, 0.0f, std::max(range, 0.0f)), range);
}

namespace
{
/// How much a new sight catches the eye: less as the whole seconds of watching go up to k_BoredSeconds
float Catches(const Target& target, float weight, float interest, float distance, float range, uint32_t turnsPerSecond)
{
	const auto watchedSeconds = turnsPerSecond > 0 ? target.watchedTurns / turnsPerSecond : 0u;
	const auto boredom =
	    1.0f - (static_cast<float>(std::min(watchedSeconds, k_BoredSeconds)) / static_cast<float>(k_BoredSeconds));
	return boredom * weight * DistanceFactor(distance, range) * interest;
}

/// Whether a sight that catches the eye this much takes over what the creature watches
bool TakesOver(const Target& target, float catches, float watchedDistance, float range, const Weights& weights)
{
	return !Watches(target) || Hold(target, watchedDistance, range, WeightOf(weights, target.sight)) < catches;
}
} // namespace

bool creature_look::Consider(Target& target, const Candidate& candidate, float watchedDistance, float range,
                             const Weights& weights, uint32_t turnsPerSecond)
{
	const auto interest = InterestOf(candidate.kind);
	const auto catches = Catches(target, weights.things, interest, candidate.distance, range, turnsPerSecond);
	if (!TakesOver(target, catches, watchedDistance, range, weights))
	{
		return false;
	}
	target = {.id = candidate.id, .sight = Sight::Thing, .interest = interest, .point = target.point, .watchedTurns = 0};
	return true;
}

bool creature_look::ConsiderLand(Target& target, const LandCandidate& candidate, float watchedDistance, float range,
                                 const Weights& weights, uint32_t turnsPerSecond)
{
	const auto catches =
	    Catches(target, WeightOf(weights, candidate.sight), k_LandInterest, candidate.distance, range, turnsPerSecond);
	if (!TakesOver(target, catches, watchedDistance, range, weights))
	{
		return false;
	}
	target = {.sight = candidate.sight,
	          .place = candidate.place,
	          .interest = k_LandInterest,
	          .point = target.point,
	          .watchedTurns = 0};
	return true;
}

void creature_look::Watch(Target& target, bool inView)
{
	if (!Watches(target))
	{
		return;
	}
	if (inView)
	{
		++target.watchedTurns;
	}
	else
	{
		target = {};
	}
}

glm::vec3 creature_look::PointAhead(const Viewer& viewer)
{
	const glm::vec3 flat {viewer.ahead.x, 0.0f, viewer.ahead.z};
	const auto ahead = glm::length(flat) > 0.0f ? glm::normalize(flat) : glm::vec3(0.0f, 0.0f, -1.0f);
	return viewer.position + glm::vec3(0.0f, k_HeadHeight * viewer.size, 0.0f) + (ahead * k_AheadMetres);
}

WhileGoing creature_look::StartWhileGoing(const WhileGoingInfo& info)
{
	return {
	    .sweepAngleStep = info.sweepAngleStep,
	    .sweepDistance = info.sweepNearest,
	    .sweepDistanceStep = info.sweepDistanceStep,
	};
}

bool creature_look::Notices(bool itself, bool moving)
{
	return moving && !itself;
}

bool creature_look::IsRecent(uint32_t noticedTurn, uint32_t turn, float turnsPerSecond)
{
	// The count of turns goes round to zero after its largest value, as the game's count does
	const auto window = static_cast<uint32_t>(std::trunc(k_MoverSeconds * turnsPerSecond));
	return turn - noticedTurn < window;
}

std::optional<WhileGoingPoint> creature_look::LookWhileGoing(WhileGoing& state, const WhileGoingInfo& info,
                                                             const WhileGoingView& view)
{
	// The table distance between the two map positions
	const auto distance = gutils::GetDistanceInMetres(view.position, view.place);
	if (distance <= k_WhileGoingLeast)
	{
		return std::nullopt;
	}
	// The game keeps the point of the look as a map position
	const auto onMap = [](glm::vec2 at) { return map_coords::ToMetres(map_coords::FromMetres(at)); };
	const WhileGoingPoint place {.at = onMap(view.place)};
	if (distance <= info.placeRange)
	{
		return place;
	}
	auto point = place;
	switch (state.look)
	{
	case WhileGoing::Look::Place:
		break;
	case WhileGoing::Look::Watched:
		// What it watches comes first. Then the last moving thing that it noticed, if it noticed the thing recently.
		if (view.watched.has_value())
		{
			point = {.at = onMap(glm::vec2(view.watched->x, view.watched->z)), .height = view.watched->y};
		}
		else if (view.mover.has_value() && IsRecent(view.moverTurn, view.turn, view.turnsPerSecond))
		{
			point = {.at = onMap(glm::vec2(view.mover->x, view.mover->z)), .height = view.mover->y};
		}
		break;
	case WhileGoing::Look::Sweep:
	{
		// The head turns a step further each turn; at each side it turns back, and looks a step further ahead
		state.sweepAngle += state.sweepAngleStep;
		if (state.sweepAngle <= -info.sweepHalfAngle || info.sweepHalfAngle <= state.sweepAngle)
		{
			state.sweepAngleStep = -state.sweepAngleStep;
			state.sweepDistance += state.sweepDistanceStep;
			if (info.sweepFurthest <= state.sweepDistance)
			{
				state.sweepDistance = info.sweepNearest;
			}
		}
		const auto angle = static_cast<uint16_t>(gutils::ConvertScawenAngleToGameAngle(view.heading + state.sweepAngle));
		// The step goes from the creature's map position, and the game makes the sum a map position again
		const auto from = onMap(view.position);
		const glm::ivec2 to {map_coords::ToFixedGUtils(from.x + gutils::GetXByAngle(angle, state.sweepDistance)),
		                     map_coords::ToFixedGUtils(from.y + gutils::GetZByAngle(angle, state.sweepDistance))};
		point = {.at = {map_coords::ToMetres(to.x), map_coords::ToMetres(to.y)}};
		break;
	}
	}
	// After two seconds of turns the next look starts
	++state.turns;
	if (static_cast<float>(state.turns) > std::trunc(k_WhileGoingLookSeconds * view.turnsPerSecond))
	{
		state.turns = 0;
		state.look = state.look == WhileGoing::Look::Sweep
		                 ? WhileGoing::Look::Place
		                 : static_cast<WhileGoing::Look>(static_cast<uint8_t>(state.look) + 1);
	}
	return point;
}
