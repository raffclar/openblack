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
#include <ctime>

#include <array>
#include <functional>
#include <string>
#include <string_view>

#include "Audio/Audio.h"

// The spooky voices: at night by the computer's clock, now and then, a voice of Guidance.sad whispers the player's
// name, picked once among the 100 HELP_TEXT_SPOOKY_NAMES_* by a Soundex comparison with the profile's name. Notes:
// docs/bw1-notes/audio.md (spooky voices).
//
// The state is a single static object: the bank, the options, the sample, a counter and a countdown. The spooky voice
// info of info.dat (5 entries) is never read by this code.

namespace openblack::audio::spooky
{

/// The HELP_TEXT of the 100 names (HELP_TEXT_SPOOKY_NAMES_01..100 = 4586..4685)
inline constexpr uint32_t k_FirstName = 4586;
inline constexpr size_t k_Names = 100;

/// The computer's clock, time() in seconds (tests); unset: the real one
using ClockFn = std::function<int64_t()>;
void SetClock(ClockFn clock);
/// time()
[[nodiscard]] int64_t UnixTime();
/// localtime of UnixTime
[[nodiscard]] std::tm LocalTime();
/// The local hour >= 23 or <= 5, or 20:45..20:59
[[nodiscard]] bool NightNow(const std::tm& time);

/// Not a letter: 0; else by (c | 0x60) - 0x61: a e i o u 0, b f p v 1, c g j k q s x z 2, d t 3, l 4, m n 5, r 6, and
/// h w y the character itself. (inferred) letters as _isalpha in the "C" locale: ASCII letters.
[[nodiscard]] int SoundexDigit(char16_t c);
/// Skips characters of code 0 up to the end or a space (0); after a coded
/// character, when the next one has the same code, the code + 1 is returned, again while the next one equals that
/// (the pointer is not moved past it, as the original)
[[nodiscard]] int GetNextSoundexCode(const char16_t*& p);
/// The same first character (exact), not the end, then three
/// GetNextSoundexCode of each from the second character equal
[[nodiscard]] bool SoundsAlike(const char16_t* text, const char16_t* word);
/// One of the name's words (split at spaces) passes SoundsAlike
[[nodiscard]] bool SoundexOverlap(const char16_t* text, const char16_t* name);
/// For the 100 names in order, the first whose text (the help text entry, entry 0 for an id out of 1..count-1)
/// overlaps the name -> its sample from the voice table; 0 when none or for an empty name
[[nodiscard]] uint32_t FindSoundAlikeName(std::u16string_view name);
/// The text of a HELP_TEXT for FindSoundAlikeName (tests); unset: helptext::GetEntry
using TextFn = std::function<std::u16string(uint32_t textId)>;
void SetNameTexts(TextFn texts);
/// The profile's name (GameQueries::profileName), then the network name and the registry's
/// "Software\Microsoft\MS Setup (ACME)\User Info" DefName: those two are not ported (openblack has no network login;
/// the old Office setup key)
[[nodiscard]] uint32_t GetName();

/// At the game's one-time init: new options (defaults) with the bank Guidance, the sample = GetName, the counter 0, the
/// countdown 100
void Init();
/// At each game init: the sample = GetName again
void UpdatePlayerName();
/// The options freed
void Shutdown();
/// Once a game turn: nothing on lands 1 and 2 or without a sample; the countdown runs down and at 0 it is 100 again
/// (then 99); then, at night (NightNow), r = LocalFloatRand(1): trunc((1 - r^3) 1000) < the counter ->
/// PlaySpookyVoice, else guidance::OneOff(1); the counter + 1
void Process();
/// a = LocalFloatRand(0.65), LocalRand(2) ? p = 1 + a^3 : 1 / (1 + a^3); field2C = LocalRand(180);
/// b = LocalFloatRand(0.8), the same for q; the options' sample, pitch trunc(100 p), volume trunc(volume x q) (the
/// options keep it: it builds up), 2D; PlaySoundEffect; the counter = 0
void PlaySpookyVoice();

struct State
{
	uint32_t sample {0};
	uint32_t counter {0};
	uint32_t countdown {0};
	bool initialised {false};
	sample_play::Options options;
	BankId bank {k_NoBank};
	int field2C {
	    90}; ///< The options' value PlaySpookyVoice picks (0..179): recorded only (not modelled by sample_play::Options)
};
[[nodiscard]] const State& GetState();
/// For the tests: the sample and the countdown set
void SetForTests(uint32_t sample, uint32_t counter, uint32_t countdown);

} // namespace openblack::audio::spooky
