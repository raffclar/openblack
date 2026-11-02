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

#include <deque>
#include <functional>
#include <future>
#include <memory>
#include <optional>
#include <vector>

#include "AudioPlayerInterface.h"
#include "Sound/CustomMusicPlayerInterface.h"
#include "Sound/CustomMusicPlaylist.h"

namespace openblack::audio
{

/// Plays the MP3 tracks of the custom music folder on an OpenAL source. Another thread reads and opens a track, finds its
/// length and decodes its first second. Other threads then decode a quarter of a second at a time, so that a frame of the
/// game does not wait for the decode. The player keeps a second of music queued on the source.
class CustomMusicPlayer final: public CustomMusicPlayerInterface
{
public:
	/// Reads all the bytes of a file, none when it cannot
	using Reader = std::function<std::vector<uint8_t>(const std::filesystem::path& path)>;

	CustomMusicPlayer(AudioPlayerInterface& player, Reader read, custom_music::Random random);
	~CustomMusicPlayer() override;
	CustomMusicPlayer(const CustomMusicPlayer&) = delete;
	CustomMusicPlayer& operator=(const CustomMusicPlayer&) = delete;
	CustomMusicPlayer(CustomMusicPlayer&&) = delete;
	CustomMusicPlayer& operator=(CustomMusicPlayer&&) = delete;

	void SetFiles(const std::vector<std::filesystem::path>& files) override;
	[[nodiscard]] bool IsRunning() const override { return !_playlist.GetTracks().empty(); }
	[[nodiscard]] PlayState GetPlayState() const override { return _state; }
	[[nodiscard]] std::string GetSongName() const override;
	[[nodiscard]] const std::vector<custom_music::Track>& GetTracks() const override { return _playlist.GetTracks(); }
	[[nodiscard]] std::optional<size_t> GetCurrentTrack() const override { return _playlist.GetCurrent(); }
	void SetCurrentTrack(size_t track) override;
	void Play() override;
	void Pause() override;
	void Stop() override;
	void NextTrack() override;
	void PreviousTrack() override;
	[[nodiscard]] uint32_t GetPosition() const override;
	[[nodiscard]] uint32_t GetLength() const override;
	void SetPosition(uint32_t milliseconds) override;
	void SetVolume(uint32_t volume) override;
	[[nodiscard]] uint32_t GetVolume() const override { return _volume; }
	void SetShuffle(bool shuffle) override { _playlist.SetShuffle(shuffle); }
	void SetRepeat(bool repeat) override { _playlist.SetRepeat(repeat); }
	[[nodiscard]] bool GetShuffle() const override { return _playlist.GetShuffle(); }
	[[nodiscard]] bool GetRepeat() const override { return _playlist.GetRepeat(); }
	void Update() override;

	/// A track read and opened for decoding
	struct OpenTrack;
	/// The parts of a track that another thread decoded, in the sequence that they play
	struct DecodedParts
	{
		/// The samples of each part, a quarter of a second each. The last part can be shorter.
		std::vector<std::vector<int16_t>> parts;
		/// The frames of each part
		std::vector<uint64_t> frames;
		/// True when the decode came to the end of the track
		bool end {false};
		/// The frame that the decode moved to before it decoded the parts, when it moved
		std::optional<uint64_t> movedTo;
		/// True when the decode could not move to the frame: it then decoded nothing
		bool moveFailed {false};
	};

private:
	/// Starts to read and open the current track on another thread
	void Open();
	/// Stops the source and lets go of the track
	void Close();
	/// The track plays again from the start: the current track is opened anew when the player plays
	void Restart();
	/// Queues the parts that other threads decoded, then starts the decode of more, up to the queue's length
	void Refill();
	/// Queues decoded parts on the source
	void QueueParts(DecodedParts decoded);
	/// Starts the decode of parts on another thread, after the decode that runs now. With a frame, the decode first moves
	/// the track to that frame.
	void StartDecode(std::optional<uint64_t> moveTo, size_t parts);
	/// Takes the played buffers off the source
	void ReleasePlayed();
	[[nodiscard]] float Gain() const;

	AudioPlayerInterface& _player;
	Reader _read;
	custom_music::Random _random;
	custom_music::Playlist _playlist;
	PlayState _state {PlayState::Stopped};
	std::future<std::shared_ptr<OpenTrack>> _opening;
	/// Opens of tracks let go of before they were done, kept until they end so that no frame waits for them
	std::vector<std::future<std::shared_ptr<OpenTrack>>> _abandoned;
	std::shared_ptr<OpenTrack> _open;
	/// The decode of parts that runs on another thread, none when no decode runs
	std::future<DecodedParts> _decoding;
	/// Decodes of tracks that the player closed before the decodes ended. The player keeps them until they end, so that no
	/// frame waits for them.
	std::vector<std::future<DecodedParts>> _abandonedDecodes;
	std::optional<SourceId> _source;
	/// The buffers queued on the source and the frames of each
	std::deque<std::pair<BufferId, uint64_t>> _queued;
	/// The frame that the source started at, and the frames it played since
	uint64_t _startFrame {0};
	uint64_t _playedFrames {0};
	bool _decodedAll {false};
	uint32_t _volume {255};
};

} // namespace openblack::audio
