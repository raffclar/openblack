/*******************************************************************************
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
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include <glm/vec2.hpp>

#include "Audio/MusicMood.h"
#include "DialogPainter.h"

namespace openblack::audio
{
class CustomMusicPlayerInterface;
}

/// The rules of the controls of the custom music player at the top right of the screen. Pure, tested with a made-up
/// player.
namespace openblack::gui::custom_music_controls
{

/// The controls are placed from the right edge of the 800 by 600 layout. The layout's right edge goes on the screen's
/// right edge (see Origin).
constexpr int k_LayoutRight = 800;

enum class Button : uint8_t
{
	/// The music button shows and hides the other controls. Its colour shows the mood of the track that plays.
	Music,
	Play,
	Pause,
	Stop,
	Previous,
	Next,
	Rewind,
	FastForward,
	Shuffle,
	Repeat,
	/// Shows and hides the play list
	PlayList,
};
constexpr size_t k_ButtonCount = 11;

/// Where a control is: its left edge from the layout's right edge, its top, its width and its height
struct Place
{
	int fromRight {0};
	int top {0};
	int width {0};
	int height {0};
};

[[nodiscard]] constexpr DialogRect ToRect(Place place) noexcept
{
	const glm::ivec2 min {k_LayoutRight - place.fromRight, place.top};
	return {.min = min, .max = min + glm::ivec2 {place.width, place.height}};
}

/// A button: where it is, its icon (one of the 16 pixel squares of the atmosphere texture), whether it has a button
/// box behind the icon, and its name for tools
struct ButtonLook
{
	Button button {Button::Play};
	Place place;
	int icon {0};
	bool framed {true};
	std::u16string_view name;
};

/// The buttons, in the order the game makes them. The music button is 40 from the right edge, beside the place of the
/// multiplayer chat button.
constexpr std::array<ButtonLook, k_ButtonCount> k_Buttons {{
    {.button = Button::Music, .place = {40, 0, 20, 20}, .icon = 10, .framed = false, .name = u"Music"},
    {.button = Button::Play, .place = {160, 20, 40, 20}, .icon = 0, .framed = true, .name = u"Play"},
    {.button = Button::Pause, .place = {120, 20, 20, 20}, .icon = 1, .framed = true, .name = u"Pause"},
    {.button = Button::Stop, .place = {100, 20, 20, 20}, .icon = 2, .framed = true, .name = u"Stop"},
    {.button = Button::Previous, .place = {80, 20, 20, 20}, .icon = 4, .framed = true, .name = u"Previous"},
    {.button = Button::Next, .place = {60, 20, 20, 20}, .icon = 7, .framed = true, .name = u"Next"},
    {.button = Button::Rewind, .place = {40, 20, 20, 20}, .icon = 5, .framed = true, .name = u"Rewind"},
    {.button = Button::FastForward, .place = {20, 20, 20, 20}, .icon = 6, .framed = true, .name = u"Fast Forward"},
    {.button = Button::Shuffle, .place = {40, 40, 20, 20}, .icon = 8, .framed = true, .name = u"Shuffle"},
    {.button = Button::Repeat, .place = {20, 40, 20, 20}, .icon = 9, .framed = true, .name = u"Repeat"},
    {.button = Button::PlayList, .place = {20, 60, 20, 20}, .icon = 3, .framed = true, .name = u"Play List"},
}};

/// The volume slider, with its label on it
constexpr Place k_VolumeSlider {.fromRight = 160, .top = 40, .width = 120, .height = 20};
/// The position slider, with the time played and the length of the track above its knob
constexpr Place k_PositionSlider {.fromRight = 160, .top = 60, .width = 140, .height = 20};
/// The name of the song that plays, to the right of its box
constexpr Place k_SongName {.fromRight = 400, .top = 0, .width = 358, .height = 20};
/// The tracks of the player, by their file names
constexpr Place k_PlayList {.fromRight = 160, .top = 80, .width = 160, .height = 320};

/// The size of the controls' text: the small text size of the game's dialogs
constexpr int k_TextSize = 20;
/// The volume slider's label. The game writes it in English for all languages.
constexpr std::u16string_view k_VolumeLabel = u"Volume";
/// How opaque the controls are: 90% of 255, made a whole number by truncation
constexpr float k_Alpha = 229.0f / 255.0f;
/// The controls look at the player again at most once in this time, and after each click
constexpr uint32_t k_RefreshMilliseconds = 1000;
/// The colour of the music button with no current track record, as 0xAARRGGBB: white
constexpr uint32_t k_DefaultColour = 0xffffffff;

/// Where a button's icon goes and the part of the atmosphere texture it shows
struct Icon
{
	/// The top left of the 16 pixel square, 9 up and left from the button's middle, or 8 when the button is pushed in
	glm::ivec2 corner {0, 0};
	glm::vec2 uvMin {0.0f, 0.0f};
	glm::vec2 uvMax {0.0f, 0.0f};
};
constexpr int k_IconSize = 16;
/// The icon of a button. The icons are four to a row from a quarter of the way across the texture, from its second row.
[[nodiscard]] Icon IconOf(int icon, const DialogRect& button, bool pushed) noexcept;

/// Blends one colour (0xAARRGGBB) towards another by amount / 256. The alpha is the second colour's.
[[nodiscard]] uint32_t Blend(uint32_t from, uint32_t to, uint32_t amount) noexcept;
/// The colour of the music button: white blended towards the tint, or white with no tint
[[nodiscard]] uint32_t MusicButtonColour(std::optional<music_mood::Tint> tint) noexcept;

/// A time as minutes and seconds, "m:ss". A time below 0 shows as 0:00.
[[nodiscard]] std::u16string TimeText(int64_t milliseconds);
/// The position slider's label: the time played and the length of the track, "m:ss / m:ss"
[[nodiscard]] std::u16string PositionLabel(uint32_t position, uint32_t length);

/// What the controls must not show over
struct Screen
{
	bool inTemple {false};
	bool videoPlays {false};
	/// The cinema bars of a cut scene are on
	bool wideScreen {false};
	/// The player's hand holds the player's creature
	bool creatureHeld {false};
	/// Another dialog, such as the game's menu, is open
	bool dialogOpen {false};
};
/// Whether the controls show
[[nodiscard]] bool Shows(const Screen& screen) noexcept;

/// Whether the player can do what a button asks. The player has no fast forward or rewind, so these buttons do nothing.
[[nodiscard]] bool CanDo(Button button) noexcept;

/// Where the layout's corner goes on the window, so that its right edge is on the window's right edge and its top on
/// the window's top. The layout keeps the scale of the front end's layout (FrontEndSpace).
[[nodiscard]] glm::vec2 Origin(glm::u16vec2 resolution) noexcept;

/// What the controls keep between clicks
struct Panel
{
	/// The controls other than the music button show
	bool controlsShown {false};
	/// The play list shows when the controls show
	bool playListShown {false};
	bool shuffle {false};
	bool repeat {false};
	/// The volume that the controls gave to the player, 0 to 255
	uint32_t volume {255};
	/// The length of the track when the controls last looked, in milliseconds
	uint32_t length {0};
	/// The controls gave the player their volume, shuffle and repeat
	bool started {false};
};

/// What the controls show, from a look at the player
struct View
{
	bool running {false};
	std::u16string positionLabel;
	/// The knobs of the sliders, 0 to 1
	float position {0.0f};
	float volume {1.0f};
	std::u16string songName;
	/// The file names of the tracks
	std::vector<std::u16string> playList;
	std::optional<size_t> currentTrack;
	uint32_t musicColour {k_DefaultColour};
};

/// The first time: the controls give the player a volume of 255, no shuffle and no repeat, when the player works
void Start(Panel& panel, audio::CustomMusicPlayerInterface& player);
/// A click on a button. It does nothing when the player does not work.
void Click(Panel& panel, audio::CustomMusicPlayerInterface& player, Button button);
/// A click on the play list: a track that is not current becomes current and plays
void PickTrack(audio::CustomMusicPlayerInterface& player, std::optional<size_t> selected);
/// The volume slider moved to a value from 0 to 1
void DragVolume(Panel& panel, audio::CustomMusicPlayerInterface& player, float value);
/// The position slider moved to a value from 0 to 1 of the track's length
void DragPosition(const Panel& panel, audio::CustomMusicPlayerInterface& player, float value);
/// A look at the player. When the player does not work, the controls other than the music button hide.
[[nodiscard]] View Refresh(Panel& panel, const audio::CustomMusicPlayerInterface& player,
                           std::optional<music_mood::Emotion> emotion);

} // namespace openblack::gui::custom_music_controls
