/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "VillagerEyes.h"

#include <cmath>

#include <glm/geometric.hpp>
#include <glm/mat3x3.hpp>

namespace openblack::ecs::villager_eyes
{

namespace
{
/// The sun the eyes are shaded by, far off in the sky
constexpr glm::vec3 k_Sun {-500000.0f, 500000.0f, -500000.0f};

/// The small skew the eyes' turn takes, from the sine of a half turn worked out in floats
constexpr float k_FlipSkew = 8.742278e-8f;

/// Each face's eyes are shaded as if their fronts were turned by these angles, in radians, across and up
struct ShadeAngles
{
	float across;
	float up;
};
constexpr std::array<std::array<ShadeAngles, 2>, 3> k_ShadeAngles {{
    {{{.across = 0.0f, .up = 0.0f}, {.across = 0.195f, .up = 0.262f}}},
    {{{.across = -0.15625f, .up = -0.045f}, {.across = 0.34f, .up = -0.253333f}}},
    {{{.across = -0.28125f, .up = -0.165f}, {.across = 0.705f, .up = -0.333333f}}},
}};

/// The column of each face's iris in the eyeball's texture, of eight across
constexpr std::array<int32_t, 3> k_IrisColumn {4, 3, 5};
/// The irises' width, and the height of their row, in the eyeball's texture
constexpr float k_IrisWidth = 0.125f;
constexpr float k_IrisRow = 0.75f;

/// How far the lids turn shut, in radians, when wholly closed
constexpr float k_UpperLidShut = 0.47f;
constexpr float k_LowerLidShut = -0.35f;
} // namespace

glm::mat4 EyeFrame(const glm::mat4& bone, const glm::mat4& frame)
{
	// The eye's pieces face out of it along its back: its front and back swap, mirrored, with the slight skew of a
	// half turn in floats
	glm::mat4 flip(1.0f);
	flip[0] = glm::vec4(1.0f, 0.0f, k_FlipSkew, 0.0f);
	flip[2] = glm::vec4(k_FlipSkew, 0.0f, -1.0f, 0.0f);
	return bone * frame * flip;
}

glm::mat4 Rolled(const glm::mat4& eye, float roll)
{
	if (roll == 0.0f)
	{
		return eye;
	}
	const float c = std::cos(roll);
	const float s = std::sin(roll);
	auto rolled = eye;
	rolled[0] = glm::vec4(glm::vec3(c * eye[0]) + glm::vec3(s * eye[2]), eye[0].w);
	rolled[2] = glm::vec4(glm::vec3(c * eye[2]) - glm::vec3(s * eye[0]), eye[2].w);
	return rolled;
}

glm::mat4 Lid(const glm::mat4& eye, float closed, bool upper)
{
	const float angle = closed * (upper ? k_UpperLidShut : k_LowerLidShut);
	const float c = std::cos(angle);
	const float s = std::sin(angle);
	auto lid = eye;
	lid[1] = glm::vec4(glm::vec3(c * eye[1]) - glm::vec3(s * eye[2]), eye[1].w);
	lid[2] = glm::vec4(glm::vec3(c * eye[2]) + glm::vec3(s * eye[1]), eye[2].w);
	return lid;
}

int32_t Shade(Face face, Side side, const glm::mat4& eye)
{
	const auto angles = k_ShadeAngles.at(static_cast<size_t>(face)).at(static_cast<size_t>(side));
	const float cosAcross = std::cos(angles.across);
	const float sinAcross = std::sin(angles.across);
	const float cosUp = std::cos(angles.up);
	const float sinUp = std::sin(angles.up);
	const glm::vec3 front {sinAcross * cosUp, -sinUp, -(cosAcross * cosUp)};
	const auto facing = glm::normalize(glm::mat3(eye) * front);
	const float lit = glm::dot(facing, glm::normalize(k_Sun)) * 255.0f;
	const auto light = static_cast<int32_t>(std::nearbyint(lit));
	return light < 0 ? k_DarkSide : (((255 - k_DarkSide) * light) >> 8) + k_DarkSide;
}

glm::vec2 IrisOffset(Face face)
{
	return {static_cast<float>(k_IrisColumn.at(static_cast<size_t>(face))) * k_IrisWidth, k_IrisRow};
}

DrawnEyes Place(Face face, const glm::mat4& model, const std::array<glm::mat4, 2>& bones,
                const std::array<glm::mat4, 2>& frames, float roll, float closed)
{
	DrawnEyes drawn {.eyes = {}, .closed = closed, .standing = glm::vec3(model[3])};
	for (const auto side : {Side::Right, Side::Left})
	{
		const auto index = static_cast<size_t>(side);
		const auto eye = EyeFrame(model * bones.at(index), frames.at(index));
		// The lids don't turn with the eyeball
		drawn.eyes.at(index) = {
		    .eyeball = Rolled(eye, roll),
		    .upperLid = Lid(eye, closed, true),
		    .lowerLid = Lid(eye, closed, false),
		    .shade = Shade(face, side, eye),
		};
	}
	return drawn;
}

} // namespace openblack::ecs::villager_eyes
