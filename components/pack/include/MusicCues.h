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

#include <span>
#include <string>
#include <string_view>
#include <vector>

/// Cues placed in a music bank: a chunk's sample header text can mark points in that chunk, written as
/// `!<frame>=<cue>` one after another, e.g. `!0=L1` or `!18047=p!10751=p`. The game passes each cue's text to whoever
/// played the music as playback reaches the cue (the missionaries' verses mark their lines and words this way).
namespace openblack::pack
{

struct AudioBankSampleHeader;

/// A cue as written in one chunk's text: the frame into the chunk and the cue's text
struct ChunkCue
{
	uint32_t frame {0};
	std::string text;

	bool operator==(const ChunkCue&) const = default;
};

/// A cue of a bank: its 1-based chunk, the frame into that chunk and the text
struct MusicCue
{
	uint32_t chunk {1};
	uint32_t frame {0};
	std::string text;

	bool operator==(const MusicCue&) const = default;
};

/// Reads the cues of one chunk's header text, in the order they are written. Text not starting with `!` holds none.
/// Each cue's frame is the run of digits after its `!` (0 without any); the character after the digits is skipped
/// (normally `=`) and the text runs to the next `!` or the end.
[[nodiscard]] std::vector<ChunkCue> ParseChunkCues(std::string_view text);

/// The cues of a whole bank in the order the game checks them: chunk by chunk from the first, and within a chunk the
/// last written first
[[nodiscard]] std::vector<MusicCue> ReadMusicCues(std::span<const AudioBankSampleHeader> headers);

} // namespace openblack::pack
