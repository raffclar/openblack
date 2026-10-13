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

#include <glm/geometric.hpp>
#include <glm/vec2.hpp>

#include "Common/GUtilsAngle.h"

using namespace openblack;
using namespace openblack::creature_look;

namespace
{
constexpr std::array<float, 8> k_Interest {0.9f, 0.9f, 0.85f, 0.7f, 0.5f, 0.4f, 0.3f, 0.25f};
/// How much things on the land catch a creature's eye, against mountains and the sea, which it doesn't look at yet
constexpr float k_ObjectWeight = 1.0f;
/// The land the creature looks over is a square of map cells, this many metres across each
constexpr float k_CellMetres = 10.0f;
/// The square is the odd number of cells nearest below the square root of this much per size, and this much more
constexpr float k_CellsSquaredPerSize = 60.0f;
constexpr float k_CellsSquaredAtSizeZero = 160.0f;
/// Creatures see things up to a sixth of a turn either side of ahead, and anything in their own cell
constexpr float k_HalfFieldOfView = std::numbers::pi_v<float> / 3.0f;
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

bool creature_look::CanSee(const Viewer& viewer, const glm::vec3& point)
{
	const glm::vec2 offset {point.x - viewer.position.x, point.z - viewer.position.z};
	const auto distance = glm::length(offset);
	if (distance < k_CellMetres)
	{
		return true;
	}
	if (distance > LookRange(viewer.size))
	{
		return false;
	}
	const glm::vec2 ahead {viewer.ahead.x, viewer.ahead.z};
	if (glm::length(ahead) <= 0.0f)
	{
		return false;
	}
	const auto cosine = glm::dot(offset / distance, glm::normalize(ahead));
	return cosine > std::cos(k_HalfFieldOfView);
}

float creature_look::Priority(const Viewer& viewer, Interest kind, const glm::vec3& point)
{
	const auto distance = glm::length(glm::vec2(point.x - viewer.position.x, point.z - viewer.position.z));
	const auto range = LookRange(viewer.size);
	return InterestOf(kind) * k_ObjectWeight * DistanceFactor(std::min(distance, range), range);
}

Target creature_look::LookAbout(Target target, std::span<const Candidate> candidates, const Viewer& viewer,
                                float turnsPerSecond)
{
	if (target.id.has_value())
	{
		const auto current = std::ranges::find(candidates, *target.id, &Candidate::id);
		if (current == candidates.end() || !CanSee(viewer, current->point))
		{
			target = {};
		}
		else
		{
			target.point = current->point;
			++target.watchedTurns;
		}
	}

	for (const auto& candidate : candidates)
	{
		if (candidate.id == target.id || !CanSee(viewer, candidate.point))
		{
			continue;
		}
		const auto watchedSeconds = turnsPerSecond > 0.0f ? static_cast<float>(target.watchedTurns) / turnsPerSecond : 0.0f;
		const auto boredom = 1.0f - (std::clamp(watchedSeconds, 0.0f, k_BoredSeconds) / k_BoredSeconds);
		const auto priority = boredom * Priority(viewer, candidate.kind, candidate.point);
		if (!target.id.has_value() || Priority(viewer, target.kind, target.point) < priority)
		{
			target = {.id = candidate.id, .kind = candidate.kind, .point = candidate.point, .watchedTurns = 0};
		}
	}
	return target;
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
	const auto distance = glm::distance(view.position, view.place);
	if (distance <= k_WhileGoingLeast)
	{
		return std::nullopt;
	}
	const WhileGoingPoint place {.at = view.place};
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
			point = {.at = glm::vec2(view.watched->x, view.watched->z), .height = view.watched->y};
		}
		else if (view.mover.has_value() && IsRecent(view.moverTurn, view.turn, view.turnsPerSecond))
		{
			point = {.at = glm::vec2(view.mover->x, view.mover->z), .height = view.mover->y};
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
		point = {.at = view.position + glm::vec2(gutils::GetXByAngle(angle, state.sweepDistance),
		                                         gutils::GetZByAngle(angle, state.sweepDistance))};
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
