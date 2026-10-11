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

#include <array>
#include <chrono>

#include <entt/core/hashed_string.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

/// Rings on the water: a flat square of the smoke texture lying on the surface where something splashed, growing and
/// fading out over 0.7 seconds of game time, added over what is behind it.
namespace openblack::water_rings
{

/// The most rings there are at once: when full, a new one isn't made
inline constexpr size_t k_MostRings = 1024;
/// A ring's life, in milliseconds of game time before its rate
inline constexpr uint32_t k_Life = 700;
/// The textures the rings are cut from, 8 by 8 cells
inline constexpr entt::hashed_string k_TextureId = entt::hashed_string("raw/smoke");
inline constexpr entt::hashed_string k_AlphaTextureId = entt::hashed_string("raw/smokea");

struct Ring
{
	glm::vec3 position {0.0f};
	/// Milliseconds of game time it has grown for, at its rate
	uint32_t age {0};
	/// How big it grows: its half width is its age times this over its life
	float growth {1.0f};
	/// How it is turned about the vertical, radians from +x towards +z: its width lies that way
	float angle {0.0f};
	/// Its depth, across its angle, over its width
	float aspect {1.0f};
	/// How fast it ages
	float rate {1.0f};
	/// Its cell of the smoke texture
	uint8_t cell {0x30};
	/// Its colour, 0xAARRGGBB, as it was made: one taking the land's light keeps that of when it was made
	uint32_t argb {0xFFFFFFFFu};
};

/// The splash the hand makes going into the water where it grips the land: on the surface, a little above, turned at
/// `angle`, its colour the land's brightest light of the moment, 0xRRGGBB
[[nodiscard]] Ring HandSplash(glm::vec2 xz, float angle, uint32_t landLight);

/// A ring after `milliseconds` more of game time; false once it has gone. It ages by the whole milliseconds times its
/// rate, the fraction dropped, as the game did once a frame
[[nodiscard]] bool Advance(Ring& ring, float milliseconds);

/// When the rings age. The game aged every ring once a frame, by that frame's game time times the ring's rate with the
/// fraction dropped, and its frames came at the screen's refresh: it waits for the vertical sync by default, at the
/// display's default refresh rate. Done every frame at hundreds of frames a second, that dropped fraction would be most
/// of each step, and a ring with a slow rate (a big body's) would hardly grow at all. So the rings step no more often
/// than that refresh, 60 times a second, each step taking all the game time since the last; slower frames step once
/// each, as the game's did.
struct Pace
{
	/// The time between steps at the screen's refresh
	static constexpr std::chrono::duration<float, std::milli> k_StepInterval {1000.0f / 60.0f};

	/// The real time since the last step that counts towards the next
	std::chrono::duration<float, std::milli> sinceStep {0.0f};
	/// The game time since the last step, which the next step ages the rings by
	std::chrono::milliseconds gameSinceStep {0};
};

/// The game time the rings age by after a frame of this real and game time: nothing on a frame they don't step
[[nodiscard]] std::chrono::milliseconds Step(Pace& pace, std::chrono::milliseconds frameRealTime,
                                             std::chrono::milliseconds frameGameTime);

/// Half the ring's width, and its opacity, 0 to 255: it fades out as it grows
[[nodiscard]] float HalfWidth(const Ring& ring);
[[nodiscard]] uint8_t Alpha(const Ring& ring);

/// The ring's four corners, flat about its position, and the corners of its cell of the texture in the same order
[[nodiscard]] std::array<glm::vec3, 4> Corners(const Ring& ring);
[[nodiscard]] std::array<glm::vec2, 4> CellUvs(uint8_t cell);

} // namespace openblack::water_rings
