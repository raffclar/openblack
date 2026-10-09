/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <cstdint>

#include <optional>

#include <glm/vec3.hpp>

namespace openblack::virtual_influence
{

/// What the hand keeps of its player's influence once it reaches past the border: a strength from 0 to 1 that wanes
/// the further the hand goes from the last place it was in influence and the longer it stays out, and comes back a
/// tenth a turn while the hand is in influence again. Outside, a hum plays at the strength's share of its pitch.
struct State
{
	/// The last place the hand was in its player's influence, none until it has been
	std::optional<glm::vec3> anchor;
	/// The game turn the hand was last in influence
	uint32_t lastInsideTurn {0};
	/// The strength left, 0 to 1
	float fraction {1.0f};
	/// The strength when the hum began
	float soundFraction {1.0f};
	/// The hum has been started and not stopped
	bool soundStarted {false};
	/// Turned off by a script: it neither changes nor sounds
	bool disabled {false};
};

/// The citadel's settings of the game's information tables
struct Settings
{
	/// How far, in metres, the strength reaches before the player's influence power lengthens it
	float maxDistance;
	/// How many turns out the strength lasts before the player's influence power lengthens it
	uint32_t maxTurns;
	/// Chants of worship that make the strength last twice as long
	float chantsToDouble;
};

/// What a turn finds about the hand
struct TurnInputs
{
	glm::vec3 hand;
	uint32_t turn;
	/// The hand is where another player's shield keeps its player out
	bool handShielded;
	/// The hand is in its player's own influence, not counting this
	bool handInInfluence;
	/// The sum of the reach of the player's citadel, towns and other sources of influence
	float influencePower;
	/// The chants waiting at the player's worship sites
	float chants;
};

/// Each turn: in influence the hand marks the place and gets a tenth of its strength back; out of it the strength drops
/// by how far it is from that place and how long it has been out, a strength under a tenth going at once; and under a
/// shield it has none.
void ProcessTurn(State& state, const TurnInputs& inputs, const Settings& settings);

/// Whether the hum plays this frame: the hand has strength left and is out of its player's influence, unshielded, while
/// the last place it was in is still in influence and unshielded
[[nodiscard]] bool HumPlays(const State& state, bool handShielded, bool handInInfluence, bool anchorShielded,
                            bool anchorInInfluence);

/// The hum's pitch in percent: its strength's share of its normal pitch, held to what it began at
[[nodiscard]] uint32_t HumPitchPercent(const State& state);

} // namespace openblack::virtual_influence
