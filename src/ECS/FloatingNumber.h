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

#include <glm/vec2.hpp>

namespace openblack::ecs
{
class Registry;
}

namespace openblack::ecs::components
{
struct FloatingNumber;
}

/// The rules of the numbers that float up from things (components::FloatingNumber)
namespace openblack::ecs::floating_number
{

/// How long a number lasts, in seconds
constexpr float k_LifeSeconds = 5.0f;
/// How fast it rises, in units a second
constexpr float k_RiseSpeed = 10.0f;
/// The height of its text on the screen, in pixels
constexpr float k_TextSize = 15.0f;
/// Its shadow is drawn this many pixels left of it and below it
constexpr float k_ShadowOffset = 1.0f;

/// The number moves on by some seconds: it rises and its life runs down. Whether it lasts.
[[nodiscard]] bool Step(components::FloatingNumber& number, float seconds);
/// Its alpha, of 255, from the life it has left: full until its last second, then fading; none once it would not show
[[nodiscard]] std::optional<uint8_t> Alpha(float life);
/// Where its text starts on the screen: it ends where its point is on the screen, there at the text's top
[[nodiscard]] glm::vec2 TextPlace(glm::vec2 screenPoint, float textWidth);

/// Every number in the registry moves on, and those whose time is up go
void StepAll(Registry& registry, float seconds);

} // namespace openblack::ecs::floating_number
