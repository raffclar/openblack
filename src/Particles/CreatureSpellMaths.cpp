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
#include "Common/PortableMath.h"
#include "Common/X87.h"

using namespace openblack::particles;
namespace x87 = openblack::x87;

namespace
{
/// How sharply a wisp narrows above its full width
constexpr float k_TaperSteepness = 3.3333333f;
/// A wisp's angles wrap at a full turn
constexpr float k_TwoPi = 2.0f * std::numbers::pi_v<float>;
/// A wisp's wobble turns at this rate against its age
constexpr float k_WispWobbleSpeed = 1.0f;
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

float maths::WispTurn(float angle, float rate, float dt)
{
	return std::fmod(angle + dt * rate, k_TwoPi);
}

glm::vec3 maths::WispOrbit(const WispBox& box, float theta, float phi, float taper, float age)
{
	// Each sine and cosine stays wide until the next operation rounds it
	const float up = x87::Add(x87::Cos(phi), 1.0f) * 0.5f;
	float width = 1.0f;
	if (!(up < k_WispFullWidthBelow))
	{
		const float over = (up - k_WispFullWidthBelow) * k_TaperSteepness;
		width = 1.0f - over * over * taper;
	}
	glm::vec3 point(box.centreX + x87::Mul(x87::Cos(theta), width) * box.radius * k_WispReach,
	                box.bottom + up * box.height * k_WispReach,
	                box.centreZ + x87::Mul(x87::Sin(theta), width) * box.radius * k_WispReach);
	// It wobbles on its way round
	const float t = age * k_WispWobbleSpeed;
	point.x += x87::Add(x87::Sin(1.3f * t), x87::Sin(t)) * k_WispWobble * box.radius;
	point.y += x87::Add(x87::Sin(1.13f * t), x87::Sin(0.53f * t)) * k_WispWobble * box.radius;
	point.z += x87::Add(x87::Sin(0.9f * t), x87::Sin(2.72f * t)) * k_WispWobble * box.radius;
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
	// The angle wraps at a full turn that is the float of two pi, held as a double. The sine uses the wrapped angle as
	// it is and multiplies at its full precision. The cosine uses the angle as a float, and the result becomes a float.
	const double angle = std::fmod(static_cast<double>(seconds * orbitSpeed), static_cast<double>(k_TwoPi));
	const double sine = x87::Sin(angle);
	const float cosine = x87::RoundToFloat(x87::Cos(x87::RoundToFloat(angle)));
	const glm::vec3 acrossPart(x87::Mul(across.x, sine), x87::Mul(across.y, sine), x87::Mul(across.z, sine));
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
	return openblack::portable_math::Cos(static_cast<double>(limit)) > static_cast<double>(dot);
}
