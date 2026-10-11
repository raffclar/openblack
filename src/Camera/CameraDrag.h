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

/// How the world's camera reads the mouse the way the game does: the camera hints ("tricons") the cursor's place on the
/// screen offers, what a drag of the land turns into, turning the camera by dragging round the edge of the screen,
/// tilting it by dragging up and down, and turning it with both buttons. Pure maths, tested on its own.
namespace openblack::camera_drag
{

/// The camera hints offered where the cursor is, which also pick the hand's pose
namespace tricon
{
constexpr uint32_t k_Rotate = 0x01;
constexpr uint32_t k_Pitch = 0x02;
/// The top of the screen, where a drag up or down tilts rather than turns
constexpr uint32_t k_Top = 0x04;
constexpr uint32_t k_Zoom = 0x08;
/// The rotate hint shows its turning
constexpr uint32_t k_Turning = 0x40;
/// Nothing is being dragged
constexpr uint32_t k_Idle = 0x80;
} // namespace tricon

/// What the camera help lets the player do, as the game has it unless a script changes it: rotate, pitch, zoom and the
/// rest
constexpr uint32_t k_DefaultFeatures = 0x1BF;
namespace feature
{
constexpr uint32_t k_Pitch = 0x01;
constexpr uint32_t k_Rotate = 0x02;
constexpr uint32_t k_Zoom = 0x04;
constexpr uint32_t k_Strafe = 0x08;
} // namespace feature

/// The height of the picture the camera's mouse controls measure by: the screen's, or with the cinema bars in, that of
/// a 16:9 picture across the screen's width (the whole screen's on screens as wide as 16:9 or wider, which have no bars)
[[nodiscard]] int ViewHeight(glm::ivec2 screenSize, bool cinemaBars);

/// The original's default screen. Mouse controls measured in pixels feel at any resolution as they did on it.
constexpr glm::ivec2 k_ReferenceScreen {800, 600};

/// The part of the screen the edge controls (the hints by the edges and corners, the ring the cursor is held on to
/// turn the camera, the turn and the cursor's put) are laid out on. The game laid them over a 4:3 screen. On a screen
/// that shape or taller it is the whole screen. On a wider screen it is a 4:3 screen of the same height whose left and
/// right halves sit against the real screen's sides: the strip down the middle that a wide screen has over a 4:3 one
/// is folded away, so the sides and corners are where the player pushes the mouse to, each the size it is on a 4:3
/// screen of that height, and the ring is the 4:3 ring with its top and bottom drawn out straight across the strip.
struct EdgeArea
{
	glm::ivec2 screen {k_ReferenceScreen};
	/// The 4:3 width, or the screen's own when it is no wider than that
	int width {k_ReferenceScreen.x};
	/// The picture's height, see ViewHeight
	int viewHeight {k_ReferenceScreen.y};

	/// The strip folded away, in pixels: none on a screen 4:3 or taller
	[[nodiscard]] int Folded() const { return screen.x - width; }
	bool operator==(const EdgeArea&) const = default;
};
[[nodiscard]] EdgeArea EdgeAreaOf(glm::ivec2 screenSize, bool cinemaBars);
/// The area that is the whole screen, whatever its shape, measured by a picture of that height
[[nodiscard]] EdgeArea WholeScreenArea(glm::ivec2 screenSize, int viewHeight);

/// The cursor on the screen, across from -0.5 at the left to 0.5 at the right, and down from the middle of the screen
/// by the view's height, -0.5 at the top and 0.5 at the bottom without the cinema bars
[[nodiscard]] glm::vec2 NormalisedCursor(glm::ivec2 cursor, glm::ivec2 screenSize, int viewHeight);
/// The cursor in the edge controls' area, measured as NormalisedCursor measures a screen of the area's size. Over the
/// folded strip it is in the middle across.
[[nodiscard]] glm::vec2 AreaCursor(glm::ivec2 cursor, const EdgeArea& area);

/// The hints offered with nothing dragged: the sides and the bottom offer turning, the very bottom tilting too, and the
/// top (or a cursor over no land in the upper part of the screen) all of them. In a window the very top and bottom
/// reach further in (to 0.45 of the view's height from the middle rather than 0.49), as their edges are harder to reach.
[[nodiscard]] uint32_t IdleTricons(glm::vec2 normalisedCursor, bool landUnderCursor, bool windowed);

/// Whether the hints follow the cursor this frame: only with no mouse button holding the camera, and then while the
/// player isn't turning, tilting, zooming or moving it, or whenever the camera is tilting itself
[[nodiscard]] bool HintsFollowCursor(bool buttonHeld, bool playerMovesCamera, bool selfTilting);

/// What a drag of the land is
enum class DragMode : uint8_t
{
	/// The land follows the hand
	Pan,
	/// Dragging round the edge of the screen turns the camera
	EdgeRotate,
	/// Dragging up and down tilts the camera, from the bottom of the screen or from its top
	Pitch,
	PitchFromTop,
};

/// Decides what a drag of the land is, from the hints where it was pressed and the way the mouse first moves
class DragClassifier
{
public:
	/// The drag starts, with the hints of the cursor just before it was pressed, where the cursor was and when
	void Start(uint32_t tricons, glm::vec2 normalisedCursor, uint32_t milliseconds);
	/// The mouse moved by so many pixels; the drag is decided once it has moved far enough, measured by the edge
	/// controls' area
	void Move(glm::ivec2 delta, const EdgeArea& area, uint32_t milliseconds, bool landUnderCursor,
	          uint32_t features = k_DefaultFeatures);

	[[nodiscard]] std::optional<DragMode> GetMode() const { return _mode; }
	/// The hints during the drag, which pick the hand's pose
	[[nodiscard]] uint32_t GetTricons() const { return _tricons; }

private:
	void Decide(uint32_t milliseconds, bool landUnderCursor, uint32_t features);

	uint32_t _tricons {0};
	glm::vec2 _pressedAt {0.0f};
	uint32_t _pressedMs {0};
	/// The mouse's movement since the press, across by the screen's height and down by its width, as the game adds it
	glm::vec2 _moved {0.0f};
	std::optional<DragMode> _mode;
};

/// Dragging round the edge: the cursor is held on a ring around the middle of the screen, and the camera turns by the
/// angle the cursor swept round the middle
struct RingStep
{
	/// Where the cursor is put, on the ring
	glm::ivec2 cursor;
	/// The angle the cursor swept round the middle of the edge controls' area, in radians. Along the ring's straight
	/// top and bottom across a wide screen's folded strip, the angle the same pixels sweep at the top or bottom of the
	/// 4:3 ring.
	float angle;
	/// The camera's turn, in radians: the swept angle scaled so a movement of the mouse along the ring turns the camera
	/// as far as the same movement in pixels at the same place on the ring of the original's default screen. On that
	/// screen it is the swept angle itself; on bigger ones, whose ring is longer in pixels, it is more.
	float turn;
};
[[nodiscard]] RingStep EdgeRotate(glm::ivec2 cursor, glm::ivec2 previousOnRing, const EdgeArea& area);

/// The scale from the angle swept round the ring of this screen, from one place to another, to the turn the same
/// movement in pixels would give on the ring of the original's default screen
[[nodiscard]] float ReferenceTurnScale(glm::ivec2 from, glm::ivec2 to, glm::ivec2 screenSize, int viewHeight);

/// How far a drag down the screen tilts the camera up, in radians, through the camera's field of view across
[[nodiscard]] float PitchStep(int deltaY, int screenHeight, float horizontalFieldOfView);

/// Both buttons zoom the camera as the mouse moves up and down, and turn it once it has moved far enough across
class TwoButtonTurn
{
public:
	/// Once a frame: whether both buttons are held, the mouse's movement across and the cursor's place across. Gives the
	/// movement across that turns the camera this frame, in pixels.
	[[nodiscard]] int Update(bool held, int deltaX, int cursorX, int screenWidth);

private:
	bool _turning {false};
	int _pressedX {0};
};

} // namespace openblack::camera_drag
