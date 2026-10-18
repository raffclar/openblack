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

#include <glm/vec2.hpp>
#include <glm/vec4.hpp>

#include "Common/Zoomer.h"

/// The sing-along of the missionaries' verses: while a script plays one of the three sung verses, a glow box glides
/// under the word of the newest dialogue text that the song has reached. Pure, tested with made-up values.
namespace openblack::help::sing_along
{

/// The music types of the three sung verses, as the scripts number them
constexpr int32_t k_FirstVerse = 51;
constexpr int32_t k_LastVerse = 53;
/// The time that the box takes to glide to a new word, in seconds
constexpr float k_GlideSeconds = 0.18f;
/// The longest frame step that the glide takes, in milliseconds
constexpr int32_t k_MostStepMilliseconds = 500;

/// Whether a script's music type is one of the sung verses
[[nodiscard]] bool IsSungVerse(int32_t scriptMusicType);

/// The frame step of the glide, in milliseconds: the real frame time inside the temple, else the game time of the frame.
/// It is never below 0 or above 500.
[[nodiscard]] int32_t StepMilliseconds(bool inTemple, int32_t realMs, int32_t gameMs);

/// The colour of the glow: the colour of the word, with half its alpha
[[nodiscard]] glm::vec4 GlowColour(uint8_t r, uint8_t g, uint8_t b, uint8_t a);

/// The box of the glow on the screen, in whole pixels
struct Box
{
	glm::ivec2 min;
	glm::ivec2 max;
};

/// The glow box. Its left, top and width glide to the word on each frame that the word shows, and stay where they are
/// while no word shows, so the next verse starts from there.
class Highlight
{
public:
	/// The box glides for one frame step towards a word at a place (its left and top), of a width, on a line of a
	/// height. Returns the box after the step.
	[[nodiscard]] Box Step(glm::vec2 wordAt, float wordWidth, float lineHeight, int32_t stepMs);

private:
	Zoomer _left;
	Zoomer _top;
	Zoomer _width;
};

} // namespace openblack::help::sing_along
