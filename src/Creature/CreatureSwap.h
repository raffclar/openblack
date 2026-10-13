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
#include <cstdint>

#include <optional>

#include <glm/vec3.hpp>

#include "BonusCreatures.h"
#include "Particles/PlasmaCommand.h"

/// Swapping creatures: a script can move the player's creature's mind into another creature's body, and that creature's
/// mind into the player's old one. What it has learnt, how it feels, its physique and its tattoos go with the mind; each
/// body keeps its name and species. The new body becomes the player's creature, and over the swap each body grows or
/// shrinks to the other's size and takes on the other's alignment, so those go with the mind too, little by little.
namespace openblack::creature_swap
{

/// A body's size and alignment, which it takes from the other body over a swap
struct Shape
{
	float size {1.0f};
	float alignment {0.0f};
};

/// Each turn a body closes this share of the gap to the shape it takes on
inline constexpr float k_EaseShare = 0.04f;
/// It has taken on the shape once both its size and its alignment are nearer than this
inline constexpr double k_Settled = 0.01;

/// Whether the body has taken on the shape: both its size and its alignment within the settled distance of it
[[nodiscard]] bool IsSettled(const Shape& now, const Shape& target);

/// One turn of a body taking on the shape: a share of each gap closed, the alignment kept from -1 to 1
[[nodiscard]] Shape Ease(const Shape& now, const Shape& target);

/// While a body takes on its new shape, every third game turn a beam of light runs from a point of it to a point of the
/// other body
inline constexpr uint32_t k_BeamEveryTurns = 3;
/// Each beam lasts a second and a half at the plain speed, faded in to a random alpha from 40 up to 74
inline constexpr float k_BeamLife = 1.5f;
inline constexpr float k_BeamSpeed = 1.0f;
inline constexpr uint32_t k_BeamLowestAlpha = 40;
inline constexpr uint32_t k_BeamAlphaDraws = 35;

/// Whether a beam runs on this game turn
[[nodiscard]] bool IsBeamTurn(uint32_t turn);

/// The beam from a point of one body to a point of the other, leaving and arriving along the line between them (straight
/// up when the points are all but the same); alphaDraw is the random draw below k_BeamAlphaDraws
[[nodiscard]] particles::PlasmaCommand Beam(glm::vec3 from, glm::vec3 to, uint32_t alphaDraw);

/// The sightings of a miracle a mind keeps as it moves into a body of another species, so that it is as far towards
/// learning it as before: scaled by the times the new species must see it over the times the old one must, rounded
/// down. Kept as they are when the old species never needs to see it.
[[nodiscard]] uint32_t RescaledSightings(uint32_t seen, float neededBefore, float neededAfter);

} // namespace openblack::creature_swap
