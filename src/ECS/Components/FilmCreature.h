/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <array>
#include <optional>
#include <vector>

#include "Common/FramePace.h"
#include "Video/FilmCreature.h"
#include "Video/FilmEffects.h"

namespace openblack::ecs::components
{

/// The copy (see CreatureCopy) of the player's creature that the falling spell's film shows. It stands at the world's
/// origin, and the renderer draws it only over the film.
struct FilmCreature
{
	/// The film's time that its pose has moved on to
	film_creature::Clock clock;
	/// Its colours as it was last drawn: none before its first draw
	std::optional<film_creature::Look> look;
	/// Its colours flicker at most once a frame of the screen's refresh
	frame_pace::Pace pace;
	/// Whether the film showed it when it was last drawn. The film starts with it shown. Only while the film shows it
	/// does it make motes.
	bool shown {true};

	/// The stars and the burst of light of the film (see film_effects): they start a time into the film
	std::array<film_effects::Star, film_effects::k_StarCount> stars {};
	bool starsStarted {false};
	bool starsOn {false};
	film_effects::Burst burst;
	bool burstOn {false};
	/// The burst's alpha when it last moved on, 0 to 255
	uint32_t burstAlpha {0};
	/// The stars and the burst move on, and the motes live, by the real time: at most once a frame of the screen's
	/// refresh
	frame_pace::Pace effectsPace;
	/// The stars as they were drawn last, and the corners of the burst's fans this frame
	std::vector<film_effects::StarSprite> starSprites;
	std::vector<std::array<film_effects::FanCorner, film_effects::k_FanCorners>> fans;
};

} // namespace openblack::ecs::components
