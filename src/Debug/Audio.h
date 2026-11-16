/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <optional>

#include "Audio/AudioManagerInterface.h"
#include "Window.h"

namespace openblack::debug::gui
{
class Audio final: public Window
{
public:
	/// The window shows in a network game. It plays sounds on this computer, and its alignment slider sends a command of the
	/// turns.
	[[nodiscard]] bool ShowsInNetworkGame() const noexcept override { return true; }
	Audio() noexcept;

protected:
	void Draw() noexcept override;
	void Update() noexcept override;
	void ProcessEventOpen(const SDL_Event& event) noexcept override;
	void ProcessEventAlways(const SDL_Event& event) noexcept override;

private:
	void Emitters() noexcept;
	void Music() noexcept;
	void AudioSettings() noexcept;
	void Atmos() noexcept;
	audio::PlayType _playType {audio::PlayType::Once};
	entt::id_type _selectedSound;
	std::string _selectedSoundPack;
	std::string _selectedMusicPack;
	entt::entity _selectedEmitter;
	/// In a network game, the value of the alignment slider while it moves
	std::optional<float> _alignmentDrag;
};
} // namespace openblack::debug::gui
