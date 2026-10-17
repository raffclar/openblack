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

/// How the game mirrors the player's hand in the sea. It mirrors the hand on each frame of the land while the hand
/// shows, wherever the hand is. It draws the hand in one flat grey, with no light, no glow and only its front faces. The
/// game mirrors the thing that the hand holds with it, flat in the light that the thing took in the hand.
namespace openblack::graphics::hand_reflection
{

/// The colour of each vertex of the mirrored hand (0xAARRGGBB). The hand's material takes its alpha from the texture
/// alone, so the alpha of this colour has no effect.
inline constexpr uint32_t k_Colour = 0x65A0A0A0u;
/// The colour added after the texture (0xRRGGBB): none
inline constexpr uint32_t k_Added = 0x000000u;

/// The colour of each vertex, red, green and blue from 0 to 1
[[nodiscard]] glm::vec3 Colour() noexcept;
/// The colour added after the texture, red, green and blue from 0 to 1
[[nodiscard]] glm::vec3 Added() noexcept;

/// Whether the game mirrors the hand: the hand shows and has a model. The height of the hand, the land under it and the
/// camera have no effect.
[[nodiscard]] constexpr bool Shown(bool handShows, bool hasModel) noexcept
{
	return handShows && hasModel;
}

/// The order of the things the game mirrors after the land. The mirrored land writes no depth, so these things show
/// over all of it. Each thing hides the parts of the things after it that are behind it.
enum class MirroredThing : uint8_t
{
	Hand = 1,
	/// The thing that the hand holds
	Held = 2,
	/// The body of the player's creature
	Creature = 3,
};

/// The sort key of a mirrored thing in a view that draws the smallest key first
[[nodiscard]] constexpr uint32_t SortKey(MirroredThing thing) noexcept
{
	return static_cast<uint32_t>(thing);
}

/// Where a held thing takes its light: a point and the distance of that point in front of the camera
struct HeldLightPoint
{
	glm::vec3 point {0.0f};
	float viewDepth {0.0f};
};

/// The light of the mirrored held thing on this frame, and the light to keep for the next frame
struct HeldLightStep
{
	HeldLightPoint mirrored;
	HeldLightPoint kept;
};

/// The game draws the held thing in the hand after the sea, and each draw keeps the light at the point of the thing.
/// The sea mirrors the thing in the light that the last draw kept. On the first frame in the hand, the thing has no
/// kept light, so it takes the light where it is.
[[nodiscard]] constexpr HeldLightStep StepHeldLight(const std::optional<HeldLightPoint>& kept, HeldLightPoint now) noexcept
{
	return {.mirrored = kept.value_or(now), .kept = now};
}

/// The shade of 256 that each vertex of the mirrored held thing takes: all of its colour, as the sun does not shade it
inline constexpr float k_HeldShade = 256.0f;

} // namespace openblack::graphics::hand_reflection
