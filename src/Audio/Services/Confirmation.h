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

#include <bit>
#include <functional>

// The sound confirmation: the good spirit's "yes" while the player turns or tilts the camera the way a tutorial asks
// (TeachRotate / TeachPitch in HandDemos.txt). START_ANGLE_SOUND 285 watches the turn, script function 348 (also named
// START_ANGLE_SOUND) the tilt; the game's turn runs Process once a turn. The camera feeds the two values.
namespace openblack::audio::confirmation
{

inline constexpr float k_Smoothing = std::bit_cast<float>(0x3ECCCCCDu);  ///< 0.4
inline constexpr float k_AngleRange = std::bit_cast<float>(0x40E00000u); ///< 7
inline constexpr float k_PitchRange = std::bit_cast<float>(0x40A00000u); ///< 5
inline constexpr uint32_t k_SayTurns = 0x14;                             ///< more than 20 turns since the last sample
inline constexpr uint32_t k_BetterTurns = 0x64;                          ///< more than 100 turns since the last "better"
/// HelpSprites samples of the help text table rows:
inline constexpr int k_Yes = 1704; ///< HELP_TEXT_ROTATION_YES_01; LocalRand(1718 - 1704)
inline constexpr int k_YesEnd = 1718;
inline constexpr int k_No = 1689; ///< ROTATION_NO_01; LocalRand(1703 - 1689): never reached
inline constexpr int k_NoEnd = 1703;
inline constexpr int k_Better = 1686; ///< ROTATION_BETTER_14 ("Great")
inline constexpr int k_Volume = 0x64; ///< 100 (a second value of 90 is not modelled, as in Guidance)

/// What the per-turn step reads and writes (one static in the original)
struct State
{
	const float* value {nullptr}; ///< the angle or the pitch
	float range {1.0f};
	uint32_t lastBetter {0};
	bool active {false};
	uint32_t lastSay {0};
};

/// The camera's feed: g = (a / b - g) x 0.4 + g, each step a float; a is the turn (tilt) of the frame and b the camera's
/// seconds of the frame, or a = 0 on the frames without one. (pending) the seven call sites wait for the port of the
/// camera mode's update
void FeedAngle(float a, float b);
void FeedPitch(float a, float b);
[[nodiscard]] float Angle();
[[nodiscard]] float Pitch();

/// The script's START_ANGLE_SOUND: on, the angle = 0 and Start(&angle, 7); off, Stop
void StartAngleSound(bool on);
/// The same with the pitch and 5
void StartPitchSound(bool on);

/// Start: the value, the range, active, both stamps = 0. Stop: not active
void Start(State& state, const float* value, float range);
void Stop(State& state);

/// Process as a pure step: the sample to play this turn, 0 for none. v = clamp(value / range, -1, 1) (a NaN becomes
/// -1), a = |v|; a == 1: 1686 when more than 100 turns since the last one (both stamps set); a > 0: when more than 20
/// turns since the last sample and a > LocalFloatRand(1) (drawn only then), 1704 + LocalRand(14). The "no" branch
/// (v < 0, 1689 + LocalRand(14)) tests a, never negative: it never plays (the original's bug, kept)
[[nodiscard]] int Step(State& state, uint32_t turn, const std::function<float(float)>& floatRand,
                       const std::function<uint32_t(int32_t)>& rand);
/// Once a turn (from audio::guidance::ProcessGameTurn): Step on the game's local random stream, then PlaySoundEffect of
/// the sample, 2D, volume 100, no owner
void Process(uint32_t turn);

/// The one confirmation state (for the tests)
[[nodiscard]] State& Get();

} // namespace openblack::audio::confirmation
