/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "GameMusic.h"

#include <cmath>
#include <cstdlib>

#include <algorithm>
#include <string>
#include <utility>

#include <glm/geometric.hpp>
#include <spdlog/spdlog.h>

#include "Audio/Engine/MusicBank.h"
#include "Audio/Engine/MusicEngine.h"
#include "Audio/Engine/MusicStream.h"
#include "Audio/Engine/SamplePlay.h"
#include "Audio/Services/ScriptAudioState.h"
#include "Debug/DebugEnv.h"
#include "ECS/Systems/AudioStateInterface.h"
#include "Locator.h"

namespace openblack::audio
{

namespace
{
/// The debug message of the music playing: "Music Playing=" and the music type's name
std::string_view PlayingMessage(int type)
{
	static const auto k_Messages = []() {
		std::array<std::string, static_cast<size_t>(MusicType::_COUNT)> messages;
		for (size_t i = 0; i < messages.size(); ++i)
		{
			messages[i] = "Music Playing=" + std::string(k_MusicBanks[i].name);
		}
		return messages;
	}();
	if (type < 0 || type >= static_cast<int>(MusicType::_COUNT))
	{
		// (openblack) the original reads past its name table; never reached: a type out of 0..84 has no bank here,
		// so nothing plays
		return "Music Playing=?";
	}
	return k_Messages[static_cast<size_t>(type)];
}

constexpr std::string_view k_PlayingNone = "Music Playing=NONE";

std::shared_ptr<spdlog::logger> Logger()
{
	return spdlog::get("audio");
}

std::shared_ptr<spdlog::logger> ScriptLogger()
{
	return spdlog::get("scripting");
}

/// A script error is logged and the script goes on
void ScriptError(std::string_view message)
{
	if (auto logger = ScriptLogger())
	{
		SPDLOG_LOGGER_ERROR(logger, "{}", message);
	}
}

/// OPENBLACK_MUSIC_TRACE, read once at start-up
const bool k_Trace = debug_env::MusicTrace();
} // namespace

int DiscreteAlignment(float alignment)
{
	// (a + 1) / 2 * 7, capped at 6, then truncated. The game runs the FPU at single precision, so each step rounds to
	// a float
	float value = (alignment - -1.0f) / (1.0f - -1.0f) * 7.0f;
	if (!(value < 6.0f))
	{
		value = 6.0f;
	}
	return static_cast<int>(value);
}

int AlignmentIndex(int discrete)
{
	// From 7 on 1 (neutral); else the table (0 evil, 1 neutral, 2 good)
	constexpr std::array<int, 7> k_Table = {0, 0, 1, 1, 1, 2, 2};
	if (discrete >= 7)
	{
		return 1;
	}
	// below 0 the original reads before the table (approximate: neutral); DiscreteAlignment of -1..1 is 0..6
	if (discrete < 0)
	{
		return 1;
	}
	return k_Table[static_cast<size_t>(discrete)];
}

int TribeMusicType(int alignmentIndex, int tribe)
{
	// From tribe 9 on 5 = CELTIC_TOWN_NEUTRAL whatever the alignment; else the tribe's first *_TOWN_EVIL music + the
	// alignment index
	constexpr std::array<int, 9> k_Table = {4, 4, 7, 10, 13, 16, 19, 22, 25};
	if (tribe >= 9)
	{
		return 5;
	}
	// below 0 the original reads before the table (approximate: as from 9 on)
	if (tribe < 0)
	{
		return 5;
	}
	return k_Table[static_cast<size_t>(tribe)] + alignmentIndex;
}

int ChantMusicType(int tribe, uint32_t dancers)
{
	// From tribe 9 on 5 = CELTIC_TOWN_NEUTRAL; else the tribe's chant (the African tribe shares the Celtic one), + 1
	// with more than 8 dancers: the _vox version, the whole chant with the voices, played alone
	constexpr std::array<int, 9> k_Table = {28, 28, 30, 32, 34, 36, 38, 40, 42};
	if (tribe >= 9)
	{
		return 5;
	}
	// below 0 the original reads before the table (approximate: as from 9 on; a site always has a tribe)
	if (tribe < 0)
	{
		return 5;
	}
	return k_Table[static_cast<size_t>(tribe)] + (dancers > 8 ? 1 : 0);
}

GameMusic::GameMusic(MusicEngine* engine, const BankProvider& banks, ScriptAudioState& script, GameQueries queries,
                     TownTrigger townTrigger)
    : _engine(engine)
    , _script(script)
    , _queries(std::move(queries))
    , _townTrigger(townTrigger)
    , _self(std::make_shared<GameMusic*>(this))
{
	// The banks of the 85 entries that have a path
	for (size_t i = 0; i < _banks.size(); ++i)
	{
		_banks[i] = k_MusicBanks[i].path.empty() || !banks ? nullptr : banks(static_cast<MusicType>(i));
	}
	// One position per music group (13 with the game's banks), all reset
	_positions.assign(_engine != nullptr ? _engine->GetTotalGroups() : 0u, 1);
	ResetPositions();
}

GameMusic::~GameMusic()
{
	*_self = nullptr;
}

MusicBank* GameMusic::GetBank(int type) const
{
	// Outside 0..84 the original reads past its bank table (85 is always 0 when ProcessScriptMusic looks: no bank)
	// (approximate: no bank)
	if (type < 0 || type >= static_cast<int>(_banks.size()))
	{
		return nullptr;
	}
	return _banks[static_cast<size_t>(type)];
}

void GameMusic::ResetPositions()
{
	// Every group starts at chunk 1
	std::fill(_positions.begin(), _positions.end(), 1);
}

void GameMusic::SavePositions()
{
	// Only while the music engine is active
	if (_engine == nullptr || !_engine->IsActive())
	{
		return;
	}
	for (int i = 0; i < k_MusicChannelCount; ++i)
	{
		const auto& ch = _engine->GetChannel(i);
		// a playing channel of a valid group (group > 0, group - 1 below the group count)
		if (ch.status != MusicStatus::Playing || ch.group <= 0)
		{
			continue;
		}
		const auto index = static_cast<uint32_t>(ch.group - 1);
		if (index >= _engine->GetTotalGroups() || index >= _positions.size())
		{
			continue;
		}
		_positions[index] = static_cast<int>(ch.playingChunk) + 2; // the audible chunk + 2
	}
}

int GameMusic::GroupPosition(int group) const
{
	// Group 0 (BigFight, the 3D banks) reads the word before the array in the original (approximate: 0, which the
	// engine starts at chunk 1)
	if (group <= 0 || static_cast<size_t>(group) > _positions.size())
	{
		return 0;
	}
	return _positions[static_cast<size_t>(group - 1)];
}

void GameMusic::SetPlaying(std::string_view message)
{
	if (message != _playing && k_Trace)
	{
		if (auto logger = Logger())
		{
			SPDLOG_LOGGER_INFO(logger, "game music: {}", message);
		}
	}
	_playing = message;
}

void GameMusic::Reset()
{
	_currentTown.reset();
	ResetPositions();
	_scriptType = 0;
	_finishedType = 0;
	_scriptStarted = 0;
	// the camera's alignment is not kept here: it comes from GameQueries::cameraAlignment
	_alignmentType = -1;
	if (_engine != nullptr) // with the audio system
	{
		_engine->Stop(0); // cut
		// switched off (stopped, inactive) and on again (nothing restarts), as the original does around the samples'
		// reset
		_engine->Switch(0);
		_engine->Switch(1);
	}
	_thingMusic.Clear();
	SetPlaying(k_PlayingNone);
}

void GameMusic::UpdateTurn(bool waveActive)
{
	// Only while the samples are active (sample_play::IsActive). The ambient and the listener are not music:
	// audio::ProcessTurn does them.
	if (_engine != nullptr && waveActive)
	{
		ProcessMusic();
	}
	PurgeThingMusic(); // always
}

void GameMusic::ProcessMusic()
{
	const auto none = [this]() {
		// No music: "Music Playing=NONE", the script music not started, no alignment music; nothing is stopped
		SetPlaying(k_PlayingNone);
		_scriptStarted = 0;
		_alignmentType = -1;
	};

	if (_queries.videoPlaying && _queries.videoPlaying())
	{
		none();
		return;
	}
	// no music engine, or land 6
	if (_engine == nullptr || !_engine->IsInstalled() || (_queries.landNumber && _queries.landNumber() == 6))
	{
		return;
	}
	if (ProcessCitadelMusic())
	{
		none();
		return;
	}
	if (ProcessScriptMusic())
	{
		_alignmentType = -1;
		return;
	}
	if ((_queries.creatureFightMusic && _queries.creatureFightMusic()) || ProcessChantMusic() ||
	    (_queries.creatureDanceMusic && _queries.creatureDanceMusic()) || ProcessThingMusic())
	{
		none();
		return;
	}
	if (ProcessAlignmentMusic())
	{
		_scriptStarted = 0;
		return;
	}
	SavePositions();
	_engine->Stop(1); // everything fades out
	none();
}

void GameMusic::StartScriptMusic(int type)
{
	_scriptType = type;
	if (type != 0)
	{
		_scriptStarted = 0; // it starts again even if it is the same type
	}
}

bool GameMusic::ProcessCitadelMusic()
{
	// Only while the camera is inside the citadel
	if (!(_queries.insideCitadel && _queries.insideCitadel()))
	{
		_citadelSamplesStopped = 0;
		return false;
	}
	// The first turn inside, every sample stops (the 16 channels, not the ambient mixer's)
	if (_citadelSamplesStopped == 0)
	{
		sample_play::StopAll();
		_citadelSamplesStopped = 1;
		if (k_Trace)
		{
			if (auto logger = Logger())
			{
				SPDLOG_LOGGER_INFO(logger, "(openblack) citadel: LHSampleStopAll");
			}
		}
	}
	// CITADEL_EVIL / NEUTRAL / GOOD by the local player's alignment, the three on citadel.sad (group 4)
	const float alignment = _queries.localPlayerAlignment ? _queries.localPlayerAlignment() : 0.0f;
	const int type = static_cast<int>(MusicType::CitadelEvil) + AlignmentIndex(DiscreteAlignment(alignment));
	auto* bank = GetBank(type);
	if (bank == nullptr)
	{
		return false; // (the stop's latch stays set)
	}
	const int group = bank->GetGroupId();
	// the group position is read before SavePositions saves the positions (ProcessAlignmentMusic saves first)
	const auto optionsStartChunk = GroupPosition(group);
	MusicPlayOptions options {
	    .bank = bank,
	    .volume = 0x7F,
	    .startChunk = optionsStartChunk,
	    .sync = 1,
	    .fade = 1,
	    .is3D = 0,
	    .pitch = 0x64,
	};
	SavePositions();
	_engine->Play(options); // every turn (the engine re-triggers the same bank)
	SetPlaying(PlayingMessage(type));
	return true;
}

bool GameMusic::ProcessChantMusic()
{
	// The worship site with dancers nearest the camera, near the nearest citadel, and that site's dance: the game's
	// side (GameQueries::chantSite)
	const auto site = _queries.chantSite ? _queries.chantSite() : std::nullopt;
	const auto camera = _queries.camera ? _queries.camera() : std::nullopt;
	if (!site || !camera) // (approximate) without a camera there is no chant; the original always has one
	{
		return false;
	}
	// The camera's height (its ground + its height above it, as a float) against the ground of the dance centre:
	// |h - ground| < 100 (below or unordered)
	const float height = site->cameraGround + camera->heightAboveGround;
	const float difference = std::fabs(height - site->ground);
	if (!(difference < 100.0f) && !std::isnan(difference))
	{
		return false;
	}
	const int type = ChantMusicType(site->tribe, site->dancers);
	auto* bank = GetBank(type);
	if (bank == nullptr)
	{
		return false;
	}
	const int group = bank->GetGroupId();
	MusicPlayOptions options {
	    .bank = bank,
	    .volume = 0x7F,
	    // the group position is read before SavePositions saves the positions
	    .startChunk = GroupPosition(group),
	    .sync = 1,
	    .fade = 0,
	    .is3D = 1,
	    .pitch = 0x64,
	    .position = site->position,
	};
	SavePositions();
	const int channel = _engine->Play(options); // every turn (the same bank is re-triggered)
	if (channel != k_NoMusicChannel)
	{
		_engine->Set3DPosition(channel, site->position);
	}
	SetPlaying(PlayingMessage(type));
	return true;
}

bool GameMusic::ProcessScriptMusic()
{
	if (_scriptType == 0)
	{
		// the script music was stopped: fade out
		if (_scriptStarted != 0)
		{
			_scriptStarted = 0;
			_engine->Stop(1);
			_alignmentType = -1;
		}
		return false;
	}
	if (_scriptStarted != 0)
	{
		SetPlaying(PlayingMessage(_scriptType));
		return true;
	}
	// The original computes an alignment index here but ignores it: the bank is the script type's
	auto* bank = GetBank(_scriptType);
	if (bank == nullptr)
	{
		return false;
	}
	MusicPlayOptions options {
	    .bank = bank,
	    .volume = 0x7F,
	    .startChunk = 1,
	    .sync = 0,
	    .fade = 0,
	    .is3D = 0,
	    .pitch = 0x64,
	};
	const std::weak_ptr<GameMusic*> self = _self;
	options.finished = [self](int data) {
		if (auto p = self.lock(); p && *p != nullptr)
		{
			(*p)->OnScriptMusicFinished(data);
		}
	};
	options.marker = [self](std::string_view label) {
		if (auto p = self.lock(); p && *p != nullptr)
		{
			(*p)->OnScriptMusicMarker(label);
		}
	};
	options.userData = _scriptType;
	SavePositions();
	_engine->Play(options);
	_scriptStarted = 1;
	SetPlaying(PlayingMessage(_scriptType));
	return true;
}

void GameMusic::OnScriptMusicFinished(int type)
{
	// the music that ended is still the script's: no script music any more
	if (type == _scriptType)
	{
		_scriptType = 0;
	}
}

void GameMusic::OnScriptMusicMarker(std::string_view label)
{
	// By the marker's first letter: L<n> starts line n, P and W count a beat
	if (label.empty())
	{
		return; // '\0' is below 'L': the default case
	}
	switch (label[0])
	{
	case 'L':
	case 'l':
		// the line number follows the letter; its first beat
		_script.musicLine = static_cast<uint32_t>(std::atoi(std::string(label.substr(1)).c_str()));
		_script.musicBeat = 1;
		break;
	case 'P':
	case 'p':
	case 'W':
	case 'w':
		++_script.musicBeat;
		break;
	default:
		break;
	}
}

void GameMusic::OnAlignmentMusicFinished(int group)
{
	// A valid group (group - 1 below the group count, unsigned) starts again at chunk 1 and the alignment type becomes
	// the finished one (silence counted from 0, no alignment music); a group 0 does nothing
	const auto index = static_cast<uint32_t>(group - 1);
	if (_engine == nullptr || index >= _engine->GetTotalGroups())
	{
		return;
	}
	if (index < _positions.size())
	{
		_positions[index] = 1;
	}
	_finishedType = _alignmentType;
	_silenceTurns = 0;
	_alignmentType = -1;
}

bool GameMusic::ProcessAlignmentMusic()
{
	// (openblack test hook) OPENBLACK_TEST_ALIGNMENT_MUSIC=<turn>: from that game turn on, every turn, as if the script
	// called ENABLE_DISABLE_ALIGNMENT_MUSIC(1) (Land 1's script turns it off at the start), and without the
	// script's wide screen (openblack's Land 1 keeps it on after the intro)
	static const long k_TestEnable = [] {
		const char* env = std::getenv("OPENBLACK_TEST_ALIGNMENT_MUSIC");
		return env != nullptr ? std::strtol(env, nullptr, 10) : -1L;
	}();
	const bool testEnabled = k_TestEnable >= 0 && _queries.turn && _queries.turn() >= static_cast<uint32_t>(k_TestEnable);
	if (testEnabled)
	{
		_script.alignmentMusic = 1;
	}
	// a camera
	const auto camera = _queries.camera ? _queries.camera() : std::nullopt;
	if (!camera)
	{
		return false;
	}
	// not while the script's wide screen is on, nor while its bars move
	if (!testEnabled && ((_queries.scriptWideScreen && _queries.scriptWideScreen()) ||
	                     (_queries.wideScreenChanging && _queries.wideScreenChanging())))
	{
		return false;
	}
	// enabled by ENABLE_DISABLE_ALIGNMENT_MUSIC, and only after game turn 20 (unsigned)
	if (_script.alignmentMusic == 0)
	{
		return false;
	}
	if (!(_queries.turn && _queries.turn() > 0x14))
	{
		return false;
	}
	const int type = AlignmentMusicType(*camera);
	if (type == 0)
	{
		return false;
	}
	// after this type ended by itself, 3500 turns of silence while it stays the chosen one
	if (_finishedType == type)
	{
		++_silenceTurns;
		if (_silenceTurns < 0xDAC)
		{
			return false;
		}
	}
	_finishedType = 0;
	auto* bank = GetBank(type);
	if (bank == nullptr)
	{
		return false;
	}
	if (_alignmentType != type)
	{
		if (k_Trace)
		{
			if (auto logger = Logger())
			{
				const float alignment = _queries.cameraAlignment ? _queries.cameraAlignment() : 0.0f;
				SPDLOG_LOGGER_INFO(logger, "(openblack) alignment music type {} (camera alignment {:.3f}, discrete {})", type,
				                   alignment, DiscreteAlignment(alignment));
			}
		}
		SavePositions();
		const int group = bank->GetGroupId();
		const auto optionsStartChunk = GroupPosition(group);
		MusicPlayOptions options {
		    .bank = bank,
		    .volume = 0x50, // 80
		    .startChunk = optionsStartChunk,
		    .sync = 1,
		    .fade = 1,
		    .is3D = 0,
		    .pitch = 0x64,
		};
		const std::weak_ptr<GameMusic*> self = _self;
		options.finished = [self](int data) {
			if (auto p = self.lock(); p && *p != nullptr)
			{
				(*p)->OnAlignmentMusicFinished(data);
			}
		};
		options.userData = group;
		// loops when it starts in the second half of the bank's segments (signed, the division rounds to 0)
		const auto half = static_cast<int32_t>(bank->GetSegmentCount()) / 2;
		options.loops = options.startChunk >= half ? 1 : 0;
		_engine->Play(options);
		_alignmentType = type;
	}
	SetPlaying(PlayingMessage(type));
	return true;
}

int GameMusic::AlignmentMusicType(const CameraState& camera)
{
	const float alignment = _queries.cameraAlignment ? _queries.cameraAlignment() : 0.0f;
	const int a = AlignmentIndex(DiscreteAlignment(alignment));
	// the nearest town to the camera within townTriggerOffDistance
	const auto town = _queries.nearestTown ? _queries.nearestTown(_townTrigger.offDistance) : std::nullopt;
	// the kept town is forgotten when it is no longer available
	std::optional<MusicTown> current;
	if (_currentTown)
	{
		current = _queries.town ? _queries.town(*_currentTown) : std::nullopt;
		if (!current)
		{
			_currentTown.reset();
		}
	}
	// a town, and the camera lower than townTriggerOffDistance over the land
	if (town && camera.heightAboveGround < _townTrigger.offDistance)
	{
		// within townTriggerDistance (<=) it becomes the kept town
		if (town->distance <= _townTrigger.distance)
		{
			_currentTown = town->id;
			return TribeMusicType(a, town->tribe);
		}
		// another kept town still within townTriggerOffDistance (<) keeps its music; the nearest
		// one itself between the two distances does not
		if (_currentTown && *_currentTown != town->id && current && current->distance < _townTrigger.offDistance)
		{
			return TribeMusicType(a, current->tribe);
		}
	}
	_currentTown.reset();
	return a + 1; // GENERIC_EVIL / NEUTRAL / GOOD
}

void GameMusic::AddThingMusic(int type, ThingId thing)
{
	// only an available thing
	if (!_queries.thingPosition || !_queries.thingPosition(thing))
	{
		return;
	}
	// a thing that has music already only changes its type
	if (auto* info = _thingMusic.Get(thing); info != nullptr)
	{
		info->type = type;
		return;
	}
	// a new info, at the head of the list; if its bank plays already, it is cut
	auto& info = _thingMusic.AddFront(type, thing);
	if (IsThingMusicPlaying(info))
	{
		StopThingMusic(info, 0);
	}
}

void GameMusic::RestartThingMusic(ThingId thing)
{
	auto* info = _thingMusic.Get(thing);
	if (info == nullptr)
	{
		return;
	}
	info->finished = 0;
	info->started = 0;
	if (IsThingMusicPlaying(*info))
	{
		StopThingMusic(*info, 0);
	}
}

float GameMusic::ThingMusicDistance(ThingId thing)
{
	const auto* info = _thingMusic.Get(thing);
	return info != nullptr ? GetPlayDistance(info->type) : 0.0f;
}

float GameMusic::GetPlayDistance(int type) const
{
	// 100 without a bank; else the bank's max distance (that of its first segment), and 100 if it is negative (0
	// stays 0)
	const auto* bank = GetBank(type);
	if (bank == nullptr)
	{
		return 100.0f;
	}
	const float distance = bank->GetMaxDistance();
	return distance < 0.0f ? 100.0f : distance;
}

bool GameMusic::IsThingMusicPlaying(const ThingMusicInfo& info) const
{
	// the type's bank is on a channel that is not free
	const auto* bank = GetBank(info.type);
	if (bank == nullptr || _engine == nullptr)
	{
		return false;
	}
	const int channel = _engine->FindChannelOfBank(bank);
	return channel != k_NoMusicChannel && _engine->GetStatus(channel) != MusicStatus::Free;
}

void GameMusic::StopThingMusic(const ThingMusicInfo& info, int fade)
{
	if (!IsThingMusicPlaying(info))
	{
		return;
	}
	const auto* bank = GetBank(info.type);
	const int channel = _engine->FindChannelOfBank(bank);
	if (channel != k_NoMusicChannel)
	{
		_engine->Stop(channel, fade);
	}
}

bool GameMusic::ThingMusicInRange(int type, glm::vec3 thingPosition) const
{
	// no bank, nothing
	if (GetBank(type) == nullptr)
	{
		return false;
	}
	// the 3D distance to the camera (approximate without a camera: out of range)
	const auto camera = _queries.camera ? _queries.camera() : std::nullopt;
	if (!camera)
	{
		return false;
	}
	const float distance = glm::length(thingPosition - camera->position);
	// in range when GetPlayDistance > the distance
	return GetPlayDistance(type) > distance;
}

bool GameMusic::PlayThingMusic(ThingMusicInfo& info, glm::vec3 thingPosition)
{
	auto* bank = GetBank(info.type);
	if (bank == nullptr)
	{
		return false;
	}
	const int group = bank->GetGroupId();
	if (!ThingMusicInRange(info.type, thingPosition))
	{
		return false; // (the range is the thing's, even with a play position)
	}
	// the play position of SET_MUSIC_PLAY_POSITION, or the thing's
	const glm::vec3 position = info.hasPlayPosition != 0 ? info.playPosition : thingPosition;

	const auto optionsStartChunk = GroupPosition(group);
	MusicPlayOptions options {
	    .bank = bank,
	    .volume = 0x7F,
	    .startChunk = optionsStartChunk,
	    .sync = group > 0 ? 1 : 0,
	    .fade = group > 0 ? 1 : 0,
	    .is3D = 1,
	    .pitch = 0x64,
	    .position = position,
	};
	SavePositions();

	// no channel for the bank, started and finished -> false; finished -> true without playing; else play
	// (every turn: the engine re-triggers the channel that has the bank)
	const int existing = _engine->FindChannelOfBank(bank);
	if (existing == k_NoMusicChannel && info.started != 0 && info.finished != 0)
	{
		return false;
	}
	if (info.finished != 0)
	{
		return true;
	}
	const int channel = _engine->Play(options);
	_engine->Set3DPosition(channel, position);
	info.started = 1;
	return true;
}

bool GameMusic::ProcessThingMusic()
{
	// from the head of the list
	bool inRange = false;
	auto& infos = _thingMusic.GetInfos();
	for (size_t i = 0; i < infos.size();)
	{
		const auto position = _queries.thingPosition ? _queries.thingPosition(infos[i].thing) : std::nullopt;
		if (!position)
		{
			// the thing is no longer available: its info goes
			infos.erase(infos.begin() + static_cast<std::ptrdiff_t>(i));
			continue;
		}
		auto& info = infos[i];
		if (info.enabled != 0 && info.finished == 0)
		{
			// the first one that plays takes the music
			if (PlayThingMusic(info, *position))
			{
				return true;
			}
		}
		else if (ThingMusicInRange(info.type, *position))
		{
			inRange = true; // disabled or finished but in range: no alignment music either
		}
		++i;
	}
	return inRange;
}

void GameMusic::PurgeThingMusic()
{
	auto& infos = _thingMusic.GetInfos();
	infos.erase(std::remove_if(infos.begin(), infos.end(),
	                           [this](const ThingMusicInfo& info) {
		                           return !_queries.thingPosition || !_queries.thingPosition(info.thing);
	                           }),
	            infos.end());
}

void GameMusic::ScriptStartMusic(int type)
{
	// below 0 or above 0x55 an error, and it goes on
	if (type < 0 || type > 0x55)
	{
		ScriptError("Jonty - Strange Music Type in script");
	}
	_script.musicLine = 0;
	_script.musicBeat = 0;
	StartScriptMusic(type);
}

std::optional<bool> GameMusic::ScriptMusicPlayed(int type) const
{
	if (!IsInstalled())
	{
		return std::nullopt; // the original runs TEXT_READ instead
	}
	return _scriptType != type;
}

std::optional<bool> GameMusic::ScriptLastMusicLine(float line) const
{
	if (!IsInstalled())
	{
		return std::nullopt; // the original runs TEXT_READ instead
	}
	// the line truncated, then compared unsigned
	const auto wanted = static_cast<uint32_t>(static_cast<int32_t>(line));
	return _script.musicLine.load() >= wanted;
}

void GameMusic::ScriptAttachMusic(int type, std::optional<ThingId> thing)
{
	// outside 1..0x54 an error; AddThingMusic all the same
	if (type <= 0 || type >= 0x55)
	{
		ScriptError("Jonty - Strange Music Type in script");
	}
	if (thing)
	{
		AddThingMusic(type, *thing);
	}
}

std::vector<float> GameMusic::ScriptMusicTypeDistances(int type) const
{
	std::vector<float> pushes;
	// outside 1..0x54 "Invalid music man!" and a push of 0.0
	if (type <= 0 || type >= 0x55)
	{
		ScriptError("Invalid music man!");
		pushes.push_back(0.0f);
	}
	pushes.push_back(GetPlayDistance(type)); // always
	return pushes;
}

namespace game_music
{
namespace
{
/// What this module keeps between calls (Locator::audioState)
struct GameMusicState
{
	std::unique_ptr<GameMusic> music {};
	std::recursive_mutex fallbackMutex {};
	/// The test hook's start already ran
	bool testDone {false};
};

GameMusicState& GameMusicData()
{
	return openblack::Locator::audioState::value().Get<GameMusicState>();
}
} // namespace

std::unique_lock<std::recursive_mutex> Lock()
{
	if (auto* system = music::Get(); system != nullptr)
	{
		return std::unique_lock(system->GetMutex());
	}
	return std::unique_lock(GameMusicData().fallbackMutex);
}

void Start(GameQueries queries, GameMusic::TownTrigger townTrigger)
{
	auto lock = Lock();
	auto* system = music::Get();
	MusicEngine* engine = system != nullptr ? &system->GetEngine() : nullptr;
	GameMusicData().music = std::make_unique<GameMusic>(
	    engine, [](MusicType type) { return music::GetBank(type); }, GetScriptAudioState(), std::move(queries), townTrigger);
}

void Shutdown()
{
	auto lock = Lock();
	GameMusicData().music.reset();
}

void ProcessTurn(uint32_t turn, bool waveActive)
{
	auto& state = GameMusicData();
	auto lock = Lock();
	if (!state.music)
	{
		return;
	}
	// A test hook, not a behaviour of the original: a START_MUSIC of the script at a given turn
	static const auto k_TestHook = []() -> std::optional<std::pair<int, uint32_t>> {
		const char* spec = std::getenv("OPENBLACK_TEST_SCRIPT_MUSIC");
		if (spec == nullptr)
		{
			return std::nullopt;
		}
		const std::string text(spec);
		const auto at = text.find('@');
		const int type = std::atoi(text.substr(0, at).c_str());
		const auto when = at == std::string::npos ? 30u : static_cast<uint32_t>(std::atoi(text.substr(at + 1).c_str()));
		return std::make_pair(type, when);
	}();
	if (k_TestHook && !state.testDone && turn >= k_TestHook->second)
	{
		state.testDone = true;
		if (auto logger = Logger())
		{
			SPDLOG_LOGGER_INFO(logger, "game music test: START_MUSIC({}) at turn {}", k_TestHook->first, turn);
		}
		state.music->ScriptStartMusic(k_TestHook->first);
	}
	state.music->UpdateTurn(waveActive);
}

GameMusic* Get()
{
	return GameMusicData().music.get();
}
} // namespace game_music

} // namespace openblack::audio
