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
#include <optional>
#include <span>
#include <vector>

#include "Audio/Game/AudioSystem.h"

// The voice of the two advisors (docs/bw1-notes/audio.md): the sound part of AdvisorSpiritController (its dudes: 0 the good
// spirit, 1 the evil one) and of AdvisorSpirit. The visual part (the models, their flight, the mouth poses and the tag
// walker, both fed by LipSyncThisFrame) is Help/Spirits.h and Help/SpiritsRuntime.h; the tags' source (the waves' cue
// labels) and the advisors' sound effect anims are not ported.
//
// A sentence plays on HelpSprites (6) with the advisor's owner directly on the sample player: none of PlaySoundEffect's
// filters applies. Only one advisor speaks at a time: the speaker is the dude that last called SaySentence and the
// sentence the sample it plays. The delays are in milliseconds of audio::TickCount, once a frame, not in game turns.

namespace openblack::audio::advisor
{

/// AdvisorSpiritController's dudes (good, evil)
inline constexpr int k_AdvisorCount = 2;
inline constexpr int k_GoodSpirit = 0;
inline constexpr int k_EvilSpirit = 1;
/// The window of the lip-sync analysis, in samples
inline constexpr int k_LipSyncWindow = 0x200;

/// The time ComputeLipSyncKey was asked for and the three mouth weights it gives
struct VoiceKey
{
	float time {0.0f};
	std::array<float, 3> weights {}; ///< By AutoVoiceParams::index
};

/// The lip-sync analysis' parameters, with the values AdvisorSpirit's init writes
struct AutoVoiceParams
{
	float offsetMs {0.0f};   ///< Added to the time, in ms
	float threshold {0.05f}; ///< A total band level below it is silence
	float level {2.5f};      ///< The total level's gain (capped at 1)
	float rate {40.0f};      ///< How fast a weight may move, x dt x 0.18
	struct Band
	{
		float low;  ///< Hz
		float high; ///< Hz
		float gain;
	};
	/// 200..400 Hz x 0.85, 400..700 Hz x 1.1, 700..10000 Hz x 10
	std::array<Band, 3> bands {{{200.0f, 400.0f, 0.85f}, {400.0f, 700.0f, 1.1f}, {700.0f, 10000.0f, 10.0f}}};
	/// Which weight of the key each band drives (0, 1, 2)
	std::array<int, 3> index {0, 1, 2};
	/// The bands' levels of the last ComputeLipSyncKey (normalised), and the total
	std::array<float, 3> bandLevel {};
	float total {0.0f};
};

/// Numerical Recipes' complex FFT (four1) in place on `data` (re, im pairs: data.size() / 2 points, a power of two),
/// isign 1 or -1.
/// The game's FPU runs at 24-bit precision: every arithmetic step rounds to a float (the double constants 2 pi, 0.5,
/// -2, 1, 0 are applied exactly), only the sine keeps its full precision (sin(theta) stays unrounded).
/// (approximate) openblack's sin is the double one, not the FPU's extended precision one.
void FastFourierTransform(std::span<float> data, int isign);
/// The n = pcm.size() samples under a triangle window that rises by 1 / (n * 32768) a sample to the middle and falls back
/// (int16 -> -1..1 x the window), as complex numbers; FastFourierTransform on the first 2n floats of out, isign 1; then
/// out[i] = sqrt((re^2 + im^2) / n) for i < n. `out` holds 2n floats.
void Analyse(std::span<const int16_t> pcm, std::span<float> out);
/// The mean of spectrum[trunc(low / rate * n) .. trunc(high / rate * n)), both capped at n = spectrum.size(); 0 when
/// empty
[[nodiscard]] float BandLevel(std::span<const float> spectrum, float rate, float low, float high);
/// The window of PCM centred at (offsetMs * 0.001 + dt + t) * rate, its three bands, and the key's weights moved
/// towards the loudest band's shape at most dt * rate * 0.18 down / twice that up a call, normalised when they add up
/// past 1. `pcm` holds the sentence's frames; `spectrum` holds 2 x window floats. (As the original: a window of pure
/// silence divides 0 by 0, the weights go NaN.)
void ComputeLipSyncKey(AutoVoiceParams& params, VoiceKey& key, float dt, float t, std::span<const int16_t> pcm, float rate,
                       std::span<float> spectrum, int window);

/// AdvisorSpiritController's init on both dudes (HelpSprites, at HelpSystem's creation): their bank and its number of samples
void Init(BankId bank);
/// For the tests only: every sentence stopped and the dudes' state as their init leaves it. Not in the original:
/// AdvisorSpiritController lives as long as HelpSystem (its reset does not touch it; only its uninit deletes it), and at a map
/// change the audio reset stops every sample, ending the advisor's channel, after which IsTalking clears the sentence
/// by itself. No game code calls it.
void Reset();

/// v = |hover| - 0.95 (0.95f); the delay is 0 for v < 0, else min((v + 1) * 250, 500) ms; SaySentence; the advisor
/// is active (the hover is the advisor's hover x, given once a frame by help::spirits through SetHover; 0 until then)
void Say(int advisor, int sample, bool onlyIfSilent);
/// The hover x of the advisor's flight (Help/Spirits.h), for Say's delay
void SetHover(int advisor, float hover);
/// The advisor becomes the speaker; with a sentence playing, nothing if onlyIfSilent and IsTalking, else StopSentence;
/// nothing for a sample outside 1..count; the sample (bank, 2D, the advisor's owner, its PCM kept) starts at
/// TickCount() + delay, then UpdateSaySentence
void SaySentence(int advisor, int sample, bool onlyIfSilent, uint32_t delayMs);
/// Once a frame, for each advisor: UpdateSaySentence (the delayed start: the sample played with its PCM kept, the start
/// tick and the sentence's PCM, frames, rate and duration; the sentence is the channel's sample) and ApplyLipSync(dt).
/// `dt`: the frame's seconds.
void Update(float dt);
/// Only the speaker; a start still waiting counts (and the last talk tick = now); else whether the sentence plays with
/// the advisor's owner (and the last talk tick = now); a sentence that ended is stopped (StopSentence)
[[nodiscard]] bool IsTalking(int advisor);
/// IsTalking, or less than 200 ms since the last talk tick
[[nodiscard]] bool TalkingOrJustStopped(int advisor);
/// 0 while the start waits, the sentence's percentage done while < 1 (both refresh the last talk tick), else 1 (also
/// for an advisor that is not the speaker)
[[nodiscard]] float PercentageDone(int advisor);
/// The delayed start cancelled; for the speaker with a sentence, the sample stopped (the 20 ms ramp), no sentence and
/// no speaker
void StopSentence(int advisor);
/// StopSentence and the advisor's interrupted and active flags cleared
void Stop(int advisor);
/// The advisor was given a sentence by Say (until Stop)
[[nodiscard]] bool Active(int advisor);
/// On a click of the help interface (arg 1 from some callers): an active advisor that talks and was not interrupted
/// yet picks an "interruption" text (arg 0 and a random 1 in 2: HELP_TEXT 0xE44 + rand 8 for the good one / 0xE51 +
/// rand 8; else 0xE3F + rand 5 / 0xE4C + rand 5), its sample from the voice table, Stop(advisor), and outside the
/// citadel, if PercentageDone < 0.9, Say(advisor, sample, 0) and marks it interrupted. (As the original:
/// PercentageDone is read after the stop, which leaves no speaker, so it is 1 and the interruption is never said.)
void Interrupt(int advisor, int arg);
/// The advisors part of HelpSystem::IsTextRead: either advisor active and TalkingOrJustStopped
[[nodiscard]] bool AnyTalking();
/// The advisor's key after the last ApplyLipSync (ComputeLipSyncKey on the PCM kept by PlaySample)
[[nodiscard]] VoiceKey LipSyncKey(int advisor);
/// The sentence's time in seconds as ApplyLipSync last set it
[[nodiscard]] float SentenceTime(int advisor);
/// What the last ApplyLipSync left for the mouth (help::spirits, Help/Spirits.h LipSyncFrame)
struct LipSyncRun
{
	/// The tick time, or the play position x 0.001 when it is >= 0; the tag walker takes it in both cases
	float time {0.0f};
	/// the play position was >= 0: the vowels run on the key
	bool playing {false};
};
/// nullopt when the last ApplyLipSync stopped at IsTalking or at no sentence
[[nodiscard]] std::optional<LipSyncRun> LipSyncThisFrame(int advisor);
/// sqrt of the sum of (s / 32768)^2 over `window` PCM samples of the sentence from trunc(t / duration * frames),
/// t = (TickCount() - start) * 0.001; 0 for sample 0, t < 0 or past the duration, no PCM or the sample not playing on
/// HelpSprites with the advisor's owner
[[nodiscard]] float Amplitude(int sample, int window);
/// The sample the speaker plays, 0 for none
[[nodiscard]] int Sentence();
/// The speaking dude, -1 for none
[[nodiscard]] int Speaker();

/// The PCM of the sentence and its frames / rate, for the tests
struct SentencePcm
{
	std::vector<int16_t> samples;
	int frames {0};
	int rate {0};
	float duration {0.0f};
};
[[nodiscard]] const SentencePcm& Pcm();

} // namespace openblack::audio::advisor
