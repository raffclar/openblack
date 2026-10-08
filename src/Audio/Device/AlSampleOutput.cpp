/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "AlSampleOutput.h"

#include <chrono>
#include <optional>
#include <vector>

#include <glm/geometric.hpp>

#include "Audio/Device/Device.h"
#include "Audio/Device/OutputSwitches.h"
#include "Audio/Device/Sound.h"
#include "Audio/Device/WaveBuffers.h"

using namespace openblack::audio;

namespace
{
/// A fade's gain steps and flush, on the device
class DeviceFadeTarget final: public RampedStops::Target
{
public:
	void SetGain(RampedStops::SourceId source, float gain) override { device::SetSourceGain(source, gain); }
	void Flush(RampedStops::SourceId source) override
	{
		device::StopSource(source);
		device::SetSourceBuffer(source, 0);
	}
};
} // namespace

AlSampleOutput::~AlSampleOutput()
{
	// the fader thread ends first, so that no fade touches a source once they are deleted
	if (_fader.joinable())
	{
		_fader.request_stop();
		_fader.join();
	}
	DeleteAll();
}

bool AlSampleOutput::Play(size_t channel, Sound& sound, const Start& start)
{
	if (channel >= _slots.size() || !device::IsOpen())
	{
		return false;
	}
	const BufferId buffer = wave_buffers::Get(sound);
	if (buffer == 0)
	{
		return false;
	}
	auto& slot = _slots[channel];
	if (slot.source == 0)
	{
		// the channel's former source may still be fading out: a spare one, or a new one
		std::optional<RampedStops::SourceId> spare;
		{
			const std::lock_guard lock(_fadeMutex);
			spare = _stops.TakeSpare();
		}
		slot.source = spare.has_value() ? *spare : device::CreateSource();
	}
	const auto source = slot.source;
	device::StopSource(source);
	device::SetSourceBuffer(source, 0);
	device::SetSourceBuffer(source, buffer);
	device::SetSourcePitch(source, start.pitch);
	slot.is3D = start.is3D;
	slot.relative = start.relative || !start.is3D;
	slot.position = start.is3D ? start.position : glm::vec3(0.0f);
	slot.maxDistance = start.maxDistance;
	slot.gain = start.gain;
	device::SetSourceRelative(source, slot.relative);
	if (start.is3D)
	{
		device::SetSourceDistance(source, start.minDistance, start.maxDistance, start.scale);
	}
	else
	{
		// 2D: no position and no distance mapping
		device::SetSourceRolloff(source, 0.0f);
	}
	device::SetSourcePosition(source, slot.position);
	slot.looping = start.loops != 0;
	device::SetSourceLooping(source, slot.looping);
	slot.loop.Start(start.loops);
	ApplyGain(slot);
	device::PlaySource(source);
	return true;
}

void AlSampleOutput::Stop(size_t channel)
{
	if (channel < _slots.size() && _slots[channel].source != 0)
	{
		device::StopSource(_slots[channel].source);
		_slots[channel].loop.Start(0);
	}
}

void AlSampleOutput::StopRamped(size_t channel)
{
	if (channel >= _slots.size() || _slots[channel].source == 0 || !Playing(channel))
	{
		Stop(channel);
		return;
	}
	// The original mixer ramps the volume to 0 in 20 ms, then the channel is flushed, and its caller waits those 20 ms.
	// Here the game does not wait: the channel gives its source to the fades and is stopped and free at once (Playing is
	// false, the next start takes another source), while the source goes down in four gain steps of 5 ms (OpenAL Soft
	// smooths each gain change; OpenAL has no volume ramp) on the fader thread, then is flushed. It sounds the same; only
	// the moment of the cut follows the fader thread's wake-ups, a few ms either way.
	auto& slot = _slots[channel];
	const float gain = AppliedGain(slot);
	slot.loop.Start(0);
	slot.looping = false;
	{
		const std::lock_guard lock(_fadeMutex);
		DeviceFadeTarget target;
		_stops.Begin(channel, slot.source, gain, RampedStops::Clock::now(), target);
		if (!_fader.joinable())
		{
			_fader = std::jthread([this](const std::stop_token& stop) { FaderMain(stop); });
		}
	}
	slot.source = 0;
	_fadeWake.notify_one();
}

void AlSampleOutput::CutFade(size_t channel)
{
	const std::lock_guard lock(_fadeMutex);
	DeviceFadeTarget target;
	_stops.Cut(channel, target);
}

void AlSampleOutput::FaderMain(const std::stop_token& stop)
{
	DeviceFadeTarget target;
	std::unique_lock lock(_fadeMutex);
	while (!stop.stop_requested())
	{
		const auto next = _stops.Advance(RampedStops::Clock::now(), target);
		if (next.has_value())
		{
			// a new fade's first step is due 5 ms after it began, never before the next one already waiting
			_fadeWake.wait_until(lock, stop, *next, [] { return false; });
		}
		else
		{
			_fadeWake.wait(lock, stop, [this] { return _stops.Fading() > 0; });
		}
	}
}

int64_t AlSampleOutput::PlayPositionMs(size_t channel) const
{
	if (!Playing(channel))
	{
		return -1;
	}
	const float seconds = device::SourceSecondOffset(_slots[channel].source);
	return static_cast<int64_t>(seconds * 1000.0f);
}

bool AlSampleOutput::Playing(size_t channel) const
{
	if (channel >= _slots.size() || _slots[channel].source == 0)
	{
		return false;
	}
	const auto state = device::SourceStatus(_slots[channel].source);
	return state == AudioStatus::Playing || state == AudioStatus::Paused;
}

void AlSampleOutput::SetGain(size_t channel, float gain)
{
	if (channel < _slots.size() && _slots[channel].source != 0)
	{
		_slots[channel].gain = gain;
		ApplyGain(_slots[channel]);
	}
}

void AlSampleOutput::SetPitch(size_t channel, float ratio)
{
	if (channel < _slots.size() && _slots[channel].source != 0)
	{
		device::SetSourcePitch(_slots[channel].source, ratio);
	}
}

void AlSampleOutput::SetPosition(size_t channel, glm::vec3 position)
{
	if (channel >= _slots.size() || _slots[channel].source == 0 || !_slots[channel].is3D)
	{
		return;
	}
	auto& slot = _slots[channel];
	slot.position = position;
	device::SetSourcePosition(slot.source, position);
	ApplyGain(slot);
}

void AlSampleOutput::ReleaseLoop(size_t channel)
{
	if (channel < _slots.size() && _slots[channel].source != 0)
	{
		auto& slot = _slots[channel];
		slot.looping = false;
		slot.loop.Start(0);
		device::SetSourceLooping(slot.source, false);
	}
}

void AlSampleOutput::SetListener(glm::vec3 position)
{
	_listener = position;
	for (auto& slot : _slots)
	{
		if (slot.source != 0 && slot.is3D)
		{
			ApplyGain(slot);
		}
	}
}

void AlSampleOutput::Update()
{
	// The debug Audio Player window's sound switch, turned since the last frame: every channel gets its gain again
	if (const bool on = GetOutputSwitches().sounds; on != _soundsOn)
	{
		_soundsOn = on;
		for (auto& slot : _slots)
		{
			if (slot.source != 0)
			{
				ApplyGain(slot);
			}
		}
	}
	for (auto& slot : _slots)
	{
		if (slot.source == 0 || !slot.looping || slot.loop.remaining <= 0)
		{
			continue;
		}
		const auto offset = device::SourceSampleOffset(slot.source);
		if (slot.loop.Feed(offset))
		{
			slot.looping = false;
			device::SetSourceLooping(slot.source, false);
		}
	}
}

size_t AlSampleOutput::Sources() const
{
	size_t count = 0;
	for (const auto& slot : _slots)
	{
		count += slot.source != 0 ? 1 : 0;
	}
	// the sources still fading out and the spares count too
	const std::lock_guard lock(_fadeMutex);
	return count + _stops.Fading() + _stops.Spares();
}

void AlSampleOutput::DeleteAll()
{
	const bool context = device::IsOpen();
	for (auto& slot : _slots)
	{
		if (slot.source != 0 && context)
		{
			device::StopSource(slot.source);
			device::DeleteSource(slot.source);
		}
		slot = {};
	}
	// the fades still sounding are cut at once, with the spares
	std::vector<RampedStops::SourceId> held;
	{
		const std::lock_guard lock(_fadeMutex);
		held = _stops.TakeAll();
	}
	for (const auto source : held)
	{
		if (context)
		{
			device::StopSource(source);
			device::DeleteSource(source);
		}
	}
}

float AlSampleOutput::AppliedGain(const Slot& slot) const
{
	// as the original mixer: a 3D channel farther than the max of its distance mapping is silent (it keeps playing)
	bool muted = false;
	if (slot.is3D)
	{
		const float distance = slot.relative ? glm::length(slot.position) : glm::distance(slot.position, _listener);
		muted = distance > slot.maxDistance;
	}
	return SwitchedGain(GetOutputSwitches().sounds, muted ? 0.0f : slot.gain);
}

void AlSampleOutput::ApplyGain(Slot& slot) const
{
	device::SetSourceGain(slot.source, AppliedGain(slot));
}
