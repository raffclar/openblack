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
#include <vector>

#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>

// How the icon of a miracle at a worship site shows its charge: a ring of light round the icon that fills with the
// charge, a shine on the icon and a copy of the pulse model that shrinks into the icon while it charges. Pure rules,
// free of the game.

namespace openblack::magic::icon_charge_looks
{

/// The clock that the shine and the pulse use. Each second of game time moves it on by a part of 3.33, and it goes
/// back to the start each time it gets past 1.
struct DecayClock
{
	float phase {0.0f};
	/// How many times it went back to the start
	uint32_t wraps {0};
};

/// The clock moves on by a frame of so many milliseconds of game time. A paused game gives no time.
[[nodiscard]] DecayClock Advance(DecayClock clock, int32_t frameMilliseconds);

/// The seconds since the clock last went past a mark, from 0 to 3.33
[[nodiscard]] float SecondsSince(float phase, float mark);

/// The ring of light round the icon: ten points on a circle of this radius round the icon's foot, the last on the first,
/// raised to this height and spread out at the top by this much. Its strength moves one point along every this many
/// seconds.
inline constexpr size_t k_RingPoints = 10;
inline constexpr float k_RingRadius = 1.0f;
inline constexpr float k_RingHeight = 6.0f;
inline constexpr float k_RingSpread = 1.5f;
inline constexpr float k_RingShiftSeconds = 0.03f;

/// The ring's points round the icon, through the icon's model matrix (axes in its columns, scale included)
[[nodiscard]] std::vector<glm::vec3> RingPoints(const glm::mat4& iconModel);

/// What the icon shows of its charge
struct Shown
{
	/// How full the ring is, 0 to 1
	float fraction {0.0f};
	/// The icon shines in a slow throb: the hand holds its seed, or its store has all that its miracle needs
	bool throbs {false};
};

/// The icon's state that its look reads
struct IconState
{
	/// A hand of the icon's player holds the seed that the icon made
	bool seedHeld {false};
	/// The prayer power that the icon has stored
	float store {0.0f};
	/// What the icon needs for a full charge
	float required {0.0f};
	/// The icon charges its miracle for a hand
	bool charging {false};
};

/// How full the ring is and whether the icon throbs. While the icon charges, the ring starts at a sixth full.
[[nodiscard]] Shown ShownCharge(const IconState& icon);

/// The seconds that the icon has charged at a frame: the whole turns since it started and the part of this turn
[[nodiscard]] float ChargeSeconds(uint32_t turn, uint32_t startTurn, float turnFraction);

/// The colour that the icon adds to its model (0xRRGGBB, 0 for none): the player's colour times a light in 256ths.
/// - A throb shines from a tenth to six tenths of the colour and back, two times in each round of the clock.
/// - Else, while the icon charges, the light flashes 8 radians a second, fading out over the first 1.5 seconds. Then
///   the icon stays at 50 256ths of the colour.
/// - Else the icon adds no colour.
[[nodiscard]] uint32_t Tint(uint32_t playerRgb, const Shown& shown, bool charging, float clockPhase, float chargeSeconds);

/// The pulse model: once in each round of the clock, while the icon charges, it shrinks into the icon over one second
/// and becomes more opaque. The clock is at this mark when the pulse starts.
inline constexpr float k_PulseMark = 0.1f;
/// How far through its pulse the model is, 0 to 1, at a phase of the clock; none while there is no pulse
[[nodiscard]] std::optional<float> PulseShare(float clockPhase);

/// The pulse model's matrix: the icon's matrix three times as large at the start of a pulse and shrinking to nothing,
/// raised along the icon's up axis by a tenth of the height of the icon's model
[[nodiscard]] glm::mat4 PulseModel(const glm::mat4& iconModel, float iconModelHeight, float share);

/// How opaque the pulse model is, 0 to 255: transparent at the start of a pulse and opaque at its end
[[nodiscard]] uint8_t PulseAlpha(float share);

/// The alpha of the effect of the seed's holder over an icon, 0 to 255: the alpha of the icon's model as the frame draws it.
/// The model has the full alpha of the land's light. A model that adds a colour multiplies that alpha by 255 in 256ths.
[[nodiscard]] constexpr uint8_t HolderAlpha(uint32_t shownTint)
{
	constexpr uint32_t k_LightAlpha = 0xFFu;
	constexpr uint32_t k_TintedAlpha = (k_LightAlpha * 0xFFu) >> 8u;
	return static_cast<uint8_t>(shownTint != 0 ? k_TintedAlpha : k_LightAlpha);
}

} // namespace openblack::magic::icon_charge_looks
