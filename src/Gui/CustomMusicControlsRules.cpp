/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "CustomMusicControlsRules.h"

#include <algorithm>
#include <utility>

#include <fmt/format.h>

#include "Audio/CustomMusicPlayerInterface.h"
#include "ScreenFit.h"
#include "TextDatabase.h"

using namespace openblack::gui;
using namespace openblack::gui::custom_music_controls;

namespace
{
/// A cell of the atmosphere texture: 16 pixels of its 256
constexpr float k_Cell = 1.0f / 16.0f;
/// The icons start a quarter of the way across the texture
constexpr float k_IconsLeft = 0.25f;
} // namespace

Icon custom_music_controls::IconOf(int icon, const DialogRect& button, bool pushed) noexcept
{
	const auto offset = pushed ? 8 : 9;
	const auto u = (static_cast<float>(icon & 3) * k_Cell) + k_IconsLeft;
	const auto v = static_cast<float>(((icon / 4) & 3) + 1) * k_Cell;
	return {
	    .corner = button.Centre() - offset,
	    .uvMin = {u, v},
	    .uvMax = {u + k_Cell, v + k_Cell},
	};
}

uint32_t custom_music_controls::Blend(uint32_t from, uint32_t to, uint32_t amount) noexcept
{
	// Each channel moves by its difference times amount / 256. The sums wrap round as whole 32 bit numbers, and the
	// mask keeps the channel's own bits, so a channel that goes down also comes out correct.
	const auto channel = [from, to, amount](uint32_t mask) {
		return ((from & mask) + ((((to & mask) - (from & mask)) * amount) >> 8u)) & mask;
	};
	return (to & 0xff000000u) | channel(0x00ff0000u) | channel(0x0000ff00u) | channel(0x000000ffu);
}

uint32_t custom_music_controls::MusicButtonColour(std::optional<music_mood::Tint> tint) noexcept
{
	return tint.has_value() ? Blend(k_DefaultColour, tint->colour, tint->amount) : k_DefaultColour;
}

std::u16string custom_music_controls::TimeText(int64_t milliseconds)
{
	const auto seconds = std::max<int64_t>(milliseconds / 1000, 0);
	return ToUtf16(fmt::format("{}:{:02}", seconds / 60, seconds % 60));
}

std::u16string custom_music_controls::PositionLabel(uint32_t position, uint32_t length)
{
	return TimeText(position) + u" / " + TimeText(length);
}

bool custom_music_controls::Shows(const Screen& screen) noexcept
{
	return !screen.inTemple && !screen.videoPlays && !screen.wideScreen && !screen.creatureHeld && !screen.dialogOpen;
}

bool custom_music_controls::CanDo(Button button) noexcept
{
	return button != Button::Rewind && button != Button::FastForward;
}

glm::vec2 custom_music_controls::Origin(glm::u16vec2 resolution) noexcept
{
	const auto space = FrontEndSpace(resolution.x, resolution.y);
	const auto rightEdge = space.ToScreen({static_cast<float>(k_LayoutRight), 0.0f}).x;
	return {static_cast<float>(resolution.x) - rightEdge, -space.offset.y};
}

void custom_music_controls::Start(Panel& panel, audio::CustomMusicPlayerInterface& player)
{
	if (std::exchange(panel.started, true) || !player.IsRunning())
	{
		return;
	}
	panel.volume = 255;
	player.SetVolume(panel.volume);
	player.SetShuffle(panel.shuffle);
	player.SetRepeat(panel.repeat);
}

void custom_music_controls::Click(Panel& panel, audio::CustomMusicPlayerInterface& player, Button button)
{
	if (!player.IsRunning() || !CanDo(button))
	{
		return;
	}
	switch (button)
	{
	case Button::Music:
		panel.controlsShown = !panel.controlsShown;
		break;
	case Button::Play:
		player.Play();
		break;
	case Button::Pause:
		player.Pause();
		break;
	case Button::Stop:
		player.Stop();
		break;
	case Button::Previous:
		player.PreviousTrack();
		break;
	case Button::Next:
		player.NextTrack();
		break;
	case Button::Shuffle:
		panel.shuffle = !panel.shuffle;
		player.SetShuffle(panel.shuffle);
		break;
	case Button::Repeat:
		panel.repeat = !panel.repeat;
		player.SetRepeat(panel.repeat);
		break;
	case Button::PlayList:
		panel.playListShown = !panel.playListShown;
		break;
	case Button::Rewind:
	case Button::FastForward:
		break;
	}
}

void custom_music_controls::PickTrack(audio::CustomMusicPlayerInterface& player, std::optional<size_t> selected)
{
	if (!player.IsRunning() || !selected.has_value() || selected == player.GetCurrentTrack())
	{
		return;
	}
	player.SetCurrentTrack(*selected);
	player.Play();
}

void custom_music_controls::DragVolume(Panel& panel, audio::CustomMusicPlayerInterface& player, float value)
{
	if (!player.IsRunning())
	{
		return;
	}
	panel.volume = static_cast<uint32_t>(std::clamp(value, 0.0f, 1.0f) * 255.0f);
	player.SetVolume(panel.volume);
}

void custom_music_controls::DragPosition(const Panel& panel, audio::CustomMusicPlayerInterface& player, float value)
{
	if (!player.IsRunning())
	{
		return;
	}
	player.SetPosition(static_cast<uint32_t>(std::clamp(value, 0.0f, 1.0f) * static_cast<float>(panel.length)));
}

View custom_music_controls::Refresh(Panel& panel, const audio::CustomMusicPlayerInterface& player,
                                    std::optional<music_mood::Emotion> emotion)
{
	View view;
	view.running = player.IsRunning();
	if (!view.running)
	{
		panel.controlsShown = false;
		return view;
	}
	const auto position = player.GetPosition();
	panel.length = player.GetLength();
	view.positionLabel = PositionLabel(position, panel.length);
	view.position =
	    panel.length > 0 ? static_cast<float>(std::min(position, panel.length)) / static_cast<float>(panel.length) : 0.0f;
	view.volume = static_cast<float>(panel.volume) / 255.0f;
	view.songName = ToUtf16(player.GetSongName());
	for (const auto& track : player.GetTracks())
	{
		view.playList.push_back(ToUtf16(track.fileName));
	}
	view.currentTrack = player.GetCurrentTrack();
	view.musicColour = MusicButtonColour(music_mood::ButtonTint(emotion));
	return view;
}
