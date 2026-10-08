/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <cstddef>
#include <cstdint>

#include <array>
#include <string_view>

// The two bank tables of the game's audio, copied from the original game (version 1.20).
// Paths are relative to the game root and are opened as they are, without a language folder;
// the original file system is case-insensitive (e.g. "Villagers.sad" is villagers.sad on disk).

namespace openblack::audio
{

/// AUDIO_SFX_BANK_TYPE: index of k_SfxBankPaths and of the registered sound effect banks
enum class SfxBank : uint8_t
{
	None = 0,
	InGame = 1,
	Editor = 2,
	Spells = 3,
	Creature = 4,
	ScriptSfx = 5,
	HelpSprites = 6,
	Villagers = 7,
	VillagersBanter = 8,
	SpellDialogue = 9,
	Guidance = 10,

	_COUNT
};

/// Sound effect and dialogue banks (11 paths). The game clears the 11 bank slots and then registers, for types 0..10,
/// each path that is not null into its slot if the slot is still empty.
/// An empty view is the null pointer of type 0.
inline constexpr std::array<std::string_view, static_cast<size_t>(SfxBank::_COUNT)> k_SfxBankPaths = {
    std::string_view {},                  // 0 (null)
    "audio/sfx/game/ingame.sad",          // 1
    "audio/sfx/game/editor.sad",          // 2
    "audio/sfx/game/spells.sad",          // 3
    "audio/sfx/creature/creature.sad",    // 4
    "audio/sfx/script/scriptsfx.sad",     // 5
    "audio/dialogue/HelpSprites.sad",     // 6
    "audio/dialogue/Villagers.sad",       // 7 (villagers.sad on disk)
    "audio/dialogue/VillagersBanter.sad", // 8
    "audio/dialogue/SpellDialogue.sad",   // 9
    "audio/dialogue/Guidance.sad",        // 10
};

[[nodiscard]] constexpr std::string_view SfxBankPath(SfxBank bank)
{
	return k_SfxBankPaths[static_cast<size_t>(bank)];
}

/// MUSIC_TYPE: index of k_MusicBanks and of the registered music banks. The game stops at 84
/// (SCRIPT_CREATURE_END_SEQUENCE): later values (MISSIONARIES_INTRO...) are not in version 1.20.
enum class MusicType : uint8_t
{
	None = 0,
	GenericEvil = 1,
	GenericNeutral = 2,
	GenericGood = 3,
	CelticTownEvil = 4,
	CelticTownNeutral = 5,
	CelticTownGood = 6,
	AztecTownEvil = 7,
	AztecTownNeutral = 8,
	AztecTownGood = 9,
	JapaneseTownEvil = 10,
	JapaneseTownNeutral = 11,
	JapaneseTownGood = 12,
	IndianTownEvil = 13,
	IndianTownNeutral = 14,
	IndianTownGood = 15,
	EgyptianTownEvil = 16,
	EgyptianTownNeutral = 17,
	EgyptianTownGood = 18,
	GreekTownEvil = 19,
	GreekTownNeutral = 20,
	GreekTownGood = 21,
	NorseTownEvil = 22,
	NorseTownNeutral = 23,
	NorseTownGood = 24,
	TibetanTownEvil = 25,
	TibetanTownNeutral = 26,
	TibetanTownGood = 27,
	CelticChant = 28,
	CelticChantVox = 29,
	AztecChant = 30,
	AztecChantVox = 31,
	JapaneseChant = 32,
	JapaneseChantVox = 33,
	IndianChant = 34,
	IndianChantVox = 35,
	EgyptianChant = 36,
	EgyptianChantVox = 37,
	GreekChant = 38,
	GreekChantVox = 39,
	NorseChant = 40,
	NorseChantVox = 41,
	TibetanChant = 42,
	TibetanChantVox = 43,
	CitadelEvil = 44,
	CitadelNeutral = 45,
	CitadelGood = 46,
	ScriptPiperTune = 47,
	ScriptPiperCaveTune = 48,
	ScriptHermit = 49,
	ScriptMissionariesBackground = 50,
	ScriptMissionariesVerse1 = 51,
	ScriptMissionariesVerse2 = 52,
	ScriptMissionariesVerse3 = 53,
	ScriptIntro = 54,
	ScriptSingingStoneCircle = 55,
	ScriptWelcomeDance = 56,
	ScriptGeneric01 = 57,
	ScriptGeneric02 = 58,
	ScriptGeneric03 = 59,
	ScriptGeneric04 = 60,
	ScriptEpic01 = 61,
	ScriptEpic02 = 62,
	ScriptEpic03 = 63,
	ScriptEpic04 = 64,
	ScriptCreatureChosen = 65,
	ScriptFuneral = 66,
	ScriptCreatureGuide = 67,
	ScriptKhazar = 68,
	ScriptNemesis = 69,
	ScriptTwinkle = 70,
	ScriptWhistleFuneral = 71,
	ScriptWhistleTwinkle = 72,
	ScriptSleg = 73,
	CreatureFight = 74,
	CreatureBigFight = 75,
	ScriptGuardianStone = 76,
	Outro = 77,
	ScriptFailure = 78,
	ScriptGregorian = 79,
	ScriptChristmas = 80,
	ScriptGregorian3D = 81,
	ScriptCircus = 82,
	ScriptCircus3D = 83,
	ScriptCreatureEndSequence = 84,

	_COUNT
};

/// One entry of the music bank table: a path and a name.
struct MusicBankEntry
{
	std::string_view path; ///< empty = null pointer (only MUSIC_TYPE_NONE)
	std::string_view name; ///< the game's MUSIC_TYPE_* string
};

/// Music banks (85 entries). The audio system registers them all at start-up: for each non-null path (and without
/// -NOLOADMUSIC), if the path is not found it uses "%c:\%s" with the CD drive, then registers the bank into slot i.
/// Quirks kept as in the original:
/// - NORSE_TOWN_* (22..24) point at the very same strings as CELTIC_TOWN_*: there is no Norse alignment music.
/// - CITADEL_EVIL/NEUTRAL/GOOD (44..46) are the same string (one citadel.sad).
/// - SCRIPT_MISSIONARIES_VERSE_1..3 (51..53) are in audio/dialogue/, but they are music banks (BankInfo flag 1).
/// - SCRIPT_WELCOME_DANCE (56) names FollowUsWelcome.sad, which is not in the installation: the registration fails and the
///   bank stays null. No script uses it.
/// - Audio\Music\script\MissionariesSad.sad is on disk but in no entry.
inline constexpr std::array<MusicBankEntry, static_cast<size_t>(MusicType::_COUNT)> k_MusicBanks = {{
    {std::string_view {}, "MUSIC_TYPE_NONE"},                                                       // 0
    {"audio/music/align/evil.sad", "MUSIC_TYPE_GENERIC_EVIL"},                                      // 1
    {"audio/music/align/neutral.sad", "MUSIC_TYPE_GENERIC_NEUTRAL"},                                // 2
    {"audio/music/align/good.sad", "MUSIC_TYPE_GENERIC_GOOD"},                                      // 3
    {"audio/music/align/celt_evil.sad", "MUSIC_TYPE_CELTIC_TOWN_EVIL"},                             // 4
    {"audio/music/align/celt_neutral.sad", "MUSIC_TYPE_CELTIC_TOWN_NEUTRAL"},                       // 5
    {"audio/music/align/celt_good.sad", "MUSIC_TYPE_CELTIC_TOWN_GOOD"},                             // 6
    {"audio/music/align/aztc_evil.sad", "MUSIC_TYPE_AZTEC_TOWN_EVIL"},                              // 7
    {"audio/music/align/aztc_neutral.sad", "MUSIC_TYPE_AZTEC_TOWN_NEUTRAL"},                        // 8
    {"audio/music/align/aztc_good.sad", "MUSIC_TYPE_AZTEC_TOWN_GOOD"},                              // 9
    {"audio/music/align/japn_evil.sad", "MUSIC_TYPE_JAPANESE_TOWN_EVIL"},                           // 10
    {"audio/music/align/japn_neutral.sad", "MUSIC_TYPE_JAPANESE_TOWN_NEUTRAL"},                     // 11
    {"audio/music/align/japn_good.sad", "MUSIC_TYPE_JAPANESE_TOWN_GOOD"},                           // 12
    {"audio/music/align/indn_evil.sad", "MUSIC_TYPE_INDIAN_TOWN_EVIL"},                             // 13
    {"audio/music/align/indn_neutral.sad", "MUSIC_TYPE_INDIAN_TOWN_NEUTRAL"},                       // 14
    {"audio/music/align/indn_good.sad", "MUSIC_TYPE_INDIAN_TOWN_GOOD"},                             // 15
    {"audio/music/align/egpt_evil.sad", "MUSIC_TYPE_EGYPTIAN_TOWN_EVIL"},                           // 16
    {"audio/music/align/egpt_neutral.sad", "MUSIC_TYPE_EGYPTIAN_TOWN_NEUTRAL"},                     // 17
    {"audio/music/align/egpt_good.sad", "MUSIC_TYPE_EGYPTIAN_TOWN_GOOD"},                           // 18
    {"audio/music/align/grek_evil.sad", "MUSIC_TYPE_GREEK_TOWN_EVIL"},                              // 19
    {"audio/music/align/grek_neutral.sad", "MUSIC_TYPE_GREEK_TOWN_NEUTRAL"},                        // 20
    {"audio/music/align/grek_good.sad", "MUSIC_TYPE_GREEK_TOWN_GOOD"},                              // 21
    {"audio/music/align/celt_evil.sad", "MUSIC_TYPE_NORSE_TOWN_EVIL"},                              // 22 (= 4)
    {"audio/music/align/celt_neutral.sad", "MUSIC_TYPE_NORSE_TOWN_NEUTRAL"},                        // 23 (= 5)
    {"audio/music/align/celt_good.sad", "MUSIC_TYPE_NORSE_TOWN_GOOD"},                              // 24 (= 6)
    {"audio/music/align/tbtn_evil.sad", "MUSIC_TYPE_TIBETAN_TOWN_EVIL"},                            // 25
    {"audio/music/align/tbtn_neutral.sad", "MUSIC_TYPE_TIBETAN_TOWN_NEUTRAL"},                      // 26
    {"audio/music/align/tbtn_good.sad", "MUSIC_TYPE_TIBETAN_TOWN_GOOD"},                            // 27
    {"audio/music/chant/celt_chant.sad", "MUSIC_TYPE_CELTIC_CHANT"},                                // 28
    {"audio/music/chant/celt_chant_vox.sad", "MUSIC_TYPE_CELTIC_CHANT_VOX"},                        // 29
    {"audio/music/chant/aztc_chant.sad", "MUSIC_TYPE_AZTEC_CHANT"},                                 // 30
    {"audio/music/chant/aztc_chant_vox.sad", "MUSIC_TYPE_AZTEC_CHANT_VOX"},                         // 31
    {"audio/music/chant/japn_chant.sad", "MUSIC_TYPE_JAPANESE_CHANT"},                              // 32
    {"audio/music/chant/japn_chant_vox.sad", "MUSIC_TYPE_JAPANESE_CHANT_VOX"},                      // 33
    {"audio/music/chant/indn_chant.sad", "MUSIC_TYPE_INDIAN_CHANT"},                                // 34
    {"audio/music/chant/indn_chant_vox.sad", "MUSIC_TYPE_INDIAN_CHANT_VOX"},                        // 35
    {"audio/music/chant/egpt_chant.sad", "MUSIC_TYPE_EGYPTIAN_CHANT"},                              // 36
    {"audio/music/chant/egpt_chant_vox.sad", "MUSIC_TYPE_EGYPTIAN_CHANT_VOX"},                      // 37
    {"audio/music/chant/grek_chant.sad", "MUSIC_TYPE_GREEK_CHANT"},                                 // 38
    {"audio/music/chant/grek_chant_vox.sad", "MUSIC_TYPE_GREEK_CHANT_VOX"},                         // 39
    {"audio/music/chant/nrse_chant.sad", "MUSIC_TYPE_NORSE_CHANT"},                                 // 40
    {"audio/music/chant/nrse_chant_vox.sad", "MUSIC_TYPE_NORSE_CHANT_VOX"},                         // 41
    {"audio/music/chant/tbtn_chant.sad", "MUSIC_TYPE_TIBETAN_CHANT"},                               // 42
    {"audio/music/chant/tbtn_chant_vox.sad", "MUSIC_TYPE_TIBETAN_CHANT_VOX"},                       // 43
    {"audio/music/citadel/citadel.sad", "MUSIC_TYPE_CITADEL_EVIL"},                                 // 44
    {"audio/music/citadel/citadel.sad", "MUSIC_TYPE_CITADEL_NEUTRAL"},                              // 45 (= 44)
    {"audio/music/citadel/citadel.sad", "MUSIC_TYPE_CITADEL_GOOD"},                                 // 46 (= 44)
    {"audio/music/script/pipertune_m.sad", "MUSIC_TYPE_SCRIPT_PIPER_TUNE"},                         // 47
    {"audio/music/script/pipercave_m.sad", "MUSIC_TYPE_SCRIPT_PIPER_CAVE_TUNE"},                    // 48
    {"audio/music/script/Hermit.sad", "MUSIC_TYPE_SCRIPT_HERMIT"},                                  // 49
    {"audio/music/script/MissionariesBackground.sad", "MUSIC_TYPE_SCRIPT_MISSIONARIES_BACKGROUND"}, // 50
    {"audio/dialogue/MissionariesVerse1.sad", "MUSIC_TYPE_SCRIPT_MISSIONARIES_VERSE_1"},            // 51
    {"audio/dialogue/MissionariesVerse2.sad", "MUSIC_TYPE_SCRIPT_MISSIONARIES_VERSE_2"},            // 52
    {"audio/dialogue/MissionariesVerse3.sad", "MUSIC_TYPE_SCRIPT_MISSIONARIES_VERSE_3"},            // 53
    {"audio/music/intro/intro.sad", "MUSIC_TYPE_SCRIPT_INTRO"},                                     // 54
    {"audio/music/script/singingstonesa.sad", "MUSIC_TYPE_SCRIPT_SINGING_STONE_CIRCLE"},            // 55
    {"audio/music/script/FollowUsWelcome.sad", "MUSIC_TYPE_SCRIPT_WELCOME_DANCE"},                  // 56 (no file)
    {"audio/music/script/Script01.sad", "MUSIC_TYPE_SCRIPT_GENERIC_01"},                            // 57
    {"audio/music/script/Script02.sad", "MUSIC_TYPE_SCRIPT_GENERIC_02"},                            // 58
    {"audio/music/script/Script03.sad", "MUSIC_TYPE_SCRIPT_GENERIC_03"},                            // 59
    {"audio/music/script/Script04.sad", "MUSIC_TYPE_SCRIPT_GENERIC_04"},                            // 60
    {"audio/music/script/Epic01.sad", "MUSIC_TYPE_SCRIPT_EPIC_01"},                                 // 61
    {"audio/music/script/Epic02.sad", "MUSIC_TYPE_SCRIPT_EPIC_02"},                                 // 62
    {"audio/music/script/Epic03.sad", "MUSIC_TYPE_SCRIPT_EPIC_03"},                                 // 63
    {"audio/music/script/Epic04.sad", "MUSIC_TYPE_SCRIPT_EPIC_04"},                                 // 64
    {"audio/music/script/CreatureChosen.sad", "MUSIC_TYPE_SCRIPT_CREATURE_CHOSEN"},                 // 65
    {"audio/music/script/Funeral.sad", "MUSIC_TYPE_SCRIPT_FUNERAL"},                                // 66
    {"audio/music/script/CreatureGuide.sad", "MUSIC_TYPE_SCRIPT_CREATURE_GUIDE"},                   // 67
    {"audio/music/script/Khazar.sad", "MUSIC_TYPE_SCRIPT_KHAZAR"},                                  // 68
    {"audio/music/script/Nemesis.sad", "MUSIC_TYPE_SCRIPT_NEMESIS"},                                // 69
    {"audio/music/script/Twinkle.sad", "MUSIC_TYPE_SCRIPT_TWINKLE"},                                // 70
    {"audio/music/script/WhistleFuneral.sad", "MUSIC_TYPE_SCRIPT_WHISTLE_FUNERAL"},                 // 71
    {"audio/music/script/WhistleTwinkle.sad", "MUSIC_TYPE_SCRIPT_WHISTLE_TWINKLE"},                 // 72
    {"audio/music/script/Sleg.sad", "MUSIC_TYPE_SCRIPT_SLEG"},                                      // 73
    {"audio/music/script/creaturefight.sad", "MUSIC_TYPE_CREATURE_FIGHT"},                          // 74
    {"audio/music/script/creatureBigfight.sad", "MUSIC_TYPE_CREATURE_BIG_FIGHT"},                   // 75
    {"audio/music/script/guardianstone.sad", "MUSIC_TYPE_SCRIPT_GUARDIAN_STONE"},                   // 76
    {"audio/music/outro/outro.sad", "MUSIC_TYPE_OUTRO"},                                            // 77
    {"audio/music/script/failure.sad", "MUSIC_TYPE_SCRIPT_FAILURE"},                                // 78
    {"audio/music/script/gregorian.sad", "MUSIC_TYPE_SCRIPT_GREGORIAN"},                            // 79
    {"audio/music/script/christmas.sad", "MUSIC_TYPE_SCRIPT_CHRISTMAS"},                            // 80
    {"audio/music/script/gregorian3d.sad", "MUSIC_TYPE_SCRIPT_GREGORIAN_3D"},                       // 81
    {"audio/music/script/circus.sad", "MUSIC_TYPE_SCRIPT_CIRCUS"},                                  // 82
    {"audio/music/script/circus3d.sad", "MUSIC_TYPE_SCRIPT_CIRCUS_3D"},                             // 83
    {"audio/music/script/creatureendsequence.sad", "MUSIC_TYPE_SCRIPT_CREATURE_END_SEQUENCE"},      // 84
}};

[[nodiscard]] constexpr const MusicBankEntry& MusicBankFor(MusicType type)
{
	return k_MusicBanks[static_cast<size_t>(type)];
}

} // namespace openblack::audio
