/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <cstdint>

#include <glm/vec3.hpp>

#include "Sound/GameMusic.h"

/// The rules that make the music's inputs of a turn from the place of the camera. They read no service, so a test can
/// give them any value.
namespace openblack::audio::game_music_turn
{

/// The cinema bars of the scripts at this turn
struct CinemaBars
{
	/// The bars show
	bool on;
	/// The script that holds the bars, 0 for none
	uint32_t owner;
	/// The bars do not slide in or out
	bool transitionFinished;
};

/// The land's music waits while a script holds the cinema bars, and while the bars slide in or out
[[nodiscard]] constexpr bool CinemaHoldsLandMusic(const CinemaBars& bars)
{
	return (bars.on && bars.owner != 0) || !bars.transitionFinished;
}

/// The place that the music of a turn hears from, and the alignments that it follows
struct Place
{
	uint32_t turn;
	glm::vec3 camera;
	/// The height of the land under the camera
	float groundHeight;
	/// The music of the citadel plays
	bool inCitadel;
	/// The alignment of the place that the camera is in
	float alignment;
	/// The alignment of the player of this computer
	float playerAlignment;
	/// The land's music waits for the cinema bars
	bool cinema;
};

/// The inputs of the music at a place. The towns, the objects with music, the dances, the fight, the film and the
/// music of the scripts start empty: the caller adds them.
[[nodiscard]] inline GameMusic::TurnInputs InputsAt(const Place& place)
{
	return {
	    .turn = place.turn,
	    .camera = place.camera,
	    .groundHeight = place.groundHeight,
	    .inCitadel = place.inCitadel,
	    .alignment = place.alignment,
	    .playerAlignment = place.playerAlignment,
	    .cinema = place.cinema,
	    .towns = {},
	    .thingPosition = {},
	};
}

} // namespace openblack::audio::game_music_turn
