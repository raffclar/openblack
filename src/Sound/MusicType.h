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

#include <string_view>

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

} // namespace openblack::audio
