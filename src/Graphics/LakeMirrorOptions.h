/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <optional>
#include <string_view>

namespace openblack::graphics::lake_mirrors
{
/// A developer switch for the mirrors of the lakes. The first steps make each mirror cheaper and do not change the look.
/// The last choices change the look a little, and are off until the user agrees to them.
struct Options
{
	/// Each level of the lakes in view gets a mirror. When false, the lakes show the sea's reflection.
	bool mirrors {true};
	/// A mirror leaves out what is under its lake's level
	bool floor {true};
	/// A mirror draws only the rectangle of the screen that its water can cover
	bool rects {true};
	/// A level whose water did not show in the last answer of the drawing gets no mirror
	bool occlusion {false};
	/// The mirrors whose rectangles do not overlap share the layers of one target
	bool shareLayers {true};
	/// A mirror whose rectangle covers less than this share of the screen draws at half the size
	float halfSizeBelow {0.0f};
	/// Levels nearer to each other than this height share one mirror, at the level of the larger rectangle
	float joinWithin {0.0f};
	/// A mirror whose rectangle covers less than this share of the screen draws every second frame
	float everySecondFrameBelow {0.0f};
};

/// The options in words that commas divide, each changing the default: "off", "no-floor", "no-rects", "occlusion",
/// "no-share", "half=<share>", "join=<height>", "alternate=<share>". None when a word is not known.
[[nodiscard]] std::optional<Options> ParseOptions(std::string_view words);
} // namespace openblack::graphics::lake_mirrors
