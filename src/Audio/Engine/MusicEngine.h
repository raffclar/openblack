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
#include <functional>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include <glm/vec3.hpp>

#include "Audio/Engine/MusicBank.h"

// The music engine of the original audio library: 6 channels, one of them the main channel, fades of +4 / -3 per pass of
// the music thread (one pass, then a 120 ms sleep), streamed segments with up to 4 queued per channel, sync groups,
// markers and the callbacks of the caller.
// What the mixer does (the channel, its wave queue and the MPEG decoder of the bank) is behind IMusicSink, so that
// the engine runs without OpenAL (test_music_engine) and over OpenAL in the game (MusicStream).
// The engine has no lock of its own: the caller serialises the calls (MusicSystem in MusicStream.h).

namespace openblack::audio
{

/// Number of music channels
inline constexpr int k_MusicChannelCount = 6;
/// The thread queues while fewer than 4 chunks are queued on the channel
inline constexpr int k_MusicQueueDepth = 4;
/// Sleep after a pass (120 ms) and after a failed read (5 s)
inline constexpr uint32_t k_MusicPassSleepMs = 0x78;
inline constexpr uint32_t k_MusicRetrySleepMs = 0x1388;
/// Fade steps per pass: +4 for the main channel up to its target, -3 for the others or above the target
inline constexpr int k_MusicFadeInStep = 4;
inline constexpr int k_MusicFadeOutStep = 3;
/// Volumes are 0..127 (play options, .sad volume and main volume)
inline constexpr int k_MusicMaxVolume = 0x7F;
/// SetPitch clamps to 50..250
inline constexpr int k_MusicMinPitch = 0x32;
inline constexpr int k_MusicMaxPitch = 0xFA;
/// No channel
inline constexpr int k_NoMusicChannel = -1;

/// The status of a channel (0 free, 1 playing, 2 paused, 3 finished, 4 last chunk queued)
enum class MusicStatus : int
{
	Free = 0,
	Playing = 1,
	Paused = 2,
	Finished = 3,
	LastQueued = 4,
};

/// The options of Play. The defaults are those of the original.
struct MusicPlayOptions
{
	MusicBank* bank {nullptr};
	int volume {0x7F};  ///< The target volume 0..127
	int field08 {0};    ///< Copied to the channel's field18 (no use seen)
	int field0C {0};    ///< Copied to the channel's field1C (no use seen)
	int field10 {0};    ///< Copied to the channel's field20 (no use seen)
	int startChunk {1}; ///< 1-based; 0 or > number of segments starts at 1
	int loops {0};      ///< Replaced by the .sad's with flag 0x40; -1 = forever
	int sync {0};       ///< Start where another channel of the same group plays
	int fade {0};       ///< Fade in (0 = straight to the volume)
	int is3D {0};
	int pitch {0x64};                                    ///< Percent
	MusicDistanceMapping distance {10.0f, 100.0f, 1.0f}; ///< Min distance, max distance, scale
	glm::vec3 position {0.0f};
	std::function<void(int userData)> finished;         ///< Called with userData when the last chunk has played
	std::function<void(std::string_view label)> marker; ///< Called with each marker's label
	int userData {0};
};

/// The marker list of a channel
struct MusicMarkerList
{
	std::vector<MusicMarker> nodes; ///< In the order of the bank's marker list
	size_t cursor {0};              ///< First node still worth looking at (nodes.size() = none)
	int64_t previous {0};           ///< Last position, (chunk << 32) | sample
	uint32_t startTick {0};         ///< Clock (ms) when the first chunk was queued
	int chunkBase {1};              ///< The chunk the clock starts from
};

/// One music channel
struct MusicChannel
{
	int is3D {0};
	int sync {0};
	int isNew {0};   ///< No chunk queued yet
	int fade {0};    ///< With 0 the next raise jumps to the target and sets it to 1
	int loops {0};   ///< Loops left (-1 = forever)
	int group {0};   ///< The bank's music group
	int field18 {0}; ///< From the options' field08
	int field1C {0}; ///< From the options' field0C
	int field20 {0}; ///< From the options' field10
	MusicStatus status {MusicStatus::Free};
	MusicStatus savedStatus {MusicStatus::Free}; ///< The status before Pause
	int sadVolume {0x7F};                        ///< 127 or the .sad's with flag 0x20
	int target {0};
	int current {0};
	int pitch {0x64};
	int startChunk {1};
	uint32_t nextChunk {1};    ///< Next chunk to read (1-based)
	uint32_t syncSample {0};   ///< Play position of the synced channel (the first chunk's start)
	uint32_t playingChunk {1}; ///< The audible chunk (advanced by the end of chunk callback)
	uint32_t chunkCount {0};   ///< Number of segments
	uint32_t sampleRate {0};   ///< Hz of the first segment
	MusicBank* bank {nullptr}; ///< Null = free
	std::function<void(int)> finished;
	std::function<void(std::string_view)> marker;
	int userData {0};
	std::unique_ptr<MusicMarkerList> markers;
	/// Play asks for a fresh decoder of the bank and the next read rebuilds it. Kept on the channel: a bank plays on one
	/// channel at a time.
	bool resetDecoder {false};
};

/// Receives the end of each queued chunk from the mixer
class IMusicChunkListener
{
public:
	virtual ~IMusicChunkListener() = default;
	virtual void OnChunkDone(int channel, bool last) = 0;
};

/// What the music engine asks of the mixer for its 6 channels, and the bank's MPEG decoder
class IMusicSink
{
public:
	virtual ~IMusicSink() = default;
	/// Where the end of chunk callbacks go. FlushChannel calls it for each flushed chunk before returning: Close spins on
	/// the queued count right after the flush, so the mixer notifies inside the flush (inferred from the spin).
	virtual void SetListener(IMusicChunkListener* listener) = 0;
	/// Enable the channel in 2D or 3D; in 3D also set its distance mapping and source position
	virtual void EnableChannel(int channel, bool is3D, const MusicDistanceMapping& mapping, glm::vec3 position) = 0;
	/// Decode one segment (with a fresh decoder if resetDecoder) and queue it starting at startSample; last is passed
	/// back to the end of chunk callback
	virtual void QueueChunk(int channel, const std::vector<uint8_t>& segment, bool resetDecoder, uint32_t startSample,
	                        bool last) = 0;
	/// Centre a 2D channel before each chunk
	virtual void SetCentred(int channel) = 0;
	virtual void SetFrequency(int channel, uint32_t hz) = 0;
	/// Volume 0..32766
	virtual void SetVolume(int channel, uint32_t volume) = 0;
	[[nodiscard]] virtual bool IsChannelDone(int channel) = 0;
	virtual void FlushChannel(int channel) = 0;
	/// The position in the playing wave (samples, inferred)
	[[nodiscard]] virtual uint32_t GetPlayPosition(int channel) = 0;
	virtual void SetSourcePosition(int channel, glm::vec3 position) = 0;
	virtual void PauseChannel(int channel) = 0;
	virtual void RestartChannel(int channel) = 0;
};

/// The music part of the audio system. Channels are numbered 0..5.
class MusicEngine final: public IMusicChunkListener
{
public:
	/// Main volume 127, no main channel, every channel free and enabled in 2D, installed and active
	explicit MusicEngine(IMusicSink& sink);
	MusicEngine(const MusicEngine&) = delete;
	MusicEngine& operator=(const MusicEngine&) = delete;
	~MusicEngine() override;

	/// The channel playing the options' bank or a new one; k_NoMusicChannel without a music bank (after Stop(1)) or
	/// without a free channel
	int Play(const MusicPlayOptions& options);
	/// fade == 1 sets every target to 0 (they fade out), otherwise they are cut
	void Stop(int fade);
	/// One channel; the main channel stops them all
	void Stop(int channel, int fade);
	/// The channel playing that bank
	[[nodiscard]] int FindChannelOfBank(const MusicBank* bank) const;
	[[nodiscard]] MusicStatus GetStatus(int channel) const;
	/// The audible chunk (0 without a channel)
	[[nodiscard]] uint32_t GetCurrentChunk(int channel) const;
	[[nodiscard]] int GetMainChannel() const;
	/// The highest group of the registered banks (the caller reports each registered bank here)
	[[nodiscard]] uint32_t GetTotalGroups() const;
	void NoteBankRegistered(const MusicBank& bank);
	/// Unsigned argument: above 127, or negative, gives 127
	void SetMainVolume(uint32_t volume);
	/// -1 when not active
	[[nodiscard]] int GetMainVolume() const;
	void SetPitch(int channel, uint32_t pitch);
	void Set3DPosition(int channel, glm::vec3 position);
	void Pause();
	void Restart();
	/// 0 = Stop(0) and inactive, 1 = active again (nothing restarts)
	void Switch(uint32_t on);
	/// The channel part of closing the music (after the thread has ended): every target to 0, volume 0 and flush (the
	/// original then waits until nothing is queued); no longer installed
	void Close();
	[[nodiscard]] bool IsInstalled() const { return _installed; }
	[[nodiscard]] bool IsActive() const { return _active; }

	/// One pass of the music thread over the 6 channels at the clock time nowMs. Returns false when a segment could
	/// not be read: the pass stops there and the thread sleeps k_MusicRetrySleepMs instead of k_MusicPassSleepMs.
	bool Process(uint32_t nowMs);

	/// The mixer's callback at the end of each queued chunk
	void OnChunkDone(int channel, bool last) override;

	[[nodiscard]] const MusicChannel& GetChannel(int channel) const { return _channels[static_cast<size_t>(channel)]; }
	/// Chunks queued on the channel
	[[nodiscard]] int GetQueued(int channel) const { return _queued[static_cast<size_t>(channel)]; }
	/// The volume sent to the mixer: floor(floor(cur * sad * 258 / 127) * mainVolume / 127)
	[[nodiscard]] uint32_t GetMixerVolume(int channel) const;

private:
	[[nodiscard]] bool IsValid(int channel) const { return channel >= 0 && channel < k_MusicChannelCount; }
	/// The checks of most entry points: installed and active (the wave system is always there)
	[[nodiscard]] bool IsUsable() const { return _installed && _active; }
	void ApplyVolume(int channel);
	/// The cut of Stop(0) for one channel
	void Cut(int channel);
	/// Fire the markers between the last position and the clock's
	void DispatchMarkers(MusicChannel& channel, uint32_t nowMs);
	/// The queueing loop of one channel; false when a read failed
	bool QueueChunks(int index, uint32_t nowMs);

	IMusicSink& _sink;
	std::array<MusicChannel, k_MusicChannelCount> _channels;
	std::array<int, k_MusicChannelCount> _queued {};
	int _mainChannel {k_NoMusicChannel};
	uint32_t _mainVolume {0x7F};
	uint32_t _totalGroups {0};
	bool _installed {false};
	bool _active {false};
	std::vector<uint8_t> _readBuffer; ///< The buffer of each read
};

} // namespace openblack::audio
