/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "SnowFaces.h"

#include <cmath>

#include "Common/ModelInverseSquareRoot.h"

namespace
{
/// The steps that the packed up part keeps, 2^28 to a unit
constexpr float k_UpSteps = 268435456.0f;
/// The size of the low part, 2^14
constexpr int32_t k_LowSize = 16384;
/// How much of the snow texture each unit of the up part moves across, and how much the part of a texture coordinate
/// moves
constexpr float k_UpScale = 0.4f;
constexpr float k_PartScale = 0.2f;
} // namespace

namespace openblack::snow_faces
{

float FaceUp(glm::vec3 first, glm::vec3 second, glm::vec3 third)
{
	// The second corner's normal is added to the first's, then the third's to that
	const float x = (second.x + first.x) + third.x;
	const float y = (second.y + first.y) + third.y;
	const float z = (second.z + first.z) + third.z;
	return gutils::ModelInverseSquareRoot((x * x + y * y) + z * z) * y;
}

std::array<int16_t, 2> PackUp(float up)
{
	// A power of two scales a float with no loss
	const auto steps = static_cast<int32_t>(std::lround(up * k_UpSteps));
	// The low part is always 0 or more: the high part takes the sign
	const int32_t high = steps >= 0 ? steps / k_LowSize : -((-steps + k_LowSize - 1) / k_LowSize);
	const int32_t low = steps - high * k_LowSize;
	return {static_cast<int16_t>(low), static_cast<int16_t>(high)};
}

float UnpackUp(int16_t low, int16_t high)
{
	return (static_cast<float>(high) * static_cast<float>(k_LowSize) + static_cast<float>(low)) / k_UpSteps;
}

glm::vec2 SnowUv(float faceUp, glm::vec2 uv)
{
	const auto partOf = [](float value) {
		const float size = std::abs(value);
		return size - std::trunc(size);
	};
	return {(faceUp + 1.0f) * k_UpScale + k_PartScale * partOf(uv.x), k_PartScale * partOf(uv.y)};
}

} // namespace openblack::snow_faces
