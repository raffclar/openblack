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
#include <map>
#include <optional>
#include <string_view>
#include <vector>

#include <glm/vec3.hpp>

namespace openblack::audio
{

/// The types of music the game plays, in the order its scripts number them
enum class MusicType : int32_t
{
	None = 0,
	GenericEvil,
	GenericNeutral,
	GenericGood,
	CelticTownEvil,
	CelticTownNeutral,
	CelticTownGood,
	AztecTownEvil,
	AztecTownNeutral,
	AztecTownGood,
	JapaneseTownEvil,
	JapaneseTownNeutral,
	JapaneseTownGood,
	IndianTownEvil,
	IndianTownNeutral,
	IndianTownGood,
	EgyptianTownEvil,
	EgyptianTownNeutral,
	EgyptianTownGood,
	GreekTownEvil,
	GreekTownNeutral,
	GreekTownGood,
	NorseTownEvil,
	NorseTownNeutral,
	NorseTownGood,
	TibetanTownEvil,
	TibetanTownNeutral,
	TibetanTownGood,
	CelticChant,
	CelticChantVox,
	AztecChant,
	AztecChantVox,
	JapaneseChant,
	JapaneseChantVox,
	IndianChant,
	IndianChantVox,
	EgyptianChant,
	EgyptianChantVox,
	GreekChant,
	GreekChantVox,
	NorseChant,
	NorseChantVox,
	TibetanChant,
	TibetanChantVox,
	CitadelEvil,
	CitadelNeutral,
	CitadelGood,
	ScriptPiperTune,
	ScriptPiperCaveTune,
	ScriptHermit,
	ScriptMissionariesBackground,
	ScriptMissionariesVerse1,
	ScriptMissionariesVerse2,
	ScriptMissionariesVerse3,
	ScriptIntro,
	ScriptSingingStoneCircle,
	ScriptWelcomeDance,
	ScriptGeneric01,
	ScriptGeneric02,
	ScriptGeneric03,
	ScriptGeneric04,
	ScriptEpic01,
	ScriptEpic02,
	ScriptEpic03,
	ScriptEpic04,
	ScriptCreatureChosen,
	ScriptFuneral,
	ScriptCreatureGuide,
	ScriptKhazar,
	ScriptNemesis,
	ScriptTwinkle,
	ScriptWhistleFuneral,
	ScriptWhistleTwinkle,
	ScriptSleg,
	CreatureFight,
	CreatureBigFight,
	ScriptGuardianStone,
	Outro,
	ScriptFailure,
	ScriptGregorian,
	ScriptChristmas,
	ScriptGregorian3D,
	ScriptCircus,
	ScriptCircus3D,
	ScriptCreatureEndSequence,

	_COUNT
};

/// The bank the game loads for a music type, relative to the game's directory. Empty for MusicType::None.
[[nodiscard]] std::string_view GetMusicBankPath(MusicType type);
[[nodiscard]] std::string_view GetMusicTypeName(MusicType type);

/// The game's music: once a game turn, picks what music plays and hands it to the audio library's music player.
///
/// In order, the first that wants to play wins: the citadel's music while inside the citadel, music a script has
/// started, and the music of the land under the camera. Over land the music follows the player's alignment, and near a
/// town that of its tribe too. Each music group remembers where it got to, so the land's music carries on in time when
/// it changes and picks up where it left off when it comes back.
// TODO(raffclar): the game also plays creature fight, chant, creature dance and object music before the land's music
class GameMusic
{
public:
	struct Town
	{
		glm::vec3 position;
		/// Tribe, from 0 (celtic) to 8 (tibetan)
		int32_t tribe;
		uint32_t id;
	};

	struct TurnInputs
	{
		uint32_t turn;
		glm::vec3 camera;
		/// Height of the land under the camera
		float groundHeight;
		/// Inside the citadel
		bool inCitadel;
		/// Alignment of the place the camera is in, -1 (evil) to 1 (good): that of the player of most influence there,
		/// neutral where none has any. The land's music follows it.
		float alignment;
		/// The local player's own alignment, -1 (evil) to 1 (good), which the temple's music follows
		float playerAlignment;
		/// A script holds the cinema bars, or they are sliding in or out: the land's music waits
		bool cinema;
		std::vector<Town> towns;
	};

	GameMusic() = default;
	/// Stops the music, which would otherwise call back into it when it ends
	~GameMusic();
	GameMusic(const GameMusic&) = delete;
	GameMusic& operator=(const GameMusic&) = delete;

	/// Picks the music for this game turn
	void ProcessTurn(const TurnInputs& inputs);
	/// Forgets what was playing, when a land is loaded
	void Reset();

	/// Music a script starts: MusicType::None stops it
	void StartScriptMusic(MusicType type);
	/// The script command that turns the alignment music on or off
	void SetAlignmentMusicEnabled(bool enabled) { _alignmentMusicEnabled = enabled; }

	// Debug introspection
	[[nodiscard]] MusicType GetPlaying() const { return _playing; }
	[[nodiscard]] MusicType GetLandType() const { return _landType; }
	[[nodiscard]] MusicType GetBlockedType() const { return _blockedType; }
	[[nodiscard]] uint32_t GetBlockedTurns() const { return _blockedTurns; }
	[[nodiscard]] MusicType GetScriptType() const { return _scriptType; }
	[[nodiscard]] bool IsAlignmentMusicEnabled() const { return _alignmentMusicEnabled; }
	[[nodiscard]] const std::map<int32_t, uint32_t>& GetResumeChunks() const { return _resumeChunks; }

	/// The alignment in seven steps, grouped into the evil, neutral or good index of the music tables
	[[nodiscard]] static int32_t GetAlignmentIndex(float alignment);
	/// The land's music at the camera
	[[nodiscard]] MusicType SelectLandType(const TurnInputs& inputs);
	/// The temple's music, in the version of the local player's own alignment
	[[nodiscard]] static MusicType SelectCitadelType(const TurnInputs& inputs);
	/// Whether the land's music may play this turn: it is turned on, the game is past its first turns, and no script
	/// holds the cinema bars nor are they sliding
	[[nodiscard]] bool LandMusicAllowed(const TurnInputs& inputs) const;

private:
	void ProcessMusic(const TurnInputs& inputs);
	bool ProcessCitadel(const TurnInputs& inputs);
	bool ProcessScript();
	bool ProcessLand(const TurnInputs& inputs);
	/// Remembers where each playing music group has got to
	void SaveResumeChunks();
	[[nodiscard]] uint32_t GetResumeChunk(int32_t groupId) const;

	/// The music type heard
	MusicType _playing {MusicType::None};
	/// The land's music playing, None when other music has taken over
	MusicType _landType {MusicType::None};
	/// The land's music that played to its end: it is not played again for a while
	MusicType _blockedType {MusicType::None};
	uint32_t _blockedTurns {0};
	MusicType _scriptType {MusicType::None};
	bool _scriptStarted {false};
	bool _alignmentMusicEnabled {true};
	std::optional<uint32_t> _rememberedTown;
	/// 1-based chunk each music group carries on from
	std::map<int32_t, uint32_t> _resumeChunks;
};

} // namespace openblack::audio
