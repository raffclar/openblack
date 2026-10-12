/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "TurnCheer.h"

#include <cmath>

namespace openblack::help::turn_cheer
{

float SmoothSpeed(float speed, float turned, float seconds)
{
	const float frameSpeed = turned / seconds;
	return speed + (frameSpeed - speed) * k_SpeedSmoothing;
}

void Start(Cheering& cheering)
{
	cheering = {.on = true, .lastCheerTurn = 0, .lastBestPraiseTurn = 0};
}

void Stop(Cheering& cheering)
{
	cheering.on = false;
}

std::optional<int32_t> ProcessTurn(Cheering& cheering, float speed, uint32_t gameTurn, const Random& random)
{
	if (!cheering.on)
	{
		return std::nullopt;
	}
	// The share of full speed, kept to -1..1: anything that isn't at least -1 (a speed that isn't a number too) counts as
	// -1, so as flat out
	float share = speed / k_FullSpeed;
	if (!(share >= -1.0f))
	{
		share = -1.0f;
	}
	else if (share > 1.0f)
	{
		share = 1.0f;
	}
	share = std::fabs(share);
	if (share == 1.0f)
	{
		if (gameTurn - cheering.lastBestPraiseTurn <= k_TurnsBetweenBestPraise)
		{
			return std::nullopt;
		}
		cheering.lastCheerTurn = gameTurn;
		cheering.lastBestPraiseTurn = gameTurn;
		return k_BestPraise;
	}
	if (share <= 0.0f || gameTurn - cheering.lastCheerTurn <= k_TurnsBetweenCheers)
	{
		return std::nullopt;
	}
	if (!(random.upTo(1.0f) < share))
	{
		return std::nullopt;
	}
	cheering.lastCheerTurn = gameTurn;
	return k_FirstCheer + static_cast<int32_t>(random.below(k_Cheers));
}

} // namespace openblack::help::turn_cheer
