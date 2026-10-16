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
#include <optional>
#include <vector>

#include <glm/vec2.hpp>

#include "Audio/MusicMood.h"
#include "CustomMusicControlsRules.h"
#include "Dialog.h"

namespace openblack::audio
{
class CustomMusicPlayerInterface;
}

namespace openblack::graphics
{
class Texture2D;
}

namespace openblack::gui
{

class GameFont;
class Slider;
class StaticText;
class List;

/// The controls of the custom music player at the top right of the screen, with no box behind them:
/// - The music button shows and hides the other controls. Its colour blends from white to green for a track that the
///   creature likes, or to red for a track that it does not like.
///   The music button shows only while the custom music folder has a track.
/// - Play, pause, stop, the last and the next track, rewind and fast forward, shuffle and repeat. Shuffle and repeat
///   stay lit and pushed in while they are on.
/// - The volume slider, and the position slider with the time played and the length of the track above it.
/// - The name of the song that plays, to the left of the music button.
/// - The play list button shows and hides the play list below the controls. A click on a track plays it.
///
/// The controls take the mouse only when the pointer is over one of them, and a control that acts clicks.
class CustomMusicControls
{
public:
	CustomMusicControls(const GameFont& font, const graphics::Texture2D* icons);
	CustomMusicControls(const CustomMusicControls&) = delete;
	CustomMusicControls& operator=(const CustomMusicControls&) = delete;

	/// Shows the controls, or hides them, as the screen lets them show
	void SetShown(bool shown);
	/// Shown and taking input
	[[nodiscard]] bool IsActive() const noexcept { return _shown; }
	/// Whether the controls take the mouse: the mouse button is held on a control, or, with no mouse button down, the
	/// pointer is over a control
	[[nodiscard]] bool WantsMouse(bool mouseButtonDown) const;
	void MouseMove(glm::ivec2 point);
	void MouseDown(glm::ivec2 point);
	/// True when a control acted, which clicks
	bool MouseUp(glm::ivec2 point);
	void Wheel(glm::ivec2 point, int steps);

	/// Every frame: the player to control (none without custom music), the mood of the track that plays, and the time.
	/// The controls look at the player at most once a second.
	void Update(audio::CustomMusicPlayerInterface* player, std::optional<music_mood::Emotion> emotion, uint32_t milliseconds);
	/// Draws the controls in the layout space. The caller puts the layout's right edge on the screen's right edge
	/// (custom_music_controls::Origin).
	void Draw(const DialogPainter& painter) const;

	/// Shows the controls and the play list, or hides them, as the music button and the play list button do
	void ShowControls(bool controls, bool playList);
	[[nodiscard]] const custom_music_controls::Panel& GetPanel() const noexcept { return _panel; }
	/// The controls that show, for tools that press them by name
	[[nodiscard]] std::vector<Dialog::NamedControl> GetNamedControls() const;

private:
	class IconButton;

	void Click(custom_music_controls::Button button);
	void Refresh();
	void ShowParts();

	Dialog _dialog;
	std::array<IconButton*, custom_music_controls::k_ButtonCount> _buttons {};
	Slider* _volume {nullptr};
	Slider* _position {nullptr};
	StaticText* _songName {nullptr};
	List* _playList {nullptr};

	custom_music_controls::Panel _panel;
	audio::CustomMusicPlayerInterface* _player {nullptr};
	std::optional<music_mood::Emotion> _emotion;
	std::optional<uint32_t> _lastRefresh;
	uint32_t _milliseconds {0};
	bool _shown {false};
	/// The player has a track
	bool _running {false};
};

} // namespace openblack::gui
