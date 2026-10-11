/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "CameraDrag.h"

#include <cmath>
#include <cstdlib>

#include <algorithm>

#include <glm/gtc/constants.hpp>

namespace openblack::camera_drag
{

namespace
{
// The hints by the cursor's place
constexpr float k_SideEdge = 0.45f;
constexpr float k_BottomEdge = 0.43f;
/// The very top and bottom of the screen, nearer the middle in a window, whose edges are harder to reach
constexpr float k_VeryEdge = 0.49f;
constexpr float k_VeryEdgeWindowed = 0.45f;
constexpr float k_TopEdgeWithoutLand = -0.4f;

/// The mouse must move this far, in shares of the screen, before a drag is decided
constexpr float k_DecideDistance = 0.02f;
/// Dragging from the side towards the middle pans the land if it is quick, within this long
constexpr uint32_t k_QuickPanMs = 300;
/// A tilting drag that goes down this soon after the press over land pans instead
constexpr uint32_t k_QuickTiltMs = 80;
/// Directions are only made unit length when they are longer than this
constexpr float k_MinDirectionLength = 0.1f;

/// The ring the cursor is held on dragging round the edge, as a share of the screen's half width and height, and the
/// band of squared distances from the middle that are left alone
constexpr float k_RingRadius = 0.9f;
constexpr float k_RingBandMin = 0.89f;
constexpr float k_RingBandMax = 0.91f;

/// A share of the field of view across the drag down the whole screen tilts by
constexpr float k_PitchPerScreen = 2.33333f;

/// With the cinema bars in, the picture is 16:9
constexpr float k_WideScreen = 0.5625f;

/// Both buttons turn the camera once the mouse has moved this share of the screen's width across
constexpr float k_TwoButtonTurnShare = 0.025f;

/// The ring's elliptical place, measured round from the right as a share of the screen's half width and the view's
/// half height, so that the same place on two screens of different shapes has the same value
float RingPlace(glm::ivec2 point, glm::ivec2 screenSize, int viewHeight)
{
	const auto x = static_cast<float>(point.x - screenSize.x / 2) / (static_cast<float>(screenSize.x) * 0.5f);
	const auto y = static_cast<float>(point.y - (screenSize.y >> 1)) / (static_cast<float>(viewHeight) * 0.5f);
	return std::atan2(y, x);
}

/// The same for a point given from the middle of a screen of the area's width
float RingPlace(glm::vec2 fromMiddle, const EdgeArea& area)
{
	return std::atan2(fromMiddle.y / (static_cast<float>(area.viewHeight) * 0.5f),
	                  fromMiddle.x / (static_cast<float>(area.width) * 0.5f));
}

/// The ring of a screen in pixels, the ellipse's half axes
glm::vec2 RingAxes(glm::ivec2 screenSize, int viewHeight)
{
	return k_RingRadius * 0.5f * glm::vec2(static_cast<float>(screenSize.x), static_cast<float>(viewHeight));
}

/// How fast the cursor moves along the ring, in pixels, as its place goes round
float RingSpeed(glm::vec2 axes, float place)
{
	const auto s = std::sin(place);
	const auto c = std::cos(place);
	return std::sqrt(axes.x * axes.x * s * s + axes.y * axes.y * c * c);
}

/// The angle round the middle of the screen swept for each pixel the cursor moves along the ring
float AnglePerPixel(glm::vec2 axes, float place)
{
	const auto s = std::sin(place);
	const auto c = std::cos(place);
	const auto squaredDistance = axes.x * axes.x * c * c + axes.y * axes.y * s * s;
	return axes.x * axes.y / (squaredDistance * RingSpeed(axes, place));
}

/// The ring is measured in this many pieces between two places
constexpr int k_RingPieces = 16;

/// The scale from the angle swept round a ring between two of its places to the turn the same pixels give on the
/// default screen's ring
float ReferenceTurnScaleBetween(float start, float end, glm::vec2 axes, glm::vec2 referenceAxes)
{
	auto swept = end - start;
	if (swept > glm::pi<float>())
	{
		swept -= glm::two_pi<float>();
	}
	if (swept < -glm::pi<float>())
	{
		swept += glm::two_pi<float>();
	}
	// Along the way, the pixels moved at each place turn the camera as they would at that place on the default
	// screen's ring, against the angle they sweep on this one. Both are added the same way, so on the default screen
	// the scale is exactly one.
	float reference = 0.0f;
	float here = 0.0f;
	for (int piece = 0; piece < k_RingPieces; ++piece)
	{
		const auto place = start + swept * (static_cast<float>(piece) + 0.5f) / static_cast<float>(k_RingPieces);
		const auto pixels = RingSpeed(axes, place);
		reference += AnglePerPixel(referenceAxes, place) * pixels;
		here += AnglePerPixel(axes, place) * pixels;
	}
	return here > 0.0f ? reference / here : 1.0f;
}

/// The default screen's ring, with or without the cinema bars as the area has them
glm::vec2 ReferenceRingAxes(const EdgeArea& area)
{
	return RingAxes(k_ReferenceScreen, ViewHeight(k_ReferenceScreen, area.viewHeight != area.screen.y));
}

/// A point of a wide screen measured for the edge controls: from the middle of the area with the folded strip taken
/// out across, whether it is over that strip, and which side of it it is on (1 right, -1 left, 0 over it)
struct FoldedPoint
{
	glm::vec2 fromMiddle;
	bool overStrip;
	float side;
};

FoldedPoint Fold(glm::ivec2 point, const EdgeArea& area)
{
	const auto halfStrip = static_cast<float>(area.Folded()) * 0.5f;
	const auto across = static_cast<float>(point.x) - static_cast<float>(area.screen.x) * 0.5f;
	const auto down = static_cast<float>(point.y) - static_cast<float>(area.screen.y) * 0.5f;
	if (std::abs(across) <= halfStrip)
	{
		return {.fromMiddle = {0.0f, down}, .overStrip = true, .side = 0.0f};
	}
	const auto side = across > 0.0f ? 1.0f : -1.0f;
	return {.fromMiddle = {across - side * halfStrip, down}, .overStrip = false, .side = side};
}

/// How far the 4:3 ring is from the middle at its top and bottom: a pixel across there sweeps one over this
float RingEndRadius(const EdgeArea& area)
{
	return k_RingRadius * 0.5f * static_cast<float>(area.viewHeight);
}

/// Where a point of a wide screen is along the ring, as an angle: round the 4:3 ring's halves by the angle about the
/// area's middle, and along the straight top and bottom by the angle the same pixels sweep at the 4:3 ring's top or
/// bottom. Right round it adds up to a whole turn and the two straight runs.
float TrackPlace(glm::ivec2 point, const EdgeArea& area)
{
	const auto strip = static_cast<float>(area.Folded());
	const auto endRadius = RingEndRadius(area);
	const auto straight = strip / endRadius;
	const auto down = static_cast<float>(point.y - (area.screen.y >> 1));
	const auto across = static_cast<float>(point.x) - static_cast<float>(area.screen.x) * 0.5f;
	if (std::abs(across) <= strip * 0.5f)
	{
		// The bottom runs left to right through no angle, the top right to left from half a turn on
		return down >= 0.0f ? across / endRadius : straight * 0.5f + glm::pi<float>() + (strip * 0.5f - across) / endRadius;
	}
	const auto side = across > 0.0f ? 1.0f : -1.0f;
	return std::atan2(across - side * strip * 0.5f, down) + side * straight * 0.5f;
}

/// Dragging round the edge of a wide screen, as EdgeRotate does on a screen 4:3 or taller
RingStep FoldedEdgeRotate(glm::ivec2 cursor, glm::ivec2 previousOnRing, const EdgeArea& area)
{
	const auto width = static_cast<float>(area.width);
	const auto height = static_cast<float>(area.screen.y);
	const auto view = static_cast<float>(area.viewHeight);
	const auto strip = static_cast<float>(area.Folded());
	const auto folded = Fold(cursor, area);
	const auto x = folded.fromMiddle.x / (width * 0.5f);
	const auto y = folded.fromMiddle.y / (view * 0.5f);
	auto onRing = cursor;
	const auto squared = x * x + y * y;
	if (squared < k_RingBandMin || k_RingBandMax < squared)
	{
		const auto scale = squared != 0.0f ? k_RingRadius / std::sqrt(squared) : 0.0f;
		// Over the strip the cursor only goes up or down onto the straight part; elsewhere onto the 4:3 ring and back
		// out to its side of the strip
		const auto across = static_cast<float>(area.screen.x) * 0.5f + scale * x * width * 0.5f + folded.side * strip * 0.5f;
		onRing = {folded.overStrip ? cursor.x : static_cast<int>(across + 0.5f),
		          static_cast<int>((scale * y * view + height + 1.0f) * 0.5f)};
	}
	const auto loop = glm::two_pi<float>() + 2.0f * strip / RingEndRadius(area);
	auto angle = TrackPlace(onRing, area) - TrackPlace(previousOnRing, area);
	if (angle > loop * 0.5f)
	{
		angle -= loop;
	}
	if (angle < -loop * 0.5f)
	{
		angle += loop;
	}
	const auto scale = ReferenceTurnScaleBetween(
	    RingPlace(Fold(previousOnRing, area).fromMiddle, area), RingPlace(Fold(onRing, area).fromMiddle, area),
	    RingAxes({area.width, area.screen.y}, area.viewHeight), ReferenceRingAxes(area));
	return {.cursor = onRing, .angle = angle, .turn = angle * scale};
}

glm::vec2 Direction(glm::vec2 vector)
{
	const auto length = std::sqrt(vector.x * vector.x + vector.y * vector.y);
	return length > k_MinDirectionLength ? vector / length : vector;
}
} // namespace

int ViewHeight(glm::ivec2 screenSize, bool cinemaBars)
{
	if (!cinemaBars)
	{
		return screenSize.y;
	}
	// The game only had 4:3 screens; on screens as wide as 16:9 or wider there are no bars, and the picture is the
	// screen's height
	return std::min(screenSize.y, screenSize.y - static_cast<int>(static_cast<float>(screenSize.y) -
	                                                              static_cast<float>(screenSize.x) * k_WideScreen));
}

EdgeArea EdgeAreaOf(glm::ivec2 screenSize, bool cinemaBars)
{
	return {.screen = screenSize,
	        .width = std::min(screenSize.x, screenSize.y * 4 / 3),
	        .viewHeight = ViewHeight(screenSize, cinemaBars)};
}

EdgeArea WholeScreenArea(glm::ivec2 screenSize, int viewHeight)
{
	return {.screen = screenSize, .width = screenSize.x, .viewHeight = viewHeight};
}

glm::vec2 NormalisedCursor(glm::ivec2 cursor, glm::ivec2 screenSize, int viewHeight)
{
	const auto width = static_cast<float>(screenSize.x);
	const auto height = static_cast<float>(viewHeight);
	return {static_cast<float>(cursor.x) / width - 0.5f, static_cast<float>(cursor.y - (screenSize.y >> 1)) / height};
}

glm::vec2 AreaCursor(glm::ivec2 cursor, const EdgeArea& area)
{
	if (area.Folded() <= 0)
	{
		return NormalisedCursor(cursor, area.screen, area.viewHeight);
	}
	// Measured from the area's left edge as the game measures a screen, so each side reads exactly as on a 4:3 screen
	const auto width = static_cast<float>(area.width);
	const auto fromLeft = Fold(cursor, area).fromMiddle.x + width * 0.5f;
	return {fromLeft / width - 0.5f, static_cast<float>(cursor.y - (area.screen.y >> 1)) / static_cast<float>(area.viewHeight)};
}

uint32_t IdleTricons(glm::vec2 normalisedCursor, bool landUnderCursor, bool windowed)
{
	const auto veryEdge = windowed ? k_VeryEdgeWindowed : k_VeryEdge;
	uint32_t tricons = tricon::k_Idle;
	if (std::abs(normalisedCursor.x) > k_SideEdge)
	{
		tricons |= tricon::k_Rotate;
	}
	if (normalisedCursor.y > k_BottomEdge)
	{
		tricons |= tricon::k_Rotate;
	}
	if (normalisedCursor.y > veryEdge)
	{
		tricons |= tricon::k_Pitch;
	}
	if ((!landUnderCursor && normalisedCursor.y < k_TopEdgeWithoutLand) || normalisedCursor.y < -veryEdge)
	{
		tricons |= tricon::k_Rotate | tricon::k_Pitch | tricon::k_Top;
	}
	if ((tricons & tricon::k_Rotate) != 0)
	{
		tricons |= tricon::k_Turning;
	}
	return tricons;
}

bool HintsFollowCursor(bool buttonHeld, bool playerMovesCamera, bool selfTilting)
{
	return !buttonHeld && (selfTilting || !playerMovesCamera);
}

void DragClassifier::Start(uint32_t tricons, glm::vec2 normalisedCursor, uint32_t milliseconds)
{
	_tricons = tricons & ~tricon::k_Idle;
	if ((_tricons & tricon::k_Rotate) != 0)
	{
		_tricons |= tricon::k_Turning;
	}
	_pressedAt = normalisedCursor;
	_pressedMs = milliseconds;
	_moved = glm::vec2(0.0f);
	_mode.reset();
	// Pressed away from the edges, the land is gripped at once
	if ((_tricons & (tricon::k_Rotate | tricon::k_Pitch)) == 0)
	{
		_mode = DragMode::Pan;
		_tricons = 0;
	}
}

void DragClassifier::Move(glm::ivec2 delta, const EdgeArea& area, uint32_t milliseconds, bool landUnderCursor,
                          uint32_t features)
{
	// Across by the height and down by the width, as the game adds them up
	_moved += glm::vec2(static_cast<float>(delta.x) / static_cast<float>(area.viewHeight),
	                    static_cast<float>(delta.y) / static_cast<float>(area.width));
	if (!_mode.has_value() && k_DecideDistance * k_DecideDistance < _moved.x * _moved.x + _moved.y * _moved.y)
	{
		Decide(milliseconds, landUnderCursor, features);
	}
}

void DragClassifier::Decide(uint32_t milliseconds, bool landUnderCursor, uint32_t features)
{
	const auto elapsed = milliseconds - _pressedMs;
	// Towards the middle of the screen from where it was pressed, and square to that
	const auto towardsMiddle = Direction(-_pressedAt);
	const auto across = Direction({_pressedAt.y, -_pressedAt.x});
	const auto alongMoved = towardsMiddle.x * _moved.x + towardsMiddle.y * _moved.y;
	const auto acrossMoved = across.x * _moved.x + across.y * _moved.y;

	const auto pitch = (_tricons & tricon::k_Top) != 0 ? DragMode::PitchFromTop : DragMode::Pitch;
	const bool canRotate = (features & feature::k_Rotate) != 0;
	auto mode = DragMode::Pan;
	if ((_tricons & (tricon::k_Rotate | tricon::k_Pitch)) == (tricon::k_Rotate | tricon::k_Pitch))
	{
		if ((features & (feature::k_Pitch | feature::k_Rotate)) == (feature::k_Pitch | feature::k_Rotate))
		{
			mode = std::abs(_moved.x) <= std::abs(_moved.y) ? pitch : DragMode::EdgeRotate;
		}
		else
		{
			mode = canRotate ? DragMode::EdgeRotate : pitch;
		}
	}
	else if ((_tricons & tricon::k_Rotate) != 0)
	{
		if ((features & (feature::k_Rotate | feature::k_Strafe)) == (feature::k_Rotate | feature::k_Strafe))
		{
			// A quick drag towards the middle pans; any other way, or a slow one, turns
			const bool turns = std::abs(alongMoved) < std::abs(acrossMoved) || alongMoved < 0.0f || elapsed > k_QuickPanMs;
			mode = turns ? DragMode::EdgeRotate : DragMode::Pan;
		}
		else
		{
			mode = canRotate ? DragMode::EdgeRotate : DragMode::Pan;
		}
	}
	else if ((_tricons & tricon::k_Pitch) != 0)
	{
		mode = pitch;
	}
	if ((mode == DragMode::Pitch || mode == DragMode::PitchFromTop) && _moved.y > 0.0f && elapsed < k_QuickTiltMs &&
	    landUnderCursor)
	{
		mode = DragMode::Pan;
	}
	_mode = mode;
	switch (mode)
	{
	case DragMode::Pan:
		_tricons = 0;
		break;
	case DragMode::EdgeRotate:
		_tricons = tricon::k_Rotate;
		break;
	case DragMode::Pitch:
	case DragMode::PitchFromTop:
		_tricons = tricon::k_Pitch;
		break;
	}
}

RingStep EdgeRotate(glm::ivec2 cursor, glm::ivec2 previousOnRing, const EdgeArea& area)
{
	if (area.Folded() > 0)
	{
		return FoldedEdgeRotate(cursor, previousOnRing, area);
	}
	const auto screenSize = area.screen;
	const auto viewHeight = area.viewHeight;
	const auto width = static_cast<float>(screenSize.x);
	const auto height = static_cast<float>(screenSize.y);
	const auto view = static_cast<float>(viewHeight);
	const auto x = (static_cast<float>(cursor.x) - width * 0.5f) / (width * 0.5f);
	const auto y = (static_cast<float>(cursor.y) - height * 0.5f) / (view * 0.5f);
	auto onRing = cursor;
	const auto squared = x * x + y * y;
	if (squared < k_RingBandMin || k_RingBandMax < squared)
	{
		const auto scale = squared != 0.0f ? k_RingRadius / std::sqrt(squared) : 0.0f;
		onRing = {static_cast<int>(((scale * x + 1.0f) * width + 1.0f) * 0.5f),
		          static_cast<int>((scale * y * view + height + 1.0f) * 0.5f)};
	}
	const auto halfWidth = screenSize.x / 2;
	const auto halfHeight = screenSize.y >> 1;
	const auto before =
	    std::atan2(static_cast<float>(previousOnRing.x - halfWidth), static_cast<float>(previousOnRing.y - halfHeight));
	const auto after = std::atan2(static_cast<float>(onRing.x - halfWidth), static_cast<float>(onRing.y - halfHeight));
	auto angle = after - before;
	if (angle > glm::pi<float>())
	{
		angle -= glm::two_pi<float>();
	}
	if (angle < -glm::pi<float>())
	{
		angle += glm::two_pi<float>();
	}
	return {
	    .cursor = onRing, .angle = angle, .turn = angle * ReferenceTurnScale(previousOnRing, onRing, screenSize, viewHeight)};
}

float ReferenceTurnScale(glm::ivec2 from, glm::ivec2 to, glm::ivec2 screenSize, int viewHeight)
{
	return ReferenceTurnScaleBetween(RingPlace(from, screenSize, viewHeight), RingPlace(to, screenSize, viewHeight),
	                                 RingAxes(screenSize, viewHeight),
	                                 ReferenceRingAxes(WholeScreenArea(screenSize, viewHeight)));
}

float PitchStep(int deltaY, int screenHeight, float horizontalFieldOfView)
{
	return static_cast<float>(deltaY) / static_cast<float>(screenHeight) * k_PitchPerScreen * horizontalFieldOfView;
}

int TwoButtonTurn::Update(bool held, int deltaX, int cursorX, int screenWidth)
{
	if (!held)
	{
		_turning = false;
		_pressedX = cursorX;
		return 0;
	}
	const auto threshold = static_cast<float>(screenWidth) * k_TwoButtonTurnShare;
	if (threshold < static_cast<float>(std::abs(deltaX)) || threshold < static_cast<float>(std::abs(cursorX - _pressedX)))
	{
		_turning = true;
	}
	return _turning ? deltaX : 0;
}

} // namespace openblack::camera_drag
