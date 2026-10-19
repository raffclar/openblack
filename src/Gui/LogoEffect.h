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
#include <span>
#include <string>
#include <vector>

#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

/// The logo effect that the first game of each run of the program shows before the game starts. 4096 dots float at
/// random depths in front of a camera. The camera turns round them inside a sky sphere, then comes back to the place
/// where the dots make the Lionhead logo. Then the camera flies through the logo, and the screen goes black. Each
/// frame blends over the last frame, so that the dots leave trails.
///
/// These are the effect's rules, free of game state: the renderer and the start-up screens use them, and the tests use
/// them with fakes.
namespace openblack::logo_effect
{

/// The logo is a picture of 64 by 64 pixels, three bytes (red, green and blue) a pixel, row after row
inline constexpr int32_t k_LogoSize = 64;
inline constexpr size_t k_LogoPixels = static_cast<size_t>(k_LogoSize) * k_LogoSize;
inline constexpr size_t k_LogoBytes = k_LogoPixels * 3;
/// One dot for each pixel of the logo
inline constexpr size_t k_DotCount = k_LogoPixels;
/// A player's own logo has a text of this many characters at most
inline constexpr size_t k_UserTextLength = 128;
/// The text that the Lionhead logo shows: two spaces, so no text shows
inline constexpr std::u16string_view k_LionheadText = u"  ";
/// The trademark sign beside the Lionhead logo
inline constexpr std::u16string_view k_TradeMark = u"TM";
/// The dots show cell 5 of the first row of the 8 by 8 cells of the text texture
inline constexpr glm::vec2 k_DotUvMin {0.625f, 0.0f};
inline constexpr glm::vec2 k_DotUvMax {0.75f, 0.125f};
/// The start-up screens show at most one frame for each refresh of a 60 Hz screen
inline constexpr float k_FrameMilliseconds = 1000.0f / 60.0f;
/// A dot or a point of the world nearer to the camera than this is not drawn
inline constexpr float k_NearClip = 0.1f;

/// The size of the screen that the effect fills, in pixels
struct Screen
{
	int32_t width;
	int32_t height;
};

/// How the camera sees: its near plane's half width and half height (with the near distance), and the scale from a
/// point in front of the camera to the clip space
struct Lens
{
	float extentX;
	float extentY;
	float nearClip;
	/// One over the tangent of half the view across the width
	float clipScaleX;
	/// The same, times the screen's width over its height
	float clipScaleY;
};

/// The lens for a view across the width of the screen (in radians), a near distance and a screen
[[nodiscard]] Lens LensOf(float viewAcross, float nearClip, Screen screen);

/// Where a camera is and where it looks: its position and its axes in the world
struct View
{
	glm::vec3 position;
	glm::vec3 right;
	glm::vec3 up;
	glm::vec3 forward;
};

/// A camera at a position that looks at a focus, the world's up kept up
[[nodiscard]] View LookAt(glm::vec3 position, glm::vec3 focus);

/// The point of the world at a depth in front of the camera, under a pixel of the screen
[[nodiscard]] glm::vec3 ScreenToWorld(const View& view, const Lens& lens, Screen screen, glm::ivec2 pixel, float depth);

/// Where a point of the world shows on the screen
struct Projected
{
	glm::ivec2 pixel;
	float depth;
	/// The pixel is in the screen
	bool onScreen;
};

/// The pixel of a point of the world, none for a point nearer to the camera than the near plane
[[nodiscard]] std::optional<Projected> WorldToScreen(const View& view, const Lens& lens, Screen screen, glm::vec3 point);

/// The picture that the dots make and the text under it
struct Picture
{
	std::array<uint8_t, k_LogoBytes> rgb {};
	std::u16string text;
	/// It is the Lionhead logo, with the trademark sign
	bool lionhead {false};
};

/// The effect tries the player's own logo on one start in four: when the seconds since the computer started divide by 4
[[nodiscard]] constexpr bool TriesUserLogo(uint64_t millisecondsSinceStart) noexcept
{
	return ((millisecondsSinceStart / 1000) & 3U) == 0;
}

/// A player's own logo from its file in the temporary folder: 4096 grey bytes, then a text of 2-byte characters. None
/// when the file has no text, as the Lionhead logo then shows
[[nodiscard]] std::optional<Picture> ReadUserLogo(std::span<const uint8_t> file);
/// The Lionhead logo from its picture file
[[nodiscard]] std::optional<Picture> LionheadLogo(std::span<const uint8_t> file);

/// A colour as the game keeps it: alpha, red, green and blue, from the high byte to the low byte
using Argb = uint32_t;

/// One dot of the logo
struct Dot
{
	glm::vec3 position;
	/// Half the width of its square, in the world
	float halfSize;
	/// How far its picture turns on the screen, in radians
	float angle;
	Argb colour;
	/// Added to what is under it, else blended over it by its alpha
	bool additive;
};

/// The corners of a dot's square in the world, in turn from the top left of its picture. The square faces the camera
/// and turns by the dot's angle. None for a dot at the near plane or behind it, as it is not drawn.
[[nodiscard]] std::optional<std::array<glm::vec3, 4>> DotCorners(const Dot& dot, const View& view, float nearClip);

/// A random number between two numbers, as the game draws it
using Random = std::function<float(float, float)>;

/// What the player did in the time of a frame
struct Input
{
	/// The time since the last frame, at least 1
	int32_t milliseconds;
	/// The pointer, in pixels of the screen
	glm::ivec2 pointer;
	/// How far the mouse moved
	glm::ivec2 mouseMove;
	/// How far the wheel turned, 120 a notch, positive away from the player
	int32_t wheel;
	/// The game camera's view across the width, in radians
	float viewAcross;
};

/// What a frame of the effect shows
struct Frame
{
	View view;
	Lens lens;
	/// The colour of the sky sphere
	Argb skyColour;
	/// How black the screen is over everything, 0 to 255
	uint8_t fade;
	/// The alpha of the text, and of the sponsor's logo
	uint8_t textAlpha;
	/// The alpha of the dots and the trademark sign
	uint8_t logoAlpha;
};

/// The time of the effect: 0.0011 a millisecond at full speed. It ends at 36, about 33 seconds after it starts.
/// The rules below are of this time.
[[nodiscard]] uint8_t FadeAt(float time);
/// The sky sphere is white and 160/255 opaque, then grey and more opaque from 22
[[nodiscard]] Argb SkyColourAt(float time);
/// The text shows from 22 and goes from 33
[[nodiscard]] float TextShareAt(float time);
/// How far along the camera is on the return to the logo, from 15 to 26: 0 to 1, eased
[[nodiscard]] float ReturnAt(float time);
/// From 1 until 30, then less: the camera goes forwards through the logo
[[nodiscard]] float FlightAt(float time);
/// The effect is done when the flight is at -2 or less
[[nodiscard]] bool EndsAt(float time);

/// The effect's state from its first frame to its last
class Effect
{
public:
	/// The dots on the screen of the picture, at random depths. lens is the game camera's lens: the dots are placed with it.
	Effect(const Picture& picture, Screen screen, const Lens& lens, const Random& random);

	/// The next frame. The random numbers turn each dot's picture.
	Frame Step(const Input& input, const Random& random);

	[[nodiscard]] bool Ended() const noexcept { return _ended; }
	[[nodiscard]] float GetTime() const noexcept { return _time; }
	[[nodiscard]] float GetSpeed() const noexcept { return _speed; }
	[[nodiscard]] std::span<const Dot> GetDots() const noexcept { return _dots; }
	[[nodiscard]] Screen GetScreen() const noexcept { return _screen; }

private:
	/// The dots near the pointer move with the mouse
	void Push(const View& view, const Lens& lens, const Input& input, float seconds);

	Screen _screen;
	std::vector<Dot> _dots;
	/// Where each dot goes back to
	std::vector<glm::vec3> _homes;
	std::vector<glm::vec3> _velocities;
	/// Each dot's half size before it grows at the end
	std::vector<float> _sizes;
	float _time {0.0f};
	/// The wheel slows the effect down to a tenth of its speed
	float _speed {1.0f};
	/// The mouse's movement, smoothed over the frames
	glm::ivec2 _smoothedMove {0, 0};
	bool _ended {false};
};

/// The text under the logo: its size and its top left corner for a width of text
struct TextPlace
{
	glm::vec2 position;
	float size;
};
/// The size of the text under the logo
[[nodiscard]] float TitleSize(Screen screen);
/// The text under the logo, centred, five sevenths down the screen
[[nodiscard]] TextPlace TitlePlace(Screen screen, float width);
/// The trademark sign at the top right of the Lionhead logo
[[nodiscard]] TextPlace TradeMarkPlace(Screen screen);

/// A box of the screen, from its top left pixel to past its bottom right pixel
struct Box
{
	glm::ivec2 min;
	glm::ivec2 max;
};
/// The sponsor's logo at the bottom left of the screen and its label above it
struct SponsorBoxes
{
	Box logo;
	Box label;
};
[[nodiscard]] SponsorBoxes SponsorPlaces(Screen screen);

/// The game shows the sponsor's logo when the C library of its compiler finds a processor of the Pentium 4 family. That
/// library reads only the family number and the features of the processor, so all processors of family 15 with SSE2
/// pass, which includes every recent AMD processor.
/// @param version the version word of the processor (its family in bits 8 to 11)
/// @param features the feature word of the processor (SSE2 in bit 26)
[[nodiscard]] constexpr bool ShowsSponsorLogo(uint32_t version, uint32_t features) noexcept
{
	constexpr uint32_t k_PentiumFourFamily = 15;
	constexpr uint32_t k_Sse2 = 1U << 26U;
	return ((version >> 8U) & 0xFU) == k_PentiumFourFamily && (features & k_Sse2) != 0;
}

/// The loop of the effect continues until a mouse button, the end of the effect or a key
[[nodiscard]] constexpr bool GoesOn(bool mouseButtonHeld, bool ended, bool keyPressed) noexcept
{
	return !mouseButtonHeld && !ended && !keyPressed;
}

} // namespace openblack::logo_effect
