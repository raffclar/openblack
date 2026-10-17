/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "CreatureSpellMaths.h"

#include <cmath>

#include <algorithm>
#include <limits>
#include <numbers>

#include <glm/geometric.hpp>

#include "Common/ModelInverseSquareRoot.h"

using namespace openblack::particles;

namespace
{
/// How sharply a wisp narrows above its full width
constexpr float k_TaperSteepness = 3.3333333f;
/// A wisp's opacity comes up from the least by the rise over this many seconds
constexpr float k_WispFadeInSeconds = 0.5f;
constexpr float k_WispLeastAlpha = 20.0f;
constexpr float k_WispAlphaRise = 30.0f;
} // namespace

maths::WispBox maths::WispBoxOf(std::span<const glm::vec3> bones)
{
	if (bones.empty())
	{
		return {};
	}
	glm::vec3 low(std::numeric_limits<float>::max());
	glm::vec3 high(std::numeric_limits<float>::lowest());
	for (const auto& bone : bones)
	{
		low = glm::min(low, bone);
		high = glm::max(high, bone);
	}
	const float dx = high.x - low.x;
	const float dz = high.z - low.z;
	return {.centreX = (low.x + high.x) * 0.5f,
	        .centreZ = (low.z + high.z) * 0.5f,
	        .bottom = low.y,
	        .height = high.y - low.y,
	        .radius = 0.5f * std::sqrt(dx * dx + dz * dz)};
}

glm::vec3 maths::WispOrbit(const WispBox& box, float theta, float phi, float taper, float age)
{
	const float up = (std::cos(phi) + 1.0f) * 0.5f;
	float width = 1.0f;
	if (!(up < k_WispFullWidthBelow))
	{
		const float over = (up - k_WispFullWidthBelow) * k_TaperSteepness;
		width = 1.0f - over * over * taper;
	}
	glm::vec3 point(box.centreX + std::cos(theta) * width * box.radius * k_WispReach,
	                box.bottom + up * box.height * k_WispReach,
	                box.centreZ + std::sin(theta) * width * box.radius * k_WispReach);
	// It wobbles on its way round
	const float t = age;
	point.x += (std::sin(1.3f * t) + std::sin(t)) * k_WispWobble * box.radius;
	point.y += (std::sin(1.13f * t) + std::sin(0.53f * t)) * k_WispWobble * box.radius;
	point.z += (std::sin(0.9f * t) + std::sin(2.72f * t)) * k_WispWobble * box.radius;
	return point;
}

glm::vec3 maths::WispPosition(glm::vec3 hand, glm::vec3 orbit, float age)
{
	const float flown = std::clamp(age / k_WispFlightSeconds, 0.0f, 1.0f);
	return hand + (orbit - hand) * flown;
}

uint8_t maths::WispAlpha(float age)
{
	const float risen = std::clamp(age / k_WispFadeInSeconds, 0.0f, 1.0f);
	return static_cast<uint8_t>(static_cast<int32_t>(risen * k_WispAlphaRise + k_WispLeastAlpha));
}

float maths::WispsDue(float due, int most, float dt, float emitSeconds)
{
	const auto mostDue = static_cast<float>(most);
	if (!(due < mostDue))
	{
		return due;
	}
	return std::min(due + mostDue * dt / emitSeconds, mostDue);
}

glm::vec3 maths::ItchOrbit(glm::vec3 rightEye, glm::vec3 leftEye, float seconds, float orbitSpeed)
{
	const auto centre = (rightEye + leftEye) * 0.5f;
	const auto out = (rightEye - centre) * k_ItchReach;
	glm::vec3 across(out.z - 0.0f, 0.0f, 0.0f - out.x);
	// The lengths add the squares in the order of the game: y, then z, then x
	const float outLength = std::sqrt((out.y * out.y + out.z * out.z) + out.x * out.x);
	if (across.x != 0.0f || across.y != 0.0f || across.z != 0.0f)
	{
		const float acrossLength = std::sqrt((across.z * across.z + across.y * across.y) + across.x * across.x);
		across *= outLength / acrossLength;
	}
	// The angle wraps at the full turn of double precision. The sine uses the angle as it is. The cosine uses the angle
	// cut to a float, and the result is cut to a float too.
	const double angle = std::fmod(static_cast<double>(seconds * orbitSpeed), 2.0 * std::numbers::pi);
	const auto sine = std::sin(angle);
	const auto cosine = static_cast<float>(std::cos(static_cast<double>(static_cast<float>(angle))));
	const glm::vec3 acrossPart(static_cast<float>(across.x * sine), static_cast<float>(across.y * sine),
	                           static_cast<float>(across.z * sine));
	return (centre + out * cosine) + acrossPart;
}

bool maths::SwungTooFar(glm::vec2 began, glm::vec2 now, float limit)
{
	// The engine's table inverse square root makes each way unit length, so a unit way is a little short.
	// A way of no length becomes a way of no length, and its dot is 0.
	const float beganScale = openblack::gutils::ModelInverseSquareRoot((began.x * began.x) + (began.y * began.y));
	const float beganX = beganScale * began.x;
	const float beganZ = began.y * beganScale;
	const float nowScale = openblack::gutils::ModelInverseSquareRoot((now.y * now.y) + (now.x * now.x));
	const float nowX = now.x * nowScale;
	const float nowZ = nowScale * now.y;
	const float dot = (nowX * beganX) + (nowZ * beganZ);
	// The cosine of the limit keeps its full precision
	return std::cos(static_cast<double>(limit)) > static_cast<double>(dot);
}
