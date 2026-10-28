/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "ScrollPictures.h"

#include <cstdlib>

namespace openblack::temple::scroll_pictures
{

void Place(Pictures& out, const WorldPicture& picture, const ProjectFn& project,
           const help::tip_bubble::ControlShownFn& controlShown, float pixelScale)
{
	namespace tip = help::tip_bubble;
	using Seen = ScreenPoint::Seen;
	const auto topLeft = project(picture.textureTopLeft);
	if (topLeft.seen == Seen::Behind)
	{
		return;
	}
	const auto bottomRight = project(picture.textureBottomRight);
	if (bottomRight.seen == Seen::Behind)
	{
		return;
	}
	const auto lineTop = project(picture.lineTop);
	if (lineTop.seen != Seen::OnScreen)
	{
		return;
	}
	const auto lineBelow = project(picture.lineBelow);
	if (lineBelow.seen != Seen::OnScreen)
	{
		return;
	}
	// The height of a line on the screen, in whole pixels
	const auto line = static_cast<float>(std::abs(static_cast<int>(lineBelow.pixel.y - lineTop.pixel.y)));
	const float size = line + line;
	const float depth = lineTop.depth - k_InFront;
	const tip::WordBounds bounds {
	    .left = topLeft.pixel.x,
	    .right = bottomRight.pixel.x,
	    .clipTop = topLeft.pixel.y,
	    .fadeTopEnd = topLeft.pixel.y,
	    .fadeBottomStart = bottomRight.pixel.y,
	    .clipBottom = bottomRight.pixel.y,
	};
	// The bottom of the picture goes two lines under the top of the line (three lines for a control). The top of the
	// picture is two lines above its bottom.
	const float lowered = size + lineTop.pixel.y;
	if (picture.kind == WorldPicture::Kind::Gesture)
	{
		if (picture.number <= 0 || picture.number >= tip::k_GestureCount)
		{
			return;
		}
		const float top = lowered - size;
		const float shadow = k_ShadowOffset * pixelScale;
		const tip::WordBounds shadowBounds {
		    .left = shadow + bounds.left,
		    .right = shadow + bounds.right,
		    .clipTop = shadow + bounds.clipTop,
		    .fadeTopEnd = shadow + bounds.fadeTopEnd,
		    .fadeBottomStart = shadow + bounds.fadeBottomStart,
		    .clipBottom = shadow + bounds.clipBottom,
		};
		std::vector<tip::GestureIcon> icons;
		tip::AddGesture(icons, picture.number, shadow + top, size, shadowBounds, {0.0f, 0.0f, 0.0f, 1.0f}, true);
		for (const auto& icon : icons)
		{
			out.gestures.push_back({.icon = icon, .depth = depth - 1.0f});
		}
		icons.clear();
		tip::AddGesture(icons, picture.number, top, size, bounds, {1.0f, 1.0f, 1.0f, 1.0f}, false);
		for (const auto& icon : icons)
		{
			out.gestures.push_back({.icon = icon, .depth = depth - 2.0f});
		}
		return;
	}
	if (!controlShown || !controlShown(picture.number))
	{
		return;
	}
	const float top = (lowered + line) - size;
	tip::AddControl(out.controls, picture.number, top, size, bounds, bottomRight.pixel.y);
}

} // namespace openblack::temple::scroll_pictures
