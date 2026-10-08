/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "Confirmation.h"

#include <cmath>

#include "Audio/Audio.h"
#include "Audio/Game/Banks.h"
#include "Common/GameRandom.h"
#include "ECS/Systems/AudioStateInterface.h"
#include "Locator.h"

namespace openblack::audio::confirmation
{
namespace
{
/// What this module keeps between calls (Locator::audioState)
struct ConfirmationState
{
	float angle {0.0f};
	float pitch {0.0f};
	State state {}; ///< all 0 at the game's one-time init
};

ConfirmationState& ConfirmationData()
{
	return openblack::Locator::audioState::value().Get<ConfirmationState>();
}

void Feed(float& value, float a, float b)
{
	value = (a / b - value) * k_Smoothing + value; // each step rounded to a float
}
} // namespace

void FeedAngle(float a, float b)
{
	Feed(ConfirmationData().angle, a, b);
}

void FeedPitch(float a, float b)
{
	Feed(ConfirmationData().pitch, a, b);
}

float Angle()
{
	return ConfirmationData().angle;
}

float Pitch()
{
	return ConfirmationData().pitch;
}

void Start(State& state, const float* value, float range)
{
	state.value = value;
	state.range = range;
	state.active = true;
	state.lastSay = 0;
	state.lastBetter = 0;
}

void Stop(State& state)
{
	state.active = false;
}

void StartAngleSound(bool on)
{
	auto& confirmationState = ConfirmationData();
	if (on)
	{
		confirmationState.angle = 0.0f;
		Start(confirmationState.state, &confirmationState.angle, k_AngleRange);
	}
	else
	{
		Stop(confirmationState.state);
	}
}

void StartPitchSound(bool on)
{
	auto& confirmationState = ConfirmationData();
	if (on)
	{
		confirmationState.pitch = 0.0f;
		Start(confirmationState.state, &confirmationState.pitch, k_PitchRange);
	}
	else
	{
		Stop(confirmationState.state);
	}
}

int Step(State& state, uint32_t turn, const std::function<float(float)>& floatRand,
         const std::function<uint32_t(int32_t)>& rand)
{
	if (!state.active || state.value == nullptr)
	{
		return 0;
	}
	float v = *state.value / state.range;
	if (!(v >= -1.0f)) // below (or unordered: a NaN)
	{
		v = -1.0f;
	}
	else if (!(v <= 1.0f))
	{
		v = 1.0f;
	}
	const float a = std::fabs(v);
	if (a == 1.0f)
	{
		if (turn - state.lastBetter <= k_BetterTurns)
		{
			return 0;
		}
		state.lastSay = turn;
		state.lastBetter = turn;
		return k_Better;
	}
	if (a > 0.0f)
	{
		// the turns first, then the draw
		if (turn - state.lastSay <= k_SayTurns || !(std::fabs(a) > floatRand(1.0f)))
		{
			return 0;
		}
		const int sample = static_cast<int>(rand(k_YesEnd - k_Yes)) + k_Yes;
		state.lastSay = turn;
		return sample;
	}
	if (a < 0.0f) // the "no": 1689 + LocalRand(14), unreachable (a = |v|)
	{
		if (turn - state.lastSay <= k_SayTurns || !(std::fabs(a) > floatRand(1.0f)))
		{
			return 0;
		}
		state.lastSay = turn;
		return static_cast<int>(rand(k_NoEnd - k_No)) + k_No;
	}
	return 0;
}

void Process(uint32_t turn)
{
	const int sample = Step(
	    ConfirmationData().state, turn, [](float x) { return game_random::LocalFloatRand(x); },
	    [](int32_t n) { return game_random::LocalRand(n); });
	if (sample == 0)
	{
		return;
	}
	// the bank HelpSprites, volume 100, no owner, 2D, no track
	PlayOptions options;
	options.sample = {Bank(SfxBank::HelpSprites), sample};
	options.volume = k_Volume;
	options.is3D = false;
	options.track = false;
	PlaySoundEffect(options);
}

State& Get()
{
	return ConfirmationData().state;
}

} // namespace openblack::audio::confirmation
