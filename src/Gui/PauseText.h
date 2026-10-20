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
#include <bit>
#include <optional>
#include <string_view>
#include <vector>

#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

#include "3D/OrientedText.h"
#include "Gui/LogoEffect.h"

/// The words that the game shows at the top of the screen while it is paused ("Game Paused"), slower ("Slow Down Time")
/// or quicker ("Speed Up Time") than normal. Each letter floats in the world in front of the camera and turns on its own,
/// a little after the letter before it. The words fade in as they go away from the camera, and fade out as they come
/// back.
///
/// These are the rules, free of game state: the pause text system steps them every frame, and the tests use them with
/// fakes.
namespace openblack::gui::pause_text
{

/// What the words say. None shows nothing.
enum class Kind : int8_t
{
	None = -1,
	Paused = 0,
	Slower = 1,
	Faster = 2,
};

/// The names of the help texts of the kinds, in their order
inline constexpr std::array<std::string_view, 3> k_TextNames {"HELP_TEXT_GAME_PAUSED", "HELP_TEXT_GAME_SLOWMO",
                                                              "HELP_TEXT_GAME_FASTMO"};

/// The words come in by this share of a full fade each millisecond, and go out by the second share
inline constexpr float k_FadeInPerMillisecond = std::bit_cast<float>(0x3B449BA6U);  // 0.003
inline constexpr float k_FadeOutPerMillisecond = std::bit_cast<float>(0x3B03126FU); // 0.002
/// The letters turn by this angle each millisecond, in radians
inline constexpr float k_TurnPerMillisecond = std::bit_cast<float>(0x3AC49BA6U); // 0.0015
/// Each letter turns this angle after the letter before it, in radians
inline constexpr float k_LetterLag = std::bit_cast<float>(0x3EAAAAABU); // a third
/// The angles of the turns, as floats
inline constexpr float k_Pi = std::bit_cast<float>(0x40490FDBU);
inline constexpr float k_HalfPi = std::bit_cast<float>(0x3FC90FDBU);
inline constexpr float k_TwoPi = std::bit_cast<float>(0x40C90FDBU);
inline constexpr float k_FourPi = std::bit_cast<float>(0x41490FDBU);
/// The words are this far from the camera when they start to fade in, and twice as far when they show fully
inline constexpr float k_NearDepth = 15.0f;
/// The letters' spaces on the screen are their share of half the screen's width at this depth
inline constexpr float k_SpacingDepth = 30.0f;
/// The depth of the two points that give the tilt of the letters
inline constexpr float k_TiltDepth = 10.0f;
/// The letters are this high in the world, and their middles are half of it under their places
inline constexpr float k_LetterSize = 2.0f;
inline constexpr float k_LetterRaise = -1.0f;
/// The font measures a letter in units of this share of its size
inline constexpr float k_FontUnit = std::bit_cast<float>(0x3C4CCCCDU); // 0.0125
/// The words' row is this share of the screen's height from its top
inline constexpr int32_t k_RowDivisor = 20;

/// The kind for the game's pause and its speed: 1 is the normal speed
[[nodiscard]] Kind KindOf(bool paused, float speed);

/// The fade made smooth: 0 under 0, 1 over 1, and half of one less the cosine of the fade's share of a half turn between
[[nodiscard]] float Ease(float fade);

/// What the game is doing at a frame
struct Situation
{
	bool paused {false};
	float speed {1.0f};
	/// A land is loaded and its game runs
	bool started {true};
	bool insideTemple {false};
	/// A script holds the game's speed
	bool scriptHasSpeed {false};
	/// The game's level editor is on
	bool editing {false};
	bool videoPlaying {false};
	/// The active box of the interface has a background: the menu, a question, the start-of-game question and the like
	bool boxWithBackground {false};
};

/// The words to show at a frame: which ones, how far they are faded in (made smooth), and the turn of the letters
struct Shown
{
	Kind kind {Kind::None};
	float eased {0.0f};
	float turn {0.0f};
};

/// The fade of the words from frame to frame. The words that show stay until they fade out fully, and then the words
/// of the game's new state fade in.
class Fader
{
public:
	/// The next frame, some real milliseconds after the last one: the words to draw, or none
	[[nodiscard]] std::optional<Shown> Step(const Situation& situation, uint32_t milliseconds);

	[[nodiscard]] Kind GetKind() const noexcept { return _kind; }
	[[nodiscard]] float GetFade() const noexcept { return _fade; }
	[[nodiscard]] float GetTurn() const noexcept { return _turn; }

private:
	Kind _kind {Kind::None};
	float _fade {0.0f};
	/// The turn of the letters carries on from words to words, and from land to land
	float _turn {0.0f};
};

/// The place of a letter on the screen, its depth, and its turn: three rows in the camera's axes (right, up, forward)
struct LetterPlace
{
	glm::ivec2 pixel {0};
	float depth {0.0f};
	std::array<glm::vec3, 3> turn {};
};

/// The width that spaces the letters: the screen's width, but no wider than the 4:3 middle of the screen
[[nodiscard]] int32_t LayoutWidth(logo_effect::Screen screen);

/// The angle between the view's middle and the words' row, which tilts the letters to face the camera
[[nodiscard]] float Tilt(const logo_effect::View& view, const logo_effect::Lens& lens, logo_effect::Screen screen);

/// The letter of a number in words of a length, as shown, on a screen: its place in the row and its turn
[[nodiscard]] LetterPlace PlaceLetter(size_t index, size_t length, const Shown& shown, logo_effect::Screen screen,
                                      int32_t layoutWidth, float tilt);

/// The frame in the world that a letter is laid out in: its place under its pixel at its depth, and its turn in the
/// camera's axes
[[nodiscard]] TextFrame FrameOf(const LetterPlace& letter, const logo_effect::View& view, const logo_effect::Lens& lens,
                                logo_effect::Screen screen);

/// The alpha of the words, of 255
[[nodiscard]] uint8_t AlphaOf(float eased);

/// How far left of its place a letter starts: half of its width at the letters' size, for the font's units that it
/// moves the pen
[[nodiscard]] float LetterOffset(float advance);

} // namespace openblack::gui::pause_text
