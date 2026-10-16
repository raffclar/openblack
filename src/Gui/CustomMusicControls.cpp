/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "CustomMusicControls.h"

#include <string_view>
#include <utility>

#include <glm/vec4.hpp>

#include "Audio/CustomMusicPlayerInterface.h"
#include "Controls.h"

using namespace openblack::gui;
using namespace openblack::gui::custom_music_controls;

namespace
{
/// A colour as 0xAARRGGBB, from 0 to 1 a channel
glm::vec4 ToColour(uint32_t argb)
{
	return glm::vec4(static_cast<float>((argb >> 16u) & 0xffu), static_cast<float>((argb >> 8u) & 0xffu),
	                 static_cast<float>(argb & 0xffu), static_cast<float>(argb >> 24u)) /
	       255.0f;
}
} // namespace

/// A button of the player: an icon of the atmosphere texture over a button box, or with no box for the music button.
/// The icon lights up under the pointer and while the button is on (shuffle and repeat). It moves a pixel down and right
/// while the mouse button holds it and while it is on.
class CustomMusicControls::IconButton final: public Control
{
public:
	IconButton(const ButtonLook& look, const graphics::Texture2D* icons)
	    : Control(ToRect(look.place))
	    , look(look)
	    , _icons(icons)
	{
	}

	void Draw(const DialogPainter& painter, bool hovered, bool /*focused*/, bool pressed) const override
	{
		if (!visible)
		{
			return;
		}
		if (look.framed)
		{
			painter.DrawBevelBox(rect, hovered ? 2 : 1, DialogPainter::All, glm::vec4(1.0f));
		}
		if (_icons == nullptr)
		{
			return;
		}
		const auto icon = IconOf(look.icon, rect, pressed || on);
		const auto tint = hovered || on ? DialogPainter::k_HoverColour : ToColour(colour);
		painter.DrawPicture({.min = icon.corner, .max = icon.corner + k_IconSize}, *_icons, icon.uvMin, icon.uvMax, tint);
	}
	[[nodiscard]] std::u16string_view GetName() const override { return look.name; }
	[[nodiscard]] std::string_view GetKind() const override { return "button"; }

	ButtonLook look;
	/// The icon's colour as 0xAARRGGBB when it does not light up
	uint32_t colour {k_DefaultColour};
	bool on {false};

private:
	const graphics::Texture2D* _icons;
};

CustomMusicControls::CustomMusicControls(const GameFont& font, const graphics::Texture2D* icons)
{
	for (size_t i = 0; i < k_Buttons.size(); ++i)
	{
		auto& button = _dialog.Add<IconButton>(k_Buttons.at(i), icons);
		button.onClick = [this, which = k_Buttons.at(i).button]() { Click(which); };
		_buttons.at(i) = &button;
	}
	_volume = &_dialog.Add<Slider>(ToRect(k_VolumeSlider), std::u16string(k_VolumeLabel), 1.0f);
	_volume->labelSize = k_TextSize;
	_volume->onChange = [this](float value) {
		if (_player != nullptr)
		{
			DragVolume(_panel, *_player, value);
		}
	};
	_position = &_dialog.Add<Slider>(ToRect(k_PositionSlider), std::u16string(), 0.0f);
	_position->labelAbove = true;
	_position->onChange = [this](float value) {
		if (_player != nullptr)
		{
			DragPosition(_panel, *_player, value);
		}
	};
	_songName =
	    &_dialog.Add<StaticText>(ToRect(k_SongName), std::u16string(u"song name"), StaticText::Layout::Right, k_TextSize);
	_playList = &_dialog.Add<List>(font, ToRect(k_PlayList), k_TextSize, false, true, true);
	_playList->onClick = [this]() {
		if (_player != nullptr)
		{
			PickTrack(*_player, _playList->GetSelection());
		}
		Refresh();
	};
	ShowParts();
}

void CustomMusicControls::SetShown(bool shown)
{
	if (shown == _shown)
	{
		return;
	}
	_shown = shown;
	_dialog.Reset();
}

bool CustomMusicControls::WantsMouse(bool mouseButtonDown) const
{
	if (!_shown)
	{
		return false;
	}
	return _dialog.IsHolding() || (!mouseButtonDown && _dialog.IsPointerOverControl());
}

void CustomMusicControls::MouseMove(glm::ivec2 point)
{
	if (_shown)
	{
		_dialog.MouseMove(point);
	}
}

void CustomMusicControls::MouseDown(glm::ivec2 point)
{
	if (_shown)
	{
		_dialog.MouseDown(point);
	}
}

bool CustomMusicControls::MouseUp(glm::ivec2 point)
{
	return _shown && _dialog.MouseUp(point);
}

void CustomMusicControls::Wheel(glm::ivec2 point, int steps)
{
	if (_shown)
	{
		_dialog.Wheel(point, steps);
	}
}

void CustomMusicControls::Update(audio::CustomMusicPlayerInterface* player, std::optional<music_mood::Emotion> emotion,
                                 uint32_t milliseconds)
{
	_milliseconds = milliseconds;
	_emotion = emotion;
	// The music button shows only while the custom music folder has a track
	const bool running = player != nullptr && player->IsRunning();
	if (running != _running)
	{
		_running = running;
		ShowParts();
	}
	if (player != _player)
	{
		_player = player;
		_lastRefresh.reset();
		if (_player != nullptr)
		{
			Start(_panel, *_player);
		}
		ShowParts();
	}
	// The controls look at the player only while they show
	if (_shown && (!_lastRefresh.has_value() || milliseconds > *_lastRefresh + k_RefreshMilliseconds))
	{
		Refresh();
	}
	_dialog.Update(0.0f);
}

void CustomMusicControls::Click(custom_music_controls::Button button)
{
	if (_player == nullptr)
	{
		return;
	}
	const bool playListShown = _panel.playListShown;
	custom_music_controls::Click(_panel, *_player, button);
	if (_panel.playListShown && !playListShown)
	{
		_playList->ScrollToSelection();
	}
	Refresh();
}

void CustomMusicControls::Refresh()
{
	if (_player == nullptr)
	{
		return;
	}
	const auto view = custom_music_controls::Refresh(_panel, *_player, _emotion);
	if (view.running)
	{
		_lastRefresh = _milliseconds;
		_position->label = view.positionLabel;
		_position->SetValue(view.position);
		_volume->SetValue(view.volume);
		_songName->text = view.songName;
		// The list is made again when the number of tracks changes
		if (view.playList.size() != _playList->GetItems().size())
		{
			std::vector<List::Item> items;
			items.reserve(view.playList.size());
			for (const auto& name : view.playList)
			{
				items.push_back({.text = name, .mouseCell = std::nullopt});
			}
			_playList->SetItems(std::move(items));
		}
		// The list keeps the track that the player picks while the mouse button holds it
		if (_dialog.GetFocus() != _playList || !_dialog.IsHolding())
		{
			_playList->SetSelection(view.currentTrack);
		}
		_buttons.at(static_cast<size_t>(custom_music_controls::Button::Music))->colour = view.musicColour;
	}
	ShowParts();
}

void CustomMusicControls::ShowParts()
{
	const bool controls = _panel.controlsShown;
	for (auto* button : _buttons)
	{
		button->visible = button->look.button == custom_music_controls::Button::Music ? _running : controls;
	}
	_buttons.at(static_cast<size_t>(custom_music_controls::Button::Shuffle))->on = _panel.shuffle;
	_buttons.at(static_cast<size_t>(custom_music_controls::Button::Repeat))->on = _panel.repeat;
	_volume->visible = controls;
	_position->visible = controls;
	_songName->visible = controls;
	_playList->visible = controls && _panel.playListShown;
}

void CustomMusicControls::Draw(const DialogPainter& painter) const
{
	if (!_shown)
	{
		return;
	}
	painter.SetAlpha(k_Alpha);
	_dialog.Draw(painter, true);
	painter.SetAlpha(1.0f);
}

void CustomMusicControls::ShowControls(bool controls, bool playList)
{
	_panel.controlsShown = controls;
	_panel.playListShown = playList;
	if (playList)
	{
		_playList->ScrollToSelection();
	}
	Refresh();
	ShowParts();
}

std::vector<Dialog::NamedControl> CustomMusicControls::GetNamedControls() const
{
	return _shown ? _dialog.GetNamedControls() : std::vector<Dialog::NamedControl> {};
}
