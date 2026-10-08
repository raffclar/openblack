/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "MusicStream.h"

#include <cmath>
#include <cstdlib>

#include <algorithm>
#include <array>
#include <chrono>
#include <span>
#include <string>
#include <string_view>

#include <fmt/format.h>
#include <glm/geometric.hpp>
#include <spdlog/spdlog.h>

#include "Audio/Codec/MpegAudio.h"
#include "Audio/Device/Device.h"
#include "Audio/Device/OutputSwitches.h"
#include "Audio/Game/BankTables.h"
#include "Audio/Game/Banks.h"
#include "Debug/DebugEnv.h"
#include "ECS/Systems/AudioStateInterface.h"
#include "EngineConfig.h"
#include "Locator.h"

namespace openblack::audio
{

namespace
{
// QMixer keeps a channel volume as vol / 32767 (as sample_play::QMixerGain)
constexpr float k_QMixerVolumeScale = 32767.0f;

// QMixer's distance law for channel flags 0x110 / 0x111 (no 0x800 clamp, no 0x1000 linear; as
// sample_play::DistanceGain): 1 up to min or with scale 0, min / ((d - min) * scale + min) up to max, 0 beyond max
float DistanceGain(const MusicDistanceMapping& mapping, float distance)
{
	if (distance > mapping.maxDistance)
	{
		return 0.0f;
	}
	if (distance <= mapping.minDistance || mapping.scale == 0.0f)
	{
		return 1.0f;
	}
	return mapping.minDistance / ((distance - mapping.minDistance) * mapping.scale + mapping.minDistance);
}

std::shared_ptr<spdlog::logger> Logger()
{
	// The "audio" logger exists in the game, not in the tests: a logger without sinks drops the lines there
	if (auto logger = spdlog::get("audio"))
	{
		return logger;
	}
	static const auto k_Silent = std::make_shared<spdlog::logger>("music-silent");
	return k_Silent;
}

const char* StatusName(MusicStatus status)
{
	switch (status)
	{
	case MusicStatus::Free:
		return "free";
	case MusicStatus::Playing:
		return "playing";
	case MusicStatus::Paused:
		return "paused";
	case MusicStatus::Finished:
		return "finished";
	case MusicStatus::LastQueued:
		return "last";
	}
	return "?";
}
} // namespace

// ---- MusicSegmentDecoder -------------------------------------------------------------------------------------------

struct MusicSegmentDecoder::State
{
	codec::MpegFrameDecoder decoder;
	std::array<int16_t, codec::MpegFrameDecoder::k_MaxSamples> frame {};
};

MusicSegmentDecoder::MusicSegmentDecoder()
    : _state(std::make_unique<State>())
{
	Reset();
}

MusicSegmentDecoder::~MusicSegmentDecoder() = default;

void MusicSegmentDecoder::Reset()
{
	// The decoder state is cleared and set up again from the next segment's first header
	_state->decoder.Reset();
	_channels = 0;
	_sampleRate = 0;
}

size_t MusicSegmentDecoder::Decode(const std::vector<uint8_t>& segment, std::vector<int16_t>& pcm)
{
	// Frame after frame up to the end of the segment's bytes (the original's "overrun detected" assert guards the
	// output buffer, which a std::vector does not need)
	size_t frames = 0;
	size_t offset = 0;
	while (offset < segment.size())
	{
		const auto frame = _state->decoder.Decode(std::span(segment).subspan(offset), _state->frame);
		if (frame.bytes == 0)
		{
			break;
		}
		offset += frame.bytes;
		if (frame.samples > 0)
		{
			_channels = frame.channels;
			_sampleRate = static_cast<int>(frame.sampleRate);
			pcm.insert(pcm.end(), _state->frame.begin(),
			           _state->frame.begin() + static_cast<ptrdiff_t>(frame.samples * frame.channels));
			frames += frame.samples;
		}
	}
	return frames;
}

// ---- MusicStream ---------------------------------------------------------------------------------------------------

MusicStream::MusicStream()
{
	// The audio device's one context (device::Open); no device of our own
	if (!device::IsOpen())
	{
		return;
	}
	for (auto& channel : _channels)
	{
		channel.source = device::CreateSource();
		// QMixer's law is applied in the gain (ApplyGain), not by OpenAL's distance model
		device::SetSourceRolloff(channel.source, 0.0f);
		device::SetSourceLooping(channel.source, false);
	}
	_available = true;
}

MusicStream::~MusicStream()
{
	if (!_available)
	{
		return;
	}
	for (auto& channel : _channels)
	{
		device::StopSource(channel.source);
		device::SetSourceBuffer(channel.source, 0);
		for (const auto& queued : channel.queue)
		{
			device::DeleteBuffer(queued.buffer);
		}
		channel.queue.clear();
		device::DeleteSource(channel.source);
	}
}

void MusicStream::EnableChannel(int channel, bool is3D, const MusicDistanceMapping& mapping, glm::vec3 position)
{
	auto& ch = _channels[static_cast<size_t>(channel)];
	ch.is3D = is3D;
	ch.mapping = mapping;
	ch.position = position;
	if (!_available)
	{
		return;
	}
	if (is3D)
	{
		device::SetSourceRelative(ch.source, false);
		device::SetSourcePosition(ch.source, position);
	}
	else
	{
		SetCentred(channel);
	}
	ApplyGain(channel);
}

void MusicStream::SetCentred(int channel)
{
	// 2D: QSWaveMixSetPolarPosition(0, 0, 0), centred on the listener
	if (!_available)
	{
		return;
	}
	const auto source = _channels[static_cast<size_t>(channel)].source;
	device::SetSourceRelative(source, true);
	device::SetSourcePosition(source, glm::vec3(0.0f));
}

void MusicStream::QueueChunk(int channel, const std::vector<uint8_t>& segment, bool resetDecoder, uint32_t startSample,
                             bool last)
{
	auto& ch = _channels[static_cast<size_t>(channel)];
	if (resetDecoder)
	{
		ch.decoder.Reset();
	}
	ch.pcm.clear();
	const auto frames = ch.decoder.Decode(segment, ch.pcm);
	ch.decodedFrames += frames;
	// (approximate) before any frame has decoded there is no wave format (built from the frame header): the silent
	// frame below goes out as stereo at 22050 Hz, the rate every music bank has
	const int channels = ch.decoder.GetChannels() == 1 ? 1 : 2;
	const int rate = ch.decoder.GetSampleRate() > 0 ? ch.decoder.GetSampleRate() : 22050;

	// lStart of QMIXPLAYPARAMS: the wave starts that many samples in (inferred unit)
	const auto skip = std::min<size_t>(startSample, frames);
	std::span<const int16_t> data = std::span<const int16_t>(ch.pcm).subspan(skip * static_cast<size_t>(channels));
	// (approximate) a segment that does not decode still has to end, so that the callback fires: one silent frame
	const std::array<int16_t, 2> silence {};
	if (data.empty())
	{
		data = std::span<const int16_t>(silence).first(static_cast<size_t>(channels));
	}
	const auto queuedFrames = static_cast<uint32_t>(data.size() / static_cast<size_t>(channels));
	if (!_available)
	{
		ch.queue.push_back({0, queuedFrames, static_cast<uint32_t>(skip), last});
		return;
	}

	const auto buffer = device::CreateBuffer(channels == 1 ? ChannelLayout::Mono : ChannelLayout::Stereo, data, rate);
	device::QueueSourceBuffer(ch.source, buffer);
	ch.queue.push_back({buffer, queuedFrames, static_cast<uint32_t>(skip), last});
	ApplyPitch(channel);
	if (!ch.paused)
	{
		if (device::SourceStatus(ch.source) != AudioStatus::Playing)
		{
			device::PlaySource(ch.source);
		}
	}
}

void MusicStream::SetFrequency(int channel, uint32_t hz)
{
	_channels[static_cast<size_t>(channel)].frequency = hz;
	ApplyPitch(channel);
}

void MusicStream::ApplyPitch(int channel)
{
	auto& ch = _channels[static_cast<size_t>(channel)];
	if (!_available || ch.frequency == 0 || ch.decoder.GetSampleRate() <= 0)
	{
		return;
	}
	// QSWaveMixSetFrequency plays the wave at that rate: OpenAL's pitch is its ratio to the wave's own rate
	const float pitch = static_cast<float>(ch.frequency) / static_cast<float>(ch.decoder.GetSampleRate());
	device::SetSourcePitch(ch.source, pitch);
}

void MusicStream::SetVolume(int channel, uint32_t volume)
{
	_channels[static_cast<size_t>(channel)].volume = volume;
	ApplyGain(channel);
}

void MusicStream::ApplyGain(int channel)
{
	auto& ch = _channels[static_cast<size_t>(channel)];
	if (!_available)
	{
		return;
	}
	float gain = static_cast<float>(ch.volume) / k_QMixerVolumeScale;
	if (ch.is3D)
	{
		const auto distance = glm::distance(ch.position, device::ListenerPosition());
		gain *= DistanceGain(ch.mapping, distance);
	}
	device::SetSourceGain(ch.source, SwitchedGain(GetOutputSwitches().music, gain));
}

bool MusicStream::IsChannelDone(int channel)
{
	return _channels[static_cast<size_t>(channel)].queue.empty();
}

void MusicStream::FlushChannel(int channel)
{
	auto& ch = _channels[static_cast<size_t>(channel)];
	if (_available)
	{
		device::StopSource(ch.source);
		device::SetSourceBuffer(ch.source, 0); // unqueues every buffer of a stopped source
	}
	auto flushed = std::move(ch.queue);
	ch.queue.clear();
	for (const auto& queued : flushed)
	{
		if (_available)
		{
			device::DeleteBuffer(queued.buffer);
		}
		// QMixer notifies each flushed wave (see IMusicSink::SetListener)
		if (_listener != nullptr)
		{
			_listener->OnChunkDone(channel, queued.last);
		}
	}
}

uint32_t MusicStream::GetPlayPosition(int channel)
{
	auto& ch = _channels[static_cast<size_t>(channel)];
	if (!_available || ch.queue.empty())
	{
		return 0;
	}
	const auto offset = device::SourceSampleOffset(ch.source);
	const auto processed = device::SourceBuffersProcessed(ch.source);
	// AL_SAMPLE_OFFSET counts from the first buffer still queued, played ones included
	auto position = static_cast<uint32_t>(std::max(offset, 0));
	size_t index = 0;
	for (; index < static_cast<size_t>(processed) && index + 1 < ch.queue.size(); ++index)
	{
		position -= std::min(position, ch.queue[index].frames);
	}
	// the position inside the segment, with the samples a synced start skipped (inferred: QMixer's position is in the
	// wave, which there starts at lStart)
	return position + ch.queue[index].startSample;
}

void MusicStream::SetSourcePosition(int channel, glm::vec3 position)
{
	auto& ch = _channels[static_cast<size_t>(channel)];
	ch.position = position;
	if (!_available || !ch.is3D)
	{
		return;
	}
	device::SetSourcePosition(ch.source, position);
	ApplyGain(channel);
}

void MusicStream::PauseChannel(int channel)
{
	auto& ch = _channels[static_cast<size_t>(channel)];
	ch.paused = true;
	if (_available)
	{
		device::PauseSource(ch.source);
	}
}

void MusicStream::RestartChannel(int channel)
{
	auto& ch = _channels[static_cast<size_t>(channel)];
	ch.paused = false;
	if (_available && !ch.queue.empty())
	{
		device::PlaySource(ch.source);
	}
}

void MusicStream::Pump()
{
	// The debug Audio Player window's music switch, turned since the last pump: every channel gets its gain again
	bool switched = false;
	if (_available)
	{
		const bool on = GetOutputSwitches().music;
		switched = on != _musicOn;
		_musicOn = on;
	}
	for (int i = 0; i < k_MusicChannelCount; ++i)
	{
		auto& ch = _channels[static_cast<size_t>(i)];
		if (!_available)
		{
			continue;
		}
		const auto processed = device::SourceBuffersProcessed(ch.source);
		for (int32_t n = 0; n < processed && !ch.queue.empty(); ++n)
		{
			device::DeleteBuffer(device::UnqueueSourceBuffer(ch.source));
			const bool last = ch.queue.front().last;
			ch.queue.pop_front();
			if (_listener != nullptr)
			{
				_listener->OnChunkDone(i, last); // The end of chunk callback
			}
		}
		if (!ch.queue.empty() && !ch.paused)
		{
			// (approximate) an underrun stops an OpenAL source; QMixer plays the next queued wave as soon as it is there
			const auto state = device::SourceStatus(ch.source);
			if (state == AudioStatus::Stopped || state == AudioStatus::Initial)
			{
				device::PlaySource(ch.source);
			}
		}
		if (ch.is3D || switched)
		{
			ApplyGain(i);
		}
	}
}

// ---- MusicSystem ---------------------------------------------------------------------------------------------------

MusicSystem::MusicSystem()
    : _engine(_stream)
{
	_thread = std::thread([this]() { ThreadMain(); });
}

MusicSystem::~MusicSystem()
{
	// The music's close: the thread is told to end and waited for, then the channels
	_quit = true;
	if (_thread.joinable())
	{
		_thread.join();
	}
	std::lock_guard lock(_mutex);
	_engine.Close();
}

void MusicSystem::ThreadMain()
{
	using namespace std::chrono;
	// QMixer pumps every 20 ms; the music thread passes, then sleeps 120 ms
	constexpr auto k_PumpPeriod = milliseconds(20);
	auto nextPass = steady_clock::now();
	auto nextPump = nextPass;
	while (!_quit)
	{
		{
			std::lock_guard lock(_mutex);
			const auto start = steady_clock::now();
			if (start >= nextPump)
			{
				_stream.Pump();
				nextPump = start + k_PumpPeriod;
			}
			if (steady_clock::now() >= nextPass)
			{
				const auto now = device::TickCount(); // GetTickCount (the one real clock of src/Audio)
				const bool ok = _engine.Process(now);
				Trace(now, ok);
				nextPass = steady_clock::now() + milliseconds(ok ? k_MusicPassSleepMs : k_MusicRetrySleepMs);
			}
		}
		std::this_thread::sleep_until(std::min(nextPass, nextPump));
	}
}

void MusicSystem::Trace(uint32_t nowMs, bool ok)
{
	static const bool k_Trace = debug_env::MusicTrace();
	if (!k_Trace)
	{
		return;
	}
	auto logger = Logger();
	if (!logger)
	{
		return;
	}
	std::string line;
	for (int i = 0; i < k_MusicChannelCount; ++i)
	{
		const auto& ch = _engine.GetChannel(i);
		if (ch.status == MusicStatus::Free && _engine.GetQueued(i) == 0)
		{
			continue;
		}
		const auto volume = _engine.GetMixerVolume(i);
		line += fmt::format(" | ch{} {} {} cur={} target={} sad={} chunk={}/{} next={} loops={} queued={} qmix={} "
		                    "gain={:.4f} decoded={}",
		                    i, ch.bank != nullptr ? ch.bank->GetPath().filename().string() : std::string("-"),
		                    StatusName(ch.status), ch.current, ch.target, ch.sadVolume, ch.playingChunk, ch.chunkCount,
		                    ch.nextChunk, ch.loops, _engine.GetQueued(i), volume,
		                    static_cast<float>(volume) / k_QMixerVolumeScale, _stream.GetDecodedFrames(i));
	}
	if (line.empty())
	{
		return;
	}
	SPDLOG_LOGGER_INFO(logger, "music: t={} {} master={} masterVolume={}{}", nowMs, ok ? "ok" : "read failed",
	                   _engine.GetMainChannel(), _engine.GetMainVolume(), line);
}

// ---- music:: -------------------------------------------------------------------------------------------------------

namespace music
{
namespace
{

struct TestStep
{
	float seconds;
	int type; ///< -1 = stop with a fade, -2 = stop at once
};
/// What this module keeps between calls (Locator::audioState)
struct MusicStreamState
{
	std::unique_ptr<MusicSystem> system {};
	std::vector<TestStep> testSteps {};
	size_t nextTestStep {0};
	std::chrono::steady_clock::time_point startTime {};
};

MusicStreamState& MusicStreamData()
{
	return openblack::Locator::audioState::value().Get<MusicStreamState>();
}

// OPENBLACK_TEST_MUSIC="<type>[,<type>@<s>][,stop@<s>][,cut@<s>]": the first play goes like the pre-intro trailer's
// (vol 127, chunk 1, no sync, no fade, 2D); each later one with sync and fade, as the citadel music asks (vol 127,
// sync 1, fade 1), to hear a synced change; stop = stop with a fade, cut = stop at once. A test hook, not a behaviour
// of the original: the citadel music's start chunk is the group's position, here always 1 (only used when no other
// channel of the group plays).
void ParseTestHook()
{
	auto& state = MusicStreamData();
	const char* spec = std::getenv("OPENBLACK_TEST_MUSIC");
	if (spec == nullptr)
	{
		return;
	}
	std::string_view rest(spec);
	while (!rest.empty())
	{
		const auto comma = rest.find(',');
		const auto item = rest.substr(0, comma);
		rest = comma == std::string_view::npos ? std::string_view {} : rest.substr(comma + 1);
		const auto at = item.find('@');
		const std::string what(item.substr(0, at));
		const float seconds =
		    at == std::string_view::npos ? 0.0f : std::strtof(std::string(item.substr(at + 1)).c_str(), nullptr);
		int type = 0;
		if (what == "stop")
		{
			type = -1;
		}
		else if (what == "cut")
		{
			type = -2;
		}
		else
		{
			type = std::atoi(what.c_str());
		}
		state.testSteps.push_back({seconds, type});
	}
	std::stable_sort(state.testSteps.begin(), state.testSteps.end(),
	                 [](const TestStep& a, const TestStep& b) { return a.seconds < b.seconds; });
}

void RunTestHook()
{
	auto& state = MusicStreamData();
	if (state.nextTestStep >= state.testSteps.size() || !state.system)
	{
		return;
	}
	const float elapsed = std::chrono::duration<float>(std::chrono::steady_clock::now() - state.startTime).count();
	while (state.nextTestStep < state.testSteps.size() && state.testSteps[state.nextTestStep].seconds <= elapsed)
	{
		const auto step = state.testSteps[state.nextTestStep];
		const bool first = state.nextTestStep == 0;
		++state.nextTestStep;
		if (step.type < 0)
		{
			state.system->With([&step](MusicEngine& engine) { engine.Stop(step.type == -1 ? 1 : 0); });
			SPDLOG_LOGGER_INFO(Logger(), "music test: {} at {:.1f} s", step.type == -1 ? "stop (fade)" : "cut", elapsed);
			continue;
		}
		if (step.type <= 0 || step.type >= static_cast<int>(MusicType::_COUNT))
		{
			SPDLOG_LOGGER_WARN(Logger(), "music test: no music type {}", step.type);
			continue;
		}
		auto* bank = GetBank(static_cast<MusicType>(step.type));
		MusicPlayOptions options;
		options.bank = bank;
		options.volume = k_MusicMaxVolume;
		options.startChunk = 1;
		options.sync = first ? 0 : 1;
		options.fade = first ? 0 : 1;
		const int channel = state.system->With([&options](MusicEngine& engine) { return engine.Play(options); });
		SPDLOG_LOGGER_INFO(Logger(), "music test: play {} {} at {:.1f} s (sync {}, fade {}) -> channel {}", step.type,
		                   k_MusicBanks[static_cast<size_t>(step.type)].name, elapsed, options.sync, options.fade, channel);
	}
}
} // namespace

MusicBank* GetBank(MusicType type)
{
	auto& state = MusicStreamData();
	// the music banks are audio::banks'; the engine learns each one as it comes
	bool registeredNow = false;
	auto* bank = banks::MusicBankOf(type, registeredNow);
	if (registeredNow && state.system)
	{
		state.system->With([bank](MusicEngine& engine) { engine.NoteBankRegistered(*bank); });
	}
	return bank;
}

void Start()
{
	auto& state = MusicStreamData();
	if (state.system)
	{
		return;
	}
	if (!device::IsOpen())
	{
		SPDLOG_LOGGER_WARN(Logger(), "music: no OpenAL context, no music");
		return;
	}
	state.system = std::make_unique<MusicSystem>();
	state.startTime = std::chrono::steady_clock::now();
	if (const char* volume = std::getenv("OPENBLACK_TEST_MUSIC_VOLUME"); volume != nullptr && Locator::config::has_value())
	{
		Locator::config::value().audioMusicMainVolume = static_cast<uint32_t>(std::atoi(volume));
	}
	ParseTestHook();
	Update();
}

void Shutdown()
{
	auto& state = MusicStreamData();
	state.system.reset();
	banks::ReleaseMusicBanks();
	state.testSteps.clear();
	state.nextTestStep = 0;
}

void Update()
{
	auto& state = MusicStreamData();
	if (!state.system)
	{
		return;
	}
	// AudioMusicMasterVolume of the configuration, applied live like the options dialog does; the engine ignores the
	// same value
	if (Locator::config::has_value())
	{
		const auto volume = Locator::config::value().audioMusicMainVolume;
		state.system->With([volume](MusicEngine& engine) { engine.SetMainVolume(volume); });
	}
	RunTestHook();
}

MusicSystem* Get()
{
	return MusicStreamData().system.get();
}
} // namespace music

} // namespace openblack::audio
