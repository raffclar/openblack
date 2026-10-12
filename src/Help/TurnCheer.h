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

#include <functional>
#include <optional>

/// The advisors cheering the player on as the camera turns, while the turning lesson asks for it: the faster the
/// player turns, the likelier a "yes!" each game turn, and turning flat out earns the best praise now and then
namespace openblack::help::turn_cheer
{

/// How fast the camera turns at full speed, in radians a second: anything faster counts as full speed
constexpr float k_FullSpeed = 7.0f;
/// How much of the way the camera's measured turning speed moves to this frame's speed, each frame
constexpr float k_SpeedSmoothing = 0.4f;
/// A cheer waits for more than this many game turns since the last
constexpr uint32_t k_TurnsBetweenCheers = 20;
/// The best praise waits for more than this many game turns since the last
constexpr uint32_t k_TurnsBetweenBestPraise = 100;

/// The advisors' lines in their speech bank: the best praise, and the first of the run of cheers one is picked from
constexpr int32_t k_BestPraise = 1686;
constexpr int32_t k_FirstCheer = 1704;
constexpr int32_t k_Cheers = 14;

/// The camera's turning speed, in radians a second, after a frame that turned it by an angle (none when the player
/// didn't turn it, which slows the measure down towards still)
[[nodiscard]] float SmoothSpeed(float speed, float turned, float seconds);

/// Whether the advisors are cheering, and the game turns of their last cheer and of their last best praise
struct Cheering
{
	bool on {false};
	uint32_t lastCheerTurn {0};
	uint32_t lastBestPraiseTurn {0};
};

/// The cheering starts afresh, its turns forgotten
void Start(Cheering& cheering);
void Stop(Cheering& cheering);

/// The random numbers a cheer is decided by: a float from 0 up to a number, and a whole number below a number
struct Random
{
	std::function<float(float)> upTo;
	std::function<uint32_t(int32_t)> below;
};

/// One game turn of cheering at the camera's turning speed: the line the advisors say, if any. Turning flat out
/// earns the best praise when it hasn't been given for long enough, and nothing else; slower turning earns a cheer,
/// once enough turns have gone by, with a chance growing with the speed.
[[nodiscard]] std::optional<int32_t> ProcessTurn(Cheering& cheering, float speed, uint32_t gameTurn, const Random& random);

} // namespace openblack::help::turn_cheer
