/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#define LOCATOR_IMPLEMENTATIONS
#include "AudioSystem.h"

#include <cctype>
#include <cmath>
#include <cstdlib>

#include <algorithm>
#include <unordered_map>
#include <vector>

#include <entt/core/hashed_string.hpp>
#include <fmt/format.h>
#include <glm/geometric.hpp>
#include <spdlog/spdlog.h>

#include "3D/SkyType.h"
#include "Audio/Audio.h"
#include "Audio/AudioManager.h"
#include "Audio/Device/Device.h"
#include "Audio/Device/Sound.h"
#include "Audio/Engine/AnimEffects.h"
#include "Audio/Engine/MusicEngine.h"
#include "Audio/Engine/MusicStream.h"
#include "Audio/Engine/QMixerLaws.h"
#include "Audio/Game/Banks.h"
#include "Audio/GameQueries.h"
#include "Audio/Services/AtmosBanks.h"
#include "Audio/Services/GameMusic.h"
#include "Audio/Services/LanternSounds.h"
#include "Audio/Services/ScriptAudioState.h"
#include "Audio/Services/SoundMap.h"
#include "Audio/Services/SoundTags.h"
#include "Camera/Camera.h"
#include "Debug/DebugEnv.h"
#include "ECS/Systems/AudioStateInterface.h"
#include "EngineConfig.h"
#include "GameClock.h"
#include "Locator.h"

using namespace openblack;
using namespace openblack::audio;

namespace
{
struct AudioSystemState
{
	GameQueries queries;
	bool initialised {false};
	/// The script's wide screen, as the script set it (Game's wide screen hook)
	bool scriptWideScreen {false};
	std::unordered_map<uint32_t, ObjectPositionFn> objects;
	/// The last id NewObjectId gave
	uint32_t nextObjectId {0};
};

/// The AudioSystem state (Locator::audioState)
AudioSystemState& AudioSystemData()
{
	return openblack::Locator::audioState::value().Get<AudioSystemState>();
}

bool Trace()
{
	static const bool k_Trace = debug_env::AudioTrace();
	return k_Trace;
}

std::optional<glm::vec3> CameraPoint()
{
	auto& state = AudioSystemData();
	if (state.queries.camera)
	{
		if (const auto camera = state.queries.camera())
		{
			return camera->position;
		}
		return std::nullopt;
	}
	if (!Locator::camera::has_value())
	{
		return std::nullopt;
	}
	return Locator::camera::value().GetOrigin();
}

/// The sound position of an owner, nullopt when it has none or is gone
std::optional<glm::vec3> OwnerPosition(const Owner& owner)
{
	auto& state = AudioSystemData();
	switch (owner.kind)
	{
	case Owner::Kind::Thing:
		if (state.queries.thingPosition)
		{
			return state.queries.thingPosition(static_cast<ThingId>(owner.thing));
		}
		return std::nullopt;
	case Owner::Kind::Object:
	{
		const auto found = state.objects.find(owner.id);
		if (found == state.objects.end() || !found->second)
		{
			return std::nullopt;
		}
		return found->second();
	}
	case Owner::Kind::SoundTag:
		// A tag's is its thing's (nullopt: no thing, the channel keeps its point)
		return tags::TagSoundPoint(owner.id);
	default:
		return std::nullopt;
	}
}

/// The filters shared by PlaySoundEffect and PlayAnimationEffect:
/// `ownerTest` is whether the unavailable owner is tested (is3D && track for the first, track for the second). The
/// reason the sample does not play (for OPENBLACK_SFX_TRACE), nullptr when it passes.
const char* FilterReason(const Sound& sound, uint32_t bank, const Owner& owner, bool ownerTest)
{
	auto& audioSystemState = AudioSystemData();
	// The sample's user parameter: only its low 16 bits are compared
	const auto kind = static_cast<uint16_t>(sound.userParam);
	if (audioSystemState.scriptWideScreen && kind == 1)
	{
		return "user parameter 1 with the script's wide screen";
	}
	if (IsInsideCitadel() && kind != 2)
	{
		return "inside the citadel, user parameter not 2";
	}
	// SET_GAME_SOUND false: only the Villagers and HelpSprites banks still play
	if (GetScriptAudioState().gameSoundOff != 0 && bank != Bank(SfxBank::Villagers) && bank != Bank(SfxBank::HelpSprites))
	{
		return "SET_GAME_SOUND false";
	}
	const int state = audioSystemState.queries.interfaceState ? audioSystemState.queries.interfaceState() : 0;
	if ((state == 0x10 || state == 0x16 || state == 0x17) && kind == 4)
	{
		return "user parameter 4 in an interface state 0x10 / 0x16 / 0x17";
	}
	if (ownerTest && OwnerUnavailable(owner))
	{
		return "owner unavailable";
	}
	return nullptr;
}

bool Filtered(const Sound& sound, uint32_t bank, const Owner& owner, bool ownerTest)
{
	return FilterReason(sound, bank, owner, ownerTest) != nullptr;
}

std::string OwnerText(const Owner& owner)
{
	switch (owner.kind)
	{
	case Owner::Kind::None:
		return "none";
	case Owner::Kind::Atmos:
		return "atmos";
	case Owner::Kind::Thing:
		return fmt::format("thing {}", static_cast<uint32_t>(owner.thing));
	case Owner::Kind::SoundTag:
		return fmt::format("tag {}", owner.id);
	case Owner::Kind::Key:
		return fmt::format("key {:#x}", owner.id);
	case Owner::Kind::Object:
		return fmt::format("object {}", owner.id);
	}
	return "?";
}

/// The pitch a started channel got (with the .sad's deviation), 0 when it did not start
int ChannelPitch(Channel channel)
{
	if (channel == k_NoChannel)
	{
		return 0;
	}
	for (const auto& info : sample_play::Channels())
	{
		if (info.handle == channel)
		{
			return info.pitch;
		}
	}
	return 0;
}

/// One OPENBLACK_SFX_TRACE line of PlaySoundEffect
void TraceSoundEffect(const sample_play::Options& options, const Sound& sound, Channel channel, const std::string& result)
{
	// the mode the sample starts with: the .sad's with its flag 0x400 unless the caller set it
	const bool sadMode = (sound.overrides & 0x400) != 0 && (options.callerMask & 0x400) == 0;
	const auto at = options.position + options.offset;
	SPDLOG_LOGGER_INFO(spdlog::get("audio"),
	                   "SFX: {}/{} ({}) {} track {} at ({:.1f}, {:.1f}, {:.1f}) mode {} loops {} pitch {} owner {} -> {}",
	                   BankGroup(static_cast<BankId>(sound.bank)), sound.id, sound.name, options.is3D ? "3D" : "2D",
	                   options.track ? 1 : 0, at.x, at.y, at.z, sadMode ? sound.playMode : options.mode, options.loops,
	                   ChannelPitch(channel), OwnerText(options.owner),
	                   channel != k_NoChannel ? fmt::format("channel {}", channel) : result);
}

/// The atmos of ProcessAudioGameTurn and its gate
void ProcessAudioGameTurn(uint32_t turn)
{
	// Only while the wave output is active does the music run; the thing music purge always runs, here inside
	// game_music::ProcessTurn right after the music (it touches nothing the atmos reads)
	const bool active = sample_play::IsActive();
	game_music::ProcessTurn(turn, active);
	if (!active)
	{
		return;
	}
	// The atmos banks
	atmos_banks::UpdateBanks();
	// The 3D channels and the listener
	sample_play::UpdateChannels();
	// The atmos mix, unless a video plays
	if (!IsVideoPlaying())
	{
		atmos_banks::Mix();
	}
}
} // namespace

// ---- banks (Banks.cpp) ------------------------------------------------------------------------------------------

BankId audio::AudioManager::CreatureBank(std::string_view species)
{
	return FindBank(fmt::format("audio/sfx/creature/{}.sad", species));
}

float audio::AudioManager::MaxDistance(Sample sample)
{
	// 0 for no bank, else the sample's max distance
	if (sample.bank == k_NoBank)
	{
		return 0.0f;
	}
	const auto* sound = sample_play::GetSound(SampleId(sample.bank, sample.number));
	return sound != nullptr ? sound->maxDistance : 0.0f;
}

std::optional<audio::Sample> audio::AudioManager::FindSample(BankId bank, std::string_view wavName)
{
	// (openblack: the original addresses samples by number only) the first sample of
	// the bank whose .sad name (the file name part) equals wavName, ignoring case; its number is the .sad's sample number
	const auto sameName = [wavName](const std::string& name) {
		return name.size() == wavName.size() && std::equal(name.begin(), name.end(), wavName.begin(), [](char a, char b) {
			       return std::tolower(static_cast<unsigned char>(a)) == std::tolower(static_cast<unsigned char>(b));
		       });
	};
	for (const auto id : banks::Samples(bank))
	{
		if (const auto* sound = sample_play::GetSound(id); sound != nullptr && sameName(sound->name))
		{
			return Sample {bank, sound->id};
		}
	}
	return std::nullopt;
}

// ---- owners -------------------------------------------------------------------------------------------------------

const GameQueries& audio::Queries()
{
	return AudioSystemData().queries;
}

std::optional<glm::vec3> audio::OwnerSoundPosition(const Owner& owner)
{
	return OwnerPosition(owner);
}

glm::vec3 audio::GuardSoundPoint(glm::vec3 point)
{
	// Each coordinate whose magnitude is above 5000 is cleared (exactly 5000, or NaN, is kept)
	const auto guard = [](float v) { return std::abs(static_cast<double>(v)) > 5000.0 ? 0.0f : v; };
	return {guard(point.x), guard(point.y), guard(point.z)};
}

std::optional<glm::vec3> audio::OwnerSoundPoint(const Owner& owner)
{
	std::optional<glm::vec3> point;
	switch (owner.kind)
	{
	case Owner::Kind::None:
		// The camera's point
		point = CameraPoint();
		break;
	case Owner::Kind::Atmos:
		// The atmos owner gives none
		return std::nullopt;
	case Owner::Kind::SoundTag:
		// A tag without a thing answers with the point it was given. For a new anim effect (asked on the channel just
		// allocated, before the start writes the options' point) that is the channel's previous point, a stale value:
		// openblack gives the tag's own point (approximate; no caller starts an anim effect owned by a tag)
		point = tags::TagSoundPoint(owner.id);
		if (!point)
		{
			point = tags::Point(owner.id);
		}
		break;
	case Owner::Kind::Key:
		// a plain number is never given to the 3D function (the voices' keys play 2D or untracked) (inferred)
		return std::nullopt;
	default:
		// An unavailable thing gives none; else its sound position
		point = OwnerPosition(owner);
		break;
	}
	if (!point)
	{
		return std::nullopt;
	}
	// The point handed back, guarded
	return GuardSoundPoint(*point);
}

std::optional<glm::vec3> audio::ListenerPoint()
{
	return CameraPoint();
}

float audio::IslandAltitude(float x, float z)
{
	auto& state = AudioSystemData();
	return state.queries.landAltitude ? state.queries.landAltitude(x, z) : 0.0f;
}

int32_t audio::SurfaceType(glm::vec3 point)
{
	auto& state = AudioSystemData();
	return state.queries.surfaceType ? state.queries.surfaceType(point) : 6;
}

bool audio::OwnerUnavailable(const Owner& owner)
{
	auto& state = AudioSystemData();
	// Only a thing can be unavailable; no owner and the atmos are not tested
	if (owner.kind != Owner::Kind::Thing || !state.queries.thingPosition)
	{
		return false;
	}
	return !state.queries.thingPosition(static_cast<ThingId>(owner.thing)).has_value();
}

void audio::AudioManager::RegisterObject(uint32_t id, ObjectPositionFn position)
{
	AudioSystemData().objects[id] = std::move(position);
}

void audio::AudioManager::UnregisterObject(uint32_t id)
{
	AudioSystemData().objects.erase(id);
}

uint32_t audio::AudioManager::NewObjectId()
{
	// (openblack) 0 is no owner's: the numbers start at 1 and are not reused
	return ++AudioSystemData().nextObjectId;
}

// ---- PlaySoundEffect ----------------------------------------------------------------------------------------------

Channel audio::PlaySoundEffectOptions(const sample_play::Options& options)
{
	if (!AudioSystemData().initialised)
	{
		return k_NoChannel;
	}
	const auto* sound = sample_play::GetSound(options.sound);
	if (sound == nullptr)
	{
		if (SfxTrace())
		{
			SPDLOG_LOGGER_INFO(spdlog::get("audio"), "SFX: sound {:#x} not loaded", options.sound);
		}
		return k_NoChannel;
	}
	// A 3D sample with a sample number
	if (options.is3D && sound->id != 0)
	{
		const auto camera = CameraPoint();
		// The .sad's max distance (raw), the options' when 0
		const float maxDistance = sound->maxDistance != 0.0f ? sound->maxDistance : options.maxDistance;
		if (camera)
		{
			// The squared camera distance of pos + offset (inside the citadel from another camera, the same point here)
			const auto d = options.position + options.offset - *camera;
			const float distanceSq = glm::dot(d, d);
			if (distanceSq > maxDistance * maxDistance)
			{
				if (Trace())
				{
					SPDLOG_LOGGER_INFO(spdlog::get("audio"), "Sample play: {} culled, camera {:.1f} > max {:.1f}", sound->name,
					                   std::sqrt(distanceSq), maxDistance);
				}
				if (SfxTrace())
				{
					TraceSoundEffect(options, *sound, k_NoChannel,
					                 fmt::format("culled (camera {:.1f} > max {:.1f})", std::sqrt(distanceSq), maxDistance));
				}
				return k_NoChannel;
			}
		}
	}
	if (const char* reason = FilterReason(*sound, sound->bank, options.owner, options.is3D && options.track))
	{
		if (SfxTrace())
		{
			TraceSoundEffect(options, *sound, k_NoChannel, fmt::format("filtered ({})", reason));
		}
		return k_NoChannel;
	}
	const auto channel = sample_play::Start(options);
	if (SfxTrace())
	{
		TraceSoundEffect(options, *sound, channel, "no channel");
	}
	return channel;
}

Channel audio::AudioManager::PlayAnimationEffect(Owner owner, float distance, const AnimKey& key, AnimAction action,
                                                 BankId bank, bool track, float minDistance, float maxDistance)
{
	// An action goes to the key variant with no filter and no min / max
	if (action != AnimAction::Play)
	{
		return anim_effects::PlayKey(owner, distance, key, action, track, bank, 0.0f, 0.0f);
	}
	// The anim effect's sample number; 0 = nothing
	const int sample = anim_effects::Number(key, bank);
	if (sample == 0 || !AudioSystemData().initialised)
	{
		return k_NoChannel;
	}
	const auto* sound = sample_play::GetSound(SampleId(bank, sample));
	if (sound == nullptr)
	{
		return k_NoChannel;
	}
	// The user parameter's filters, the banks after SET_GAME_SOUND, the interface states, and an unavailable owner
	// when tracking (no is3D test)
	const char* reason = FilterReason(*sound, bank, owner, track);
	const auto channel =
	    reason != nullptr ? k_NoChannel : anim_effects::Play(owner, distance, sample, track, bank, minDistance, maxDistance);
	if (SfxTrace())
	{
		SPDLOG_LOGGER_INFO(
		    spdlog::get("audio"),
		    "SFX: anim effect {{{}, {}, {}, {}, {}}} -> {}/{} ({}) 3D track {} distance {:.1f} pitch {} owner {} "
		    "-> {}",
		    key[0], key[1], key[2], key[3], key[4], BankGroup(bank), sample, sound->name, track ? 1 : 0, distance,
		    ChannelPitch(channel), OwnerText(owner),
		    reason != nullptr        ? fmt::format("filtered ({})", reason)
		    : channel != k_NoChannel ? fmt::format("channel {}", channel)
		                             : std::string("not started (800 / max distance, or no channel)"));
	}
	return channel;
}

Channel audio::AudioManager::PlaySoundEffect(const PlayOptions& options)
{
	sample_play::Options lh = options;
	if (lh.sound == 0)
	{
		lh.sound = SampleId(options.sample.bank, options.sample.number);
	}
	return PlaySoundEffectOptions(lh);
}

// ---- the script's switches ----------------------------------------------------------------------------------------

void audio::SetGameSound(bool enabled)
{
	// SET_GAME_SOUND
	if (!enabled)
	{
		StopAllSoundEffects();
		GetScriptAudioState().gameSoundOff = 1;
		return;
	}
	GetScriptAudioState().gameSoundOff = 0;
}

void audio::SetScriptWideScreen(bool on)
{
	auto& state = AudioSystemData();
	// (openblack test hook, audio session) OPENBLACK_AUDIO_TEST_NO_WIDESCREEN=1: the audio never sees the script's wide
	// screen (the Land 1 intro holds it until a click), to compare the anim effects with the wide screen filters off
	static const bool k_Ignore = std::getenv("OPENBLACK_AUDIO_TEST_NO_WIDESCREEN") != nullptr;
	if (k_Ignore)
	{
		on = false;
	}
	if (Trace() && on != state.scriptWideScreen)
	{
		// the samples of user parameter 1 and the villagers' footsteps are skipped while it is on
		SPDLOG_LOGGER_INFO(spdlog::get("audio"), "Sample play: script wide screen {}", on ? "on" : "off");
	}
	state.scriptWideScreen = on;
}

bool audio::IsScriptWideScreen()
{
	return AudioSystemData().scriptWideScreen;
}

bool audio::IsInsideCitadel()
{
	auto& state = AudioSystemData();
	return state.queries.insideCitadel && state.queries.insideCitadel();
}

void audio::AudioManager::MusicStop(int fade)
{
	// MusicEngine::Stop, under the music thread's lock
	if (auto* system = music::Get(); system != nullptr)
	{
		const std::lock_guard<std::recursive_mutex> lock(system->GetMutex());
		system->GetEngine().Stop(fade);
	}
}

void audio::AudioManager::LeaveCitadel()
{
	// Samples 2 and 12 of the InGame bank, no owner
	StopSoundEffect(2, Owner {}, SfxBank::InGame);
	StopSoundEffect(12, Owner {}, SfxBank::InGame);
}

bool audio::IsVideoPlaying()
{
	auto& state = AudioSystemData();
	return state.queries.videoPlaying && state.queries.videoPlaying();
}

// ---- life cycle ---------------------------------------------------------------------------------------------------

void audio::AudioManager::Init(GameQueries queries)
{
	auto& state = AudioSystemData();
	state.queries = std::move(queries);
	sample_play::Backend backend {
	    .camera = []() { return CameraPoint(); },
	    .ownerPosition = [](const Owner& owner) { return OwnerPosition(owner); },
	};
	sample_play::SetBackend(std::move(backend));
	// The setup's sample main volume
	if (Locator::config::has_value())
	{
		auto& config = Locator::config::value();
		if (const char* volume = std::getenv("OPENBLACK_TEST_SAMPLE_VOLUME"); volume != nullptr)
		{
			config.audioSampleMainVolume = static_cast<uint32_t>(std::atoi(volume));
		}
		sample_play::SetMainVolume(static_cast<int>(config.audioSampleMainVolume));
	}
	state.initialised = true;
	if (auto logger = spdlog::get("audio"))
	{
		SPDLOG_LOGGER_INFO(logger, "GAudio: sample master volume {}", sample_play::MainVolume());
	}
	// The sample banks, and with them the atmos ones: every .sad of Audio\ (Banks.h). (approximate) the original
	// registers the 14 atmos banks later, when the game finishes its initialisation; nothing plays in between
	banks::LoadAll();
}

void audio::AudioManager::Shutdown()
{
	auto& state = AudioSystemData();
	// Everything stopped, the sources released before the context
	sample_play::StopAll();
	sample_play::ReleaseSources();
	sample_play::SetBackend({});
	state.objects.clear();
	anim_effects::Clear();
	state.initialised = false;
}

bool audio::AudioManager::SoundExists()
{
	// Whether a wave device was installed. (approximate) here the audio is initialised on the OpenAL device
	// (device::Open; without one the channels' output is the NullSampleOutput)
	return AudioSystemData().initialised && device::IsOpen();
}

uint32_t audio::AudioManager::TickCount()
{
	// The one real clock of src/Audio (Device.h)
	return device::TickCount();
}

bool audio::AudioManager::SfxTrace()
{
	static const bool k_Trace = std::getenv("OPENBLACK_SFX_TRACE") != nullptr;
	return k_Trace;
}

void audio::AudioManager::ProcessTurn()
{
	if (!AudioSystemData().initialised)
	{
		return;
	}
	// At the end of the turn: the sound map. Its volumes read the sky type, which only the sky drawing writes, once a
	// frame (sky_type::SampleFrame from the Renderer): the value of the last frame drawn, one frame behind the turn's
	// visual time
	sound_map::Update(sky_type::Frame());
	// The sound tags (the street lanterns' tags among the others; openblack first gives the lanterns made since the
	// last turn their tag, as their creation does in the original)
	lantern_sounds::ProcessTurn();
	tags::ProcessSoundTags();
	// ProcessAudioGameTurn past turn 5 (unsigned), else the atmos silenced
	const uint32_t turn = game_clock::Turn();
	if (turn > 5)
	{
		ProcessAudioGameTurn(turn);
	}
	else
	{
		atmos_banks::Silence();
	}
}

void audio::AudioManager::ProcessCitadelTurn()
{
	if (!AudioSystemData().initialised)
	{
		return;
	}
	// Inside the citadel the temple's turn runs the audio game turn alone, on the world's frozen turn
	ProcessAudioGameTurn(game_clock::Turn());
}

void audio::AudioManager::Paused()
{
	// The end of a paused turn: the atmos silenced
	atmos_banks::Silence();
}

void audio::AudioManager::UpdateFrame()
{
	// the 16 channels' finite loops (QMixer counts them as it mixes)
	sample_play::UpdateFrame();
	// the options dialog's slider applies the sample main volume at once; setting the same value again does nothing
	if (AudioSystemData().initialised && Locator::config::has_value())
	{
		sample_play::SetMainVolume(static_cast<int>(Locator::config::value().audioSampleMainVolume));
	}
}

void audio::AudioManager::OnThingDeleted(entt::entity thing)
{
	sample_play::OnThingDeleted(thing);
}

void audio::AudioManager::ClearMap()
{
	// the map's sound tags go with the objects of ClearMap
	lantern_sounds::Clear();
	tags::Clear();
	// The audio reset's music part (the music stopped and switched off and on, every thing music info released)
	{
		const auto lock = game_music::Lock();
		if (auto* gameMusic = game_music::Get(); gameMusic != nullptr)
		{
			gameMusic->Reset();
		}
	}
	// The atmos silenced
	atmos_banks::Clear();
	// Every sample stopped, the output switched off (stopping all again), the wait for no sample playing (nothing to
	// wait for here), the info list cleared (a no-op while inactive) and the output switched back on
	sample_play::StopAll();
	sample_play::Switch(false);
	sample_play::ClearChannelOwners();
	sample_play::Switch(true);
	// (openblack) the channels' OpenAL sources: a new map starts with none
	sample_play::ReleaseSources();
}

void audio::AudioManager::OnFocus(bool active)
{
	// The global switch: the samples and the music (midi and CD audio unused)
	if (Trace())
	{
		SPDLOG_LOGGER_INFO(spdlog::get("audio"), "GAudio: LHGlobalSwitch({})", active ? 1 : 0);
	}
	sample_play::Switch(active);
	if (auto* system = music::Get(); system != nullptr)
	{
		system->With([active](MusicEngine& engine) { engine.Switch(active ? 1 : 0); });
	}
}

void audio::AudioManager::SetSampleMainVolume(int volume)
{
	// More than 127 is ignored
	if (volume < 0 || volume > qmixer::k_MaxVolume)
	{
		return;
	}
	if (Locator::config::has_value())
	{
		Locator::config::value().audioSampleMainVolume = static_cast<uint32_t>(volume);
	}
	sample_play::SetMainVolume(volume);
}

int audio::AudioManager::SampleMainVolume()
{
	return sample_play::MainVolume();
}
