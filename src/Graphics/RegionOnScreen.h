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

#include <algorithm>

#include <glm/geometric.hpp>
#include <glm/gtx/norm.hpp>
#include <glm/mat4x4.hpp>
#include <glm/matrix.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>

#include "3D/AffineMatrix.h"
#include "3D/AxisAlignedBoundingBox.h"

/// openblack's one stand-in for the original's "region on screen" test (the one models, particle atoms, mists and the
/// SuperVillagers make before a draw). Header only. It was three copies of the same function (Renderer.cpp,
/// RendererMists.cpp, RendererSmoke.cpp).
/// SphereInView / BoxInView: (approximate) a sphere against the six planes, what the renderer's culls use.
/// ScreenView / PointOnScreen / SphereOnScreen: the faithful screen tests (a point's and the region's own) the scripts'
/// field of view needs (GAME_THING_FIELD_OF_VIEW 011 / POS_FIELD_OF_VIEW 012, Camera/FieldOfView.h)
namespace openblack::graphics::region_on_screen
{

/// Whether a sphere touches the view volume of a view-projection matrix (the planes of its rows, Gribb-Hartmann)
[[nodiscard]] inline bool SphereInView(const glm::mat4& viewProjection, const glm::vec3& centre, float radius)
{
	const glm::mat4 rows = glm::transpose(viewProjection);
	for (int plane = 0; plane < 6; ++plane)
	{
		const glm::vec4 p = rows[3] + (plane % 2 == 0 ? 1.0f : -1.0f) * rows[plane / 2];
		if (glm::dot(glm::vec3(p), centre) + p.w < -radius * glm::length(glm::vec3(p)))
		{
			return false;
		}
	}
	return true;
}

/// A mesh's bounding box under a drawn model matrix (Renderer.cpp's two culls: boned instances, particle mesh atoms): the
/// sphere round the box's centre with half its diagonal times the largest axis scale (column length) of the matrix
[[nodiscard]] inline bool BoxInView(const glm::mat4& viewProjection, const AxisAlignedBoundingBox& box, const glm::mat4& model)
{
	const float scale =
	    std::max({glm::length(glm::vec3(model[0])), glm::length(glm::vec3(model[1])), glm::length(glm::vec3(model[2]))});
	return SphereInView(viewProjection, glm::vec3(model * glm::vec4(box.Center(), 1.0f)),
	                    glm::length(box.Size()) * 0.5f * scale);
}

/// The drawn camera as the engine keeps it for the frame
struct ScreenView
{
	/// The world-to-clipping matrix as the original computes it (billboard::CameraFrame::clipMatrices,
	/// affine::FrameMatrices): a point through it gives (X, Y, Z), Z the camera depth
	affine::AffineMatrix worldToClipping;
	float nearW {1.0f};           ///< the near clip distance
	glm::ivec2 screen {640, 480}; ///< the screen size (the original also keeps the half size)
	glm::vec3 eye {0.0f};         ///< the camera position
	float tanHalfFov {1.0f};      ///< the near plane's half width / near: tan(horizontal fov / 2)
};

/// Z < near -> false; inv = 1 / Z; sx = trunc((X inv + 1) halfW), sy = trunc((1 - Y inv) halfH)
/// (truncated towards 0: a point under a pixel left of / above the screen still counts); on the screen when
/// 0 <= sx < W and 0 <= sy < H
[[nodiscard]] inline bool PointOnScreen(const ScreenView& view, const glm::vec3& point)
{
	// X, Y and Z, the depth
	const auto clip = affine::ToClipForScreenTest(view.worldToClipping, point);
	if (clip.z < view.nearW)
	{
		return false;
	}
	const float inv = 1.0f / clip.z;
	const glm::vec2 half = glm::vec2(view.screen) * 0.5f;
	const auto sx = static_cast<int32_t>((clip.x * inv + 1.0f) * half.x);
	const auto sy = static_cast<int32_t>((1.0f - clip.y * inv) * half.y);
	return sx >= 0 && sy >= 0 && sx < view.screen.x && sy < view.screen.y;
}

/// The region test, its usual path (the other one is pending): `centre` = the box centre through the object's matrix,
/// `radius` = the object's scale x the box's radius, `origin` = the object's own position. Z + r < near -> false; the
/// eye closer than r to `origin` -> true; else rs = r near inv W 0.5 / (near T) = r halfW / (Z T) round the centre's
/// (sx, sy) (not truncated): false when sx + rs < 0, sx - rs > W, sy + rs < 0 or sy - rs > H, else true (partly on the
/// screen counts). (not ported) its side effects: the last on-screen flag, the last selected box, the last distance
/// and the 3D object's notification
[[nodiscard]] inline bool SphereOnScreen(const ScreenView& view, const glm::vec3& centre, float radius, const glm::vec3& origin)
{
	// `centre`: the callers' affine::BoxCentreThroughObject, (approximate) over openblack's model matrix
	const auto clip = affine::ToClipForBoxTest(view.worldToClipping, centre);
	if (clip.z + radius < view.nearW)
	{
		return false;
	}
	if (radius * radius > glm::distance2(origin, view.eye)) // only r^2 > d^2
	{
		return true;
	}
	const float inv = 1.0f / clip.z;
	const glm::vec2 half = glm::vec2(view.screen) * 0.5f;
	const float sx = (clip.x * inv + 1.0f) * half.x;
	const float sy = (1.0f - clip.y * inv) * half.y;
	const auto width = static_cast<float>(view.screen.x);
	const auto height = static_cast<float>(view.screen.y);
	const float rs = radius * inv * width * 0.5f / view.tanHalfFov;
	if (rs + sx < 0.0f || sx - rs > width || rs + sy < 0.0f)
	{
		return false;
	}
	return !(sy - rs > height);
}

} // namespace openblack::graphics::region_on_screen
