/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "VirtualInfluence.h"

#include <cmath>

#include <algorithm>

#include "3D/MapCoords.h"
#include "Common/GUtilsDistance.h"

using namespace openblack;

namespace
{
/// Strength won back each turn in influence, and the least kept outside it
constexpr float k_TurnStep = 0.1f;
/// Share of the player's influence power added to how far and how long the strength lasts
constexpr float k_PowerShare = 0.2f;
constexpr float k_PercentPerFraction = 100.0f;
/// A spark of the mana path is let out each time this many milliseconds, weighed by the strength, have been shown
constexpr float k_ManaPathMilliseconds = 10.0f;
constexpr float k_ByteScale = 255.0f;
} // namespace

std::optional<float> virtual_influence::Grant(const State& state, const map_coords::MapCoords& hand,
                                              const map_coords::MapCoords& place)
{
	// Switching it off takes the strength with it, so only the strength is asked
	const float strength = std::min(state.fraction, 1.0f);
	if (gutils::GetDistanceInMetres(hand, place) < strength * k_RadiusPerFraction)
	{
		return strength;
	}
	return std::nullopt;
}

void virtual_influence::ProcessTurn(State& state, const TurnInputs& inputs, const Settings& settings)
{
	// Where the hand is is noted every turn, whatever else happens
	state.turnHand = inputs.hand;
	if (state.disabled)
	{
		return;
	}
	if (inputs.handShielded)
	{
		state.fraction = 0.0f;
		return;
	}
	if (inputs.handInInfluence)
	{
		state.anchor = inputs.hand;
		state.manaPathStart.reset();
		state.lastInsideTurn = inputs.turn;
		state.fraction = std::min(state.fraction + k_TurnStep, 1.0f);
		return;
	}
	// Measured as the game measures between map positions, from where the hand last was in influence
	const auto anchor = state.anchor.value_or(glm::vec3(0.0f));
	const float distance = gutils::GetDistanceInMetres(map_coords::FromMetres({anchor.x, anchor.z}),
	                                                   map_coords::FromMetres({inputs.hand.x, inputs.hand.z}));
	// Chants at the worship sites slow the waning with time
	const float timeScale = 1.0f / (inputs.chants / settings.chantsToDouble + 1.0f);
	const float power = inputs.influencePower * k_PowerShare;
	const float nearness = gutils::GetDistanceModifier(distance, power + settings.maxDistance);
	const float freshness = gutils::GetDistanceModifier(timeScale * static_cast<float>(inputs.turn - state.lastInsideTurn),
	                                                    power + static_cast<float>(settings.maxTurns));
	state.fraction -= 1.0f - freshness * nearness;
	if (state.fraction < k_TurnStep)
	{
		state.fraction = 0.0f;
	}
}

bool virtual_influence::HumPlays(const State& state, bool handShielded, bool handInInfluence, bool anchorShielded,
                                 bool anchorInInfluence)
{
	return state.fraction != 0.0f && state.anchor.has_value() && !state.disabled && !anchorShielded && anchorInInfluence &&
	       !handShielded && !handInInfluence;
}

map_coords::MapCoords virtual_influence::ManaPathStart(const map_coords::MapCoords& hand, const map_coords::MapCoords& anchor)
{
	// Rounded to the nearest, a half to the even one, as the processor rounds
	const auto halfway = [](int32_t from, int32_t to) {
		return from + static_cast<int32_t>(std::nearbyint(static_cast<float>(to - from) * 0.5f));
	};
	return {.x = halfway(hand.x, anchor.x), .z = halfway(hand.z, anchor.z), .altitude = 0.0f};
}

std::optional<uint8_t> virtual_influence::EmitManaPath(State& state, float frameMilliseconds)
{
	const float strength = std::min(state.fraction, 1.0f);
	state.manaPathEmission = frameMilliseconds * strength + state.manaPathEmission;
	if (!(k_ManaPathMilliseconds < state.manaPathEmission))
	{
		return std::nullopt;
	}
	state.manaPathEmission = 0.0f;
	return static_cast<uint8_t>(map_coords::FtoL(strength * k_ByteScale));
}

uint32_t virtual_influence::ScaleColour(uint32_t argb, uint8_t scale)
{
	uint32_t scaled = 0;
	for (uint32_t shift = 0; shift < 32; shift += 8)
	{
		scaled |= ((((argb >> shift) & 0xFFu) * scale) >> 8u) << shift;
	}
	return scaled;
}

uint32_t virtual_influence::HumPitchPercent(const State& state)
{
	return static_cast<uint32_t>(std::min(state.soundFraction, state.fraction) * k_PercentPerFraction);
}
