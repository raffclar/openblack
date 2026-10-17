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

#include "Common/FramePace.h"
#include "Video/FilmCreature.h"

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
	/// Whether the film shows it at the film's time now
	bool shown {false};
};

} // namespace openblack::ecs::components
