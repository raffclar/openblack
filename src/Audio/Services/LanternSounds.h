/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

namespace openblack::audio::lantern_sounds
{

/// The looping crackle of the street lanterns: a sound tag per lantern (audio::tags) made when the
/// lantern is created, on the lantern at (0, its height, 0), sample 0x93 LH_SAMPLE_G_LANTERN_01 of InGame.sad, track 0,
/// mode 2, loops -1, extra3DFlag 0, 3D, InGame bank, delay 0, then set active by the dark flag, unless the lantern is
/// unavailable (openblack's lanterns never are). Town lanterns and country ones both get it. The tag replays it while
/// active and the camera is within the sample's max distance (5); a gone lantern's tag releases its loop.

/// The dark flag: sets every lantern's tag active or not. Called every frame from night_lights::Update with "it is
/// dark" (the mean of the light-table base colour under 120). Turning it off stops the samples at once.
void SetOn(bool on);

/// Before tags::ProcessSoundTags, once per game turn: the lanterns made since the last turn get their tag
/// (openblack has no creation hook: inferred to be the same, a turn later at most), the gone
/// ones are forgotten (their tag goes on its own); the tags themselves are processed with the others (audio::tags).
void ProcessTurn();

/// No tags and the flag off (a new map; the tags go with tags::Clear)
void Clear();

} // namespace openblack::audio::lantern_sounds
