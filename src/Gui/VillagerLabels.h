/*******************************************************************************
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
#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

/// The labels over villagers' heads that the Show Villager Names and Show Villager Details keys bring up.
///
/// A details label tells what a villager is doing, its age, its life and how full it is (and whether it is pregnant
/// or poisoned), under its name when it is a named villager. A name label shows only a named villager's name, and grows
/// in and out with the names display. Each is a dark, see-through rounded box standing on a black pointer down to a
/// point over the villager's head, its words in the colour of the player who owns the villager's town. Labels that
/// would cover one another are stacked upwards; one that would go off the top of the screen is not shown.
namespace openblack::gui::villager_labels
{

/// The labels' point is this far above the villager's position
constexpr float k_HeadHeight = 1.75f;
/// The names display grows in and out by this much of its full size a millisecond
constexpr float k_NameFadePerMillisecond = 0.003f;
/// Smaller than this a label isn't drawn; a name label shows past it
constexpr float k_SmallestSize = 0.01f;

/// At full size: a line of words is this tall, the box is this much wider than its widest line and its words start
/// this far in, its box is this much taller than its lines, and its bottom stands this far above the point
constexpr float k_LineHeight = 16.0f;
constexpr float k_ExtraWidth = 6.0f;
constexpr float k_Inset = 2.0f;
constexpr float k_ExtraHeight = 4.0f;
constexpr float k_RaisedBy = 50.0f;
/// A label holds this many lines at most, and this many characters
constexpr size_t k_MaxLines = 8;
constexpr size_t k_MaxCharacters = 255;
/// The box's alpha at full size out of 255, and the words'
constexpr float k_BoxAlpha = 120.0f;
constexpr float k_WordsAlpha = 255.0f;
/// The pointer: a stalk this wide from the box down to this far above the point, then a head this wide down to it
constexpr float k_StalkHalfWidth = 3.0f;
constexpr float k_HeadHalfWidth = 6.0f;
constexpr float k_HeadHeightPixels = 16.0f;

/// The keys do nothing while a script has the cinema bars in and holds the interface, nor inside the temple
[[nodiscard]] constexpr bool KeysAct(bool scriptHoldsInterface, bool inTemple)
{
	return !scriptHoldsInterface && !inTemple;
}
/// What the Show Villager Details key turns on or off: the details of the living thing the hand is over (a villager,
/// an animal or a creature), else every villager's
enum class DetailsToggle : uint8_t
{
	Pointed,
	Everyone,
};
[[nodiscard]] constexpr DetailsToggle DetailsKeyToggles(bool overLivingThing)
{
	return overLivingThing ? DetailsToggle::Pointed : DetailsToggle::Everyone;
}

/// The names display's size after a frame of so many milliseconds: towards 1 while shown, towards 0 while not
[[nodiscard]] float StepNameFade(float fade, bool shown, float milliseconds);

/// What a villager shows
enum class Kind : uint8_t
{
	None,
	/// What it is doing and how it is, under its name if it may show one, at full size
	Details,
	/// Only its name, at the names display's size
	Name,
};
/// What decides a villager's label
struct Who
{
	/// It is a named villager
	bool named {false};
	/// It may show its name: no script holds it and no script made it
	bool nameAllowed {true};
	/// It is a named villager speaking in its speech bubble, which shows instead of any label
	bool speaking {false};
	/// Details are shown for every villager, or for this one
	bool detailsForAll {false};
	bool detailsForThis {false};
	/// The names display's size
	float nameFade {0.0f};
};
struct Choice
{
	Kind kind {Kind::None};
	/// The label's size, 1 for full size
	float size {0.0f};
	/// Its name heads its details
	bool withName {false};
};
[[nodiscard]] Choice Choose(const Who& who);

/// The labels in a details label's text (" A:", " L:", " F:", " P:" and " S:" in English)
struct DetailWords
{
	std::u16string_view age;
	std::u16string_view life;
	std::u16string_view food;
	std::u16string_view pregnant;
	std::u16string_view poisoned;
};
/// What a details label tells
struct Details
{
	/// Its name, for a named villager that may show it
	std::optional<std::u16string_view> name;
	/// What it is doing, the text of its state
	std::u16string_view doing;
	uint32_t age {0};
	/// Its life and how full it is, 1 for full
	float life {0.0f};
	float food {0.0f};
	bool pregnant {false};
	bool poisoned {false};
};
/// The details label's words: its name on a line of its own, then what it is doing, then on a line of their own its
/// age in years and its life and food in whole percents ("Going Somewhere\n A:23  L:100%  F:54%"), then the
/// pregnant and poisoned labels when they apply
[[nodiscard]] std::u16string DetailsText(const Details& details, const DetailWords& words);
/// For a state without a text, which the game's tables never have: the state's name from the tables, then the age, life
/// and food in the debug text's format ("A:%d L:%.02f F:%.02f"), then " P" and " S"
[[nodiscard]] std::u16string DebugDetailsText(const Details& details, std::u16string_view format);
/// A number as printf's "%.Nf" in the game's runtime writes it: rounded half away from zero on its exact value
[[nodiscard]] std::u16string FormatFixed(double value, int decimals);

/// The colour of a player's things by its number on a land of the story: on some lands the computer players' colours
/// are swapped for others. An index into the players' colours.
[[nodiscard]] uint8_t PlayerColourIndex(uint8_t player, int32_t landNumber);
/// A label's colour from the colour of the player owning the villager's town (none for no player: white). A woman's
/// details are lightened most of the way to white. Then a colour all but black is taken as white, and every colour is
/// put half way to white.
[[nodiscard]] glm::u8vec3 LabelColour(std::optional<glm::u8vec3> player, bool lighterForWoman);

/// The game sizes its labels in pixels at its default screen of 800 by 600. They are scaled so they take the same share
/// of the screen on any screen: by the width a 4:3 screen of the same height would have, or the width when the screen
/// is taller than 4:3. 1 at 800 by 600.
constexpr float k_ReferenceWidth = 800.0f;
[[nodiscard]] float PixelScale(glm::vec2 screen);

/// The width of a line of words at a line height, as the game measures it
using WidthFn = std::function<float(std::u16string_view text, float lineHeight)>;

/// A label wanted this frame
struct Request
{
	/// The whole pixel the point over the head falls in (on the screen), and its distance from the camera
	glm::ivec2 pixel {0};
	float distance {0.0f};
	float size {1.0f};
	std::u16string text;
	glm::u8vec3 colour {255, 255, 255};
};
/// A label placed on the screen
struct Label
{
	float left {0.0f};
	float top {0.0f};
	float right {0.0f};
	float bottom {0.0f};
	glm::vec2 pixel {0.0f};
	float distance {0.0f};
	/// The screen's scale of the label's pixel sizes
	float pixelScale {1.0f};
	float lineHeight {0.0f};
	float inset {0.0f};
	std::vector<std::u16string> lines;
	glm::u8vec3 colour {255, 255, 255};
	/// The box's and the words' alpha, 0 to 255
	uint8_t boxAlpha {0};
	uint8_t wordsAlpha {0};
};
/// The labels of a frame, placed as they come: each goes over its point, kept on the screen across, and moved up over
/// any placed label it would cover until it covers none; one pushed off the top of the screen is dropped
class Board
{
public:
	/// Places a label on a screen so wide, its pixel sizes scaled for the screen; false when it isn't shown
	bool Add(const Request& request, float screenWidth, const WidthFn& width, float pixelScale);
	/// The labels placed, from left to right
	[[nodiscard]] const std::vector<Label>& GetLabels() const noexcept { return _labels; }
	/// The labels in the order they are drawn: the furthest first
	[[nodiscard]] std::vector<const Label*> DrawOrder() const;

private:
	std::vector<Label> _labels;
};

/// A corner of a piece of a label: a pixel and a place on the bubble texture
struct Corner
{
	glm::vec2 position {0.0f};
	glm::vec2 uv {0.0f};
};
/// A piece, its corners top left, top right, bottom right, bottom left
using Quad = std::array<Corner, 4>;
/// The box: nine pieces of the bubble texture's first tile, its corners rounded by a quarter of its height
[[nodiscard]] std::vector<Quad> BoxShape(const Label& label);
/// The pointer from the box's bottom to the point: the stalk, then its head
[[nodiscard]] std::array<Quad, 2> PointerShape(const Label& label);

} // namespace openblack::gui::villager_labels
