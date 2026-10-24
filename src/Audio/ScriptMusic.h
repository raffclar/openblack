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

#include <chrono>
#include <functional>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "MusicType.h"

namespace openblack::audio
{

/// A point the bank marks in its music: the 1-based chunk, the frames into it and the cue's text
struct MusicCue
{
	uint32_t chunk {1};
	uint32_t frame {0};
	std::string text;
};

/// Where playback is as the bank's cues count it: a 1-based chunk and the frames into it
struct MusicCuePosition
{
	uint32_t chunk {0};
	uint32_t frame {0};

	auto operator<=>(const MusicCuePosition&) const = default;
};

/// Cues are timed as if every chunk were this many frames long, counted from the chunk playback started at
constexpr uint32_t k_CueChunkFrames = 24192;

/// Where the cues' clock is after framesHeard frames from the start of startChunk
[[nodiscard]] MusicCuePosition CuePositionAfter(uint32_t startChunk, uint64_t framesHeard);

/// The texts of the cues passed going from one position to the next: those after `from` and up to `to`, in the bank's
/// cue order
[[nodiscard]] std::vector<std::string_view> CuesPassed(std::span<const MusicCue> cues, MusicCuePosition from,
                                                       MusicCuePosition to);

/// The times of a music bank that the world uses: its cues, the frames of sound in each of its chunks, the sample rate
/// of its first chunk, and the loop count of its header where the header sets one
struct MusicTiming
{
	/// The cues marked in the chunks' texts, in the order they are checked
	std::vector<MusicCue> cues;
	std::vector<uint32_t> chunkFrames;
	uint32_t sampleRate {0};
	std::optional<int32_t> loopOverride;

	/// The frames of sound in one pass through all the chunks
	[[nodiscard]] uint64_t PieceFrames() const;
};

/// A place in the chunks of a bank: the 1-based chunk and the frames into it
struct MusicPiecePosition
{
	uint32_t chunk {1};
	uint32_t frame {0};

	bool operator==(const MusicPiecePosition&) const = default;
};

/// The place in the chunks after the given frames of sound from the start of the first chunk. After the last chunk, the
/// count starts again at the first chunk, as a bank that loops does.
[[nodiscard]] MusicPiecePosition PiecePositionAfter(std::span<const uint32_t> chunkFrames, uint64_t frames);

/// How far a sung piece of script music has got, from the cues it carries as it plays
struct MusicLyrics
{
	/// The number of the last line cue heard, 0 before any
	int32_t line {0};
	/// The word of that line reached, counting from 1
	int32_t word {0};
};

/// A cue in a piece of script music: "L" (or "l") and a number starts that line at its first word, "W" or "P" (in
/// either case) moves on a word, and any other cue changes nothing
void ApplyMusicCue(MusicLyrics& lyrics, std::string_view cue);

/// Whether the music has reached a line: its last line cue was that line or a later one. Both are compared as unsigned
/// numbers, so a negative line is never reached.
[[nodiscard]] bool ReachedMusicLine(int32_t lastLine, int32_t line);

/// Whether the world has the game's music. A game on one computer has it when the computer had a sound device at the
/// start, as Black & White has. In a network game and on the server, every computer counts as one with a sound device,
/// so that the world of each computer is the same.
[[nodiscard]] constexpr bool WorldHasMusic(bool soundDevice, bool networkGame, bool dedicatedServer)
{
	return soundDevice || networkGame || dedicatedServer;
}

/// The music that the scripts start, as the world sees it: which piece the scripts want, the line and word that the
/// piece has reached, and whether it is over.
///
/// The cues and the end of a piece follow a clock that the game turns move on. Each turn adds the real time that the turn
/// takes at the game's speed. Thus every computer and the server see the same cues at the same turns, with or without a
/// sound device. The music that the player hears starts at the place of this clock.
///
/// The piece plays on one channel, as the music thread plays it:
/// - The thread looks at the channel every 120 ms of the clock. Each look passes the cues that the clock has reached
///   since the last look, while the channel is the music heard.
/// - At the first look after the last chunk has played, the piece ends. If the script still wants this piece, the
///   script music becomes none.
/// - When the script stops the piece, the channel fades out over 43 looks and then stops. Its cues continue while it
///   fades. If the script starts the same piece again during the fade, the channel continues from where it is.
class ScriptMusic
{
public:
	static constexpr auto k_Tick = std::chrono::milliseconds(120);
	/// The looks that a channel at full volume takes to fade to silence, at 3 of 127 a look
	static constexpr uint32_t k_FadeTicks = 43;

	/// The times of a piece of music, none when its bank cannot play
	using TimingFor = std::function<std::shared_ptr<const MusicTiming>(MusicType)>;

	/// A script starts a piece of music, or stops it with MusicType::None. A piece starts again from its start, unless
	/// its channel still plays.
	void Start(MusicType type);
	/// A script starts music: the lyrics start again with no line and no word
	void ResetLyrics() { _lyrics = {}; }
	/// The end of a dialogue starts the sung line again at no word, and keeps the line
	void ResetWord() { _lyrics.word = 0; }

	/// One game turn: the channel plays for the real time of the turn, then the script's music starts or stops
	void Turn(std::chrono::microseconds turnTime, const TimingFor& timingFor);

	/// The piece that the scripts want, MusicType::None for none
	[[nodiscard]] MusicType GetType() const { return _type; }
	/// The script music of a type is over: it played to its end, or a script stopped it or started other music
	[[nodiscard]] bool IsOver(MusicType type) const { return _type != type; }
	/// The piece of the scripts plays, or plays no sound because its channel is gone
	[[nodiscard]] bool IsStarted() const { return _started; }
	[[nodiscard]] const MusicLyrics& GetLyrics() const { return _lyrics; }
	/// How many times the game has played the scripts' music since the start. The music heard plays again when it changes.
	[[nodiscard]] uint32_t GetPlayCount() const { return _playCount; }
	/// The place that the piece of the scripts has reached, none when no channel plays it
	[[nodiscard]] std::optional<MusicPiecePosition> GetPlace() const;
	/// The time since the channel of the scripts' piece started, none when no channel plays
	[[nodiscard]] std::optional<std::chrono::microseconds> GetChannelTime() const;

	/// A saved game continues with the scripts' piece, whether it played and the lyrics. A piece that played before the
	/// save gives no sound and does not end, because the save keeps no channel.
	void Restore(MusicType type, bool started, MusicLyrics lyrics);

private:
	struct Channel
	{
		MusicType type {MusicType::None};
		std::shared_ptr<const MusicTiming> timing;
		/// Times to play the bank again after the first, -1 for ever
		int32_t loops {0};
		std::chrono::microseconds time {0};
		uint64_t ticks {0};
		MusicCuePosition checked;
		/// The channel is the music heard, so it passes its cues
		bool current {true};
		/// The looks left before a channel that fades out stops
		std::optional<uint32_t> fadeTicks;
	};

	/// One look of the music thread at the channel. True when the channel stops.
	bool Tick(Channel& channel);
	void ProcessScript(const TimingFor& timingFor);

	MusicType _type {MusicType::None};
	bool _started {false};
	uint32_t _playCount {0};
	std::optional<Channel> _channel;
	MusicLyrics _lyrics;
};

} // namespace openblack::audio
