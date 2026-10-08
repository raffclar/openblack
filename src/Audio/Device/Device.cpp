/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "Device.h"

#include <cstdlib>

#include <array>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>

#include <fmt/format.h>
#include <spdlog/spdlog.h>

extern "C" {
#include <AL/al.h>
#include <AL/alc.h>
#include <AL/alext.h>
}

#include "Audio/Device/AlSampleOutput.h"
#include "Audio/Device/FixedClockSampleOutput.h"
#include "Audio/Device/SampleOutput.h"
#include "Audio/Device/WaveBuffers.h"
#include "Debug/FixedClock.h"
#include "ECS/Systems/AudioStateInterface.h"
#include "GameClock.h"
#include "Locator.h"

using namespace openblack::audio;

namespace
{
/// Every OpenAL call of openblack goes through alCheckCall: the call, then alGetError logged with the call's text
void AlErrorCheck(const char* call, const char* file, int line)
{
	std::string errorMessage;
	const ALenum error = alGetError();
	switch (error)
	{
	case AL_NO_ERROR:
		return;
	case AL_INVALID_NAME:
		errorMessage = "AL_INVALID_NAME: a bad name (ID) was passed to an OpenAL function";
		break;
	case AL_INVALID_ENUM:
		errorMessage = "AL_INVALID_ENUM: an invalid enum value was passed to an OpenAL function";
		break;
	case AL_INVALID_VALUE:
		errorMessage = "AL_INVALID_VALUE: an invalid value was passed to an OpenAL function";
		break;
	case AL_INVALID_OPERATION:
		errorMessage = "AL_INVALID_OPERATION: the requested operation is not valid";
		break;
	case AL_OUT_OF_MEMORY:
		errorMessage = "AL_OUT_OF_MEMORY: the requested operation resulted in OpenAL running out of memory";
		break;
	default:
		errorMessage = "UNKNOWN AL ERROR: " + std::to_string(error);
	}
	if (auto logger = spdlog::get("audio"))
	{
		SPDLOG_LOGGER_ERROR(logger, R"(OpenAL error: {} with call "{}" at file "{}" on line {})", errorMessage, call, file,
		                    line);
	}
}

#define alCheckCall(FUNCTION_CALL) \
	FUNCTION_CALL;                 \
	AlErrorCheck(#FUNCTION_CALL, __FILE__, __LINE__)

void ALC_APIENTRY AlLogger([[maybe_unused]] void* userptr, char level, const char* message,
                           [[maybe_unused]] int length) noexcept
{
	switch (level)
	{
	case 'E':
		SPDLOG_LOGGER_ERROR(spdlog::get("audio"), "{}", message);
		break;
	case 'W':
		SPDLOG_LOGGER_WARN(spdlog::get("audio"), "{}", message);
		break;
	case 'I':
		SPDLOG_LOGGER_INFO(spdlog::get("audio"), "{}", message);
		break;
	default:
		break;
	}
}

/// OpenAL Soft's log goes to the "audio" logger (before any other call to OpenAL)
void SetupLogging()
{
	auto* alsoftSetLogCallback = reinterpret_cast<void (*)(void (*)(void*, char, const char*, int) noexcept, void*) noexcept>(
	    alcGetProcAddress(nullptr, "alsoft_set_log_callback"));
	if (alsoftSetLogCallback != nullptr)
	{
		alsoftSetLogCallback(AlLogger, nullptr);
		return;
	}
	SPDLOG_LOGGER_WARN(spdlog::get("audio"), "Could not set openal logging callback");
	SPDLOG_LOGGER_WARN(spdlog::get("audio"), "Falling back to tracing directly in openal");
	enum class LogLevel
	{
		Disable,
		Error,
		Warning,
		Trace
	};
	LogLevel level;
	switch (spdlog::get("audio")->level())
	{
	case spdlog::level::trace:
	case spdlog::level::debug:
	case spdlog::level::info:
		level = LogLevel::Trace;
		break;
	case spdlog::level::warn:
		level = LogLevel::Warning;
		break;
	case spdlog::level::err:
	case spdlog::level::critical:
		level = LogLevel::Error;
		break;
	default:
		level = LogLevel::Disable;
		break;
	}
	std::array<char, 2> levelStr = {'0', '\0'};
	levelStr[0] += static_cast<char>(level);
#if defined(_MSC_VER)
	_putenv_s("ALSOFT_LOGLEVEL", levelStr.data());
#else
	setenv("ALSOFT_LOGLEVEL", levelStr.data(), 0);
#endif
}

struct DeviceState
{
	ALCdevice* device {nullptr};
	ALCcontext* context {nullptr};
	/// The 16 channels' sources (made after the context, deleted before it)
	std::unique_ptr<SampleOutput> output;
};

/// The Device state (Locator::audioState)
DeviceState& DeviceData()
{
	return openblack::Locator::audioState::value().Get<DeviceState>();
}

/// openblack's axes -> OpenAL's (and back: the swap is its own inverse)
glm::vec3 ToAl(glm::vec3 p)
{
	return {p.z, p.y, p.x};
}
} // namespace

bool device::Open()
{
	auto& deviceState = DeviceData();
	if (deviceState.output)
	{
		return IsOpen();
	}
	try
	{
		// Register audio logging before doing any other calls to openal
		SetupLogging();
		deviceState.device = alcOpenDevice(nullptr);
		if (deviceState.device == nullptr)
		{
			throw std::runtime_error(
			    fmt::format("Error creating audio device {}", alcGetString(nullptr, alcGetError(nullptr))));
		}
		deviceState.context = alcCreateContext(deviceState.device, nullptr);
		if (deviceState.context == nullptr)
		{
			throw std::runtime_error(fmt::format("Error creating audio context {}",
			                                     alcGetString(deviceState.device, alcGetError(deviceState.device))));
		}
		ALCint majorVersion;
		ALCint minorVersion;
		alcGetIntegerv(deviceState.device, ALC_MAJOR_VERSION, 1, &majorVersion);
		alcGetIntegerv(deviceState.device, ALC_MINOR_VERSION, 1, &minorVersion);
		SPDLOG_LOGGER_INFO(spdlog::get("audio"), "ALC Version {}.{}", majorVersion, minorVersion);
		alCheckCall(alcMakeContextCurrent(deviceState.context));
		// QMixer's distance curve up to maxDistance is OpenAL's inverse distance clamped with reference = min and
		// rolloff = scale; beyond max QMixer mutes the channel (AlSampleOutput does that)
		alCheckCall(alDistanceModel(AL_INVERSE_DISTANCE_CLAMPED));
		deviceState.output = std::make_unique<AlSampleOutput>();
		// OPENBLACK_FIXED_FRAME_MS (test runs): the play state the logic waits on follows the fixed clock
		// (the help and advisor voices), not the device's real time; the sound is still played
		if (fixed_clock::Enabled())
		{
			deviceState.output = std::make_unique<FixedClockSampleOutput>(std::move(deviceState.output));
		}
		return true;
	}
	catch (std::runtime_error& error)
	{
		SPDLOG_LOGGER_ERROR(spdlog::get("audio"), "Falling back to no-op audio: {}", error.what());
		if (deviceState.context != nullptr)
		{
			alcDestroyContext(deviceState.context);
			deviceState.context = nullptr;
		}
		if (deviceState.device != nullptr)
		{
			alcCloseDevice(deviceState.device);
			deviceState.device = nullptr;
		}
		deviceState.output = std::make_unique<NullSampleOutput>();
		return false;
	}
}

void device::Close()
{
	auto& state = DeviceData();
	// the channels' sources, then the wave buffers they played (one per sample, shared), before the context
	state.output.reset();
	wave_buffers::DeleteAll();
	if (state.context != nullptr)
	{
		alcMakeContextCurrent(nullptr);
		alcDestroyContext(state.context);
		state.context = nullptr;
	}
	if (state.device != nullptr)
	{
		alcCloseDevice(state.device);
		state.device = nullptr;
	}
}

bool device::IsOpen()
{
	return DeviceData().context != nullptr;
}

uint32_t device::TickCount()
{
	return game_clock::TickCount();
}

SampleOutput* device::Output()
{
	return DeviceData().output.get();
}

void device::SetListener(glm::vec3 position, glm::vec3 velocity, glm::vec3 forward, glm::vec3 up)
{
	if (!IsOpen())
	{
		return;
	}
	// the orientation goes through the same x <-> z swap as the points, or left and right come out wrong
	const auto p = ToAl(position);
	const auto v = ToAl(velocity);
	const auto f = ToAl(forward);
	const auto u = ToAl(up);
	alCheckCall(alListener3f(AL_POSITION, p.x, p.y, p.z));
	alCheckCall(alListener3f(AL_VELOCITY, v.x, v.y, v.z));
	std::array<ALfloat, 6> orientation = {f.x, f.y, f.z, u.x, u.y, u.z};
	alCheckCall(alListenerfv(AL_ORIENTATION, orientation.data()));
}

glm::vec3 device::ListenerPosition()
{
	if (!IsOpen())
	{
		return glm::vec3(0.0f);
	}
	glm::vec3 listener(0.0f);
	alCheckCall(alGetListener3f(AL_POSITION, &listener.x, &listener.y, &listener.z));
	return ToAl(listener);
}

// ---- sources ----------------------------------------------------------------------------------------------------------

SourceId device::CreateSource()
{
	ALuint source = 0;
	alCheckCall(alGenSources(1, &source));
	return source;
}

void device::DeleteSource(SourceId source)
{
	ALuint id = source;
	alCheckCall(alDeleteSources(1, &id));
}

void device::SetSourceBuffer(SourceId source, BufferId buffer)
{
	alCheckCall(alSourcei(source, AL_BUFFER, static_cast<ALint>(buffer)));
}

void device::SetSourcePitch(SourceId source, float pitch)
{
	alCheckCall(alSourcef(source, AL_PITCH, pitch));
}

void device::SetSourceGain(SourceId source, float gain)
{
	alCheckCall(alSourcef(source, AL_GAIN, gain));
}

void device::SetSourceLooping(SourceId source, bool looping)
{
	alCheckCall(alSourcei(source, AL_LOOPING, looping ? AL_TRUE : AL_FALSE));
}

void device::SetSourceRelative(SourceId source, bool relative)
{
	alCheckCall(alSourcei(source, AL_SOURCE_RELATIVE, relative ? AL_TRUE : AL_FALSE));
}

void device::SetSourcePosition(SourceId source, glm::vec3 position)
{
	const auto p = ToAl(position);
	alCheckCall(alSource3f(source, AL_POSITION, p.x, p.y, p.z));
}

void device::SetSourceDistance(SourceId source, float minDistance, float maxDistance, float rolloff)
{
	alCheckCall(alSourcef(source, AL_REFERENCE_DISTANCE, minDistance));
	alCheckCall(alSourcef(source, AL_MAX_DISTANCE, maxDistance));
	alCheckCall(alSourcef(source, AL_ROLLOFF_FACTOR, rolloff));
}

void device::SetSourceRolloff(SourceId source, float rolloff)
{
	alCheckCall(alSourcef(source, AL_ROLLOFF_FACTOR, rolloff));
}

void device::PlaySource(SourceId source)
{
	alCheckCall(alSourcePlay(source));
}

void device::StopSource(SourceId source)
{
	alCheckCall(alSourceStop(source));
}

void device::PauseSource(SourceId source)
{
	alCheckCall(alSourcePause(source));
}

AudioStatus device::SourceStatus(SourceId source)
{
	ALint state = AL_STOPPED;
	alCheckCall(alGetSourcei(source, AL_SOURCE_STATE, &state));
	switch (state)
	{
	case AL_PLAYING:
		return AudioStatus::Playing;
	case AL_PAUSED:
		return AudioStatus::Paused;
	case AL_INITIAL:
		return AudioStatus::Initial;
	default:
		return AudioStatus::Stopped;
	}
}

int32_t device::SourceSampleOffset(SourceId source)
{
	ALint offset = 0;
	alCheckCall(alGetSourcei(source, AL_SAMPLE_OFFSET, &offset));
	return offset;
}

float device::SourceSecondOffset(SourceId source)
{
	ALfloat seconds = 0.0f;
	alCheckCall(alGetSourcef(source, AL_SEC_OFFSET, &seconds));
	return seconds;
}

int32_t device::SourceBuffersProcessed(SourceId source)
{
	ALint processed = 0;
	alCheckCall(alGetSourcei(source, AL_BUFFERS_PROCESSED, &processed));
	return processed;
}

void device::QueueSourceBuffer(SourceId source, BufferId buffer)
{
	ALuint id = buffer;
	alCheckCall(alSourceQueueBuffers(source, 1, &id));
}

BufferId device::UnqueueSourceBuffer(SourceId source)
{
	ALuint buffer = 0;
	alCheckCall(alSourceUnqueueBuffers(source, 1, &buffer));
	return buffer;
}

// ---- buffers ----------------------------------------------------------------------------------------------------------

BufferId device::CreateBuffer(ChannelLayout layout, std::span<const int16_t> samples, int rate)
{
	ALuint buffer = 0;
	alCheckCall(alGenBuffers(1, &buffer));
	const auto format = layout == ChannelLayout::Stereo ? AL_FORMAT_STEREO16 : AL_FORMAT_MONO16;
	alCheckCall(alBufferData(buffer, format, samples.data(), static_cast<ALsizei>(samples.size_bytes()), rate));
	return buffer;
}

bool device::SetBufferLoopPoints(BufferId buffer, int32_t start, int32_t end)
{
	if (alIsExtensionPresent("AL_SOFT_loop_points") != AL_TRUE)
	{
		return false;
	}
	const std::array<ALint, 2> points = {start, end};
	alCheckCall(alBufferiv(buffer, AL_LOOP_POINTS_SOFT, points.data()));
	return true;
}

void device::DeleteBuffer(BufferId buffer)
{
	ALuint id = buffer;
	alCheckCall(alDeleteBuffers(1, &id));
}

void device::DeleteBuffers(const std::vector<BufferId>& buffers)
{
	if (buffers.empty())
	{
		return;
	}
	alCheckCall(alDeleteBuffers(static_cast<ALsizei>(buffers.size()), buffers.data()));
}
