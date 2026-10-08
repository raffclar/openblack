/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <array>
#include <chrono>
#include <optional>
#include <string>
#include <string_view>

#include <bgfx/bgfx.h>

#include "Graphics/UniqueHandle.h"
#include "Video/BikFile.h"
#include "Video/BinkDecoder.h"
#include "VideoPlayback.h"
#include "Window.h"

namespace openblack::debug::gui
{

/// The Video debug window: the switch that turns the game's films off for testing, and one of the game's films played
/// with openblack's own Bink decoder, to check it, with play, pause and single steps, and the frame and time against the
/// game player's pacing. Apart from that switch, the game's own films are not affected.
class VideoViewer final: public Window
{
public:
	/// The game's films, as the game names them
	static constexpr std::array<std::string_view, 5> k_Films = {
	    "Data/logo.bik", "Data/pre_intro.bik", "Data/INTRO.bik", "Data/tips.bik", "Data/Spells/fall/fall.bik",
	};

	VideoViewer() noexcept;

protected:
	void Draw() noexcept override;
	void Update() noexcept override;
	void ProcessEventOpen(const SDL_Event& event) noexcept override;
	void ProcessEventAlways(const SDL_Event& event) noexcept override;

private:
	/// The switch of the game's films, and one entry per film (disabled when the game folder does not have it)
	void DrawFilms() noexcept;
	/// Opens `film` paused on its first frame; the error is shown when it cannot
	void Load(std::string_view film) noexcept;
	/// Decodes every frame up to the one due, and uploads the last one
	void CatchUp() noexcept;

	std::string _film;
	std::string _error;
	video::BikFile _file;
	video::BinkDecoder _decoder;
	graphics::UniqueHandle<bgfx::TextureHandle> _texture; ///< RGBA8, the film's size
	VideoPlayback _playback;
	uint32_t _shown {0};    ///< frames decoded since the film was opened or restarted
	uint32_t _caughtUp {0}; ///< frames decoded in the last update (more than 1: it fell behind)
	double _decodeMs {0.0}; ///< the last frame's decoding time
	std::optional<std::chrono::steady_clock::time_point> _lastUpdate;
};

} // namespace openblack::debug::gui
