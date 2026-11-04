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

#include <optional>
#include <span>
#include <variant>
#include <vector>

#include <glm/vec3.hpp>

// The usual draw of a creature body that shows its hair keeps the light and the added colour of that hair. The draw of
// a body with the environment map does not make these colours. It lights its hair with the colours that the last usual
// hair draw kept, in the order that the game draws the bodies. Nothing clears the two colours between frames, so a
// body that the game draws before all usual hair in a frame takes the colours of the last usual hair of an earlier
// frame. Before the first usual hair draw, the light is white and the added colour is black.
//
// The game draws the bodies in this order: the creatures of the land's draw list in the order of the list, then the
// interface. A body draws only when its sphere is on the screen. A hand never holds a creature. The statistics box draws
// last, with the colours that the world's draw of the frame gave each body: the light and the haze at its place, or a
// white light and no added colour when the world did not draw it.
namespace openblack::creature_hair_light
{

/// The light and the added colour of a hair draw, 0 to 255 on each channel
struct Colours
{
	glm::vec3 light {255.0f};
	glm::vec3 added {0.0f};

	[[nodiscard]] bool operator==(const Colours&) const = default;
};

/// The colours before the first usual hair draw: a white light and no added colour
inline constexpr Colours k_Start {.light = glm::vec3(255.0f), .added = glm::vec3(0.0f)};

/// A creature body that a frame draws, in the order that the game draws the bodies
struct BodyDraw
{
	/// The body draws with the environment map
	bool environmentMapped {false};
	/// The body has the usual draw and the draw shows its hair, so the draw keeps the colours of its hair
	bool keepsHairLight {false};
};

/// The colours of the usual hair draw at this place in the frame's order
struct FromDraw
{
	size_t draw {0};

	[[nodiscard]] bool operator==(const FromDraw&) const = default;
};

/// The colours that an earlier frame kept: those of its last usual hair draw, or the start colours
struct FromFrameBefore
{
	[[nodiscard]] bool operator==(const FromFrameBefore&) const = default;
};

/// Where the hair of a body with the environment map gets its colours
using Source = std::variant<FromFrameBefore, FromDraw>;

/// The colours of the hair of each body with the environment map, and what the frame keeps for the next frame
struct Plan
{
	/// One entry for each draw of the frame: the source of the colours for a body with the environment map, none for
	/// the other bodies
	std::vector<std::optional<Source>> hair;
	/// The last usual hair draw of the frame, none when the frame has no usual hair draw. The next frame takes its
	/// colours.
	std::optional<size_t> last;
};

/// Find the source of the hair colours for each body with the environment map, from the frame's draws in the game's
/// order
[[nodiscard]] Plan PlanFrame(std::span<const BodyDraw> draws);

/// The colours that the hair of a usual draw takes at its place in this frame. The renderer finds them on the GPU,
/// from the land's light and the haze at that place.
struct AtPlace
{
	size_t draw {0};

	[[nodiscard]] bool operator==(const AtPlace&) const = default;
};

/// The colours that the renderer kept on the GPU at the end of an earlier frame
struct Kept
{
	[[nodiscard]] bool operator==(const Kept&) const = default;
};

/// How the renderer finds the colours of a hair: colours that it knows, the colours at the place of a draw, or the
/// colours that it kept
using Look = std::variant<Colours, AtPlace, Kept>;

/// What a frame keeps for the next frame: colours that the renderer knows, or the colours that it kept on the GPU
using Carried = std::variant<Colours, Kept>;

/// How the renderer lights the hair of a body with the environment map. `known` has the colours of each usual draw
/// that has colours of its own, as the creature in the falling spell's film has. `before` is what the frame before
/// kept.
[[nodiscard]] Look LookOf(const Source& source, std::span<const std::optional<Colours>> known, const Carried& before);

/// What a frame keeps for the next frame: the colours of its last usual hair draw, else what the frame before kept
[[nodiscard]] Carried Carry(const Plan& plan, std::span<const std::optional<Colours>> known, const Carried& before);

/// The colours of the hair that the statistics box draws, when they are colours of its own. A body that the world's draw
/// of the frame drew has the colours at its place in the world, so the result is empty. Else the body has a white light
/// and no added colour.
[[nodiscard]] std::optional<Colours> StatisticsBoxColours(bool drawnByTheWorld) noexcept;

} // namespace openblack::creature_hair_light
