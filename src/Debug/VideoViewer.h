/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <chrono>
#include <filesystem>
#include <memory>
#include <optional>
#include <vector>

#include <BinkDecoder.h>

#include "Video/PreviewPlayback.h"
#include "Window.h"

namespace openblack::graphics
{
class Texture2D;
}

namespace openblack::debug::gui
{

/// The game's videos: switches for films and their colour, the full-screen video playing, and a player for any of the
/// five in this window (play, pause, step, restart)
class VideoViewer final: public Window
{
public:
	VideoViewer() noexcept;
	~VideoViewer() noexcept override;

protected:
	void Draw() noexcept override;
	void Update() noexcept override;
	void ProcessEventOpen(const SDL_Event& event) noexcept override;
	void ProcessEventAlways(const SDL_Event& event) noexcept override;

private:
	void OpenFilm(const std::filesystem::path& path);
	void CloseFilm();
	void ShowFrame(uint32_t frame);
	/// Plays one of the five films in the window
	void DrawPreview();

	std::optional<std::filesystem::path> _path;
	std::optional<bink::FrameReader> _reader;
	std::optional<video::PreviewPlayback> _playback;
	std::unique_ptr<graphics::Texture2D> _texture;
	std::vector<uint8_t> _rgba;
	std::optional<uint32_t> _shown;
	std::chrono::steady_clock::time_point _last;
};

} // namespace openblack::debug::gui
