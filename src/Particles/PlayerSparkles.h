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

#include "ECS/PlayerKind.h"
#include "Particles/LiquidParticles.h"

/// The sparkles that show where the other players of a network game look. Each game turn, each other player gets one
/// mote at the place of its camera, in a tint of the player's colour. The hands of the other players are not drawn.
namespace openblack::player_sparkles
{

/// The number of tints that a sparkle takes at random. Four of them change the colour, the other three keep it.
inline constexpr int32_t k_Tints = 7;
/// The full size of a sparkle
inline constexpr float k_Size = 10.0f;
/// The alpha of a sparkle
inline constexpr uint32_t k_Alpha = 0x80u;

/// The colour of a sparkle, 0xAARRGGBB: each channel of the player's colour at half strength, then changed by the tint
/// (0 to 6), with the sparkle's alpha. Tint 0 goes to red, 1 to yellow, 2 to green, and 3 makes the colour darker.
[[nodiscard]] uint32_t Colour(uint32_t playerColour, uint32_t tint);

/// The kind of a sparkle from a random number of 0 or 1: a mote with its frames reversed for 0, else a mote
[[nodiscard]] liquid_particles::Kind KindOf(uint32_t roll);

/// What another player shows a sparkle from
struct Watched
{
	/// Who plays the player
	player_kind::Kind kind {player_kind::Kind::None};
	/// The computer mind of the player plays now
	bool mindOn {false};
	/// The camera of the computer mind of the player
	glm::vec3 mindCamera {0.0f};
	/// The camera that a person reported last, none before the first report
	std::optional<glm::vec3> reportedCamera;
	/// The person left the network game
	bool left {false};
};

/// The place of the sparkle of another player, none when it shows no sparkle:
/// - A computer player shows one at the camera of its mind, only while its mind plays. A stopped mind shows none, also
///   when the player has hands and a reported camera.
/// - A human player shows one at its reported camera, only while the person is in the game.
/// - A player that nobody plays shows none.
[[nodiscard]] std::optional<glm::vec3> PlaceOf(const Watched& watched);

} // namespace openblack::player_sparkles
