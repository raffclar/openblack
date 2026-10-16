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
#include "CustomMusicPlayerInterface.h"
#include "CustomMusicPlaylist.h"

namespace openblack::audio
{

/// Plays the MP3 tracks of the custom music folder on an OpenAL source. Another thread reads and opens a track and
/// finds its length. The player decodes a quarter of a second at a time and keeps a second of music queued on the source.
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

private:
	/// Starts to read and open the current track on another thread
	void Open();
	/// Stops the source and lets go of the track
	void Close();
	/// The track plays again from the start: the current track is opened anew when the player plays
	void Restart();
	/// Decodes more of the track onto the source, up to the queue's length
	void Refill();
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
