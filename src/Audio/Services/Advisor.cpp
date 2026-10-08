/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "Advisor.h"

#include <cmath>
#include <cstdlib>

#include <algorithm>

#include <spdlog/spdlog.h>

#include "Audio/Audio.h"
#include "Audio/Device/Sound.h"
#include "Audio/Device/WaveBuffers.h"
#include "Audio/Services/Voices.h"
#include "Debug/DebugEnv.h"
#include "ECS/Systems/AudioStateInterface.h"
#include "Locator.h"

using namespace openblack;
using namespace openblack::audio;
using namespace openblack::audio::advisor;

namespace
{
/// The sound part of an AdvisorSpirit
struct AdvisorVoice
{
	BankId bank {k_NoBank};
	int samples {0};                   ///< The bank's number of samples
	uint32_t startTick {0};            ///< Tick at which the delayed sentence starts (0 = none)
	int sample {0};                    ///< The sentence's sample
	uint32_t lastTalkTick {0};         ///< The last tick IsTalking / PercentageDone saw it talking
	float hover {0.0f};                ///< help::spirits' hover x (SetHover); 0 until then
	float time {0.0f};                 ///< The sentence's time in seconds
	std::optional<LipSyncRun> lipSync; ///< what the last ApplyLipSync left for the mouth (LipSyncThisFrame)
	AutoVoiceParams params;
	VoiceKey key;
	bool active {false};      ///< Given a sentence by Say (until Stop)
	bool interrupted {false}; ///< Already interrupted once
};

struct AdvisorState
{
	std::array<AdvisorVoice, k_AdvisorCount> dudes;
	int speaker {-1};       ///< The dude that last called SaySentence
	int sentence {0};       ///< The sample the speaker plays
	uint32_t startTick {0}; ///< Tick of the last PlaySample
	SentencePcm pcm;
	/// The spectrum buffer of ApplyLipSync (2 x 512 floats)
	std::array<float, 2 * k_LipSyncWindow> spectrum {};
};

/// The Advisor state (Locator::audioState)
AdvisorState& AdvisorData()
{
	return openblack::Locator::audioState::value().Get<AdvisorState>();
}

bool Trace()
{
	static const bool k_Trace = debug_env::AudioTrace();
	return k_Trace && spdlog::get("audio") != nullptr;
}

AdvisorVoice* Get(int advisor)
{
	return advisor >= 0 && advisor < k_AdvisorCount ? &AdvisorData().dudes[static_cast<size_t>(advisor)] : nullptr;
}

/// TickCount() - since (32-bit wrapping subtraction, as the original)
uint32_t Since(uint32_t since)
{
	return TickCount() - since;
}

/// Plays the advisor's sample directly (no sound effect filter), keeping its PCM; on a start records the tick, a copy
/// of the PCM, its frames, rate and duration
Channel PlaySample(const AdvisorVoice& advisor)
{
	auto& state = AdvisorData();
	sample_play::Options options {
	    .sound = SampleId(advisor.bank, advisor.sample),
	    .is3D = false,
	    .owner = Owner::Key(k_OwnerAdvisor),
	    .keepPcm = true,
	};
	// The advisor's position is not set: a 2D channel does not use it
	const Channel channel = sample_play::Start(options);
	if (channel == k_NoChannel)
	{
		return k_NoChannel;
	}
	state.startTick = TickCount();
	state.pcm = {}; // The previous copy freed
	if (const auto* sound = sample_play::GetSound(options.sound); sound != nullptr)
	{
		// (the original's audio library keeps the converted PCM, keepPcm): the PCM the channel's buffer was just made from,
		// else (the buffer was made before) decoded once more, the same PCM
		wave_buffers::Pcm pcm;
		if (wave_buffers::TakeDecoded(*sound, pcm) || wave_buffers::Decode(*sound, pcm))
		{
			const int channels = pcm.layout == ChannelLayout::Stereo ? 2 : 1;
			state.pcm.samples = std::move(pcm.samples);
			// frames = bytes / (2 * nChannels), duration = frames / sample rate
			state.pcm.frames = static_cast<int>(state.pcm.samples.size()) / channels;
			state.pcm.rate = pcm.sampleRate > 0 ? pcm.sampleRate : sound->sampleRate;
			state.pcm.duration =
			    state.pcm.rate > 0 ? static_cast<float>(state.pcm.frames) / static_cast<float>(state.pcm.rate) : 0.0f;
		}
	}
	return channel;
}

/// Starts the delayed sentence once its tick is reached
void UpdateSaySentence(AdvisorVoice& advisor)
{
	auto& state = AdvisorData();
	if (advisor.startTick == 0)
	{
		return;
	}
	if (static_cast<int32_t>(TickCount()) < static_cast<int32_t>(advisor.startTick)) // Signed comparison
	{
		return;
	}
	advisor.startTick = 0;
	const Channel channel = PlaySample(advisor);
	if (channel == k_NoChannel)
	{
		state.sentence = 0;
		return;
	}
	// The sentence is the channel's sample
	state.sentence = advisor.sample;
	// The original also builds the wave's audio tags here: they drive the advisor's gestures (not ported)
	if (Trace())
	{
		SPDLOG_LOGGER_INFO(spdlog::get("audio"), "Advisor: dude {} says HelpSprites {} ({:.2f} s)",
		                   static_cast<int>(&advisor - state.dudes.data()), advisor.sample, state.pcm.duration);
	}
}

/// The sound part of the dude's lip sync
void ApplyLipSync(int index, float dt)
{
	auto& state = AdvisorData();
	auto& dude = state.dudes[static_cast<size_t>(index)];
	dude.lipSync.reset();
	if (!IsTalking(index) || state.sentence == 0)
	{
		return;
	}
	// The time since the start
	dude.time = static_cast<float>(static_cast<double>(Since(state.startTick)) * static_cast<double>(0.001f));
	const int64_t position = sample_play::PlayPosition(dude.bank, Owner::Key(k_OwnerAdvisor));
	if (position < 0)
	{
		// No longer playing and more than half a second in
		if (dude.time > 0.5f)
		{
			StopSentence(index);
		}
		// The tag walker runs on the tick time: help::spirits (LipSyncThisFrame)
		dude.lipSync = LipSyncRun {dude.time, false};
		return;
	}
	const float t = static_cast<float>(static_cast<double>(position) * static_cast<double>(0.001f));
	dude.time = t;
	// The mouth poses and the tag walker run on t: help::spirits (LipSyncThisFrame)
	dude.lipSync = LipSyncRun {t, true};
	if (state.sentence != 0 && !state.pcm.samples.empty())
	{
		ComputeLipSyncKey(dude.params, dude.key, dt, t,
		                  std::span<const int16_t>(state.pcm.samples).first(static_cast<size_t>(state.pcm.frames)),
		                  static_cast<float>(state.pcm.rate), state.spectrum, k_LipSyncWindow);
	}
}
} // namespace

// ---- AudioAnalyse and AutoVoiceParams ------------------------------------------------------------------------------

void advisor::FastFourierTransform(std::span<float> data, int isign)
{
	const auto nn = static_cast<int>(data.size() / 2);
	// data is used 1-based
	float* d = data.data() - 1;
	const int n = nn << 1;
	int j = 1;
	for (int i = 1; i < n; i += 2) // Bit reversal
	{
		if (j > i)
		{
			std::swap(d[j], d[i]);
			std::swap(d[j + 1], d[i + 1]);
		}
		int m = nn; // n >> 1
		while (m >= 2 && j > m)
		{
			j -= m;
			m >>= 1;
		}
		j += m;
	}
	// The original runs the FPU at 24-bit precision: every add / sub / mul / div rounds to a float (R), the double
	// constants are exact and sin keeps the full precision (wpi stays unrounded)
	const auto R = [](double v) { return static_cast<float>(v); };
	int mmax = 2;
	while (n > mmax) // Danielson-Lanczos
	{
		const int istep = mmax << 1;
		const float theta = R(6.2831853071795898 / static_cast<double>(mmax * isign)); // 2 pi / (mmax * isign)
		const double half = std::sin(static_cast<double>(R(theta * 0.5)));
		const float wpr = R(static_cast<double>(R(half * half)) * -2.0);
		const double wpi = std::sin(static_cast<double>(theta)); // Sine of the stored double
		float wr = 1.0f;
		float wi = 0.0f;
		for (int m = 1; m < mmax; m += 2)
		{
			for (int i = m; i <= n; i += istep)
			{
				const int k = i + mmax;
				const float tempr = d[k] * wr - d[k + 1] * wi;
				const float tempi = d[k] * wi + d[k + 1] * wr; // Stored as a float
				d[k] = d[i] - tempr;
				d[k + 1] = d[i + 1] - tempi;
				d[i] = tempr + d[i];
				d[i + 1] = tempi + d[i + 1];
			}
			const float wrOld = wr;
			// wr + (wr * wpr - wi * wpi); wi * (wpr + 1) + wpi * wr (the old wr)
			wr = (wrOld * wpr - R(static_cast<double>(wi) * wpi)) + wrOld;
			wi = wi * (wpr + 1.0f) + R(wpi * static_cast<double>(wrOld));
		}
		mmax = istep;
	}
}

void advisor::Analyse(std::span<const int16_t> pcm, std::span<float> out)
{
	const auto n = static_cast<int>(pcm.size());
	// The window's step 1 / (n * 32768) as a float; the window itself at 24-bit precision, so a float
	const float step = 1.0f / (static_cast<float>(n) * 32768.0f);
	float w = 0.0f;
	const int half = n / 2;
	int i = 0;
	for (; i < half; ++i) // Rising half
	{
		w += step;
		out[2 * i] = static_cast<float>(pcm[i]) * w;
		out[2 * i + 1] = 0.0f;
	}
	for (; i < n; ++i) // Falling half
	{
		w -= step;
		out[2 * i] = static_cast<float>(pcm[i]) * w;
		out[2 * i + 1] = 0.0f;
	}
	FastFourierTransform(out.first(2 * pcm.size()), 1);
	const auto count = static_cast<float>(n);
	for (i = 0; i < n; ++i) // In place, the magnitudes packed at the front
	{
		const float re = out[2 * i];
		const float im = out[2 * i + 1];
		out[i] = std::sqrt((re * re + im * im) / count);
	}
}

float advisor::BandLevel(std::span<const float> spectrum, float rate, float low, float high)
{
	const auto n = static_cast<int>(spectrum.size());
	// Truncation of (low / rate) * n and (high / rate) * n, each capped at n; the original's FPU is at 24-bit
	// precision, so the steps are float ones (n converts exactly)
	int from = static_cast<int>(low / rate * static_cast<float>(n));
	int to = static_cast<int>(high / rate * static_cast<float>(n));
	from = std::min(from, n);
	to = std::min(to, n);
	if (from >= to)
	{
		return 0.0f;
	}
	float sum = 0.0f;
	for (int i = from; i < to; ++i)
	{
		sum += spectrum[i];
	}
	return static_cast<float>(static_cast<double>(sum) / (to - from)); // Rounded to a float
}

void advisor::ComputeLipSyncKey(AutoVoiceParams& params, VoiceKey& key, float dt, float t, std::span<const int16_t> pcm,
                                float rate, std::span<float> spectrum, int window)
{
	const auto frames = static_cast<int>(pcm.size());
	key.time = t;
	// The centre sample, ((offsetMs * 0.001f) + dt + t) * rate in float steps
	const auto centre = static_cast<int>((params.offsetMs * 0.001f + dt + t) * rate);
	int start = centre - window / 2;
	start = std::max(start, 0);
	start = std::min(start, frames - window);
	// (defensive, not in the original: a sentence shorter than the window would be read before its PCM)
	if (start < 0 || frames < window)
	{
		return;
	}
	Analyse(pcm.subspan(static_cast<size_t>(start), static_cast<size_t>(window)), spectrum);
	float total = 0.0f;
	for (size_t k = 0; k < params.bands.size(); ++k)
	{
		const auto& band = params.bands[k];
		params.bandLevel[k] = BandLevel(spectrum.first(static_cast<size_t>(window)), rate, band.low, band.high) * band.gain;
		total += params.bandLevel[k];
	}
	if (total < params.threshold)
	{
		total = 0.0f;
	}
	params.total = total;
	const float squared = total * total;
	if (squared != 0.0f)
	{
		for (auto& level : params.bandLevel)
		{
			level = level * level / squared;
		}
	}
	// The loudest band
	size_t loudest = 2;
	const auto& b = params.bandLevel;
	if (b[0] > b[1] && b[0] > b[2])
	{
		loudest = 0;
	}
	else if (b[1] > b[2] && b[1] > b[0])
	{
		loudest = 1;
	}
	float level = params.level * params.total;
	if (level > 1.0f)
	{
		level = 1.0f;
	}
	float sum = 0.0f;
	for (size_t k = 0; k < params.bands.size(); ++k)
	{
		auto& weight = key.weights[static_cast<size_t>(params.index[k])];
		// In float steps: v and the sum stay unrounded in the original, the limit is stored
		const float ratio = params.bandLevel[k] / b[loudest];
		float v = ratio * ratio * level;
		const float limit = dt * params.rate * 0.18f;
		const float previous = weight;
		if (previous - v > limit) // Falls at most by limit
		{
			v = previous - limit;
		}
		const float limitUp = limit + limit;
		if (v - previous > limitUp) // Rises at most by twice that
		{
			v = limitUp + previous;
		}
		weight = v;
		sum += weight;
	}
	if (sum > 1.0f)
	{
		const float scale = 1.0f / sum;
		for (const int index : params.index)
		{
			key.weights[static_cast<size_t>(index)] *= scale;
		}
	}
}

// ---- AdvisorSpiritController / AdvisorSpirit ---------------------------------------------------------------------

void advisor::Init(BankId bank)
{
	for (auto& dude : AdvisorData().dudes)
	{
		dude.bank = bank;
		dude.samples = BankSampleCount(bank);
	}
}

void advisor::Reset()
{
	auto& state = AdvisorData();
	for (int i = 0; i < k_AdvisorCount; ++i)
	{
		Stop(i);
	}
	const auto bank = state.dudes[0].bank;
	state = {};
	Init(bank);
}

void advisor::SetHover(int advisor, float hover)
{
	if (auto* d = Get(advisor); d != nullptr)
	{
		d->hover = hover;
	}
}

void advisor::Say(int advisor, int sample, bool onlyIfSilent)
{
	auto* d = Get(advisor);
	if (d == nullptr)
	{
		return;
	}
	// |hover| minus 0.95f (a float subtraction at the original's 24-bit precision), (v + 1) * 250 and the cap 500,
	// all floats
	float v = std::fabs(d->hover) - 0.95f;
	if (v < 0.0f)
	{
		v = 0.0f;
	}
	else
	{
		v = (v + 1.0f) * 250.0f;
		if (v > 500.0f)
		{
			v = 500.0f;
		}
	}
	SaySentence(advisor, sample, onlyIfSilent, static_cast<uint32_t>(static_cast<int32_t>(v)));
	d->active = true;
}

void advisor::SaySentence(int advisor, int sample, bool onlyIfSilent, uint32_t delayMs)
{
	auto& state = AdvisorData();
	auto* d = Get(advisor);
	if (d == nullptr)
	{
		return;
	}
	state.speaker = advisor;
	if (d->bank == k_NoBank)
	{
		return;
	}
	if (state.sentence != 0)
	{
		if (onlyIfSilent)
		{
			if (IsTalking(advisor))
			{
				return;
			}
		}
		else
		{
			StopSentence(advisor);
		}
	}
	// 1..the bank's number of samples
	if (sample < 1 || sample > d->samples)
	{
		return;
	}
	d->sample = sample;
	d->startTick = TickCount() + delayMs;
	UpdateSaySentence(*d);
}

void advisor::Update(float dt)
{
	for (int i = 0; i < k_AdvisorCount; ++i)
	{
		UpdateSaySentence(AdvisorData().dudes[static_cast<size_t>(i)]);
		ApplyLipSync(i, dt);
	}
}

bool advisor::IsTalking(int advisor)
{
	auto& state = AdvisorData();
	auto* d = Get(advisor);
	if (d == nullptr || state.speaker != advisor)
	{
		return false;
	}
	bool talking = d->startTick != 0; // A start still waiting counts
	if (!talking)
	{
		if (d->bank == k_NoBank || state.sentence == 0)
		{
			return false;
		}
		// Is the sentence still playing with the advisor's owner
		talking = sample_play::IsPlaying(SampleId(d->bank, state.sentence), Owner::Key(k_OwnerAdvisor));
		if (!talking)
		{
			StopSentence(advisor); // A sentence that ended is stopped
			return false;
		}
	}
	d->lastTalkTick = TickCount();
	return true;
}

bool advisor::TalkingOrJustStopped(int advisor)
{
	const auto* d = Get(advisor);
	if (d == nullptr)
	{
		return false;
	}
	if (IsTalking(advisor))
	{
		return true;
	}
	return Since(d->lastTalkTick) < 200; // Unsigned, in ms
}

float advisor::PercentageDone(int advisor)
{
	auto& state = AdvisorData();
	auto* d = Get(advisor);
	if (d == nullptr || state.speaker != advisor)
	{
		return 1.0f;
	}
	if (d->startTick != 0)
	{
		d->lastTalkTick = TickCount();
		return 0.0f;
	}
	if (d->bank == k_NoBank || state.sentence == 0)
	{
		return 1.0f;
	}
	const float done = sample_play::PercentageDone(SampleId(d->bank, state.sentence), Owner::Key(k_OwnerAdvisor));
	if (done < 1.0f)
	{
		d->lastTalkTick = TickCount();
		return done;
	}
	return 1.0f;
}

void advisor::StopSentence(int advisor)
{
	auto& state = AdvisorData();
	auto* d = Get(advisor);
	if (d == nullptr)
	{
		return;
	}
	d->startTick = 0;
	if (d->bank == k_NoBank || state.sentence == 0 || state.speaker != advisor)
	{
		return;
	}
	// Stops the sentence playing with the advisor's owner
	sample_play::Stop(SampleId(d->bank, state.sentence), Owner::Key(k_OwnerAdvisor));
	// The original also runs the tag walker past the end to close the mouth (not ported)
	state.sentence = 0;
	state.speaker = -1; // (and the audio tags cleared)
}

void advisor::Stop(int advisor)
{
	auto* d = Get(advisor);
	if (d == nullptr)
	{
		return;
	}
	StopSentence(advisor);
	d->interrupted = false;
	d->active = false;
}

bool advisor::Active(int advisor)
{
	const auto* d = Get(advisor);
	return d != nullptr && d->active;
}

void advisor::Interrupt(int advisor, int arg)
{
	auto* d = Get(advisor);
	if (d == nullptr || !d->active || !IsTalking(advisor) || d->interrupted)
	{
		return;
	}
	const int spirit = advisor + 1; // 1 good, 2 evil
	int text = 0;
	bool interrupted = true;
	if (arg == 0 && tags::RandomSample(0, 2) == 0)
	{
		// HELP_TEXT_INTERRUPTION_06..13 (good) / 19..26 (evil)
		text = (spirit == 1 ? 0xE44 : 0xE51) + tags::RandomSample(0, 8);
		interrupted = false;
	}
	else
	{
		// HELP_TEXT_INTERRUPTION_01..05 (good) / 14..18 (evil)
		text = (spirit == 1 ? 0xE3F : 0xE4C) + tags::RandomSample(0, 5);
	}
	if (text >= 0x1B3E) // Past the last help text
	{
		text = 0;
	}
	const int sample = static_cast<int>(voices::Table().Get(static_cast<uint32_t>(text)).sample); // From the voice table
	Stop(advisor);
	if (IsInsideCitadel())
	{
		return;
	}
	if (PercentageDone(advisor) < 0.9)
	{
		Say(advisor, sample, false);
		d->interrupted = interrupted;
	}
}

bool advisor::AnyTalking()
{
	return (Active(k_GoodSpirit) && TalkingOrJustStopped(k_GoodSpirit)) ||
	       (Active(k_EvilSpirit) && TalkingOrJustStopped(k_EvilSpirit));
}

VoiceKey advisor::LipSyncKey(int advisor)
{
	const auto* d = Get(advisor);
	return d != nullptr ? d->key : VoiceKey {};
}

float advisor::SentenceTime(int advisor)
{
	const auto* d = Get(advisor);
	return d != nullptr ? d->time : 0.0f;
}

std::optional<advisor::LipSyncRun> advisor::LipSyncThisFrame(int advisor)
{
	const auto* d = Get(advisor);
	return d != nullptr ? d->lipSync : std::nullopt;
}

float advisor::Amplitude(int sample, int window)
{
	auto& state = AdvisorData();
	if (sample == 0)
	{
		return 0.0f;
	}
	const auto t = static_cast<float>(static_cast<double>(Since(state.startTick)) * static_cast<double>(0.001f));
	if (t < 0.0f || !(t < state.pcm.duration) || state.pcm.samples.empty())
	{
		return 0.0f;
	}
	// The sample must be playing on HelpSprites with the advisor's owner
	if (!sample_play::IsPlaying(SampleId(Bank(SfxBank::HelpSprites), sample), Owner::Key(k_OwnerAdvisor)))
	{
		return 0.0f;
	}
	const int frames = state.pcm.frames;
	// Divide, multiply (rounded to a float), truncate
	int from = static_cast<int>(static_cast<float>(static_cast<double>(t / state.pcm.duration) * frames));
	int to = from + window;
	from = from > 0 ? std::min(from, frames) : 0;
	to = to > 0 ? std::min(to, frames) : 0;
	float sum = 0.0f; // In float steps
	for (int i = from; i < to; ++i)
	{
		const float s = static_cast<float>(state.pcm.samples[static_cast<size_t>(i)]) * 3.0517578125e-05f; // 1 / 32768
		sum += s * s;
	}
	return std::sqrt(sum);
}

int advisor::Sentence()
{
	return AdvisorData().sentence;
}

int advisor::Speaker()
{
	return AdvisorData().speaker;
}

const SentencePcm& advisor::Pcm()
{
	return AdvisorData().pcm;
}
