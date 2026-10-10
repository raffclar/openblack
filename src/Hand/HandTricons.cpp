/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "HandTricons.h"

#include <cmath>

#include <algorithm>
#include <utility>

#include <glm/gtc/constants.hpp>

namespace openblack::hand_tricons
{

namespace
{
/// The cross falls slowly to a faint mark while shown
constexpr size_t k_CrossIndex = 3;
constexpr float k_CrossAlpha = 0.2f;
constexpr float k_CrossFallSpeed = 0.3f;
/// The icons in use while the world camera moves the view itself
constexpr float k_BusyAlpha = 0.9f;

/// The demonstration's mouse: its size, how far it sits from the icons, the gap to its word
constexpr int k_MouseSize = 32;
constexpr int k_MouseOffset = 32;
constexpr float k_Gap = 2.0f;
/// mousehelp.raw is 4 by 4 pictures; the wheel mouse's row
constexpr float k_MouseCell = 0.25f;
constexpr float k_WheelMouseRow = 2.0f;
/// The glows: the mouse's at full strength, the word's a third of it
constexpr int k_GlowAlpha = 255;
} // namespace

uint32_t Shown(uint32_t icons, bool toolTipsOn, bool handStateShowsIcons)
{
	if (!toolTipsOn || !handStateShowsIcons)
	{
		return icons & ~(icon::k_CameraIcons | (icon::k_CameraIcons << icon::k_OfferedShift));
	}
	return icons;
}

void Fade(Fades& fades, const FadeFrame& frame)
{
	for (size_t i = 0; i < k_IconCount; ++i)
	{
		const uint32_t bit = 1u << i;
		auto& alpha = fades.at(i);
		float fallSpeed = k_FadeSpeed;
		if ((frame.icons & bit) == 0)
		{
			alpha = std::max(alpha - (fallSpeed * frame.seconds), 0.0f);
			continue;
		}
		float target = k_UsedAlpha;
		if ((frame.icons & (bit << icon::k_OfferedShift)) != 0)
		{
			target = k_OfferedAlpha;
		}
		else if (frame.cameraBusy)
		{
			target = k_BusyAlpha;
		}
		if (i == k_CrossIndex)
		{
			fallSpeed = k_CrossFallSpeed;
			target = k_CrossAlpha;
			// Brighter with a thing under the hand, at once full with the action button held
			if (frame.crossNudge == CrossNudge::OverThing)
			{
				target = k_OfferedAlpha;
			}
			else if (frame.crossNudge == CrossNudge::Action)
			{
				target = k_UsedAlpha;
				alpha = target;
			}
			// Never while a demonstration plays: it goes at once
			if (frame.demonstration)
			{
				target = 0.0f;
				alpha = target;
			}
		}
		if (alpha > target)
		{
			alpha = std::max(alpha - (fallSpeed * frame.seconds), target);
		}
		else
		{
			alpha = std::min(alpha + (k_FadeSpeed * frame.seconds), target);
		}
	}
}

WorldCameraIcons WorldCamera(const WorldCameraFrame& frame, float rotateAngle)
{
	// The camera's mouse hints
	constexpr uint32_t k_HintRotate = 0x01;
	constexpr uint32_t k_HintPitch = 0x02;
	constexpr uint32_t k_HintZoom = 0x08;
	constexpr uint32_t k_HintTurning = 0x40;
	constexpr uint32_t k_HintIdle = 0x80;
	// What the scripts let the camera do
	constexpr uint32_t k_FeaturePitch = 0x01;
	constexpr uint32_t k_FeatureRotate = 0x02;
	constexpr uint32_t k_FeatureZoom = 0x04;
	constexpr uint32_t k_CrossBlinkMs = 200;
	const auto offeredOf = [](uint32_t iconBit) { return iconBit << icon::k_OfferedShift; };

	uint32_t mouse = frame.mouseHints;
	// Gripping the land, the hints are no longer only offered, and the rotate hint turns with the drag
	if (frame.grippingOnly)
	{
		mouse &= ~k_HintIdle;
		if ((mouse & k_HintRotate) != 0)
		{
			mouse |= k_HintTurning;
		}
	}
	// The keys: unless the land is gripped, the zoom key offers zooming and turning and the rotate key tilting and
	// turning; tilting or turning, by keys or by dragging, shows its icon
	uint32_t keys = 0;
	if (!frame.gripping)
	{
		if (frame.zoomKeyHeld)
		{
			keys |= k_HintRotate | k_HintZoom;
		}
		else if (frame.rotateKeyHeld)
		{
			keys |= k_HintRotate | k_HintPitch;
		}
	}
	if (frame.tilt != 0.0f || frame.pitchDragging)
	{
		keys |= k_HintPitch;
	}
	if (frame.turn != 0.0f || frame.edgeTurning)
	{
		keys |= k_HintRotate;
	}
	if (frame.fight)
	{
		mouse &= ~k_HintPitch;
	}

	// The mouse's hints, only offered while nothing is dragged
	const bool offered = (mouse & k_HintIdle) != 0;
	uint32_t icons = 0;
	for (const auto [hint, iconBit] :
	     {std::pair(k_HintPitch, icon::k_Pitch), std::pair(k_HintRotate, icon::k_Rotate), std::pair(k_HintZoom, icon::k_Zoom)})
	{
		if ((mouse & hint) != 0)
		{
			icons |= iconBit | (offered ? offeredOf(iconBit) : 0u);
		}
	}
	bool turning = (mouse & k_HintTurning) != 0;
	if (!frame.grippingOnly)
	{
		if (frame.rotatingAroundMouse)
		{
			turning = false;
			rotateAngle = 0.0f;
		}
		// The keys' icons where the mouse shows none: in use while asked, else only offered
		const auto keyIcon = [&](uint32_t hint, uint32_t iconBit, float asked) {
			if ((icons & iconBit) == 0 && (keys & hint) != 0)
			{
				icons = (icons & ~offeredOf(iconBit)) | iconBit | (asked != 0.0f ? 0u : offeredOf(iconBit));
				return true;
			}
			return false;
		};
		keyIcon(k_HintPitch, icon::k_Pitch, frame.tilt);
		if (keyIcon(k_HintRotate, icon::k_Rotate, frame.turn))
		{
			turning = false;
			rotateAngle = 0.0f;
		}
		keyIcon(k_HintZoom, icon::k_Zoom, frame.zoom);
	}
	// Coming in to the clear view, only the zoom glass
	if (frame.clearView > 0.5f)
	{
		icons = icon::k_Zoom;
	}
	// Nothing the scripts don't let the camera do
	if ((frame.features & k_FeaturePitch) == 0)
	{
		icons &= ~(icon::k_Pitch | offeredOf(icon::k_Pitch));
	}
	if ((frame.features & k_FeatureRotate) == 0)
	{
		icons &= ~(icon::k_Rotate | offeredOf(icon::k_Rotate));
	}
	if ((frame.features & k_FeatureZoom) == 0)
	{
		icons &= ~(icon::k_Zoom | offeredOf(icon::k_Zoom));
	}
	// The cross where the hand is out of its influence
	if ((frame.features & k_HelpFeature) != 0 && !frame.handInInfluence)
	{
		icons |= icon::k_Cross;
	}
	// The rotate arrow lies along the circle round the screen's middle, where the cursor crosses it
	if (turning)
	{
		rotateAngle = std::atan2(frame.cursor.y, frame.cursor.x) - glm::half_pi<float>();
	}
	// A drag gone too far blinks the cross alone
	if ((mouse & k_TooFar) != 0)
	{
		if (((frame.tickMs / k_CrossBlinkMs) & 1u) != 0)
		{
			icons = icon::k_Cross;
		}
		else
		{
			icons &= ~icon::k_Cross;
		}
	}
	if (frame.inputOff || frame.blocked)
	{
		icons = 0;
	}
	if (frame.blocked && (frame.features & k_HelpFeature) != 0)
	{
		icons |= icon::k_Cross;
	}
	return {.icons = icons, .rotateAngle = rotateAngle};
}

glm::ivec2 Place(glm::ivec2 hand, glm::ivec2 lastGrip, glm::ivec2 screen, bool cinemaBars, bool leans)
{
	int x = leans ? ((hand.x * 3) + lastGrip.x) / 4 : hand.x;
	int y = leans ? ((hand.y * 3) + lastGrip.y) / 4 : hand.y;
	// With the cinema bars on, the picture is 16:9 across the screen's width, whatever the bars' slide
	int visibleHeight = screen.y;
	if (cinemaBars)
	{
		visibleHeight -= static_cast<int>(static_cast<float>(screen.y) - (static_cast<float>(screen.x) * (9.0f / 16.0f)));
	}
	x = x > k_EdgeMargin ? std::min(x, screen.x - k_EdgeMargin) : k_EdgeMargin;
	const int top = (screen.y - visibleHeight) / 2;
	y = y > top + k_EdgeMargin ? std::min(y, top + visibleHeight - k_EdgeMargin) : top + k_EdgeMargin;
	return {x, y};
}

std::array<std::optional<Sprite>, k_IconCount> Sprites(glm::vec2 centre, float halfSize, const Fades& fades, float rotateAngle)
{
	constexpr float k_Cell = 1.0f / static_cast<float>(k_AtlasColumns);
	constexpr std::array<glm::vec2, 4> k_Corners = {glm::vec2(-1.0f, -1.0f), glm::vec2(1.0f, -1.0f), glm::vec2(1.0f, 1.0f),
	                                                glm::vec2(-1.0f, 1.0f)};
	std::array<std::optional<Sprite>, k_IconCount> sprites;
	for (size_t i = 0; i < k_IconCount; ++i)
	{
		const int alpha = static_cast<int>(fades.at(i) * 255.0f);
		if (fades.at(i) <= 0.0f || alpha == 0)
		{
			continue;
		}
		const int frame = k_FirstFrame + static_cast<int>(i);
		const auto cellMin =
		    glm::vec2(static_cast<float>(frame % k_AtlasColumns), static_cast<float>(frame / k_AtlasColumns)) * k_Cell;
		const float angle = i == 1 ? rotateAngle : 0.0f;
		const float cosine = std::cos(angle);
		const float sine = std::sin(angle);
		Sprite sprite;
		sprite.alpha = static_cast<float>(alpha) / 255.0f;
		for (size_t c = 0; c < k_Corners.size(); ++c)
		{
			const auto offset = k_Corners.at(c) * halfSize;
			// Turned clockwise on the screen, whose rows run down
			sprite.corners.at(c) =
			    centre + glm::vec2((offset.x * cosine) - (offset.y * sine), (offset.x * sine) + (offset.y * cosine));
			sprite.uvs.at(c) = cellMin + ((k_Corners.at(c) + 1.0f) * 0.5f * k_Cell);
		}
		sprites.at(i) = sprite;
	}
	return sprites;
}

bool LabelOnLeft(int x, int screenWidth, bool wasOnLeft)
{
	if (x > (screenWidth * 2) / 3)
	{
		wasOnLeft = true;
	}
	if (x < screenWidth / 3)
	{
		wasOnLeft = false;
	}
	return wasOnLeft;
}

int DemoLabelSize(bool biggerText)
{
	return biggerText ? (4 * k_MouseSize) / 5 : (2 * k_MouseSize) / 3;
}

DemoMouse LayoutDemoMouse(glm::ivec2 centre, bool labelOnLeft, float labelWidth, bool biggerText, bool moveHeld,
                          bool actionHeld)
{
	const int labelSize = DemoLabelSize(biggerText);
	const auto size = static_cast<float>(k_MouseSize);
	const auto top = static_cast<float>(centre.y - (k_MouseSize / 2));
	float mouseX = 0.0f;
	float labelX = 0.0f;
	if (labelOnLeft)
	{
		// Right-aligned left of the icons, the word first
		const float fullWidth = labelWidth + size + k_Gap;
		labelX = std::trunc(static_cast<float>(centre.x - k_MouseOffset) - fullWidth);
		mouseX = std::trunc(labelX + labelWidth + k_Gap);
	}
	else
	{
		mouseX = static_cast<float>(centre.x + k_MouseOffset);
		labelX = std::trunc(mouseX + size + k_Gap);
	}
	const float pad = static_cast<float>(k_MouseSize - labelSize) * 0.5f;

	// No button lit, the left one (the right one's picture mirrored), the right one, or both
	float column = 0.0f;
	bool mirrored = false;
	if (moveHeld && actionHeld)
	{
		column = 2.0f;
	}
	else if (moveHeld || actionHeld)
	{
		column = 1.0f;
		mirrored = moveHeld;
	}
	DemoMouse mouse;
	mouse.mouseMin = {mouseX, top};
	mouse.mouseMax = {mouseX + size, top + size};
	mouse.uvMin = {(mirrored ? column + 1.0f : column) * k_MouseCell, k_WheelMouseRow * k_MouseCell};
	mouse.uvMax = {(mirrored ? column : column + 1.0f) * k_MouseCell, (k_WheelMouseRow + 1.0f) * k_MouseCell};
	mouse.mouseGlow = {1.0f, 1.0f, 1.0f, static_cast<float>(k_GlowAlpha) / 255.0f};
	mouse.labelGlowMin = {labelX, std::trunc(top + pad)};
	mouse.labelGlowMax = {std::trunc(labelX + labelWidth), std::trunc(top + size - pad)};
	mouse.labelGlow = {1.0f, 0.0f, 0.0f, static_cast<float>(k_GlowAlpha / 3) / 255.0f};
	mouse.labelAt = {labelX, top + pad};
	mouse.labelSize = static_cast<float>(labelSize);
	mouse.labelColour = {1.0f, 0.0f, 0.0f, 1.0f};
	mouse.shadowColour = {0.0f, 0.0f, 0.0f, 1.0f};
	return mouse;
}

} // namespace openblack::hand_tricons
