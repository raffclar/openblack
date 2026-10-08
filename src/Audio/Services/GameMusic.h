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
#include <mutex>
#include <optional>
#include <string_view>
#include <vector>

#include <glm/vec3.hpp>

#include "Audio/Game/BankTables.h"
#include "Audio/GameQueries.h"
#include "Audio/Services/ThingMusic.h"

// The game's music: the 85 music banks, the group positions, the script music, the alignment and tribe music, the
// music attached to objects and ProcessMusic, over MusicEngine. Every call must be made under the music lock
// (game_music::Lock()), the one the music thread holds when the engine calls back (end of a track, markers).
// See docs/bw1-notes/audio.md (Music).

namespace openblack::audio
{

class MusicBank;
class MusicEngine;
struct ScriptAudioState;

/// The alignment -1..1 as 0..6: (a + 1) / 2 * 7 capped at 6, truncated
[[nodiscard]] int DiscreteAlignment(float alignment);
/// The table {0, 0, 1, 1, 1, 2, 2} (0 evil, 1 neutral, 2 good), 1 from 7 on
[[nodiscard]] int AlignmentIndex(int discrete);
/// The tribe music, 5 (CELTIC_TOWN_NEUTRAL) from tribe 9 on, else the table
/// {4, 4, 7, 10, 13, 16, 19, 22, 25} + alignment index
[[nodiscard]] int TribeMusicType(int alignmentIndex, int tribe);
/// The chant of a tribe, 5 (CELTIC_TOWN_NEUTRAL) from tribe 9 on, else the table
/// {28, 28, 30, 32, 34, 36, 38, 40, 42} (*_CHANT), + 1 (*_CHANT_VOX) with more than 8 dancers
[[nodiscard]] int ChantMusicType(int tribe, uint32_t dancers);

class GameMusic
{
public:
	/// The bank of a music type (nullptr when its registration failed)
	using BankProvider = std::function<MusicBank*(MusicType type)>;

	/// townTriggerDistance / townTriggerOffDistance, read from info.dat
	struct TownTrigger
	{
		float distance;
		float offDistance;
	};

	/// The 85 banks (through the provider, which registers them), then one position per music group, all 1.
	/// engine may be null (no OpenAL): then nothing plays and IsInstalled is false.
	GameMusic(MusicEngine* engine, const BankProvider& banks, ScriptAudioState& script, GameQueries queries,
	          TownTrigger townTrigger);
	~GameMusic();
	GameMusic(const GameMusic&) = delete;
	GameMusic& operator=(const GameMusic&) = delete;

	/// The original asks whether the sample system is installed; here the music engine's existence (approximate:
	/// openblack has no such sample system yet)
	[[nodiscard]] bool IsInstalled() const { return _engine != nullptr; }

	/// The music part of the audio reset (game start, map cleared)
	void Reset();
	/// The music part of the audio game turn: ProcessMusic when the audio is active, then always the purge of the
	/// music of things no longer available
	void UpdateTurn(bool waveActive);
	void ProcessMusic();

	/// The script music type; with a type, it starts again even if it is the same
	void StartScriptMusic(int type);

	// --- the CHL functions -------------------------------------------------------------------------------------------
	/// START_MUSIC: an error outside 0..0x55 (and it goes on), the music line and beat back to 0, StartScriptMusic
	void ScriptStartMusic(int type);
	/// STOP_MUSIC: StartScriptMusic(0)
	void ScriptStopMusic() { StartScriptMusic(0); }
	/// MUSIC_PLAYED 350: the script type is no longer type; nullopt without audio (the original then runs
	/// TEXT_READ)
	[[nodiscard]] std::optional<bool> ScriptMusicPlayed(int type) const;
	/// LAST_MUSIC_LINE: the music line >= the truncated line (unsigned); nullopt without audio (TEXT_READ)
	[[nodiscard]] std::optional<bool> ScriptLastMusicLine(float line) const;
	/// ATTACH_MUSIC: an error outside 1..84, then AddThingMusic anyway if the script's thing is
	/// valid (nullopt: the script gave none)
	void ScriptAttachMusic(int type, std::optional<ThingId> thing);
	/// GET_MUSIC_ENUM_DISTANCE: outside 1..84 an error and a push of 0, then (always) GetPlayDistance(type):
	/// the values pushed, in order
	[[nodiscard]] std::vector<float> ScriptMusicTypeDistances(int type) const;

	// --- ThingMusicInfo --------------------------------------------------------------------------------------------
	void AddThingMusic(int type, ThingId thing);
	void RemoveThingMusic(ThingId thing) { _thingMusic.Remove(thing); }
	/// MOVE_MUSIC
	void MoveThingMusic(ThingId from, ThingId to) { _thingMusic.Move(from, to); }
	/// ENABLE_DISABLE_MUSIC
	void EnableThingMusic(ThingId thing, int on) { _thingMusic.Enable(thing, on); }
	/// SET_MUSIC_PLAY_POSITION
	void SetPlayPosition(ThingId thing, glm::vec3 point) { _thingMusic.SetPlayPosition(thing, point); }
	void RestartThingMusic(ThingId thing);
	[[nodiscard]] int IsThingMusicFinished(ThingId thing) { return _thingMusic.IsFinished(thing); }
	/// GET_MUSIC_OBJ_DISTANCE: GetPlayDistance of the thing's type, 0 without an info
	[[nodiscard]] float ThingMusicDistance(ThingId thing);
	/// The max distance of the type's bank, 100 without a bank or if negative
	[[nodiscard]] float GetPlayDistance(int type) const;

	// --- state (debug window, tests) -----------------------------------------------------------------------------
	[[nodiscard]] int GetScriptType() const { return _scriptType; }
	[[nodiscard]] int GetScriptStarted() const { return _scriptStarted; }
	[[nodiscard]] int GetAlignmentType() const { return _alignmentType; }
	[[nodiscard]] uint32_t GetSilenceTurns() const { return _silenceTurns; }
	[[nodiscard]] int GetFinishedType() const { return _finishedType; }
	[[nodiscard]] std::optional<uint32_t> GetCurrentTown() const { return _currentTown; }
	[[nodiscard]] int GetCitadelSamplesStopped() const { return _citadelSamplesStopped; }
	[[nodiscard]] const std::vector<int>& GetGroupPositions() const { return _positions; }
	[[nodiscard]] const ThingMusicList& GetThingMusic() const { return _thingMusic; }
	/// The debug message: "Music Playing=<music type name>" or "Music Playing=NONE"
	[[nodiscard]] std::string_view GetPlayingMessage() const { return _playing; }
	[[nodiscard]] MusicBank* GetBank(int type) const;

private:
	/// Every group position 1
	void ResetPositions();
	/// Before each play, for each playing channel with a group > 0, pos[group - 1] = its
	/// audible chunk + 2
	void SavePositions();
	/// The position of a group as the callers read it
	[[nodiscard]] int GroupPosition(int group) const;

	bool ProcessCitadelMusic();
	bool ProcessScriptMusic();
	/// The chant of the worship site near the camera (GameQueries::chantSite), 3D at its
	/// dance centre
	bool ProcessChantMusic();
	bool ProcessThingMusic();
	bool PlayThingMusic(ThingMusicInfo& info, glm::vec3 thingPosition);
	/// A bank, and the camera nearer than GetPlayDistance(type) to the thing (3D)
	[[nodiscard]] bool ThingMusicInRange(int type, glm::vec3 thingPosition) const;
	/// The type's bank plays on a channel that is not free
	[[nodiscard]] bool IsThingMusicPlaying(const ThingMusicInfo& info) const;
	/// Stops its channel, with the given fade
	void StopThingMusic(const ThingMusicInfo& info, int fade);
	/// The infos of things no longer available go (every turn)
	void PurgeThingMusic();
	bool ProcessAlignmentMusic();
	int AlignmentMusicType(const CameraState& camera);

	/// The callbacks of the play options (end of a track, markers)
	void OnScriptMusicFinished(int type);
	void OnScriptMusicMarker(std::string_view label);
	void OnAlignmentMusicFinished(int group);

	void SetPlaying(std::string_view message);

	MusicEngine* _engine;
	ScriptAudioState& _script;
	GameQueries _queries;
	TownTrigger _townTrigger;
	std::array<MusicBank*, static_cast<size_t>(MusicType::_COUNT)> _banks {};
	std::vector<int> _positions;
	int _alignmentType {-1};
	uint32_t _silenceTurns {0};
	int _finishedType {0};
	int _scriptType {0};
	int _scriptStarted {0};
	ThingMusicList _thingMusic;
	std::optional<uint32_t> _currentTown;
	/// The samples were stopped on entering the citadel (a global in the original: the reset does not clear it)
	int _citadelSamplesStopped {0};
	std::string_view _playing {"Music Playing=NONE"};
	/// The engine keeps the callbacks after a channel is freed: they reach this object only while it lives
	std::shared_ptr<GameMusic*> _self;
};

/// The game's music (one)
namespace game_music
{
/// After music::Start: the GameMusic over the music system (or without one)
void Start(GameQueries queries, GameMusic::TownTrigger townTrigger);
/// Before music::Shutdown
void Shutdown();
/// The music part of the end of each game turn, under the lock; and the test hook
/// OPENBLACK_TEST_SCRIPT_MUSIC="<type>[@<turn>]" (START_MUSIC(type) at that game turn, 30 by default)
void ProcessTurn(uint32_t turn, bool waveActive);
/// The lock of every call to Get(): the music system's (the engine calls back under it), or a lock of its own
[[nodiscard]] std::unique_lock<std::recursive_mutex> Lock();
/// nullptr before Start
[[nodiscard]] GameMusic* Get();
} // namespace game_music

} // namespace openblack::audio
