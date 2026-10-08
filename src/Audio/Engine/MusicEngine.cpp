/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "MusicEngine.h"

#include <algorithm>

namespace openblack::audio
{

namespace
{
// Mixer volume of one channel: ((cur * sad * 258) / 127) * mainVolume / 127 in unsigned 32-bit arithmetic
uint32_t MixerVolume(int current, int sadVolume, uint32_t mainVolume)
{
	const auto v = static_cast<uint32_t>(current) * static_cast<uint32_t>(sadVolume);
	const uint32_t scaled = (v * 258u) / 127u;
	return (scaled * mainVolume) / 127u;
}

// The marker clock: (chunk << 32) | sample, compared as 64-bit values
int64_t MarkerPosition(int chunk, uint32_t sample)
{
	return (static_cast<int64_t>(chunk) << 32) | static_cast<int64_t>(sample);
}
} // namespace

MusicEngine::MusicEngine(IMusicSink& sink)
    : _sink(sink)
{
	_sink.SetListener(this);
	// Every channel free with sad volume 127, and its mixer channel enabled in 2D
	for (int i = 0; i < k_MusicChannelCount; ++i)
	{
		_sink.EnableChannel(i, false, {}, glm::vec3(0.0f));
	}
	_installed = true;
	_active = true;
}

MusicEngine::~MusicEngine()
{
	_sink.SetListener(nullptr);
}

void MusicEngine::NoteBankRegistered(const MusicBank& bank)
{
	// The highest music group id of the registered banks
	if (bank.IsMusic() && bank.GetGroupId() >= 0)
	{
		_totalGroups = std::max(_totalGroups, static_cast<uint32_t>(bank.GetGroupId()));
	}
}

uint32_t MusicEngine::GetTotalGroups() const
{
	// 0 when not installed
	return _installed ? _totalGroups : 0;
}

uint32_t MusicEngine::GetMixerVolume(int channel) const
{
	const auto& ch = _channels[static_cast<size_t>(channel)];
	return MixerVolume(ch.current, ch.sadVolume, _mainVolume);
}

void MusicEngine::ApplyVolume(int channel)
{
	_sink.SetVolume(channel, GetMixerVolume(channel));
}

int MusicEngine::Play(const MusicPlayOptions& options)
{
	// Not installed -> no channel; no bank or not a music bank -> Stop(1) and no channel
	if (!_installed)
	{
		return k_NoMusicChannel;
	}
	if (options.bank == nullptr || !options.bank->IsMusic())
	{
		Stop(1);
		return k_NoMusicChannel;
	}

	// The channel already playing this bank is "re-triggered"
	for (int i = 0; i < k_MusicChannelCount; ++i)
	{
		auto& ch = _channels[static_cast<size_t>(i)];
		if (ch.bank != options.bank)
		{
			continue;
		}
		if (_mainChannel == i)
		{
			// The main channel takes the new fade, target and sync and plays again
			ch.fade = options.fade;
			ch.target = options.volume;
			ch.sync = options.sync;
			ch.status = MusicStatus::Playing;
		}
		else if (ch.isNew == 0)
		{
			// An older channel becomes the main channel (the others fade out)
			ch.fade = options.fade;
			ch.target = options.volume;
			ch.sync = options.sync;
			_mainChannel = i;
		}
		// a channel that has not queued its first chunk yet is left as it is
		return i;
	}

	// The first free channel (no bank), or none if all 6 are busy
	int index = k_NoMusicChannel;
	for (int i = 0; i < k_MusicChannelCount; ++i)
	{
		if (_channels[static_cast<size_t>(i)].bank == nullptr)
		{
			index = i;
			break;
		}
	}
	if (index == k_NoMusicChannel)
	{
		return k_NoMusicChannel;
	}

	auto& ch = _channels[static_cast<size_t>(index)];
	auto& bank = *options.bank;
	ch.target = options.volume;
	ch.sadVolume = k_MusicMaxVolume;
	ch.fade = options.fade;
	ch.current = options.fade == 0 ? options.volume : 0; // no fade = at the volume at once
	ch.pitch = options.pitch;
	ch.bank = options.bank;
	ch.group = bank.GetGroupId();
	ch.syncSample = 0;
	ch.loops = options.loops;
	ch.isNew = 1;
	ch.is3D = options.is3D;
	ch.field18 = options.field08;
	ch.field1C = options.field0C;
	ch.field20 = options.field10;
	ch.chunkCount = static_cast<uint32_t>(bank.GetSegmentCount());
	ch.savedStatus = MusicStatus::Free;
	ch.nextChunk = 1;
	ch.playingChunk = 1;
	ch.sadVolume = bank.GetVolume();         // the .sad's volume with flag 0x20
	ch.loops = bank.GetLoops(options.loops); // the .sad's loops with flag 0x40
	ch.sampleRate = bank.GetSampleRate();

	// The mixer channel, 3D with the bank's distances over the options' or 2D
	if (options.is3D == 1)
	{
		_sink.EnableChannel(index, true, bank.GetDistanceMapping(options.distance), options.position);
	}
	else
	{
		_sink.EnableChannel(index, false, options.distance, options.position);
	}

	ch.sync = options.sync != 0 ? 1 : 0;
	ch.startChunk = options.startChunk;
	ch.status = MusicStatus::Playing;
	ch.resetDecoder = true; // the next read starts a fresh decoder
	ch.finished = options.finished;
	ch.marker = options.marker;
	ch.userData = options.userData;

	// With a marker callback, a new marker list whose clock starts at max(startChunk, 1)
	if (options.marker)
	{
		ch.markers.reset();
		auto list = std::make_unique<MusicMarkerList>();
		list->nodes = bank.ParseMarkers();
		list->cursor = 0;
		list->previous = 0;
		list->startTick = 0;
		list->chunkBase = options.startChunk > 1 ? options.startChunk : 1;
		ch.markers = std::move(list);
	}
	return index;
}

void MusicEngine::Cut(int channel)
{
	auto& ch = _channels[static_cast<size_t>(channel)];
	ch.bank = nullptr;
	ch.status = MusicStatus::Free;
	_sink.SetVolume(channel, 0);
	if (!_sink.IsChannelDone(channel))
	{
		_sink.FlushChannel(channel);
	}
	ch.target = 0;
}

void MusicEngine::Stop(int fade)
{
	// Needs installed, active and the wave system
	if (!IsUsable())
	{
		return;
	}
	for (int i = 0; i < k_MusicChannelCount; ++i)
	{
		auto& ch = _channels[static_cast<size_t>(i)];
		if (ch.status == MusicStatus::Free)
		{
			continue;
		}
		if (fade == 1)
		{
			ch.target = 0; // the thread fades it out
			continue;
		}
		Cut(i);
		if (_mainChannel == i)
		{
			_mainChannel = k_NoMusicChannel;
		}
	}
}

void MusicEngine::Stop(int channel, int fade)
{
	if (!IsUsable() || !IsValid(channel))
	{
		return;
	}
	if (_channels[static_cast<size_t>(channel)].status == MusicStatus::Free)
	{
		return;
	}
	if (_mainChannel == channel)
	{
		Stop(fade); // the main channel stops all of them
		return;
	}
	if (fade == 1)
	{
		return; // a fade of a channel that is not the main channel does nothing (it is already fading out)
	}
	Cut(channel); // the main channel's index is not touched here
}

int MusicEngine::FindChannelOfBank(const MusicBank* bank) const
{
	if (!IsUsable())
	{
		return k_NoMusicChannel;
	}
	for (int i = 0; i < k_MusicChannelCount; ++i)
	{
		if (_channels[static_cast<size_t>(i)].bank == bank)
		{
			return i;
		}
	}
	return k_NoMusicChannel;
}

MusicStatus MusicEngine::GetStatus(int channel) const
{
	if (!IsUsable() || !IsValid(channel))
	{
		return MusicStatus::Free;
	}
	return _channels[static_cast<size_t>(channel)].status;
}

uint32_t MusicEngine::GetCurrentChunk(int channel) const
{
	// Only needs installed
	if (!_installed || !IsValid(channel))
	{
		return 0;
	}
	return _channels[static_cast<size_t>(channel)].playingChunk;
}

int MusicEngine::GetMainChannel() const
{
	if (!_installed)
	{
		return k_NoMusicChannel;
	}
	return _mainChannel;
}

void MusicEngine::SetMainVolume(uint32_t volume)
{
	// Same value -> nothing; above 127 -> 127 (unsigned compare); reapplied to the channels at status 1 or 4
	if (!IsUsable() || volume == _mainVolume)
	{
		return;
	}
	_mainVolume = std::min<uint32_t>(volume, k_MusicMaxVolume);
	for (int i = 0; i < k_MusicChannelCount; ++i)
	{
		const auto status = _channels[static_cast<size_t>(i)].status;
		if (status == MusicStatus::Playing || status == MusicStatus::LastQueued)
		{
			ApplyVolume(i);
		}
	}
}

int MusicEngine::GetMainVolume() const
{
	if (!IsUsable())
	{
		return -1;
	}
	return static_cast<int>(_mainVolume);
}

void MusicEngine::SetPitch(int channel, uint32_t pitch)
{
	// Same pitch -> nothing; clamped to 50..250 (unsigned); SetFrequency(Hz * pitch / 100)
	if (!IsUsable() || !IsValid(channel))
	{
		return;
	}
	auto& ch = _channels[static_cast<size_t>(channel)];
	if (pitch == static_cast<uint32_t>(ch.pitch))
	{
		return;
	}
	ch.pitch = static_cast<int>(std::clamp<uint32_t>(pitch, k_MusicMinPitch, k_MusicMaxPitch));
	_sink.SetFrequency(channel, ch.sampleRate * static_cast<uint32_t>(ch.pitch) / 100u);
}

void MusicEngine::Set3DPosition(int channel, glm::vec3 position)
{
	// Only needs installed; a channel with a bank in 3D
	if (!_installed || !IsValid(channel))
	{
		return;
	}
	const auto& ch = _channels[static_cast<size_t>(channel)];
	if (ch.bank != nullptr && ch.is3D == 1)
	{
		_sink.SetSourcePosition(channel, position);
	}
}

void MusicEngine::Pause()
{
	// Every channel paused, its status kept in savedStatus and set to Paused
	if (!IsUsable())
	{
		return;
	}
	for (int i = 0; i < k_MusicChannelCount; ++i)
	{
		auto& ch = _channels[static_cast<size_t>(i)];
		_sink.PauseChannel(i);
		ch.savedStatus = ch.status;
		ch.status = MusicStatus::Paused;
	}
}

void MusicEngine::Restart()
{
	// Every channel restarted with its saved status
	if (!IsUsable())
	{
		return;
	}
	for (int i = 0; i < k_MusicChannelCount; ++i)
	{
		auto& ch = _channels[static_cast<size_t>(i)];
		_sink.RestartChannel(i);
		ch.status = ch.savedStatus;
	}
}

void MusicEngine::Switch(uint32_t on)
{
	// Installed and the wave system; 0 -> Stop(0) then inactive; 1 -> active
	if (!_installed)
	{
		return;
	}
	if (on == 0)
	{
		Stop(0);
		_active = false;
	}
	else if (on == 1)
	{
		_active = true;
	}
}

void MusicEngine::Close()
{
	// Installed and the wave system
	if (!_installed)
	{
		return;
	}
	for (int i = 0; i < k_MusicChannelCount; ++i)
	{
		_channels[static_cast<size_t>(i)].target = 0;
		_sink.SetVolume(i, 0);
		if (!_sink.IsChannelDone(i))
		{
			_sink.FlushChannel(i);
		}
	}
	_installed = false;
}

void MusicEngine::OnChunkDone(int channel, bool last)
{
	// The wave is freed, the queued count drops; the last chunk ends the channel (status 3), any other moves the
	// audible chunk on (back to 1 after the last segment)
	if (!IsValid(channel))
	{
		return;
	}
	auto& ch = _channels[static_cast<size_t>(channel)];
	--_queued[static_cast<size_t>(channel)];
	if (last)
	{
		ch.status = MusicStatus::Finished;
		return;
	}
	if (ch.playingChunk == ch.chunkCount)
	{
		ch.playingChunk = 1;
	}
	else
	{
		++ch.playingChunk;
	}
}

void MusicEngine::DispatchMarkers(MusicChannel& channel, uint32_t nowMs)
{
	auto& list = *channel.markers;
	if (list.cursor >= list.nodes.size())
	{
		return;
	}
	// samples played since the first chunk: truncate((now - start) * Hz * 0.001f), here in double (inferred: the same
	// result if the original's FPU runs at the default 53-bit precision)
	const uint32_t elapsed = nowMs - list.startTick;
	const auto hz = static_cast<double>(static_cast<int32_t>(channel.sampleRate));
	const auto samples = static_cast<int32_t>(static_cast<double>(elapsed) * hz * static_cast<double>(0.001f));
	// chunk = samples / samples per segment + chunkBase, sample = the remainder
	const int chunk = samples / k_MusicSamplesPerSegment + list.chunkBase;
	const auto sample = static_cast<uint32_t>(samples % k_MusicSamplesPerSegment);
	const int64_t position = MarkerPosition(chunk, sample);

	for (size_t i = list.cursor; i < list.nodes.size(); ++i)
	{
		const auto& node = list.nodes[i];
		if (node.chunk < chunk)
		{
			list.cursor = i; // the cursor moves to the last node of an earlier chunk
		}
		if (node.chunk > chunk)
		{
			break;
		}
		const int64_t nodePosition = MarkerPosition(node.chunk, static_cast<uint32_t>(node.sample));
		// fired when previous < node <= now
		if (nodePosition <= position && nodePosition > list.previous && channel.marker)
		{
			channel.marker(node.label);
		}
	}
	list.previous = position;
}

bool MusicEngine::QueueChunks(int index, uint32_t nowMs)
{
	auto& ch = _channels[static_cast<size_t>(index)];
	auto& queued = _queued[static_cast<size_t>(index)];
	// Playing, active and fewer than 4 queued; the loop goes back to the active check while fewer than 4 are queued
	if (ch.status != MusicStatus::Playing || !_active || queued == k_MusicQueueDepth)
	{
		return true;
	}
	while (_active)
	{
		int other = k_NoMusicChannel;
		if (ch.isNew != 0)
		{
			// With sync, the first other channel playing (status 1) in the same group (not -1) gives the chunk;
			// otherwise the start chunk if it is 1..n (unsigned compare), else 1
			if (ch.sync == 1)
			{
				for (int j = 0; j < k_MusicChannelCount; ++j)
				{
					const auto& o = _channels[static_cast<size_t>(j)];
					if (j != index && o.status == MusicStatus::Playing && o.group == ch.group && o.group != -1)
					{
						ch.nextChunk = o.playingChunk;
						if (ch.markers)
						{
							ch.markers->chunkBase = static_cast<int>(o.playingChunk);
						}
						other = j;
						break;
					}
				}
			}
			if (other == k_NoMusicChannel)
			{
				const auto start = static_cast<uint32_t>(ch.startChunk);
				ch.nextChunk = (start <= ch.chunkCount && start != 0) ? start : 1;
			}
			ch.playingChunk = ch.nextChunk;
		}

		// (approximate) The original does not look at the status again inside this loop: after the last chunk
		// (status 4) with fewer than 4 queued, or when a main channel at status 4 is re-triggered to 1, it would read the
		// sample record n, past the end of the table. Here the end stays the end.
		if (ch.nextChunk == 0 || ch.nextChunk > ch.chunkCount)
		{
			ch.status = queued == 0 ? MusicStatus::Finished : MusicStatus::LastQueued;
			return true;
		}

		// The segment nextChunk - 1; past the end, loops == 0 makes it the last chunk (status 4), otherwise one loop
		// less and back to chunk 1
		const size_t segment = ch.nextChunk - 1;
		++ch.nextChunk;
		bool last = false;
		if (ch.nextChunk > ch.chunkCount)
		{
			if (ch.loops == 0)
			{
				last = true;
				ch.status = MusicStatus::LastQueued;
			}
			else
			{
				--ch.loops;
				ch.nextChunk = 1;
			}
		}

		// Read the segment's bytes from the open bank file; a failure ends the pass
		if (!ch.bank->ReadSegment(segment, _readBuffer))
		{
			return false;
		}

		// The first chunk of a synced channel starts at the other channel's play position, and the channel becomes the
		// main channel
		uint32_t startSample = 0;
		if (ch.isNew != 0)
		{
			ch.isNew = 0;
			if (ch.sync == 1 && other != k_NoMusicChannel &&
			    _channels[static_cast<size_t>(other)].status == MusicStatus::Playing)
			{
				ch.syncSample = _sink.GetPlayPosition(other);
				startSample = ch.syncSample;
			}
			_mainChannel = index;
		}
		else
		{
			ch.syncSample = 0;
		}

		++queued;
		if (ch.is3D == 0)
		{
			_sink.SetCentred(index);
		}
		_sink.SetFrequency(index, ch.sampleRate * static_cast<uint32_t>(ch.pitch) / 100u);
		ApplyVolume(index);
		if (ch.markers && ch.markers->startTick == 0)
		{
			ch.markers->startTick = nowMs;
		}
		const bool resetDecoder = ch.resetDecoder;
		ch.resetDecoder = false;
		_sink.QueueChunk(index, _readBuffer, resetDecoder, startSample, last);

		if (queued == k_MusicQueueDepth)
		{
			break;
		}
	}
	return true;
}

bool MusicEngine::Process(uint32_t nowMs)
{
	for (int i = 0; i < k_MusicChannelCount; ++i)
	{
		auto& ch = _channels[static_cast<size_t>(i)];

		// The main channel's markers
		if ((ch.status == MusicStatus::Playing || ch.status == MusicStatus::LastQueued) && _mainChannel == i && _active &&
		    ch.markers)
		{
			DispatchMarkers(ch, nowMs);
		}

		// A finished channel is freed and calls back
		if (ch.status == MusicStatus::Finished)
		{
			ch.bank = nullptr;
			ch.status = MusicStatus::Free;
			if (ch.finished)
			{
				ch.finished(ch.userData);
			}
			_sink.SetVolume(i, 0);
			if (!_sink.IsChannelDone(i))
			{
				_sink.FlushChannel(i);
			}
			ch.target = 0;
			if (_mainChannel == i)
			{
				_mainChannel = k_NoMusicChannel;
			}
		}

		if (!QueueChunks(i, nowMs))
		{
			return false;
		}

		// Fades, for the channels at status 1 or 4 while active
		if ((ch.status != MusicStatus::Playing && ch.status != MusicStatus::LastQueued) || !_active)
		{
			continue;
		}
		if (ch.current < ch.target && _mainChannel == i)
		{
			if (ch.fade == 0)
			{
				ch.current = ch.target; // straight to the target, once
				ApplyVolume(i);
				ch.fade = 1;
			}
			else
			{
				ch.current = std::min(ch.current + k_MusicFadeInStep, ch.target);
				ApplyVolume(i);
			}
		}
		else if (ch.current > ch.target || _mainChannel != i)
		{
			// Towards 0, not towards the target
			ch.current = std::max(ch.current - k_MusicFadeOutStep, 0);
			ApplyVolume(i);
			if (ch.current == 0)
			{
				if (_mainChannel == i)
				{
					_mainChannel = k_NoMusicChannel;
				}
				ch.status = MusicStatus::Free;
				ch.bank = nullptr;
				if (!_sink.IsChannelDone(i))
				{
					_sink.FlushChannel(i);
				}
			}
		}
	}
	return true;
}

} // namespace openblack::audio
