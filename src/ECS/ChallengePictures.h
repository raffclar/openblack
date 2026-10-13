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

#include <functional>
#include <optional>
#include <span>
#include <string>
#include <vector>

#include <entt/entity/fwd.hpp>

#include "Graphics/ScreenshotCapture.h"

namespace openblack::ecs
{
class Registry;
}

namespace openblack::ecs::components
{
struct ChallengePicture;
}

/// The pictures of the player's challenges (components::ChallengePicture): a small, softened copy of the screen taken
/// when a challenge's script asks for one, for the temple's challenge room
namespace openblack::ecs::challenge_pictures
{

/// A picture in high detail, RGB
constexpr uint32_t k_Width = 256;
constexpr uint32_t k_Height = 128;
/// A picture in low detail, RGB: every other row of the high detail one, each pixel two of its pixels side by side
constexpr uint32_t k_SmallWidth = k_Width / 2;
constexpr uint32_t k_SmallHeight = k_Height / 2;

/// The part of the screen a picture is taken of
struct Region
{
	uint32_t x {0};
	uint32_t y {0};
	uint32_t width {0};
	uint32_t height {0};
};

/// The game only ever ran at 4:3, where the picture is of the whole screen: on any other shape it is of the largest 4:3
/// part of the middle of the screen, so that it looks the same
[[nodiscard]] Region PictureRegion(uint32_t width, uint32_t height);

/// The picture in high detail of a frame of the screen (BGRA, as read back): 256 by 128 pixels picked evenly across the
/// region, each the mean of itself and the pixel picked before it in its row, then softened by blending each pixel half
/// and half with the ones to its right, below it, and below to the right (the last row and column stay as picked).
/// Empty if the frame is too small to hold its rows. It uses no game data, so the renderer can call it on its own thread.
[[nodiscard]] std::vector<uint8_t> Take(const graphics::ScreenView& screen);

/// The picture in low detail made from one in high detail
[[nodiscard]] std::vector<uint8_t> Shrink(std::span<const uint8_t> full);

/// Asks for a picture for a challenge's record: the name the screen's picture is to be asked under, which is kept as
/// waiting until it comes back. A picture still waiting is replaced by the new one.
[[nodiscard]] std::string Ask(components::ChallengePicture& picture, entt::entity record);

/// What the renderer has done with the pictures asked for
struct Screens
{
	/// The picture in high detail made for a name (empty if the frame was too small), when it is back
	std::function<std::optional<std::vector<uint8_t>>(const std::string& name)> take;
	/// Whether the picture asked for under a name was given up
	std::function<bool(const std::string& name)> failed;
};

/// Whether a picture of the screen is asked for and not back yet: the game had no debug windows, so they are left out
/// of the frames a picture may be taken of
[[nodiscard]] bool AnyWaiting(const Registry& registry);

/// Brings every record's picture up to date with the screens that are back: one given up leaves the old picture
void Collect(Registry& registry, const Screens& screens);

} // namespace openblack::ecs::challenge_pictures
