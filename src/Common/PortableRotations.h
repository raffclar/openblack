/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <glm/common.hpp>
#include <glm/ext/scalar_constants.hpp>
#include <glm/geometric.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>

#include "Common/PortableMath.h"

/// The rotation matrices of glm, with the sine and cosine of portable_math, so that each build makes the same bits. Each
/// function makes its matrix with the same steps in the same order as the glm function of the same name.
namespace openblack::portable_math
{

/// A turn about the x axis
[[nodiscard]] inline glm::mat4 EulerAngleX(float angle)
{
	const float c = Cos(angle);
	const float s = Sin(angle);
	return {1.0f, 0.0f, 0.0f, 0.0f, 0.0f, c, s, 0.0f, 0.0f, -s, c, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f};
}

/// A turn about the y axis
[[nodiscard]] inline glm::mat4 EulerAngleY(float angle)
{
	const float c = Cos(angle);
	const float s = Sin(angle);
	return {c, 0.0f, -s, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, s, 0.0f, c, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f};
}

/// A turn about the z axis
[[nodiscard]] inline glm::mat4 EulerAngleZ(float angle)
{
	const float c = Cos(angle);
	const float s = Sin(angle);
	return {c, s, 0.0f, 0.0f, -s, c, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f};
}

/// The turns about x, then y, then z, as glm::eulerAngleXYZ
[[nodiscard]] inline glm::mat4 EulerAngleXYZ(float t1, float t2, float t3)
{
	const float c1 = Cos(-t1);
	const float c2 = Cos(-t2);
	const float c3 = Cos(-t3);
	const float s1 = Sin(-t1);
	const float s2 = Sin(-t2);
	const float s3 = Sin(-t3);
	glm::mat4 result;
	result[0][0] = c2 * c3;
	result[0][1] = -c1 * s3 + s1 * s2 * c3;
	result[0][2] = s1 * s3 + c1 * s2 * c3;
	result[0][3] = 0.0f;
	result[1][0] = c2 * s3;
	result[1][1] = c1 * c3 + s1 * s2 * s3;
	result[1][2] = -s1 * c3 + c1 * s2 * s3;
	result[1][3] = 0.0f;
	result[2][0] = -s2;
	result[2][1] = s1 * c2;
	result[2][2] = c1 * c2;
	result[2][3] = 0.0f;
	result[3][0] = 0.0f;
	result[3][1] = 0.0f;
	result[3][2] = 0.0f;
	result[3][3] = 1.0f;
	return result;
}

/// A matrix turned by an angle about an axis, as glm::rotate
[[nodiscard]] inline glm::mat4 Rotate(const glm::mat4& m, float angle, const glm::vec3& v)
{
	const float c = Cos(angle);
	const float s = Sin(angle);
	const glm::vec3 axis(glm::normalize(v));
	const glm::vec3 temp((1.0f - c) * axis);

	glm::mat4 rotate;
	rotate[0][0] = c + temp[0] * axis[0];
	rotate[0][1] = temp[0] * axis[1] + s * axis[2];
	rotate[0][2] = temp[0] * axis[2] - s * axis[1];

	rotate[1][0] = temp[1] * axis[0] - s * axis[2];
	rotate[1][1] = c + temp[1] * axis[1];
	rotate[1][2] = temp[1] * axis[2] + s * axis[0];

	rotate[2][0] = temp[2] * axis[0] + s * axis[1];
	rotate[2][1] = temp[2] * axis[1] - s * axis[0];
	rotate[2][2] = c + temp[2] * axis[2];

	glm::mat4 result;
	result[0] = m[0] * rotate[0][0] + m[1] * rotate[0][1] + m[2] * rotate[0][2];
	result[1] = m[0] * rotate[1][0] + m[1] * rotate[1][1] + m[2] * rotate[1][2];
	result[2] = m[0] * rotate[2][0] + m[1] * rotate[2][1] + m[2] * rotate[2][2];
	result[3] = m[3];
	return result;
}

/// A turn by an angle about an axis, as glm::rotate of an angle and an axis
[[nodiscard]] inline glm::mat4 Rotate(float angle, const glm::vec3& v)
{
	return Rotate(glm::mat4(1.0f), angle, v);
}

/// The turn part of the way from one rotation to another along the shortest arc, as glm::slerp
[[nodiscard]] inline glm::quat Slerp(const glm::quat& x, const glm::quat& y, float a)
{
	glm::quat z = y;
	float cosTheta = glm::dot(x, y);
	// The long way round: one rotation is negated
	if (cosTheta < 0.0f)
	{
		z = -y;
		cosTheta = -cosTheta;
	}
	// Nearly the same rotation: a straight blend, as the sine of the angle is nearly zero
	if (cosTheta > 1.0f - glm::epsilon<float>())
	{
		return glm::quat::wxyz(glm::mix(x.w, z.w, a), glm::mix(x.x, z.x, a), glm::mix(x.y, z.y, a), glm::mix(x.z, z.z, a));
	}
	const float angle = Acos(cosTheta);
	return (Sin((1.0f - a) * angle) * x + Sin(a * angle) * z) / Sin(angle);
}

} // namespace openblack::portable_math
