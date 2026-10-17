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

#include <bit>
#include <optional>

#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

/// How the game mirrors the player's creature in the sea. It mirrors only the local player's creature, only its body,
/// and only when the creature stands low near the camera. It draws the mirrored body in one flat colour, with no light
/// on it, and adds a small amount of blue after the texture.
namespace openblack::graphics::creature_reflection
{

/// The colour of each vertex of the mirrored body (0xAARRGGBB). The game draws the body's materials as they are, so
/// the alpha has no effect on the opaque body.
inline constexpr uint32_t k_Colour = 0x65A0A0D0u;
/// The colour added after the texture (0xRRGGBB): blue only
inline constexpr uint32_t k_Added = 0x000030u;
/// The body is mirrored only while it stands lower than this height
inline constexpr float k_HighestStand = std::bit_cast<float>(0x40C00000u);
/// The body is mirrored only while the middle of its land block is this near the camera, or nearer
inline constexpr float k_Range = std::bit_cast<float>(0x442F0000u);
/// A land cell's side, as the game finds the cell under a point: it multiplies by one tenth
inline constexpr float k_PerCell = std::bit_cast<float>(0x3DCCCCCDu);
/// The cells along each side of the map, and the cells along each side of a block
inline constexpr int32_t k_MapCells = 512;
inline constexpr int32_t k_BlockCellsShift = 4;

/// The colour of each vertex, red, green and blue from 0 to 1
[[nodiscard]] glm::vec3 Colour() noexcept;
/// The colour added after the texture, red, green and blue from 0 to 1
[[nodiscard]] glm::vec3 Added() noexcept;

/// The land block, as its x and z along the map's blocks, of the cell under a point. The game truncates each
/// coordinate times one tenth towards zero. None off the map.
[[nodiscard]] std::optional<glm::ivec2> BlockUnder(glm::vec3 position) noexcept;

/// What the game knows about the creature and its land block when it decides to mirror the creature
struct Situation
{
	/// The creature is the first creature of the local player
	bool primaryCreature {false};
	/// A land block is under the creature
	bool onBlock {false};
	/// Some part of the block's box is in the camera's view
	bool blockInView {false};
	/// How far the camera is from the middle of the block
	float blockDistance {0.0f};
	/// How high the creature's body stands
	float height {0.0f};
	/// How far the creature has fizzed out of sight, 0 to 1
	float fizz {0.0f};
};

/// Whether the game mirrors the creature's body in the sea
[[nodiscard]] bool Shown(const Situation& situation) noexcept;

} // namespace openblack::graphics::creature_reflection
