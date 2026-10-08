/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "AffineMatrix.h"

#include <cmath>

namespace openblack::affine
{

AffineMatrix Mul(const AffineMatrix& a, const AffineMatrix& b)
{
	// a is read whole first (in place is safe); per row i: (ai2 b6 + ai1 b3) + ai0 b0, then the other two columns in
	// the original's order
	const auto& x = a.m;
	const auto& y = b.m;
	AffineMatrix o;
	for (int i = 0; i < 3; ++i)
	{
		const float a0 = x[3 * i];
		const float a1 = x[3 * i + 1];
		const float a2 = x[3 * i + 2];
		o.m[3 * i] = (a2 * y[6] + a1 * y[3]) + a0 * y[0];
		o.m[3 * i + 1] = (a0 * y[1] + a2 * y[7]) + a1 * y[4];
		o.m[3 * i + 2] = (a0 * y[2] + a2 * y[8]) + a1 * y[5];
	}
	o.m[9] = ((x[11] * y[6] + x[10] * y[3]) + x[9] * y[0]) + y[9];
	o.m[10] = ((x[9] * y[1] + x[11] * y[7]) + x[10] * y[4]) + y[10];
	o.m[11] = ((x[9] * y[2] + x[11] * y[8]) + x[10] * y[5]) + y[11];
	return o;
}

AffineMatrix MultiplyReversed(const AffineMatrix& a, const AffineMatrix& b)
{
	// The result's row r is b's row r times a: rows 0 / 1 / 2 / translation each in the original's order
	const auto& x = a.m;
	const auto& y = b.m;
	AffineMatrix o;
	for (int c = 0; c < 3; ++c)
	{
		o.m[c] = (x[6 + c] * y[2] + x[3 + c] * y[1]) + x[c] * y[0];
		o.m[3 + c] = (x[c] * y[3] + x[6 + c] * y[5]) + x[3 + c] * y[4];
		o.m[6 + c] = (x[c] * y[6] + x[6 + c] * y[8]) + x[3 + c] * y[7];
		o.m[9 + c] = ((x[c] * y[9] + x[6 + c] * y[11]) + x[3 + c] * y[10]) + x[9 + c];
	}
	return o;
}

AffineMatrix Inverse(const AffineMatrix& a)
{
	const auto& b = a.m;
	// det = ((b2 b7 - b8 b1) b3 + (b5 b1 - b2 b4) b6) + (b8 b4 - b7 b5) b0
	const float c0 = b[8] * b[4] - b[7] * b[5];
	float det = ((b[2] * b[7] - b[8] * b[1]) * b[3] + (b[5] * b[1] - b[2] * b[4]) * b[6]) + c0 * b[0];
	// |det| < 1e-10 -> +-1e-10 (|det| equal to 1e-10 is kept); the sign of det, 0 counting as positive
	constexpr float k_Epsilon = 1e-10f;
	if (std::fabs(det) < k_Epsilon)
	{
		det = det < 0.0f ? -k_Epsilon : k_Epsilon;
	}
	const float inv = 1.0f / det;
	AffineMatrix o;
	auto& r = o.m;
	r[0] = c0 * inv;
	r[3] = (b[6] * b[5] - b[8] * b[3]) * inv;
	r[6] = (b[7] * b[3] - b[6] * b[4]) * inv;
	r[1] = (b[2] * b[7] - b[8] * b[1]) * inv;
	r[4] = (b[8] * b[0] - b[6] * b[2]) * inv;
	r[7] = (b[6] * b[1] - b[0] * b[7]) * inv;
	r[2] = (b[5] * b[1] - b[2] * b[4]) * inv;
	r[5] = (b[2] * b[3] - b[0] * b[5]) * inv;
	r[8] = (b[0] * b[4] - b[3] * b[1]) * inv;
	// The translation
	r[9] = -((b[10] * r[3] + b[11] * r[6]) + b[9] * r[0]);
	r[10] = -((r[7] * b[11] + b[9] * r[1]) + r[4] * b[10]);
	r[11] = -((b[9] * r[2] + r[8] * b[11]) + r[5] * b[10]);
	return o;
}

AffineMatrix WorldToCamera(const glm::vec3& eye, const glm::vec3& target)
{
	// d = target - eye
	float dx = target.x - eye.x;
	float dy = target.y - eye.y;
	float dz = target.z - eye.z;
	// guard 1: |dx| < 1e-4 and |dz| < 1e-4 (compared as the double of 1e-4f): dx = +-1e-4
	constexpr double k_Tiny = 9.9999997473787516e-05;
	bool normalise = true;
	if (std::fabs(static_cast<double>(dx)) < k_Tiny && std::fabs(static_cast<double>(dz)) < k_Tiny)
	{
		dx = dx > 0.0f ? 0.0001f : -0.0001f;
	}
	else if (dx == 0.0f && dy == 0.0f && dz == 0.0f) // guard 2: no normalisation
	{
		normalise = false;
	}
	float Dx = dx;
	float Dy = dy;
	float Dz = dz;
	if (normalise)
	{
		const float n = 1.0f / std::sqrt((dz * dz + dy * dy) + dx * dx);
		Dx = dx * n;
		Dy = n * dy;
		Dz = n * dz;
	}
	// up (0, 1, 0) less its part along D
	constexpr float k_UpX = 0.0f;
	constexpr float k_UpY = 1.0f;
	constexpr float k_UpZ = 0.0f;
	const float k = (k_UpZ * Dz + k_UpY * Dy) + k_UpX * Dx;
	float ux = k_UpX - k * Dx;
	float uy = k_UpY - k * Dy;
	float uz = k_UpZ - k * Dz;
	if (!(ux == 0.0f && uy == 0.0f && uz == 0.0f)) // all zero -> not normalised
	{
		const float m = 1.0f / std::sqrt((uz * uz + uy * uy) + ux * ux);
		ux = ux * m;
		uy = uy * m;
		uz = uz * m;
	}
	// R = U x D
	const float Rx = uy * Dz - uz * Dy;
	const float Ry = uz * Dx - ux * Dz;
	const float Rz = ux * Dy - uy * Dx;
	AffineMatrix o;
	o.m = {Rx, ux, Dx, Ry, uy, Dy, Rz, uz, Dz, 0.0f, 0.0f, 0.0f};
	// t = -(R . eye, U . eye, D . eye), each ((-(x0 e.x)) - x1 e.y) - x2 e.z
	o.m[9] = (-(Rx * eye.x) - Ry * eye.y) - Rz * eye.z;
	o.m[10] = (-(ux * eye.x) - uy * eye.y) - uz * eye.z;
	o.m[11] = (-(Dx * eye.x) - Dy * eye.y) - Dz * eye.z;
	return o;
}

AffineMatrix WorldToClipping(const AffineMatrix& worldToCamera, float sx, float sy)
{
	// x column times sx, y column times sy, z copied (the translation too)
	AffineMatrix o = worldToCamera;
	for (int row = 0; row < 4; ++row)
	{
		o.m[3 * row] = worldToCamera.m[3 * row] * sx;
		o.m[3 * row + 1] = worldToCamera.m[3 * row + 1] * sy;
	}
	return o;
}

LensScales LensScalesFromFov(float horizontalFov, float aspect)
{
	// half, tan at full precision, sx = 1 / T, sy = aspect sx
	const float half = horizontalFov * 0.5f;
	const double t = std::tan(static_cast<double>(half));
	const auto sx = static_cast<float>(1.0 / t);
	const float sy = aspect * sx;
	return {sx, sy};
}

CameraMatrices FrameMatrices(const glm::vec3& eye, const glm::vec3& target, float horizontalFov, float aspect)
{
	CameraMatrices out;
	out.worldToCamera = WorldToCamera(eye, target);
	const auto [sx, sy] = LensScalesFromFov(horizontalFov, aspect);
	out.worldToClipping = WorldToClipping(out.worldToCamera, sx, sy);
	out.clippingToWorld = Inverse(out.worldToClipping);
	return out;
}

glm::vec3 ToClipForScreenTest(const AffineMatrix& w, const glm::vec3& p)
{
	const auto& m = w.m;
	return {((m[0] * p.x + m[6] * p.z) + m[3] * p.y) + m[9], ((m[1] * p.x + m[7] * p.z) + m[4] * p.y) + m[10],
	        ((m[2] * p.x + m[8] * p.z) + m[5] * p.y) + m[11]};
}

glm::vec3 ToClipForBoxTest(const AffineMatrix& w, const glm::vec3& p)
{
	const auto& m = w.m;
	// z term first, x last, in all three columns
	return {((m[6] * p.z + m[3] * p.y) + m[0] * p.x) + m[9], ((m[7] * p.z + m[4] * p.y) + m[1] * p.x) + m[10],
	        ((m[8] * p.z + m[5] * p.y) + m[2] * p.x) + m[11]};
}

glm::vec3 ToClipForShadowBlocks(const AffineMatrix& w, const glm::vec3& p)
{
	const auto& m = w.m;
	// The corner is stored as [z, y, x]; Y's order differs from X's and Z's
	return {((m[6] * p.z + m[3] * p.y) + p.x * m[0]) + m[9], ((m[1] * p.x + m[7] * p.z) + m[4] * p.y) + m[10],
	        ((m[8] * p.z + m[5] * p.y) + m[2] * p.x) + m[11]};
}

glm::vec3 BoxCentreThroughObject(const AffineMatrix& object, const glm::vec3& b)
{
	const auto& m = object.m;
	// Then stored as [cz, cy, cx] for ToClipForBoxTest
	return {((m[3] * b.y + m[6] * b.z) + b.x * m[0]) + m[9], ((m[1] * b.x + m[4] * b.y) + m[7] * b.z) + m[10],
	        ((m[2] * b.x + m[5] * b.y) + m[8] * b.z) + m[11]};
}

AffineMatrix FromModel(const glm::mat4& model)
{
	AffineMatrix out;
	for (int r = 0; r < 4; ++r)
	{
		for (int c = 0; c < 3; ++c)
		{
			out.m[static_cast<size_t>(3 * r + c)] = model[r][c];
		}
	}
	return out;
}

AffineMatrix BoneRootToClip(const AffineMatrix& object, const AffineMatrix& w)
{
	const auto& o = object.m;
	const auto& m = w.m;
	AffineMatrix out;
	for (int r = 0; r < 4; ++r)
	{
		for (int c = 0; c < 3; ++c)
		{
			const float sum = (m[c] * o[3 * r] + m[6 + c] * o[3 * r + 2]) + m[3 + c] * o[3 * r + 1];
			out.m[static_cast<size_t>(3 * r + c)] = r < 3 ? sum : sum + m[9 + c];
		}
	}
	return out;
}

void SkinBones(const std::vector<AffineMatrix>& locals, const std::vector<uint32_t>& parents, const AffineMatrix& root,
               std::vector<AffineMatrix>& out)
{
	out.resize(locals.size());
	for (size_t i = 0; i < locals.size(); ++i)
	{
		// (openblack guard) a parent at or after its child (never in the meshes) is taken as the root
		const bool isRoot = i >= parents.size() || parents[i] == UINT32_MAX || parents[i] >= i;
		out[i] = Mul(locals[i], isRoot ? root : out[parents[i]]);
	}
}

} // namespace openblack::affine
