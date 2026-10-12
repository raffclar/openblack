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

#include <array>
#include <chrono>
#include <deque>
#include <functional>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include <glm/vec3.hpp>

namespace openblack::audio
{

/// A point the bank marks in its music: the 1-based chunk, the frames into it and the cue's text
struct MusicCue
{
	uint32_t chunk {1};
	uint32_t frame {0};
	std::string text;
};

/// A music bank: a sound bank whose samples are chunks of one piece of music, sect0000.mpg onwards, played back to
/// back. Banks of one music group are variations of the same piece with the same chunk lengths, so the game can switch
/// between them in time.
struct MusicBank
{
	std::string path;
	/// LHBankGetMusicGroupId: the group of the first sample, 0 for none
	int32_t groupId {0};
	/// Volume of the bank, 0 to 127: the first sample's header volume where it overrides it
	uint32_t bankVolume {127};
	/// Loop count from the first sample's header where it overrides the caller's
	std::optional<int32_t> loopOverride;
	/// A positional piece's distances from the first sample's header, where it overrides the player's (see
	/// MusicPlacement)
	std::optional<float> minDistance;
	std::optional<float> maxDistance;
	std::optional<float> distanceScale;
	/// How far the music carries: the first sample's far distance whether or not its header overrides it, -1 for a bank
	/// without samples
	float musicMaxDistance {-1.0f};
	/// How many chunks the bank's music is cut into
	uint32_t chunkCount {0};
	std::vector<uint32_t> chunkSampleRates;
	/// The cues marked in the chunks' texts, in the order they are checked
	std::vector<MusicCue> cues;
	/// Reads a chunk's data, 0-based. A bank's music is far too big to hold: like the game, it stays in the bank's file,
	/// and each chunk is read from it only as it is queued to play. Empty if it couldn't be read.
	std::function<std::vector<uint8_t>(uint32_t chunk)> readChunk;

	[[nodiscard]] uint32_t GetChunkCount() const { return chunkCount; }
	[[nodiscard]] std::vector<uint8_t> ReadChunk(uint32_t chunk) const
	{
		return readChunk && chunk < chunkCount ? readChunk(chunk) : std::vector<uint8_t> {};
	}
};

/// Where a positional piece is heard from in the world, and how it fades with its distance from the listener, as QSound
/// maps it: every distance is first stretched by distanceScale; the piece is heard fully up to minDistance, beyond it at
/// minDistance over the distance, and no quieter past maxDistance
struct MusicPlacement
{
	glm::vec3 position;
	float minDistance;
	float maxDistance;
	float distanceScale;
};

/// Where LHAudioDLL's music channels are streamed
class MusicBackend
{
public:
	using Stream = uint32_t;
	static constexpr Stream k_InvalidStream = 0;

	virtual ~MusicBackend() = default;
	/// A stream for a channel, playing at pitchPercent of the chunks' rate
	[[nodiscard]] virtual Stream CreateStream(int32_t pitchPercent) = 0;
	/// Decodes a chunk of a bank, 0-based, and queues it behind the stream's queued chunks, leaving out its first
	/// skipFrames frames. False if the chunk could not be decoded.
	virtual bool QueueChunk(Stream stream, const MusicBank& bank, uint32_t chunk, uint32_t skipFrames) = 0;
	/// Releases the queued chunks that have finished playing and says how many there were
	[[nodiscard]] virtual uint32_t ReleasePlayedChunks(Stream stream) = 0;
	/// Frames played of the oldest queued chunk
	[[nodiscard]] virtual uint32_t GetPlayPosition(Stream stream) const = 0;
	/// Frames heard since the stream was made, across every chunk queued on it
	[[nodiscard]] virtual uint64_t GetFramesPlayed(Stream stream) const = 0;
	/// Plays the queued chunks, or picks up again after running out of them
	virtual void Play(Stream stream) = 0;
	[[nodiscard]] virtual bool IsPlaying(Stream stream) const = 0;
	/// Volume from 0 to 1, before the music and master volumes
	virtual void SetVolume(Stream stream, float volume) = 0;
	/// Places the stream in the world, or centres it on the listener when there is no placement
	virtual void SetPlacement(Stream stream, const std::optional<MusicPlacement>& placement) = 0;
	/// Stops the stream and releases it, the handle is invalid afterwards
	virtual void Destroy(Stream stream) = 0;
};

/// LH_MusicPlayOptions
struct MusicPlayOptions
{
	std::shared_ptr<const MusicBank> bank;
	/// Target volume, 0 to 127
	int32_t volume {127};
	/// 1-based chunk to start from, the first one if out of range
	uint32_t startChunk {1};
	/// Times to play the bank again after the first, -1 for ever. The bank header can override it.
	int32_t loops {0};
	/// Start in time with a channel already playing a bank of the same music group
	bool sync {false};
	/// Fade in from silence rather than starting at full volume
	bool fadeIn {false};
	int32_t pitchPercent {100};
	/// Positional music is heard from here in the world; otherwise it is centred on the listener
	std::optional<glm::vec3> position;
	/// A positional piece's distances, unless the bank's header overrides them
	float minDistance {10.0f};
	float maxDistance {100.0f};
	float distanceScale {1.0f};
	/// Called when the bank has played to its end
	std::function<void()> onFinished;
	/// Called with a cue's text as playback passes each cue of the bank
	std::function<void(std::string_view)> onCue;
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

/// Port of the music part of LHAudioDLL ver7.0 (LHMusicPlay, LHMusicStop, LHMusicIsActive and the music thread).
///
/// Six channels each stream a bank chunk by chunk, keeping four chunks queued. A channel plays its bank through,
/// starting again from the first chunk while it has loops left. One channel at a time is current: it fades in to its
/// target volume while the others fade out, so a new piece crossfades with the old. Volumes are LHAudio units, 0 to
/// 127, and change once per tick of the music thread. On each tick the current channel also passes the cues its
/// playback has reached since the last tick to whoever played it.
class MusicPlayer
{
public:
	static constexpr size_t k_ChannelCount = 6;
	static constexpr auto k_Tick = std::chrono::milliseconds(120);
	static constexpr uint32_t k_QueuedChunks = 4;
	static constexpr int32_t k_MaxVolume = 127;
	static constexpr int32_t k_FadeInStep = 4;
	static constexpr int32_t k_FadeOutStep = 3;
	static constexpr int k_NoChannel = -1;

	struct Channel
	{
		bool active {false};
		std::shared_ptr<const MusicBank> bank;
		int32_t groupId {0};
		/// 0 to 127
		int32_t targetVolume {0};
		int32_t volume {0};
		bool fadeIn {false};
		int32_t loops {0};
		bool sync {false};
		int32_t pitchPercent {100};
		std::function<void()> onFinished;
		std::function<void(std::string_view)> onCue;
		/// The cues' clock: the chunk and frames into it playback started at, and where it was last checked
		uint32_t cueStartChunk {1};
		uint32_t cueStartFrames {0};
		MusicCuePosition cueChecked;
		/// Where a positional channel is heard from
		std::optional<MusicPlacement> placement;
		MusicBackend::Stream stream {MusicBackend::k_InvalidStream};
		/// 0-based chunks queued on the stream, oldest first
		std::deque<uint32_t> queued;
		/// 0-based chunk to queue next
		uint32_t nextChunk {0};
		/// The bank has no more chunks to queue
		bool ending {false};
		/// 1-based chunk being heard
		uint32_t playingChunk {1};
	};

	explicit MusicPlayer(MusicBackend& backend);
	~MusicPlayer();
	MusicPlayer(const MusicPlayer&) = delete;
	MusicPlayer& operator=(const MusicPlayer&) = delete;

	/// LHMusicPlay: plays a bank, or retargets the channel already playing it. Without a bank, fades all music out. A
	/// positional channel already playing it is moved to the new position.
	bool Play(const MusicPlayOptions& options);
	/// Stops one bank: the channel playing it stops at once, unless asked to fade, which leaves it be. When
	/// it is the channel heard, all the music stops as Stop does.
	void StopBank(const std::shared_ptr<const MusicBank>& bank, bool fadeOut);
	/// LHMusicStop: fades every channel out, or stops them at once
	void Stop(bool fadeOut);
	/// LHMusicIsActive
	[[nodiscard]] bool IsActive() const;
	/// Keeps the streams fed and changes volumes once per tick
	void Update(std::chrono::microseconds dt);

	[[nodiscard]] const std::array<Channel, k_ChannelCount>& GetChannels() const { return _channels; }
	[[nodiscard]] int GetCurrentChannel() const { return _current; }

private:
	void Start(size_t index, uint32_t startChunk);
	void Feed(Channel& channel);
	void Release(size_t index);
	void VolumeTick();
	void CueTick();
	void ApplyVolume(const Channel& channel);

	MusicBackend& _backend;
	std::array<Channel, k_ChannelCount> _channels;
	int _current {k_NoChannel};
	std::chrono::microseconds _tickTime {0};
};

} // namespace openblack::audio
