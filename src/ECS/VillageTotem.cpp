/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "VillageTotem.h"

#include <cmath>

#include <algorithm>

namespace openblack::ecs::village_totem
{

void SetShare(Ease& ease, float share)
{
	// The time is measured from the share it was last set to, not from where it now stands
	const float duration = std::abs(ease.target - share) * k_EaseMsPerShare;
	ease.moving = true;
	if (duration < k_SnapMs)
	{
		ease = {.share = share, .target = share, .startShare = share, .moving = true};
		return;
	}
	ease.duration = duration;
	ease.startShare = ease.share;
	ease.startSpeed = ease.speed;
	ease.target = share;
	ease.elapsed = 0.0f;
	// It must cover what its own speed won't, and lose that speed, arriving with no acceleration left
	const float t = duration;
	const float distance = share - ease.startShare - ease.startSpeed * t;
	const float speedChange = -ease.startSpeed;
	ease.snap = 72.0f * (distance - 2.0f * speedChange * t / 3.0f) / (t * t * t * t);
	ease.jerk = -2.0f * speedChange / (t * t) - 2.0f * ease.snap * t / 3.0f;
	ease.acceleration = -ease.jerk * t - ease.snap * t * t / 2.0f;
}

void Step(Ease& ease, float milliseconds)
{
	ease.elapsed += milliseconds;
	if (ease.elapsed < ease.duration)
	{
		const float t = ease.elapsed;
		const float t2 = t * t * 0.5f;
		const float t3 = t * t2 / 3.0f;
		const float t4 = t2 * t2 / 6.0f;
		ease.speed = ease.startSpeed + ease.acceleration * t + ease.jerk * t2 + ease.snap * t3;
		ease.share = ease.startShare + ease.startSpeed * t + ease.acceleration * t2 + ease.jerk * t3 + ease.snap * t4;
		return;
	}
	ease.share = ease.target;
	ease.speed = 0.0f;
	ease.elapsed = ease.duration;
}

bool TakeArrival(Ease& ease)
{
	if (!ease.moving || std::abs(ease.share - ease.target) >= k_ArrivedWithin)
	{
		return false;
	}
	ease.moving = false;
	return true;
}

float RiseOf(float share)
{
	return share * k_RiseAtFullShare;
}

float HandSlide(float upPixels, float screenHeight, float iconHeight, float handAboveLand)
{
	if (screenHeight <= 0.0f)
	{
		return 0.0f;
	}
	const float slide = upPixels * iconHeight * k_SlidePerScreen / screenHeight;
	// Going down, the hand stops at the land
	if (slide < 0.0f && handAboveLand < -slide)
	{
		return -handAboveLand;
	}
	return slide;
}

float SlideShare(float held, float slide)
{
	return std::clamp(held + slide * k_SharePerSlide, 0.0f, 1.0f);
}

float GripY(float iconY, float iconHeight)
{
	return iconY + iconHeight * k_GripHeight;
}

float HandTiltAt(float handY)
{
	if (handY >= k_LowHand)
	{
		return k_HandTilt;
	}
	return k_HandTilt + std::atan((k_LowHand - handY) / k_HandSize);
}

float GripClosure(float iconRadius)
{
	return std::min(iconRadius / (k_HandSize * 0.5f), 1.0f);
}

} // namespace openblack::ecs::village_totem
