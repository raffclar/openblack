/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <cstddef>

#include <optional>
#include <span>

#include "3D/MapCoords.h"

/// The rules of what the players are to each other: alliances, the attitude of a computer mind to each player, and how
/// much a computer god wants to attack a player. The rules use plain values and no game state.
namespace openblack::player_relations
{

/// A script gives an attitude, and the mind keeps two times that value
[[nodiscard]] float KeptAttitude(float given);
/// A script reads half of the attitude that the mind keeps
[[nodiscard]] float ReadAttitude(float kept);

/// A player is the ally of another player when the strength of the alliance is above 0
[[nodiscard]] bool IsAlly(float strength);
/// An ally lends its influence when the strength of the alliance is above 0.1, as a wide compare
[[nodiscard]] bool LendsInfluence(float strength);

/// The running average of the influence power of a player after one more turn: half of the old average and the power
[[nodiscard]] float NextAverageInfluence(float average, float power);

/// What the god knows of one side when it thinks about an attack
struct AttackSide
{
	/// The influence power of the side and its running average
	float power {0.0f};
	float averagePower {0.0f};
	/// The place of the side: its temple's heart, else its hand
	map_coords::MapCoords place;
	/// The alignment of the side, from -1 to 1
	float alignment {0.0f};
	/// The harm that the miracles of the other side did to the things of this side
	float harmFromOther {0.0f};
};

/// The distance from the god's hand to the nearest abode of its towns, and 100 metres when no abode is nearer
[[nodiscard]] float NearestAbode(const map_coords::MapCoords& hand, std::span<const map_coords::MapCoords> abodes);

/// How much the god wants to attack the rival. The value adds these parts, and the nearness of the two sides
/// multiplies the sum:
/// - the abodes of the god near its hand,
/// - a large difference of alignment,
/// - more harm from the rival than the god did to the rival,
/// - a rival that is much weaker,
/// - a stronger rival that is near,
/// - a rival whose power grows more than the god's power.
[[nodiscard]] float DesireToAttack(const AttackSide& god, const AttackSide& rival, float nearestAbode);

/// The rival that the god wants to defeat, from the value of each rival in the order of the players. The first rival
/// is the choice until a later rival has a larger value. None when there is no rival.
[[nodiscard]] std::optional<size_t> ChooseRival(std::span<const float> values);
/// How much the god wants to defeat the rival that it chose: the value of the rival, but not more than 1
[[nodiscard]] float DefeatIntensity(float value);

} // namespace openblack::player_relations
