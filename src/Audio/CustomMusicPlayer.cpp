/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "CustomMusicPlayer.h"

#include <algorithm>
#include <chrono>
#include <utility>

#include <spdlog/spdlog.h>

extern "C" {
#include <dr_mp3.h>
}

using namespace openblack::audio;

/// The bytes of a track and the decoder that reads them. The decoder points into the bytes, so both stay together.
struct CustomMusicPlayer::OpenTrack
{
	std::vector<uint8_t> bytes;
	drmp3 decoder {};
	bool decoderOpen {false};
	uint32_t sampleRate {0};
	uint32_t channels {0};
	uint64_t frames {0};

	OpenTrack() = default;
	OpenTrack(const OpenTrack&) = delete;
	OpenTrack& operator=(const OpenTrack&) = delete;
	OpenTrack(OpenTrack&&) = delete;
	OpenTrack& operator=(OpenTrack&&) = delete;
	~OpenTrack()
	{
		if (decoderOpen)
		{
			drmp3_uninit(&decoder);
		}
	}
};

namespace
{
/// Frames decoded at a time, as a share of a second, and the buffers kept queued on the source
constexpr uint32_t k_PartsOfASecond = 4;
constexpr size_t k_QueuedBuffers = 4;
constexpr uint32_t k_MostVolume = 255;

void Log(const std::string& text)
{
	if (const auto logger = spdlog::get("audio"); logger != nullptr)
	{
		logger->warn("{}", text);
	}
}

/// Reads the track and opens it, then finds its length with a second decoder over the same bytes
std::shared_ptr<CustomMusicPlayer::OpenTrack> OpenOnThread(const CustomMusicPlayer::Reader& read,
                                                           const std::filesystem::path& path)
{
	auto track = std::make_shared<CustomMusicPlayer::OpenTrack>();
	track->bytes = read(path);
	if (track->bytes.empty() ||
	    drmp3_init_memory(&track->decoder, track->bytes.data(), track->bytes.size(), nullptr) == DRMP3_FALSE)
	{
		return nullptr;
	}
	track->decoderOpen = true;
	track->sampleRate = track->decoder.sampleRate;
	track->channels = track->decoder.channels;
	if (track->sampleRate == 0 || (track->channels != 1 && track->channels != 2))
	{
		return nullptr;
	}
	drmp3 counter {};
	if (drmp3_init_memory(&counter, track->bytes.data(), track->bytes.size(), nullptr) != DRMP3_FALSE)
	{
		track->frames = drmp3_get_pcm_frame_count(&counter);
		drmp3_uninit(&counter);
	}
	return track;
}

[[nodiscard]] bool Ready(const std::future<std::shared_ptr<CustomMusicPlayer::OpenTrack>>& future)
{
	return future.valid() && future.wait_for(std::chrono::seconds(0)) == std::future_status::ready;
}
} // namespace

CustomMusicPlayer::CustomMusicPlayer(AudioPlayerInterface& player, Reader read, custom_music::Random random)
    : _player(player)
    , _read(std::move(read))
    , _random(std::move(random))
{
}

CustomMusicPlayer::~CustomMusicPlayer()
{
	Close();
}

void CustomMusicPlayer::SetFiles(const std::vector<std::filesystem::path>& files)
{
	Stop();
	_playlist.SetTracks(custom_music::ListTracks(files));
}

std::string CustomMusicPlayer::GetSongName() const
{
	const auto* track = _playlist.GetCurrentTrack();
	return track != nullptr ? track->songName : std::string {};
}

void CustomMusicPlayer::SetCurrentTrack(size_t track)
{
	_playlist.SetCurrent(track);
	Restart();
}

void CustomMusicPlayer::Play()
{
	if (!IsRunning())
	{
		return;
	}
	if (_state == PlayState::Paused && _source.has_value())
	{
		_player.StartSource(*_source);
		_state = PlayState::Playing;
		return;
	}
	if (_state == PlayState::Stopped)
	{
		_state = PlayState::Playing;
		Open();
		return;
	}
	_state = PlayState::Playing;
}

void CustomMusicPlayer::Pause()
{
	if (_state != PlayState::Playing)
	{
		return;
	}
	if (_source.has_value())
	{
		_player.PauseSource(*_source);
	}
	_state = PlayState::Paused;
}

void CustomMusicPlayer::Stop()
{
	Close();
	_state = PlayState::Stopped;
}

void CustomMusicPlayer::NextTrack()
{
	_playlist.Next(_random);
	Restart();
}

void CustomMusicPlayer::PreviousTrack()
{
	_playlist.Previous(_random);
	Restart();
}

uint32_t CustomMusicPlayer::GetPosition() const
{
	if (_open == nullptr || _open->sampleRate == 0)
	{
		return 0;
	}
	auto frames = _startFrame + _playedFrames;
	if (_source.has_value() && !_queued.empty())
	{
		frames += _player.GetSampleOffset(*_source);
	}
	return static_cast<uint32_t>(frames * 1000 / _open->sampleRate);
}

uint32_t CustomMusicPlayer::GetLength() const
{
	if (_open == nullptr || _open->sampleRate == 0)
	{
		return 0;
	}
	return static_cast<uint32_t>(_open->frames * 1000 / _open->sampleRate);
}

void CustomMusicPlayer::SetPosition(uint32_t milliseconds)
{
	if (_open == nullptr || !_source.has_value())
	{
		return;
	}
	const auto frame = std::min<uint64_t>(static_cast<uint64_t>(milliseconds) * _open->sampleRate / 1000, _open->frames);
	_player.StopSource(*_source);
	ReleasePlayed();
	if (drmp3_seek_to_pcm_frame(&_open->decoder, frame) == DRMP3_FALSE)
	{
		return;
	}
	_startFrame = frame;
	_playedFrames = 0;
	_decodedAll = false;
	Refill();
	if (_state == PlayState::Playing)
	{
		_player.StartSource(*_source);
	}
}

void CustomMusicPlayer::SetVolume(uint32_t volume)
{
	_volume = std::min(volume, k_MostVolume);
	if (_source.has_value())
	{
		_player.SetVolume(*_source, Gain());
	}
}

void CustomMusicPlayer::Update()
{
	std::erase_if(_abandoned, [](const auto& future) { return Ready(future); });
	if (Ready(_opening))
	{
		_open = _opening.get();
		if (_open == nullptr)
		{
			const auto* track = _playlist.GetCurrentTrack();
			Log(fmt::format("The custom music track {} could not be opened", track != nullptr ? track->fileName : ""));
			_state = PlayState::Stopped;
			return;
		}
		_source = _player.CreateSource(1.0f, true);
		_startFrame = 0;
		_playedFrames = 0;
		_decodedAll = false;
		Refill();
		if (_state == PlayState::Playing)
		{
			_player.PlaySource(*_source, Gain(), false);
		}
		else
		{
			_player.SetVolume(*_source, Gain());
		}
	}
	if (_open == nullptr || !_source.has_value())
	{
		return;
	}
	ReleasePlayed();
	if (_state != PlayState::Playing)
	{
		return;
	}
	Refill();
	if (_queued.empty() && _decodedAll)
	{
		// The track has played to its end: the next track plays, or the music stops
		if (_playlist.AfterEnd(_random))
		{
			Close();
			Open();
		}
		else
		{
			Stop();
		}
		return;
	}
	// The source ran out of music before more was queued: it plays on
	if (!_queued.empty() && _player.GetStatus(*_source) != AudioStatus::Playing)
	{
		_player.StartSource(*_source);
	}
}

void CustomMusicPlayer::Open()
{
	const auto* track = _playlist.GetCurrentTrack();
	if (track == nullptr)
	{
		_state = PlayState::Stopped;
		return;
	}
	_opening = std::async(std::launch::async, [read = _read, path = track->path]() { return OpenOnThread(read, path); });
}

void CustomMusicPlayer::Close()
{
	if (_opening.valid())
	{
		_abandoned.push_back(std::move(_opening));
	}
	if (_source.has_value())
	{
		_player.StopSource(*_source);
		ReleasePlayed();
		_player.DeleteSource(*_source);
		_source.reset();
	}
	_queued.clear();
	_open.reset();
	_startFrame = 0;
	_playedFrames = 0;
	_decodedAll = false;
}

void CustomMusicPlayer::Restart()
{
	const bool playing = _state != PlayState::Stopped;
	Close();
	if (playing)
	{
		_state = PlayState::Playing;
		Open();
	}
}

void CustomMusicPlayer::Refill()
{
	if (_open == nullptr || !_source.has_value())
	{
		return;
	}
	const auto frames = _open->sampleRate / k_PartsOfASecond;
	std::vector<int16_t> samples;
	while (!_decodedAll && _queued.size() < k_QueuedBuffers)
	{
		samples.resize(static_cast<size_t>(frames) * _open->channels);
		const auto read = drmp3_read_pcm_frames_s16(&_open->decoder, frames, samples.data());
		if (read == 0)
		{
			_decodedAll = true;
			break;
		}
		samples.resize(static_cast<size_t>(read) * _open->channels);
		const auto layout = _open->channels == 1 ? ChannelLayout::Mono : ChannelLayout::Stereo;
		const auto buffer = _player.CreateBuffer(layout, samples, static_cast<int>(_open->sampleRate));
		_player.QueueBuffer(*_source, buffer);
		_queued.emplace_back(buffer, read);
	}
}

void CustomMusicPlayer::ReleasePlayed()
{
	if (!_source.has_value())
	{
		return;
	}
	for (const auto buffer : _player.UnqueueProcessedBuffers(*_source))
	{
		const auto found = std::ranges::find(_queued, buffer, &std::pair<BufferId, uint64_t>::first);
		if (found != _queued.end())
		{
			_playedFrames += found->second;
			_queued.erase(found);
		}
		_player.DeleteBuffer(buffer);
	}
}

float CustomMusicPlayer::Gain() const
{
	return static_cast<float>(_volume) / static_cast<float>(k_MostVolume);
}
