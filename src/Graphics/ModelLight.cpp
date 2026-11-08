/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "ModelLight.h"

#include <cmath>

#include <algorithm>

#include <glm/geometric.hpp>

#include "Common/X87.h"

using namespace openblack;

namespace
{
constexpr float k_HandLift = 10.0f;
/// When the sky type, as the game counts it, is more than this double, the light moves to the hand
constexpr double k_HandLightSkyType = 1.5;
constexpr float k_DistanceFromHand = 3.0f;
} // namespace

glm::vec3 model_light::FrameLight(glm::vec3 hand, float groundUnderHand, const glm::vec3& camera, float skyType,
                                  std::optional<glm::vec3> templeOrigin)
{
	if (templeOrigin.has_value())
	{
		return *templeOrigin + k_Temple;
	}
	if (!x87::Greater(skyType, k_HandLightSkyType))
	{
		return k_Sun;
	}
	hand.y = std::max(hand.y, groundUnderHand + k_HandLift);
	auto direction = camera - hand;
	// The length adds z, then y, then x. The game multiplies by the inverse of the length.
	if (direction != glm::vec3(0.0f))
	{
		const float lengthSquared = x87::Add(x87::Add(x87::Mul(direction.z, direction.z), x87::Mul(direction.y, direction.y)),
		                                     x87::Mul(direction.x, direction.x));
		const float inverse = 1.0f / std::sqrt(lengthSquared);
		direction *= inverse;
	}
	return hand + direction * k_DistanceFromHand;
}

glm::vec4 model_light::Uniform(const glm::vec3& light, float ambient)
{
	return {light, ambient};
}

glm::vec3 model_light::LocalDirection(const glm::vec3& light, const glm::vec3& axisX, const glm::vec3& axisY,
                                      const glm::vec3& axisZ, const glm::vec3& origin)
{
	const auto toLight = light - origin;
	const auto row0 = glm::cross(axisY, axisZ);
	const auto row1 = glm::cross(axisZ, axisX);
	const auto row2 = glm::cross(axisX, axisY);
	const float determinant = glm::dot(axisX, row0);
	auto local = glm::vec3(glm::dot(row0, toLight), glm::dot(row1, toLight), glm::dot(row2, toLight));
	local *= determinant < 0.0f ? -1.0f : 1.0f;
	const float lengthSquared = glm::dot(local, local);
	return lengthSquared > 0.0f ? local / std::sqrt(lengthSquared) : glm::vec3(0.0f);
}

float model_light::Factor(const glm::vec3& normal, const glm::vec3& localLight, float ambient)
{
	// Rounded to the nearest whole number, halves to even, as the game's FPU stores it
	const float intensity = std::nearbyint(255.0f * glm::dot(normal, localLight));
	return intensity < 0.0f ? ambient : ambient + std::floor((255.0f - ambient) * intensity / 256.0f);
}

float model_light::PosedFacing(const glm::vec3& normal, const glm::vec3& localLight)
{
	// The game adds the z part to the x part first, then the y part, each step rounded to a float
	const float zx = normal.z * localLight.z + normal.x * localLight.x;
	return zx + normal.y * localLight.y;
}

float model_light::PosedFactor(const glm::vec3& normal, const glm::vec3& localLight)
{
	constexpr float k_Gain = 1.3f;
	constexpr float k_Floor = 0.4f;
	const float facing = PosedFacing(normal, localLight);
	// The fraction dropped, as the game does turning it into a whole number
	return std::floor(std::min((facing < 0.0f ? 0.0f : facing * k_Gain) + k_Floor, 1.0f) * 255.0f);
}
