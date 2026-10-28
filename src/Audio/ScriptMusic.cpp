/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "ScriptMusic.h"

#include <charconv>

#include <numeric>
#include <utility>

#include <spdlog/spdlog.h>

using namespace openblack::audio;

namespace
{
/// The frames of sound that a clock at this time has reached, at the sample rate. The music thread counts the time in
/// whole milliseconds.
uint64_t FramesAt(std::chrono::microseconds time, uint32_t sampleRate)
{
	const auto milliseconds = static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::milliseconds>(time).count());
	return milliseconds * sampleRate / 1000;
}
} // namespace

MusicCuePosition openblack::audio::CuePositionAfter(uint32_t startChunk, uint64_t framesHeard)
{
	return {
	    .chunk = startChunk + static_cast<uint32_t>(framesHeard / k_CueChunkFrames),
	    .frame = static_cast<uint32_t>(framesHeard % k_CueChunkFrames),
	};
}

std::vector<std::string_view> openblack::audio::CuesPassed(std::span<const MusicCue> cues, MusicCuePosition from,
                                                           MusicCuePosition to)
{
	std::vector<std::string_view> passed;
	for (const auto& cue : cues)
	{
		const auto at = MusicCuePosition {.chunk = cue.chunk, .frame = cue.frame};
		if (at > from && at <= to)
		{
			passed.emplace_back(cue.text);
		}
	}
	return passed;
}

uint64_t MusicTiming::PieceFrames() const
{
	return std::accumulate(chunkFrames.begin(), chunkFrames.end(), uint64_t {0});
}

MusicPiecePosition openblack::audio::PiecePositionAfter(std::span<const uint32_t> chunkFrames, uint64_t frames)
{
	const auto piece = std::accumulate(chunkFrames.begin(), chunkFrames.end(), uint64_t {0});
	if (piece == 0)
	{
		return {};
	}
	auto left = frames % piece;
	for (uint32_t i = 0; i < chunkFrames.size(); ++i)
	{
		if (left < chunkFrames[i])
		{
			return {.chunk = i + 1, .frame = static_cast<uint32_t>(left)};
		}
		left -= chunkFrames[i];
	}
	return {};
}

void openblack::audio::ApplyMusicCue(MusicLyrics& lyrics, std::string_view cue)
{
	if (cue.empty())
	{
		return;
	}
	switch (cue.front())
	{
	case 'L':
	case 'l':
	{
		// The line's number is read as far as its digits go, and as 0 without any
		auto line = int32_t {0};
		const auto digits = cue.substr(1);
		std::from_chars(digits.data(), digits.data() + digits.size(), line);
		lyrics.line = line;
		lyrics.word = 1;
		break;
	}
	case 'P':
	case 'p':
	case 'W':
	case 'w':
		++lyrics.word;
		break;
	default:
		break;
	}
}

bool openblack::audio::ReachedMusicLine(int32_t lastLine, int32_t line)
{
	return static_cast<uint32_t>(lastLine) >= static_cast<uint32_t>(line);
}

void ScriptMusic::Start(MusicType type)
{
	_type = type;
	// Music a script starts plays from its beginning, even over the script's music already playing
	if (type != MusicType::None)
	{
		_started = false;
	}
}

void ScriptMusic::Turn(std::chrono::microseconds turnTime, const TimingFor& timingFor)
{
	// The music thread plays the channel for the time of the turn
	if (_channel.has_value())
	{
		auto& channel = *_channel;
		channel.time += turnTime;
		while (std::chrono::microseconds(k_Tick) * static_cast<int64_t>(channel.ticks + 1) <= channel.time)
		{
			++channel.ticks;
			if (Tick(channel))
			{
				_channel.reset();
				break;
			}
		}
	}
	// Then the game's music turn starts or stops the script's music
	ProcessScript(timingFor);
}

bool ScriptMusic::Tick(Channel& channel)
{
	const auto& timing = *channel.timing;
	const auto frames = FramesAt(std::chrono::microseconds(k_Tick) * static_cast<int64_t>(channel.ticks), timing.sampleRate);

	// The piece ends when the sound of its last chunk has played. The cues are not checked after that.
	if (channel.loops >= 0 && frames >= timing.PieceFrames() * static_cast<uint64_t>(channel.loops + 1))
	{
		if (const auto logger = spdlog::get("audio"); logger != nullptr)
		{
			SPDLOG_LOGGER_DEBUG(logger, "Script music {} ends after {} ms", GetMusicTypeName(channel.type),
			                    std::chrono::duration_cast<std::chrono::milliseconds>(channel.time).count());
		}
		if (_type == channel.type)
		{
			_type = MusicType::None;
		}
		return true;
	}

	// Only the channel heard passes its cues. A channel that is not heard keeps its clock and catches up later.
	if (channel.current)
	{
		const auto now = CuePositionAfter(1, frames);
		for (const auto cue : CuesPassed(timing.cues, channel.checked, now))
		{
			if (const auto logger = spdlog::get("audio"); logger != nullptr)
			{
				SPDLOG_LOGGER_DEBUG(logger, "Script music {} passes cue {} (chunk {}, frame {})",
				                    GetMusicTypeName(channel.type), cue, now.chunk, now.frame);
			}
			ApplyMusicCue(_lyrics, cue);
		}
		channel.checked = now;
	}

	// A channel that fades out stops when it reaches silence
	if (channel.fadeTicks.has_value())
	{
		--*channel.fadeTicks;
		return *channel.fadeTicks == 0;
	}
	return false;
}

void ScriptMusic::ProcessScript(const TimingFor& timingFor)
{
	if (_type == MusicType::None)
	{
		// The script stopped its music: all the music fades out
		if (_started)
		{
			_started = false;
			if (_channel.has_value() && !_channel->fadeTicks.has_value())
			{
				_channel->fadeTicks = k_FadeTicks;
			}
		}
		return;
	}
	if (_started)
	{
		return;
	}
	// A piece without a bank does not start and is never over
	auto timing = timingFor ? timingFor(_type) : nullptr;
	if (!timing || timing->chunkFrames.empty() || timing->sampleRate == 0)
	{
		// Other music then takes over, and the channel of an earlier piece fades out
		if (_channel.has_value() && _channel->type != _type)
		{
			_channel->current = false;
			if (!_channel->fadeTicks.has_value())
			{
				_channel->fadeTicks = k_FadeTicks;
			}
		}
		return;
	}
	_started = true;
	++_playCount;
	// A channel that still plays the piece continues from where it is, and is the music heard again
	if (_channel.has_value() && _channel->type == _type)
	{
		_channel->current = true;
		_channel->fadeTicks.reset();
		return;
	}
	// The piece plays once through, unless the header of its bank loops it
	const auto loops = timing->loopOverride.value_or(0);
	_channel = Channel {.type = _type, .timing = std::move(timing), .loops = loops};
}

std::optional<MusicPiecePosition> ScriptMusic::GetPlace() const
{
	if (!_channel.has_value())
	{
		return std::nullopt;
	}
	const auto& timing = *_channel->timing;
	return PiecePositionAfter(timing.chunkFrames, FramesAt(_channel->time, timing.sampleRate));
}

std::optional<std::chrono::microseconds> ScriptMusic::GetChannelTime() const
{
	return _channel.has_value() ? std::optional(_channel->time) : std::nullopt;
}

std::array<int64_t, 5> ScriptMusic::SyncedValues() const
{
	return {static_cast<int64_t>(_type), _started ? 1 : 0, _lyrics.line, _lyrics.word,
	        _channel.has_value() ? _channel->time.count() : -1};
}

void ScriptMusic::Restore(MusicType type, bool started, MusicLyrics lyrics)
{
	_type = type;
	_started = started;
	_lyrics = lyrics;
	_channel.reset();
}
