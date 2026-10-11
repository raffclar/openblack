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
	return screenSize.y - static_cast<int>(static_cast<float>(screenSize.y) - static_cast<float>(screenSize.x) * k_WideScreen);
}

glm::vec2 NormalisedCursor(glm::ivec2 cursor, glm::ivec2 screenSize, int viewHeight)
{
	const auto width = static_cast<float>(screenSize.x);
	const auto height = static_cast<float>(viewHeight);
	return {static_cast<float>(cursor.x) / width - 0.5f, static_cast<float>(cursor.y - (screenSize.y >> 1)) / height};
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

void DragClassifier::Move(glm::ivec2 delta, glm::ivec2 screenSize, int viewHeight, uint32_t milliseconds, bool landUnderCursor,
                          uint32_t features)
{
	// Across by the height and down by the width, as the game adds them up
	_moved += glm::vec2(static_cast<float>(delta.x) / static_cast<float>(viewHeight),
	                    static_cast<float>(delta.y) / static_cast<float>(screenSize.x));
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

RingStep EdgeRotate(glm::ivec2 cursor, glm::ivec2 previousOnRing, glm::ivec2 screenSize, int viewHeight)
{
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
	const auto start = RingPlace(from, screenSize, viewHeight);
	auto swept = RingPlace(to, screenSize, viewHeight) - start;
	if (swept > glm::pi<float>())
	{
		swept -= glm::two_pi<float>();
	}
	if (swept < -glm::pi<float>())
	{
		swept += glm::two_pi<float>();
	}
	const auto axes = RingAxes(screenSize, viewHeight);
	const auto referenceAxes = RingAxes(k_ReferenceScreen, ViewHeight(k_ReferenceScreen, viewHeight != screenSize.y));
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
