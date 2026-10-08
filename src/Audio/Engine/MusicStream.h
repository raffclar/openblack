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
#include <atomic>
#include <deque>
#include <filesystem>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>

#include "Audio/Engine/MusicEngine.h"
#include "Audio/Game/BankTables.h"

// The device side of the music: what QMixer and the original audio library's MPEG decoder did for the 6 music
// channels, on the sources and buffers of the audio device (Device.h: its one OpenAL context, no second device), and
// the "music" thread that runs MusicEngine. The banks are audio::banks' (Banks.h, MusicBankOf).
// Trace: OPENBLACK_MUSIC_TRACE=1 writes one "music:" line per pass of the thread with every busy channel.

namespace openblack::audio
{

/// The bank's MPEG decoder (audio::codec::MpegFrameDecoder, layer II). The state is kept from one segment to the next
/// and only reset when a play asks for it, so the segments of a track decode as one continuous stream.
class MusicSegmentDecoder
{
public:
	MusicSegmentDecoder();
	~MusicSegmentDecoder();
	MusicSegmentDecoder(const MusicSegmentDecoder&) = delete;
	MusicSegmentDecoder& operator=(const MusicSegmentDecoder&) = delete;

	void Reset();
	/// Decode every frame of one segment and append the interleaved 16-bit samples. Returns the PCM frames decoded.
	size_t Decode(const std::vector<uint8_t>& segment, std::vector<int16_t>& pcm);
	/// Of the last frame decoded (0 before any): the wave format the original builds from the frame header
	[[nodiscard]] int GetChannels() const { return _channels; }
	[[nodiscard]] int GetSampleRate() const { return _sampleRate; }

private:
	struct State;
	std::unique_ptr<State> _state;
	int _channels {0};
	int _sampleRate {0};
};

/// The 6 QMixer music channels over OpenAL: one source each, its queue of chunk buffers, and the decoder of the bank
/// that plays on it. Gain = QMixer volume / 32767 times, in 3D, QMixer's distance law.
class MusicStream final: public IMusicSink
{
public:
	MusicStream();
	~MusicStream() override;
	MusicStream(const MusicStream&) = delete;
	MusicStream& operator=(const MusicStream&) = delete;

	/// Sources are made on the current OpenAL context; false (and nothing plays) without one
	[[nodiscard]] bool IsAvailable() const { return _available; }

	void SetListener(IMusicChunkListener* listener) override { _listener = listener; }
	void EnableChannel(int channel, bool is3D, const MusicDistanceMapping& mapping, glm::vec3 position) override;
	void QueueChunk(int channel, const std::vector<uint8_t>& segment, bool resetDecoder, uint32_t startSample,
	                bool last) override;
	void SetCentred(int channel) override;
	void SetFrequency(int channel, uint32_t hz) override;
	void SetVolume(int channel, uint32_t volume) override;
	[[nodiscard]] bool IsChannelDone(int channel) override;
	void FlushChannel(int channel) override;
	[[nodiscard]] uint32_t GetPlayPosition(int channel) override;
	void SetSourcePosition(int channel, glm::vec3 position) override;
	void PauseChannel(int channel) override;
	void RestartChannel(int channel) override;

	/// What QMixer's pump (every 20 ms) does for music: the end of chunk callbacks of the buffers OpenAL has played, and
	/// the 3D gain against the listener
	void Pump();

	/// PCM frames decoded so far on a channel (trace)
	[[nodiscard]] uint64_t GetDecodedFrames(int channel) const { return _channels[static_cast<size_t>(channel)].decodedFrames; }

private:
	struct Queued
	{
		uint32_t buffer;
		uint32_t frames;
		uint32_t startSample;
		bool last;
	};
	struct Channel
	{
		uint32_t source {0};
		std::deque<Queued> queue;
		MusicSegmentDecoder decoder;
		bool is3D {false};
		MusicDistanceMapping mapping {10.0f, 100.0f, 1.0f};
		glm::vec3 position {0.0f};
		uint32_t volume {0};
		uint32_t frequency {0};
		bool paused {false};
		uint64_t decodedFrames {0};
		std::vector<int16_t> pcm;
	};

	void ApplyGain(int channel);
	void ApplyPitch(int channel);

	std::array<Channel, k_MusicChannelCount> _channels;
	IMusicChunkListener* _listener {nullptr};
	bool _available {false};
	bool _musicOn {true}; ///< the music switch (OutputSwitches) as the last Pump saw it
};

/// The audio system's music as openblack runs it: the engine, its OpenAL channels and the "music" thread. One pass,
/// then Sleep(120) (5000 after a failed read); the QMixer pump of the channels every 20 ms. Every call goes through the
/// lock that stands for the original's critical section (recursive, as a Win32 critical section: the engine's callbacks
/// may call it again).
class MusicSystem
{
public:
	MusicSystem();
	~MusicSystem();
	MusicSystem(const MusicSystem&) = delete;
	MusicSystem& operator=(const MusicSystem&) = delete;

	/// Run f(MusicEngine&) under the lock
	template <typename F>
	auto With(F&& f)
	{
		std::lock_guard lock(_mutex);
		return f(_engine);
	}
	/// The lock of With, for a caller with state of its own that the engine's callbacks touch (GameMusic)
	[[nodiscard]] std::recursive_mutex& GetMutex() { return _mutex; }
	/// The engine itself: only under GetMutex()
	[[nodiscard]] MusicEngine& GetEngine() { return _engine; }
	[[nodiscard]] bool IsAvailable() const { return _stream.IsAvailable(); }
	[[nodiscard]] uint64_t GetDecodedFrames(int channel)
	{
		std::lock_guard lock(_mutex);
		return _stream.GetDecodedFrames(channel);
	}

private:
	void ThreadMain();
	void Trace(uint32_t nowMs, bool ok);

	std::recursive_mutex _mutex;
	MusicStream _stream;
	MusicEngine _engine;
	std::atomic<bool> _quit {false};
	std::thread _thread;
};

namespace music
{
/// Start the music system on the audio device (call after device::Open), and the test hook
void Start();
/// Stop the thread and free the channels (the music's close); before the OpenAL context goes
void Shutdown();
/// Per frame: the main volume of the configuration and the test hook OPENBLACK_TEST_MUSIC
void Update();
/// nullptr when not started or without OpenAL
MusicSystem* Get();
/// The bank of a MUSIC_TYPE (banks::MusicBankOf, registered on first use; the original registers the 85 at once when
/// the audio system is created), told to the engine when new; nullptr if it is not installed (WELCOME_DANCE)
MusicBank* GetBank(MusicType type);
} // namespace music

} // namespace openblack::audio
