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

#include <array>
#include <string>

#include <glm/mat3x3.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>

#include "Enums.h"

/// The three leashes hanging on the temple's exterior. Each is the same collar, a ring drawn with one band of the
/// leash texture: the spiked blade for aggression, the twisted rope for learning, the rainbow fur for compassion. Each
/// tumbles slowly where it hangs, its texture running round it, inside a glow of smoke that is about half seen by day
/// and fades at night. Only the leashes the player's creature knows are hung, and only while the player has a creature.
/// The picked leash is carried in the hand, its glow turning orange.
namespace openblack::temple_leashes
{

/// How a leash moves: how far its texture has run round it, the two angles it has tumbled to, and the glow's picture
struct Look
{
	float scroll {0.0f};
	float pitch {0.0f};
	float roll {0.0f};
	float glow {0.0f};
};

/// It tumbles at these many radians a second about its two axes
constexpr float k_PitchRate = 0.1f;
constexpr float k_RollRate = 1.0f;
/// Its texture runs round it at this many turns a second
constexpr float k_ScrollRate = 0.5f;
/// The glow plays the first pictures of the smoke sheet at this many a second, going round
constexpr float k_GlowRate = 10.0f;
constexpr float k_GlowPictures = 15.0f;
/// Each leash starts at a random point of each: the scroll of one turn, the angles of a whole turn, the glow's pictures
constexpr float k_FullTurn = 6.2831855f;

/// The look a leash starts with, from four draws of the C library's random numbers (0 to 32767), one for each in turn
[[nodiscard]] Look Start(const std::array<int32_t, 4>& draws);
/// A draw of 0 to 32767 is this share of 1
constexpr float k_DrawShare = 3.0518509e-05f;
/// The look some seconds of game time on
[[nodiscard]] Look Advance(const Look& look, float seconds);
/// How the leash is turned where it hangs: tumbled by its two angles, about the axes the game turns it by
[[nodiscard]] glm::mat3 Turn(const Look& look);
/// The picture of the smoke sheet its glow shows
[[nodiscard]] uint8_t GlowPicture(const Look& look);

/// The band of the leash texture each leash is drawn with, by how far down the texture it starts
[[nodiscard]] float Band(LeashType type);
/// The glow's alpha, of 255, from the land's light at its brightest (0xRRGGBB): about half by day, less at night
[[nodiscard]] uint8_t GlowAlpha(uint32_t light);
/// The glow's colour: white, blended over what is behind it, or orange and added to it for the picked leash
struct Glow
{
	glm::vec4 tint {1.0f};
	bool additive {false};
};
[[nodiscard]] Glow GlowOf(bool picked, uint8_t alpha);
/// The glow is this many metres across
constexpr float k_GlowSize = 2.0f;
/// A leash is tapped within this many metres of where it hangs
constexpr float k_TapRadius = 1.0f;

/// Carried in the hand, the picked leash is drawn at half the hand's scale times this, shifted along the hand by these
/// shares of that
constexpr float k_HandScale = 173.52948f;
constexpr float k_InHandShare = 0.5f;
constexpr glm::vec3 k_InHandOffset {0.05f, 0.25f, 0.0f};

/// The hand's tooltip over a leash on the temple, by its place among the game's tooltips: "Leash Of Aggression",
/// "Leash Of Learning" or "Leash Of Compassion"
[[nodiscard]] uint32_t ToolTipOf(LeashType type);
constexpr uint32_t k_AggressionTip = 85;
constexpr uint32_t k_CompassionTip = 86;
constexpr uint32_t k_LearningTip = 87;

/// The name of the copy of the collar mesh a player's leash of a kind is drawn with: each slides its texture by its own
/// random start, as each of the game's leashes does
[[nodiscard]] std::string CollarMeshName(PlayerNames owner, LeashType type);

/// Whether a leash hangs on the temple: its player has a creature, and the creature knows that leash
[[nodiscard]] constexpr bool Hung(bool hasCreature, bool knows)
{
	return hasCreature && knows;
}

} // namespace openblack::temple_leashes
