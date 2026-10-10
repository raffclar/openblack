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

#include <array>
#include <optional>

#include <glm/vec2.hpp>
#include <glm/vec4.hpp>

/// The camera's helper icons by the hand: the pitch arrow, the rotate arrow, the zoom glass and the red cross, cut from
/// the bottom row of atmos.raw. Each fades in and out on its own, all four share one place by the hand and one size on
/// the screen, and the rotate arrow turns with the drag round the screen. While a tutorial demonstration plays, the
/// icons follow the recording, and a mouse with the word "Demo" beside them shows which buttons the recording holds.
/// Pure rules, tested on their own.
namespace openblack::hand_tricons
{

/// The icons' bits, as the camera offers them and as the demonstrations recorded them
namespace icon
{
constexpr uint32_t k_Pitch = 0x01;
constexpr uint32_t k_Rotate = 0x02;
constexpr uint32_t k_Zoom = 0x04;
constexpr uint32_t k_Cross = 0x08;
/// The camera icons, which the hand only shows in its empty and camera states
constexpr uint32_t k_CameraIcons = k_Pitch | k_Rotate | k_Zoom;
/// An icon only offered, the cursor hovering where a drag would use it rather than dragging: its bit shifted up by
/// this much, and it shows dimmer
constexpr uint32_t k_OfferedShift = 8;
} // namespace icon

constexpr size_t k_IconCount = 4;
/// How fast an icon fades in and out, in alpha a second
constexpr float k_FadeSpeed = 5.0f;
/// The strength of an icon only offered, and of one in use
constexpr float k_OfferedAlpha = 0.6f;
constexpr float k_UsedAlpha = 1.0f;
/// The icons keep this many pixels inside the picture
constexpr int k_EdgeMargin = 16;
/// An icon's half-width on the screen is the horizontal focal length in pixels divided by this
constexpr float k_FocalDivisor = 22.0f;
/// The icons are frames 12 to 15 of atmos.raw's 4 by 4 frames
constexpr int k_AtlasColumns = 4;
constexpr int k_FirstFrame = 12;

/// Each icon's strength, kept from frame to frame
using Fades = std::array<float, k_IconCount>;

/// The icons shown: the camera icons go where the tooltips are switched off or the hand's state doesn't show them (it
/// holds something); the cross stays
[[nodiscard]] uint32_t Shown(uint32_t icons, bool toolTipsOn, bool handStateShowsIcons);

/// What the world camera does with the mouse and keys this frame, which picks its icons
struct WorldCameraFrame
{
	/// The camera's mouse hints, as camera_drag::tricon, with k_TooFar for a drag of the land gone too far
	uint32_t mouseHints {0};
	/// The land is gripped with the move button, and whether that is all the camera has from the player (nothing else
	/// turning, tilting, zooming or moving it)
	bool gripping {false};
	bool grippingOnly {false};
	/// The keys that make the move keys zoom, or tilt and turn
	bool zoomKeyHeld {false};
	bool rotateKeyHeld {false};
	/// What the player asks of the camera this frame: only whether each is non-zero matters
	float turn {0.0f};
	float tilt {0.0f};
	float zoom {0.0f};
	/// The land's drag turns the camera round the edge, or tilts it
	bool edgeTurning {false};
	bool pitchDragging {false};
	/// The camera turned and tilted round the cursor with its button
	bool rotatingAroundMouse {false};
	/// The camera watches a fight: no tilting
	bool fight {false};
	/// How far the clear view (both keys held) has come in, 0 to 1
	float clearView {0.0f};
	/// What the scripts let the camera do, as camera_drag::feature, with k_HelpFeature
	uint32_t features {0};
	/// The hand is in its player's influence
	bool handInInfluence {true};
	/// The camera takes nothing from the player, or waits for every input to be let go
	bool inputOff {false};
	bool blocked {false};
	/// The cursor from the screen's middle, as camera_drag::NormalisedCursor
	glm::vec2 cursor {0.0f};
	/// The computer's clock, for the cross's blink
	uint32_t tickMs {0};
};
/// A mouse hint for a drag of the land gone too far from the camera: the cross blinks
constexpr uint32_t k_TooFar = 0x10;
/// The camera help's own feature, which shows the cross where the hand is out of its influence
constexpr uint32_t k_HelpFeature = 0x80;
/// The world camera's icons for a frame, and the rotate arrow's turn (kept from the frame before unless it changes)
struct WorldCameraIcons
{
	uint32_t icons {0};
	float rotateAngle {0.0f};
};
[[nodiscard]] WorldCameraIcons WorldCamera(const WorldCameraFrame& frame, float rotateAngle);
/// Once the clear view has come in further than this, the world camera's icons sit at the hand without leaning towards
/// its last grip
constexpr float k_ClearViewLeans = 0.01f;

/// How the cross is nudged this frame in the world camera: brighter with a thing under the hand, full at once with the
/// action button held
enum class CrossNudge : uint8_t
{
	None,
	OverThing,
	Action,
};

/// What the icons fade with this frame
struct FadeFrame
{
	/// The icons wanted, with their offered bits
	uint32_t icons {0};
	float seconds {0.0f};
	/// A demonstration plays: the cross is never shown
	bool demonstration {false};
	/// The world camera is moving the view itself: the icons in use show a little dimmer
	bool cameraBusy {false};
	CrossNudge crossNudge {CrossNudge::None};
};
/// The icons fade towards their strength: in at the fade speed, out at the fade speed (the cross falling slowly to a
/// faint mark)
void Fade(Fades& fades, const FadeFrame& frame);

/// Where the icons go on the screen: three quarters of the way from the hand's last gripping point to the hand (at the
/// hand when they don't lean), kept inside the picture between the cinema bars when they are on
[[nodiscard]] glm::ivec2 Place(glm::ivec2 hand, glm::ivec2 lastGrip, glm::ivec2 screen, bool cinemaBars, bool leans);

/// One icon to draw: its corners on the screen (top left, top right, bottom right, bottom left), the matching places on
/// atmos.raw, and its white's alpha
struct Sprite
{
	std::array<glm::vec2, 4> corners {};
	std::array<glm::vec2, 4> uvs {};
	float alpha {0.0f};
};
/// The icons to draw this frame at a place, in order pitch, rotate, zoom, cross; the rotate arrow turned clockwise on
/// the screen by `rotateAngle` radians. An icon too faint for a step of alpha is left out.
[[nodiscard]] std::array<std::optional<Sprite>, k_IconCount> Sprites(glm::vec2 centre, float halfSize, const Fades& fades,
                                                                     float rotateAngle);

/// The demonstration's mouse goes on the right of the icons, or on the left in the screen's right third, until it is
/// back in the left third
[[nodiscard]] bool LabelOnLeft(int x, int screenWidth, bool wasOnLeft);

/// The demonstration's mouse and word: the mouse at 32 pixels, its buttons the recording holds lit (the move button
/// is the left one, the action button the right one, both for zooming), on a white glow; the word "Demo" in red over
/// two black shadows, on a faint red glow
struct DemoMouse
{
	glm::vec2 mouseMin {0.0f};
	glm::vec2 mouseMax {0.0f};
	/// Its place on mousehelp.raw, left and right swapped to light the left button
	glm::vec2 uvMin {0.0f};
	glm::vec2 uvMax {0.0f};
	glm::vec4 mouseGlow {0.0f};
	glm::vec2 labelGlowMin {0.0f};
	glm::vec2 labelGlowMax {0.0f};
	glm::vec4 labelGlow {0.0f};
	glm::vec2 labelAt {0.0f};
	float labelSize {0.0f};
	glm::vec4 labelColour {0.0f};
	glm::vec4 shadowColour {0.0f};
};
/// The help text of the word: "Demo"
constexpr uint32_t k_DemoText = 5127;
/// The word's size, two thirds of the mouse (four fifths for the languages that need bigger text)
[[nodiscard]] int DemoLabelSize(bool biggerText);
/// The demonstration's mouse beside the icons at `centre`, `labelWidth` the word's width at its size
[[nodiscard]] DemoMouse LayoutDemoMouse(glm::ivec2 centre, bool labelOnLeft, float labelWidth, bool biggerText, bool moveHeld,
                                        bool actionHeld);

} // namespace openblack::hand_tricons
