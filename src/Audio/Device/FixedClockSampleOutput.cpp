/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "FixedClockSampleOutput.h"

#include <cmath>

#include <algorithm>
#include <utility>

#include "Device.h"
#include "Sound.h"

namespace openblack::audio
{

void FixedClockSampleOutput::Timeline::Start(uint32_t now, double length, float ratio, int passes)
{
	playing = length > 0.0;
	startTick = now;
	playedMs = 0.0;
	lengthMs = length;
	pitch = ratio > 0.0f ? ratio : 1.0f;
	loops = passes;
}

double FixedClockSampleOutput::Timeline::Elapsed(uint32_t now) const
{
	// the wave's ms: the ticks since the last change times the pitch, plus what was played before it
	return playedMs + static_cast<double>(now - startTick) * static_cast<double>(pitch);
}

bool FixedClockSampleOutput::Timeline::Sounding(uint32_t now) const
{
	if (!playing)
	{
		return false;
	}
	if (loops < 0)
	{
		return true; // for ever, until Stop or ReleaseLoop
	}
	return Elapsed(now) < lengthMs * static_cast<double>(loops + 1);
}

int64_t FixedClockSampleOutput::Timeline::PositionMs(uint32_t now) const
{
	if (!Sounding(now))
	{
		return -1;
	}
	return static_cast<int64_t>(std::fmod(Elapsed(now), lengthMs));
}

void FixedClockSampleOutput::Timeline::ChangePitch(uint32_t now, float ratio)
{
	playedMs = Elapsed(now);
	startTick = now;
	pitch = ratio > 0.0f ? ratio : pitch;
}

void FixedClockSampleOutput::Timeline::ReleaseLoop(uint32_t now)
{
	// QSWaveMixStopChannel(0x1000): the remaining loops go to 0, the current pass ends. Only a sample still sounding with
	// loops left, and loops only go down: a sample that has ended is never brought back
	if (Sounding(now) && loops != 0 && lengthMs > 0.0)
	{
		const int current = static_cast<int>(Elapsed(now) / lengthMs);
		loops = loops < 0 ? current : std::min(loops, current);
	}
}

FixedClockSampleOutput::FixedClockSampleOutput(std::unique_ptr<SampleOutput> real)
    : _real(std::move(real))
{
}

FixedClockSampleOutput::Timeline* FixedClockSampleOutput::At(size_t channel)
{
	if (channel >= _channels.size())
	{
		_channels.resize(channel + 1);
	}
	return &_channels[channel];
}

const FixedClockSampleOutput::Timeline* FixedClockSampleOutput::At(size_t channel) const
{
	return channel < _channels.size() ? &_channels[channel] : nullptr;
}

bool FixedClockSampleOutput::Play(size_t channel, Sound& sound, const Start& start)
{
	const bool played = _real->Play(channel, sound, start);
	auto* timeline = At(channel);
	// the real Play decodes the wave first (wave_buffers::Get sets Sound::duration): one pass in ms
	timeline->Start(device::TickCount(), played ? static_cast<double>(sound.duration) * 1000.0 : 0.0, start.pitch, start.loops);
	return played;
}

void FixedClockSampleOutput::Stop(size_t channel)
{
	_real->Stop(channel);
	At(channel)->playing = false;
}

void FixedClockSampleOutput::StopRamped(size_t channel)
{
	_real->StopRamped(channel);
	At(channel)->playing = false;
}

void FixedClockSampleOutput::CutFade(size_t channel)
{
	_real->CutFade(channel);
}

int64_t FixedClockSampleOutput::PlayPositionMs(size_t channel) const
{
	const auto* timeline = At(channel);
	return timeline != nullptr ? timeline->PositionMs(device::TickCount()) : -1;
}

bool FixedClockSampleOutput::Playing(size_t channel) const
{
	const auto* timeline = At(channel);
	return timeline != nullptr && timeline->Sounding(device::TickCount());
}

void FixedClockSampleOutput::SetGain(size_t channel, float gain)
{
	_real->SetGain(channel, gain);
}

void FixedClockSampleOutput::SetPitch(size_t channel, float ratio)
{
	_real->SetPitch(channel, ratio);
	At(channel)->ChangePitch(device::TickCount(), ratio);
}

void FixedClockSampleOutput::SetPosition(size_t channel, glm::vec3 position)
{
	_real->SetPosition(channel, position);
}

void FixedClockSampleOutput::ReleaseLoop(size_t channel)
{
	_real->ReleaseLoop(channel);
	At(channel)->ReleaseLoop(device::TickCount());
}

void FixedClockSampleOutput::SetListener(glm::vec3 position)
{
	_real->SetListener(position);
}

void FixedClockSampleOutput::Update()
{
	_real->Update();
}

size_t FixedClockSampleOutput::Sources() const
{
	return _real->Sources();
}

void FixedClockSampleOutput::DeleteAll()
{
	_real->DeleteAll();
	for (auto& timeline : _channels)
	{
		timeline.playing = false;
	}
}

} // namespace openblack::audio
