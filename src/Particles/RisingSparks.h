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
#include <functional>
#include <optional>
#include <string_view>

#include <glm/vec3.hpp>

// The shower of sparks: the sparks that rise from a gold scroll, and from a thing a script marks with the highlight effect
// (such as the gate stones of Land 1). Twenty sparks, starting 0.4 seconds apart, each rising 20 metres over 8 seconds
// of the frames' game time (faster as it goes), shrinking from 2 to 1.2 metres across, fading in over 5 seconds and out
// over the last 3, turned its own random way, and going through the first sprite sheet's first 32 pictures, one each 50
// milliseconds. Each starts again, turned another way, once its 8 seconds are up. Their time passes only on the frames
// they are drawn. Free of the game, so tests can check them.

namespace openblack::particles::rising_sparks
{

inline constexpr size_t k_Sparks = 20;
inline constexpr int32_t k_SparkLifeMilliseconds = 8000;
/// The sheet the sparks are drawn from (its alpha is the same name with an "a"), added to what is behind by their alpha,
/// with this many pictures a row
inline constexpr std::string_view k_SparkSheet = "S_SpriteSheet1";
inline constexpr uint32_t k_SparkSheetPictures = 8;

struct Spark
{
	/// Milliseconds since it started; it waits to start while this is below none
	int32_t ageMilliseconds {0};
	/// Its turn in the picture, in radians
	float angle {0.0f};
};

struct Sparks
{
	std::array<Spark, k_Sparks> sparks {};
	/// The picture the first spark starts at, of 32
	uint32_t firstPicture {0};
};

/// A random angle up to a whole turn, and a random number below 32, from the game's local random numbers
[[nodiscard]] Sparks MakeSparks(const std::function<float(float)>& randomFloat,
                                const std::function<uint32_t(uint32_t)>& random);
/// The sparks as a frame of the game's time passes
void StepSparks(Sparks& sparks, uint32_t gameMilliseconds, const std::function<float(float)>& randomFloat);

/// How a spark is drawn: where, its half size, how much of its colour it adds (of 255), its picture of the sheet and its
/// turn
struct SparkLook
{
	glm::vec3 position {0.0f};
	float halfSize {0.0f};
	uint8_t alpha {0};
	uint32_t picture {0};
	float angle {0.0f};
};
/// None while it waits to start. `from` is where they rise from; `strength` (of 255) scales how much each adds: all of
/// it over a gold scroll, the effect's own share over a thing it marks.
[[nodiscard]] std::optional<SparkLook> LookOf(const Sparks& sparks, size_t index, glm::vec3 from, uint8_t strength = 255);

/// The game's milliseconds pass a whole number at a time: the frame's share, kept with what was left over last time
struct FrameMilliseconds
{
	float carried {0.0f};
	/// The whole milliseconds this frame adds
	[[nodiscard]] uint32_t Take(float frameMilliseconds);
};

} // namespace openblack::particles::rising_sparks
