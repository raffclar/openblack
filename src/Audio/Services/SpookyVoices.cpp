/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "SpookyVoices.h"

#include <cstdlib>

#include <string>
#include <utility>

#include <spdlog/spdlog.h>

#include "Audio/Game/AudioSystem.h"
#include "Audio/GameQueries.h"
#include "Audio/Services/Guidance.h"
#include "Audio/Services/Voices.h"
#include "Common/HelpText.h"
#include "Debug/DebugEnv.h"
#include "ECS/Systems/AudioStateInterface.h"
#include "Locator.h"

// Every function's behaviour is described in SpookyVoices.h.

using namespace openblack;
using namespace openblack::audio;
using namespace openblack::audio::spooky;

namespace
{
/// What this module keeps between calls (Locator::audioState)
struct SpookyVoicesState
{
	State spooky {};
	ClockFn clock {};
	TextFn texts {};
};

SpookyVoicesState& SpookyVoicesData()
{
	return openblack::Locator::audioState::value().Get<SpookyVoicesState>();
}

bool Trace()
{
	static const bool k_Trace = debug_env::GuidanceTrace();
	return k_Trace;
}

/// _isalpha in the "C" locale (inferred): the ASCII letters
bool IsAlpha(char16_t c)
{
	return (c >= u'A' && c <= u'Z') || (c >= u'a' && c <= u'z');
}
} // namespace

void spooky::SetNameTexts(TextFn texts)
{
	SpookyVoicesData().texts = std::move(texts);
}

void spooky::SetClock(ClockFn clock)
{
	SpookyVoicesData().clock = std::move(clock);
}

int64_t spooky::UnixTime()
{
	auto& state = SpookyVoicesData();
	return state.clock ? state.clock() : static_cast<int64_t>(std::time(nullptr));
}

std::tm spooky::LocalTime()
{
	const auto now = static_cast<std::time_t>(UnixTime());
	std::tm local {};
#ifdef _WIN32
	localtime_s(&local, &now);
#else
	localtime_r(&now, &local);
#endif
	return local;
}

bool spooky::NightNow(const std::tm& time)
{
	// tm_hour >= 23 or <= 5 -> 1; < 20 -> 0; tm_min < 45 -> 0; < 21 -> 1
	const int hour = time.tm_hour;
	if (hour >= 23 || hour <= 5)
	{
		return true;
	}
	if (hour < 20 || time.tm_min < 45)
	{
		return false;
	}
	return hour < 21;
}

int spooky::SoundexDigit(char16_t c)
{
	if (!IsAlpha(c))
	{
		return 0;
	}
	const auto index = static_cast<uint32_t>((c | 0x60) - 0x61);
	// a b c d e f g h i j k l m n o p q r s t u v w x y z ('h', 'w', 'y': -1, the character)
	constexpr std::array<int, 26> k_Codes {0, 1, 2, 3, 0, 1, 2, -1, 0, 2, 2, 4, 5, 5, 0, 1, 2, 6, 2, 3, 0, 1, -1, 2, -1, 2};
	if (index > 25)
	{
		return static_cast<int>(c); // Outside the table: the character
	}
	const int code = k_Codes.at(index);
	return code < 0 ? static_cast<int>(c) : code;
}

int spooky::GetNextSoundexCode(const char16_t*& p)
{
	int code = 0;
	// Skip the uncoded characters
	for (;;)
	{
		const char16_t c = *p;
		if (c == 0 || c == u' ')
		{
			return 0;
		}
		const char16_t* at = p;
		++p;
		code = SoundexDigit(*at);
		if (code != 0)
		{
			break;
		}
	}
	// The next character (not passed)
	if (SoundexDigit(*p) != code)
	{
		return code;
	}
	do
	{
		++code;
	} while (SoundexDigit(*p) == code);
	return code;
}

bool spooky::SoundsAlike(const char16_t* text, const char16_t* word)
{
	if (text[0] != word[0] || text[0] == 0)
	{
		return false;
	}
	const char16_t* a = text + 1;
	const char16_t* b = word + 1;
	for (int i = 0; i < 3; ++i)
	{
		const int ca = GetNextSoundexCode(a);
		const int cb = GetNextSoundexCode(b);
		if (ca != cb)
		{
			return false;
		}
	}
	return true;
}

bool spooky::SoundexOverlap(const char16_t* text, const char16_t* name)
{
	if (*name == 0)
	{
		return false;
	}
	for (;;)
	{
		if (SoundsAlike(text, name))
		{
			return true;
		}
		// To the next space (or the end), past it
		while (*name != 0 && *name != u' ')
		{
			++name;
		}
		if (*name == 0)
		{
			return false;
		}
		++name;
		if (*name == 0)
		{
			return false;
		}
	}
}

uint32_t spooky::FindSoundAlikeName(std::u16string_view name)
{
	auto& state = SpookyVoicesData();
	if (name.empty() || name.front() == 0)
	{
		return 0;
	}
	const std::u16string nameText(name); // the original reads a 0-terminated wide string
	for (size_t k = 0; k < k_Names; ++k)
	{
		const auto id = static_cast<uint32_t>(k_FirstName + k);
		// The help text entry (entry 0 for an id out of 1..count-1: helptext::GetEntry's rule)
		const std::u16string text = state.texts ? state.texts(id) : helptext::GetEntry(id).text;
		if (SoundexOverlap(text.c_str(), nameText.c_str()))
		{
			return voices::Table().Get(id).sample;
		}
	}
	return 0;
}

uint32_t spooky::GetName()
{
	const auto& queries = Queries();
	const auto profile = queries.profileName ? queries.profileName() : std::u16string();
	if (const uint32_t sample = FindSoundAlikeName(profile); sample != 0)
	{
		return sample;
	}
	// The network login and the registry's DefName: not ported
	return 0;
}

void spooky::Init()
{
	auto& state = SpookyVoicesData();
	state.spooky.bank = Bank(SfxBank::Guidance);
	state.spooky.options = sample_play::Options {};
	state.spooky.field2C = 90;
	state.spooky.sample = GetName();
	state.spooky.counter = 0;
	state.spooky.countdown = 100;
	state.spooky.initialised = true;
	if (Trace())
	{
		SPDLOG_LOGGER_INFO(spdlog::get("audio"), "SpookyVoices: Init, name sample {}", state.spooky.sample);
	}
}

void spooky::UpdatePlayerName()
{
	SpookyVoicesData().spooky.sample = GetName();
}

void spooky::Shutdown()
{
	SpookyVoicesData().spooky.initialised = false;
}

const State& spooky::GetState()
{
	return SpookyVoicesData().spooky;
}

void spooky::SetForTests(uint32_t sample, uint32_t counter, uint32_t countdown)
{
	auto& state = SpookyVoicesData();
	state.spooky.bank = Bank(SfxBank::Guidance);
	state.spooky.options = sample_play::Options {};
	state.spooky.field2C = 90;
	state.spooky.sample = sample;
	state.spooky.counter = counter;
	state.spooky.countdown = countdown;
	state.spooky.initialised = true;
}

void spooky::Process()
{
	auto& state = SpookyVoicesData();
	if (!state.spooky.initialised)
	{
		return;
	}
	const auto& queries = Queries();
	const int land = queries.landNumber ? queries.landNumber() : 0;
	if (land == 1 || land == 2 || state.spooky.sample == 0)
	{
		return;
	}
	if (state.spooky.countdown != 0)
	{
		--state.spooky.countdown;
		return;
	}
	if (NightNow(LocalTime()))
	{
		// r = LocalFloatRand(1), trunc((1 - r^3) x 1000) compared unsigned with the counter
		// (float steps: the game's FPU runs at 24-bit precision)
		const float r = guidance::LocalFloatRand(1.0f);
		const auto chance = static_cast<uint32_t>(static_cast<int32_t>((1.0f - r * r * r) * 1000.0f));
		if (chance < state.spooky.counter)
		{
			PlaySpookyVoice();
		}
		else
		{
			guidance::OneOff(1);
		}
		++state.spooky.counter;
	}
	state.spooky.countdown = 99; // 100, then the decrement
}

void spooky::PlaySpookyVoice()
{
	auto& state = SpookyVoicesData();
	// In float steps (the game's FPU runs at 24-bit precision)
	const float a = guidance::LocalFloatRand(0.65f);
	float p = a * a * a + 1.0f;
	if (guidance::LocalRand(2) == 0)
	{
		p = 1.0f / p;
	}
	const float pitchFactor = p;
	const auto pan = static_cast<int>(guidance::LocalRand(180));
	const float b = guidance::LocalFloatRand(0.8f);
	float q = b * b * b + 1.0f;
	if (guidance::LocalRand(2) == 0)
	{
		q = 1.0f / q;
	}
	auto& options = state.spooky.options;
	options.pitch = static_cast<int>(pitchFactor * 100.0f);
	options.volume = static_cast<int>(static_cast<float>(options.volume) * q); // Exact conversion, float multiply
	state.spooky.field2C = pan;
	options.is3D = false;
	PlayOptions play;
	static_cast<sample_play::Options&>(play) = options;
	play.sound = 0;
	play.sample = {state.spooky.bank, static_cast<int>(state.spooky.sample)};
	const auto channel = PlaySoundEffect(play);
	state.spooky.counter = 0;
	if (Trace())
	{
		SPDLOG_LOGGER_INFO(spdlog::get("audio"), "SpookyVoices: sample {} pitch {} volume {} (channel {})", state.spooky.sample,
		                   options.pitch, options.volume, channel);
	}
}
