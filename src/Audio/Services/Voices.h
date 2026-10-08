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

#include <string>
#include <string_view>
#include <vector>

#include <glm/vec3.hpp>

#include "Audio/Engine/SamplePlay.h"
#include "Audio/Game/BankTables.h"

// The voices of the help texts: the table HELP_TEXT -> {bank, sample}, rebuilt from the data because
// it is not in any data file: the name of a text is the name of the wave of its sample (see docs/bw1-notes/audio.md,
// Voices and texts). Playing them: RunTextVoice, Say, IsSaying, CutByClick below; the advisors in
// Advisor.h.

namespace openblack::audio
{

/// The owners of the voice channels
enum class VoiceOwner : uint32_t
{
	Advisor = 0x270C,   ///< An advisor's sentence (the two spirits)
	SayExtra = 0x270D,  ///< GAME_PLAY_SAY_SOUND_EFFECT with alt
	StopOnly = 0x270E,  ///< only STOP_SOUND_EFFECT(isSay) stops it; nothing plays it
	Narration = 0x270F, ///< RUN_TEXT not by a spirit, and GAME_PLAY_SAY_SOUND_EFFECT without alt
};

/// One entry of the voice table: bank 0 = no voice; sample 1-based in the bank
struct TextVoice
{
	SfxBank bank {SfxBank::None};
	uint32_t sample {0};

	/// A bank and a sample
	[[nodiscard]] bool HasVoice() const { return bank != SfxBank::None && sample != 0; }
};

/// The key a wave name is matched by: the last component of the sample's wave name (e.g.
/// "K:\4frosty\Spanish\1622p\HELP_TEXT_X.wav") without ".wav", in upper case
[[nodiscard]] std::string VoiceSampleKey(std::string_view waveName);

class VoiceTable
{
public:
	/// The wave names of a bank, in sample order: sample n is element n - 1
	using SampleNames = std::vector<std::string>;

	/// Text i (the i-th ADD_TEXT) has the first sample whose wave is named like the text, looked up in villagers (7),
	/// then HelpSprites (6), then Guidance (10). It gives the original table exactly (6974 texts, 1922 in
	/// HelpSprites, 1328 in villagers, 227 in Guidance). The only name in two banks, HELP_TEXT_LAND_2_WORKSHOP_10
	/// (HelpSprites 801 and villagers 399), is villagers 399 in the original: hence villagers first (inferred: one
	/// case). Never VillagersBanter (8) or SpellDialogue (9): the original gives none of their samples to a text (35
	/// names would match).
	[[nodiscard]] static VoiceTable Build(const std::vector<std::string>& textNames, const SampleNames& villagers,
	                                      const SampleNames& helpSprites, const SampleNames& guidance);

	/// The entry of a text; no voice for an id out of the table
	[[nodiscard]] TextVoice Get(uint32_t textId) const;
	[[nodiscard]] size_t Size() const { return _voices.size(); }

private:
	std::vector<TextVoice> _voices;
};

namespace voices
{
/// The dialogue banks of the table: villagers (7), HelpSprites (6) and Guidance (10)
[[nodiscard]] bool IsTableBank(SfxBank bank);
/// The wave names of a dialogue bank, as Game reads the .sad of k_SfxBankPaths (call before BuildTable)
void SetBankSampleNames(SfxBank bank, VoiceTable::SampleNames names);
/// Builds the game's table from the help texts (helptext::GetEntry names) and the names set so far
void BuildTable();
/// The game's table (empty until BuildTable)
[[nodiscard]] const VoiceTable& Table();
/// The table of the tests
void SetTable(VoiceTable table);

/// The bank of a voice is registered
[[nodiscard]] bool BankRegistered(SfxBank bank);

/// The voice of a shown text, after it is shown and kept in the history:
///  - HelpSprites (6) and narrator 2 (the good spirit) / 3 (the evil one): advisor::Stop of the evil spirit, then of
///    the good one, then advisor::Say (owner Advisor, played directly);
///  - any other voice with a sample and a bank: default play options with the bank, the sample and the owner
///    Narration: 2D, the .sad decides the rest; PlaySoundEffect (with its filters: inside the citadel only user
///    parameter 2 plays, and after SET_GAME_SOUND false only HelpSprites / villagers).
/// Returns the channel of the second branch (the advisor's sentence starts in advisor::Update).
Channel RunTextVoice(int32_t narrator, TextVoice voice);

/// GAME_PLAY_SAY_SOUND_EFFECT: text 0 for one >= 6974; nothing without a sample in the say table (the original zeroes
/// the bank when the entry's id is not the text, which never happens); default play options with the bank, the
/// sample, the owner alt ? SayExtra : Narration, is3D = withPosition, the point only when withPosition, no tracking;
/// PlaySoundEffect. No text and no advisor, even for HelpSprites.
Channel Say(uint32_t textId, bool withPosition, bool alt, glm::vec3 position);

/// SAY_SOUND_EFFECT_PLAYING(alt, text): text 0 for one >= 6974; false without a sample in the say table; else
/// whether the owner alt ? SayExtra : Narration plays that sample of the entry's bank (the id is not compared here)
[[nodiscard]] bool IsSaying(bool alt, uint32_t textId);

/// A click in the interface stops every Narration sample of villagers.sad, each with the 20 ms ramp. (The narration
/// of the other banks is not cut; the advisors are cut by advisor::Interrupt.)
void CutByClick();
} // namespace voices

} // namespace openblack::audio
