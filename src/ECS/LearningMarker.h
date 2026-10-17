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

#include <string>

#include <glm/vec3.hpp>
#include <glm/vec4.hpp>

namespace openblack::ecs
{
class Registry;
}

namespace openblack::ecs::components
{
struct LearningMarker;
}

/// The rules of the mark that floats up over a creature as it learns (components::LearningMarker)
namespace openblack::ecs::learning_marker
{

/// How long a mark lasts, in milliseconds
constexpr float k_LifeMilliseconds = 6000.0f;
/// Its text is fully seen this long, then fades out to the end of its life
constexpr float k_FadeStartMilliseconds = 2500.0f;
/// How fast it rises, in units a millisecond for each unit of its size
constexpr float k_RisePerMillisecond = 0.003f;
/// A mark is this share of the creature's height
constexpr float k_SizeOfHeight = 0.1f;
/// It starts this far over the creature's head
constexpr float k_OverHead = 1.0f;
/// Its glow is never smaller than this
constexpr float k_MinimumSize = 0.0001f;
/// The point its text is centred on is this many sizes over the glow's middle
constexpr float k_TextOver = 1.5f;
/// The light bulb hangs this many sizes under the glow's middle, a third of the size big
constexpr float k_BulbUnder = 0.8666666f;
constexpr float k_BulbScale = 0.33333334f;
/// The height of its text on the screen, in pixels, and how far its shadow is moved right and down
constexpr float k_TextSize = 15.0f;
constexpr float k_TextHalfHeight = 7.5f;
constexpr float k_ShadowOffset = 1.0f;
/// The mark's age turns into the blink of the bulb by this many radians a millisecond: one turn a second
constexpr float k_BlinkPerMillisecond = 0.0062831854f;

/// The mark's text for a share, 0 to 1: the share in hundredths, with no point, and a percent sign
[[nodiscard]] std::string Text(float share);
/// A mark that starts over a creature's head, sized by the creature's height
[[nodiscard]] components::LearningMarker Start(const glm::vec3& head, float creatureHeight, float share);

/// The mark moves on by some milliseconds: it gets older and rises. Whether it lasts.
[[nodiscard]] bool Step(components::LearningMarker& marker, float milliseconds);
/// How big it is at its age: from its size to twice its size over its life
[[nodiscard]] float Grown(const components::LearningMarker& marker);
/// The alpha of its text, of 255: full at first, then fading
[[nodiscard]] uint8_t TextAlpha(float milliseconds);
/// How white the light bulb is, of 255: it blinks from white to yellow and back twice a second
[[nodiscard]] uint8_t BulbWhite(float milliseconds);
/// The glow's alpha, of 255: half as strong as the bulb is yellow
[[nodiscard]] uint8_t GlowAlpha(float milliseconds);

/// What a mark shows this frame, in the world
struct Look
{
	/// The glow: its middle, half its width, and its colour and alpha, 0..1
	glm::vec3 glow {0.0f};
	float glowHalfSize {0.0f};
	glm::vec4 glowColour {0.0f};
	/// The light bulb: where it is, its scale, and its colour, 0..1
	glm::vec3 bulb {0.0f};
	float bulbScale {0.0f};
	glm::vec4 bulbColour {0.0f};
	/// The point its text is centred on, and the text's alpha, 0..1
	glm::vec3 textPoint {0.0f};
	float textAlpha {0.0f};
};
[[nodiscard]] Look LookOf(const components::LearningMarker& marker);

/// Every mark in the registry moves on, and those whose time is up go
void StepAll(Registry& registry, float milliseconds);

} // namespace openblack::ecs::learning_marker
