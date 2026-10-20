/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "PushScroll.h"

#include <cstdlib>

#include <algorithm>

#include "Common/X87.h"

namespace openblack::camera_push_scroll
{

namespace
{
// The game's numbers, as the executable holds them
constexpr float k_SpeedPerHeight = 3.0f;
constexpr float k_SlowestSpeed = 60.0f;
constexpr float k_FastestBelowFactor = 4.0f;
constexpr float k_FastestSpeed = 2000.0f;
constexpr double k_SpeedShare = 0.5;
constexpr float k_UpDownShare = 0.87f;
constexpr float k_PushFactor = 0.14f;
constexpr float k_TurnFactor = 1.0f;
constexpr float k_StoppedSquare = 0.01f;
constexpr float k_MoveAcross = -0.16f;
constexpr float k_MoveForwards = 0.16f;
// The double constants of the slope's limits hold the floats 0.2 and 2
constexpr double k_LeastSlope = static_cast<double>(0.2f);
constexpr double k_MostSlope = 2.0;
constexpr float k_HalfTurn = 3.14159274f;
// The double constant holds the float 0.01
constexpr double k_CountedTurn = static_cast<double>(0.01f);
constexpr float k_FadeInside = -7.0f;
constexpr float k_FadeAtSide = -0.2f;
constexpr float k_FadeTurnAtSide = -2.2f;
constexpr float k_MillisecondSeconds = 0.001f;

[[nodiscard]] float SpeedSquare(glm::vec3 speed)
{
	return speed.z * speed.z + speed.y * speed.y + speed.x * speed.x;
}

/// The cube of a place's share of the screen from its middle
[[nodiscard]] float CubedShare(int place, int last)
{
	const float share = static_cast<float>(place - last / 2) / static_cast<float>(last);
	return share * share * share;
}
} // namespace

glm::ivec2 PushScreenPosition(glm::ivec2 position, glm::ivec2 screenSize)
{
	if (screenSize.x <= 0 || screenSize.y <= 0)
	{
		return position;
	}
	// The edge controls' area: a 4:3 screen as tall, its halves against the real sides
	const int width = std::min(screenSize.x, screenSize.y * 4 / 3);
	const int folded = screenSize.x - width;
	const int half = width / 2;
	int across = position.x;
	if (folded > 0 && position.x >= half)
	{
		across = position.x >= screenSize.x - (width - half) ? position.x - folded : half;
	}
	const auto x = static_cast<int64_t>(across) * k_Screen.x / width;
	const auto y = static_cast<int64_t>(position.y) * k_Screen.y / screenSize.y;
	return {static_cast<int>(x), static_cast<int>(y)};
}

glm::bvec2 Inside(glm::ivec2 position, glm::ivec2 screenSize)
{
	return {position.x > 0 && position.x < screenSize.x - 1, position.y > 0 && position.y < screenSize.y - 1};
}

void Track(Pointer& pointer, glm::ivec2 position, glm::ivec2 delta, glm::bvec2 inside, bool windowed)
{
	pointer.position = position;
	pointer.delta = delta;
	pointer.insideX = inside.x || windowed;
	pointer.insideY = inside.y || windowed;
	if (pointer.insideX)
	{
		pointer.beyond.x = position.x;
	}
	else
	{
		pointer.beyond.x += delta.x;
	}
	if (pointer.insideY)
	{
		pointer.beyond.y = position.y;
	}
	else
	{
		pointer.beyond.y += delta.y;
	}
}

float Speed(float cameraHeight, float focusHeight)
{
	float speed = 0.0f;
	if (cameraHeight < focusHeight)
	{
		const float height = (focusHeight - cameraHeight) * k_SpeedPerHeight;
		if (height <= k_SlowestSpeed)
		{
			speed = k_SlowestSpeed;
		}
		else
		{
			const float fastest = k_SlowestSpeed * k_FastestBelowFactor;
			speed = height < fastest ? height : fastest;
		}
	}
	else
	{
		const float height = (cameraHeight - focusHeight) * k_SpeedPerHeight;
		if (height <= k_SlowestSpeed)
		{
			speed = k_SlowestSpeed;
		}
		else
		{
			speed = height < k_FastestSpeed ? height : k_FastestSpeed;
		}
	}
	return x87::Mul(speed, k_SpeedShare);
}

void Push(glm::vec3& speed, const Pointer& pointer, float pushSpeed, float seconds)
{
	const int lastX = k_Screen.x - 1;
	const int lastY = k_Screen.y - 1;
	const int dx = pointer.delta.x;
	const int dy = pointer.delta.y;
	const int edge = lastX / 8;
	if (dx == 0 && dy == 0)
	{
		return;
	}
	const bool aboveMiddle = pointer.position.y < lastY / 2;
	// Past the left or the right side
	if ((pointer.beyond.x <= -edge && dx < 0) || (pointer.beyond.x >= edge + lastX && dx > 0))
	{
		float turnShare = CubedShare(pointer.position.y, lastY);
		const auto across = static_cast<float>(dx);
		speed.x = across * k_PushFactor * pushSpeed * seconds + speed.x;
		if (aboveMiddle)
		{
			turnShare = -turnShare;
		}
		speed.z = across * k_TurnFactor * turnShare * pushSpeed * seconds + speed.z;
		return;
	}
	// Past the top or the bottom, the same edge beyond them as beyond the sides
	const float upDownSpeed = pushSpeed * k_UpDownShare;
	if ((pointer.beyond.y <= -edge && dy < 0) || (pointer.beyond.y >= edge + lastY && dy > 0))
	{
		float turnShare = CubedShare(pointer.position.x, lastX);
		const auto down = static_cast<float>(dy);
		speed.y = down * k_PushFactor * upDownSpeed * seconds + speed.y;
		if (aboveMiddle)
		{
			turnShare = -turnShare;
		}
		speed.z = down * k_TurnFactor * turnShare * upDownSpeed * seconds + speed.z;
	}
}

Strafe StrafeStep(glm::vec3& speed, float seconds, float heading, float pitch, uint32_t features)
{
	Strafe strafe;
	if ((features & k_FeaturePushScroll) == 0)
	{
		speed = glm::vec3(0.0f);
		return strafe;
	}
	const bool wasStopped = SpeedSquare(speed) < k_StoppedSquare;
	const float across = seconds * speed.x;
	const float forwards = seconds * speed.y;
	const float round = seconds * speed.z;
	if ((features & k_FeatureMove) != 0)
	{
		strafe.moved = across != 0.0f || forwards != 0.0f;
		// The slope of the view, kept wide, held between 0.2 and 2
		double slope = x87::Tan(pitch);
		if (!(slope > k_LeastSlope))
		{
			slope = k_LeastSlope;
		}
		else if (!(slope < k_MostSlope))
		{
			slope = k_MostSlope;
		}
		const float moveAcross = across * k_MoveAcross;
		const float moveForwards = x87::Div(forwards * k_MoveForwards, slope);
		// Turned by the heading about the up axis; the sine stays wide, the cosine is kept as a float
		const double sine = x87::Sin(-static_cast<double>(heading));
		const float cosine = x87::RoundToFloat(x87::Cos(-static_cast<double>(heading)));
		const float x = x87::Mul(-sine, moveForwards) + cosine * moveAcross;
		const float z = moveForwards * cosine + x87::Mul(sine, moveAcross);
		strafe.move = {x, 0.0f, z};
	}
	strafe.turned = round != 0.0f;
	if (strafe.turned && (features & k_FeatureRotate) != 0)
	{
		const float turn = round * k_HalfTurn;
		strafe.turn = turn / static_cast<float>(k_Screen.x);
		if (x87::Greater(std::abs(round), k_CountedTurn))
		{
			strafe.turnCounted = true;
			strafe.turnedPositive = round > 0.0f;
			strafe.turnedNegative = round < 0.0f;
		}
	}
	strafe.started = wasStopped && !(SpeedSquare(speed) < k_StoppedSquare);
	return strafe;
}

void Fade(glm::vec3& speed, float seconds, bool inside)
{
	const float acrossAndForwards = x87::Exp((inside ? k_FadeInside : k_FadeAtSide) * seconds);
	speed.x = speed.x * acrossAndForwards;
	speed.y = speed.y * acrossAndForwards;
	speed.z = speed.z * x87::Exp((inside ? k_FadeInside : k_FadeTurnAtSide) * seconds);
	if (SpeedSquare(speed) < k_StoppedSquare)
	{
		speed = glm::vec3(0.0f);
	}
}

float Seconds(uint32_t milliseconds)
{
	return x87::Mul(static_cast<double>(milliseconds), static_cast<double>(k_MillisecondSeconds));
}

uint32_t MillisecondClock::Step(std::chrono::microseconds dt)
{
	_left += std::max(dt, std::chrono::microseconds::zero());
	const auto whole = std::chrono::duration_cast<std::chrono::milliseconds>(_left);
	_left -= whole;
	return static_cast<uint32_t>(whole.count());
}

} // namespace openblack::camera_push_scroll
