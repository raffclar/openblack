/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "CaveCreature.h"

#include <cmath>

#include <algorithm>

#include <glm/vec2.hpp>

#include "Creature/CreatureLandTilt.h"
#include "Creature/CreatureLocomotion.h"

using namespace openblack;
using namespace openblack::cave_creature;

namespace
{
namespace animations = creature_layers::animations;

/// Sits or lies down, played left to right or not by the toss of a coin, unless the body is busy
void StartRest(State& state, Body& body, const Random& random)
{
	if (Busy(state, body))
	{
		return;
	}
	const bool sleeps = state.rest == Rest::Sleep;
	auto rest =
	    sleeps ? creature_layers::PlaySequence(body.body, animations::k_StartSleep, animations::k_Sleep, animations::k_EndSleep)
	           : creature_layers::PlaySequence(body.body, animations::k_StartSit, animations::k_Sit, animations::k_EndSit);
	if (rest.has_value())
	{
		rest->mirrored = random(2) != 0;
		body.body = *rest;
	}
}

/// The editor's fidgets: every 20 to 40 seconds one of a show-off, a scratch or a yawn, and from the tenth on it sits
/// down if it isn't busy. Pressing the left button starts the count again and stops what it is doing.
void Fidget(State& state, Body& body, const Frame& frame, const Random& random)
{
	state.idleMs -= frame.milliseconds;
	if (state.idleMs < 1.0f)
	{
		++state.fidgets;
		if (state.fidgets < k_IdleFidgets)
		{
			switch (random(3))
			{
			case 0:
				// The start draws the side first, also when the body is busy
				if (const auto once = creature_layers::PlayOnce(body.body, animations::k_Impress, random(2) == 1))
				{
					body.body = *once;
				}
				break;
			case 1:
				if (const auto once = creature_layers::PlayOnce(body.body, animations::k_Scratch, random(2) == 1))
				{
					body.body = *once;
				}
				break;
			default:
				if (const auto yawn = creature_layers::PlayGesture(body.gesture, animations::k_Yawn))
				{
					body.gesture = *yawn;
				}
				break;
			}
		}
		else if (!Busy(state, body))
		{
			auto sit =
			    creature_layers::PlaySequence(body.body, animations::k_StartSit, animations::k_Sit, animations::k_EndSit);
			if (sit.has_value())
			{
				sit->mirrored = random(2) != 0;
				body.body = *sit;
			}
		}
		state.idleMs = k_IdleBaseMs + static_cast<float>(random(k_IdleRandomMs));
	}
	if (frame.leftHeld)
	{
		StartIdle(state, random);
		if (Busy(state, body))
		{
			body.body = creature_layers::EndLoop(body.body);
		}
	}
}
} // namespace

float cave_creature::SizeOf(float grownSize)
{
	return std::clamp(grownSize, k_MinSize, k_MaxSize);
}

Rest cave_creature::RestFor(float size, bool coin)
{
	return coin && size < k_SleepsBelowSize ? Rest::Sleep : Rest::Sit;
}

glm::vec3 cave_creature::StandsAt(const Places& places, float size, Rest rest)
{
	auto along = (2.0f - size) * 0.5f;
	if (rest == Rest::Sleep)
	{
		along = (along - 0.5f) + (along - 0.5f);
	}
	along = std::clamp(along, 0.0f, 1.0f);
	return {places.seat.x + ((places.back.x - places.seat.x) * along), places.seat.y,
	        places.seat.z + ((places.back.z - places.seat.z) * along)};
}

GameHeading cave_creature::HeadingTowards(const glm::vec3& from, const glm::vec3& to)
{
	return creature_locomotion::HeadingOf(glm::vec2(to.x - from.x, to.z - from.z));
}

bool cave_creature::MustTurn(GameHeading heading, GameHeading wanted)
{
	return std::abs(wanted.Radians() - heading.Radians()) >= k_FacingWithin;
}

glm::mat4 cave_creature::Placement(const glm::vec3& position, const glm::mat3& rotation, const glm::vec3& scale)
{
	// All four heights of the floor are the same, so each slope is zero
	return creature_land_tilt::Placement(position, rotation, scale, creature_land_tilt::Tilt {});
}

glm::vec3 cave_creature::PointerPoint(const glm::vec3& eye, const glm::vec3& onNearPlane, float nearDistance)
{
	return eye + ((onNearPlane - eye) * (k_PointerDepth / nearDistance));
}

bool cave_creature::Busy(const State& state, const Body& body)
{
	return creature_layers::IsPlaying(body.body) || state.turn.has_value();
}

void cave_creature::StartIdle(State& state, const Random& random)
{
	state.idleMs = k_IdleBaseMs + static_cast<float>(random(k_IdleRandomMs));
	state.fidgets = 0;
}

void cave_creature::InEditor(State& state, Body& body, const Frame& frame, const Random& random)
{
	// A button held for under a second and a half has it look at the pointer
	if (frame.buttonHeld)
	{
		state.heldMs = state.heldMs.has_value() ? *state.heldMs + frame.milliseconds : 0.0f;
	}
	else
	{
		state.heldMs.reset();
	}
	if (state.heldMs.has_value() && *state.heldMs < k_PointerLookMs)
	{
		body.lookAt = frame.pointer;
	}
	else
	{
		body.lookAt.reset();
	}
	Fidget(state, body, frame, random);
}

std::optional<GameHeading> cave_creature::Step(State& state, Body& body, const Frame& frame, const Random& random)
{
	if (!frame.zoomed)
	{
		// It faces the fire: resting there, sitting looking into it; else it gets up to turn to it
		const auto wanted = HeadingTowards(frame.position, frame.fire);
		if (MustTurn(state.heading, wanted))
		{
			body.body = creature_layers::EndLoop(body.body);
			state.wantedHeading = wanted;
		}
		else
		{
			StartRest(state, body, random);
			if (state.rest == Rest::Sit)
			{
				body.lookAt = frame.fire;
			}
		}
	}
	else
	{
		// As the camera zooms onto it, it gets up and turns to face the camera. The fidgets of the editor start again
		if (!state.zoomed)
		{
			StartIdle(state, random);
			body.body = creature_layers::EndLoop(body.body);
			state.wantedHeading = HeadingTowards(frame.focus, frame.eye);
		}
		InEditor(state, body, frame, random);
	}
	state.zoomed = frame.zoomed;

	// A turn waits until the body is free
	std::optional<GameHeading> turn;
	if (state.wantedHeading.has_value() && !Busy(state, body))
	{
		turn = state.wantedHeading;
		state.wantedHeading.reset();
	}
	// Sitting by the fire, it yawns now and then
	if (state.rest == Rest::Sit && random(k_YawnRange) == k_YawnDraw)
	{
		if (const auto yawn = creature_layers::PlayGesture(body.gesture, animations::k_Yawn))
		{
			body.gesture = *yawn;
		}
	}
	return turn;
}

void cave_creature::AdvanceTurn(State& state, float milliseconds)
{
	if (!state.turn.has_value())
	{
		return;
	}
	state.turn->timeMs += milliseconds;
	if (state.turn->timeMs >= state.turn->durationMs)
	{
		state.heading = state.turn->to;
		state.turn.reset();
	}
}
