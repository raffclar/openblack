/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <memory>

#include "ECS/Systems/VideoSystemInterface.h"
#include "Video/FallingSpellVideo.h"
#include "Video/VideoPlayer.h"

#if !defined(LOCATOR_IMPLEMENTATIONS)
#error "Locator interface implementations should only be included in Locator.cpp, use interface instead."
#endif

namespace openblack::ecs::systems
{
/// Owns the film player and the falling spell video. The player is declared first so that it goes after the falling
/// spell video, which plays on it
class VideoSystem final: public VideoSystemInterface
{
public:
	[[nodiscard]] video::VideoPlayer& Player() override
	{
		if (!_player)
		{
			_player = std::make_unique<video::VideoPlayer>(video::VideoPlayer::GameHooks());
		}
		return *_player;
	}

	[[nodiscard]] video::FallingSpellVideo& FallingSpell() override
	{
		if (!_fallingSpell)
		{
			_fallingSpell = std::make_unique<video::FallingSpellVideo>(Player(), video::FallingSpellVideo::GameHooks());
		}
		return *_fallingSpell;
	}

	[[nodiscard]] bool FilmsEnabled() const override { return _filmsEnabled; }
	void SetFilmsEnabled(bool enabled) override { _filmsEnabled = enabled; }

private:
	std::unique_ptr<video::VideoPlayer> _player;
	std::unique_ptr<video::FallingSpellVideo> _fallingSpell;
	bool _filmsEnabled {true};
};
} // namespace openblack::ecs::systems
