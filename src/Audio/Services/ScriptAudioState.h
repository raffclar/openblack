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

#include <atomic>

// The audio switches of the script state. The fields are atomic: the music thread writes the music line and the beats
// (the music marker callback) while the scripts read them.

namespace openblack::audio
{

struct ScriptAudioState
{
	/// The creatures' sounds. SET_CREATURE_SOUND, END_DIALOGUE (=1) and START_CAMERA_CONTROL (=1) write it; the
	/// creature sound code reads it (with 0, only the local player's creature sounds). 1 after Reset.
	std::atomic<int32_t> creatureSound {1};
	/// Only the dialogue banks sound (SET_GAME_SOUND: false -> 1 and every sample stopped, true -> 0); read by
	/// PlaySoundEffect and PlayAnimationEffect. 0 after Reset. (Kept here for the reset; SET_GAME_SOUND lives
	/// elsewhere.)
	std::atomic<int32_t> gameSoundOff {0};
	/// The alignment music (ENABLE_DISABLE_ALIGNMENT_MUSIC); the alignment music plays nothing with 0. 1 after Reset.
	std::atomic<int32_t> alignmentMusic {1};
	/// The last "L<n>" marker of the script music; START_MUSIC sets 0; LAST_MUSIC_LINE compares it unsigned. 0 after
	/// Reset.
	std::atomic<uint32_t> musicLine {0};
	/// The beats: 1 at an "L" marker, +1 at "P"/"W"; START_MUSIC and END_DIALOGUE set 0. Read by the game (inferred:
	/// the text of a song follows it). 0 after Reset.
	std::atomic<int32_t> musicBeat {0};

	/// The audio part of the script reset
	void Reset();
	/// The audio part of END_DIALOGUE: creatureSound = 1, musicBeat = 0, done only when the calling task owns the
	/// dialogue (Help/ScriptControl.cpp)
	void EndDialogue();
};

/// The script audio state of the running game (one)
ScriptAudioState& GetScriptAudioState();

} // namespace openblack::audio
