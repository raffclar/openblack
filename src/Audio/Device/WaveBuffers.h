/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <cstddef>
#include <cstdint>

#include <vector>

#include "Audio/Device/Sound.h"

// The waves of the .sad banks, decoded at their first use.
//
// The original registers the banks with only their headers (in every call of the game) and reads a wave from the open
// file when a sample first starts, keeping it in a FIFO cache of RAM / 8; QMixer converts ADPCM and MPEG with ACM.
// openblack keeps the .sad bytes of each sample (Sound::buffer; the dialogue banks of Audio\Dialogue
// are read as the original does, headers only, and a wave is read from the file when it is decoded: Sound::waveFile),
// decodes them once to PCM at the first use and keeps one OpenAL buffer per sample until the audio closes (no eviction: the
// budget of the original only matters with less than 1 GB of RAM) (approximate: one buffer per sample record, while the
// original caches per wave, so the clones of a wave are decoded once each).

namespace openblack::audio::wave_buffers
{

/// A decoded wave
struct Pcm
{
	std::vector<int16_t> samples;
	ChannelLayout layout {ChannelLayout::Mono};
	int sampleRate {0};
	[[nodiscard]] size_t Frames() const { return layout == ChannelLayout::Stereo ? samples.size() / 2 : samples.size(); }
};

/// The sample's wave as PCM (read from its .sad first when it was left there: banks::ReadWave). A .sad wave is a RIFF
/// file (the original opens it from memory with QMixer): wFormatTag 1 (PCM) and 2 (MS-ADPCM) go to
/// WavAudioDecoder; 0x50 (MPEG-1/2 layer II: all of HelpSprites and villagers, most of Guidance) has its "data" chunk
/// decoded by MpegAudioDecoder, as ACM does (both audio::codec). A wave that is not RIFF is tried as raw MPEG (the music
/// segments). False when nothing decodes (an empty sample: InGame 165, spells 31).
[[nodiscard]] bool Decode(const Sound& sound, Pcm& out);

/// The sample's OpenAL buffer, decoded and made at the first call and kept in `sound.bufferId` (with its duration and
/// size); 0 when the wave does not decode or there is no OpenAL context. With a loop section (start < end, both set)
/// the buffer gets it as AL_LOOP_POINTS_SOFT, so a looping channel repeats only that section after its first pass, as
/// QMixer's play parameters do.
BufferId Get(Sound& sound);

/// The PCM Get decoded for `sound` when it made its buffer, if that was the last buffer made: moved out (once), so a
/// reader of the wave's PCM does not decode it again. False otherwise (then Decode gives the same PCM)
[[nodiscard]] bool TakeDecoded(const Sound& sound, Pcm& out);

/// The buffer of a sample that is going away
void Release(Sound& sound);

/// Every buffer made, deleted (the audio closes: after every source that used them)
void DeleteAll();

/// Statistics for the traces and the debug panel: the buffers alive and the ones ever made (alGenBuffers calls)
[[nodiscard]] size_t Alive();
[[nodiscard]] size_t Made();

} // namespace openblack::audio::wave_buffers
