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

#include <array>
#include <vector>

#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>

/// The original's 4x3 matrix as it computes it on the CPU: 12 floats, three rows of 3 and the translation; a point is
/// a row vector, p' = p M3 + t. Every function keeps the original's operation order, one float rounding per operation
/// (the game's 24-bit FPU, docs/bw1-notes/audio.md «The game's FPU runs at 24 bits»), so the results are
/// bit-identical. Built with /fp:precise and no FMA contraction (MSVC x64 does not contract by default;
/// test_affine_matrix pins one product to catch a change). Pure maths, no Locator.
namespace openblack::affine
{

struct AffineMatrix
{
	std::array<float, 12> m {1.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f};
};

/// Aliasing: these functions read their inputs whole and return a new matrix, so `m = Mul(m, m)` is the product of the
/// unchanged m. The original's in-place products copy `a` first and then write it while they read `b`: a call there
/// with the same matrix in and out gives a different result, and must be ported explicitly.
/// Mul: a b, the point through a first. Cell order of the original: out0 = ((a2 b6 + a1 b3) + a0 b0), out1 =
/// ((a0 b1 + a2 b7) + a1 b4), ..., out9 = (((a11 b6 + a10 b3) + a9 b0) + b9)
[[nodiscard]] AffineMatrix Mul(const AffineMatrix& a, const AffineMatrix& b);
/// b a, the point through b first. out0 = ((a6 b2 + a3 b1) + a0 b0), ..., out9 = (((a0 b9 + a6 b11) + a3 b10) + a9)
[[nodiscard]] AffineMatrix MultiplyReversed(const AffineMatrix& a, const AffineMatrix& b);
/// The cofactors times 1 / det, |det| < 1e-10 clamped to +-1e-10 with det's sign; the translation
/// -(t1 out3 + t2 out6 + t0 out0, ...) in the original's order
[[nodiscard]] AffineMatrix Inverse(const AffineMatrix& a);
/// The translation
[[nodiscard]] inline glm::vec3 Translation(const AffineMatrix& a)
{
	return {a.m[9], a.m[10], a.m[11]};
}
/// The world-to-camera matrix of (eye, target) with the up (0, 1, 0): D = normalised target - eye (with its two
/// guards), U = up - (up . D) D normalised, R = U x D; rows (Rx, Ux, Dx), (Ry, Uy, Dy), (Rz, Uz, Dz); the translation
/// -(R . eye, U . eye, D . eye) in the original's order
[[nodiscard]] AffineMatrix WorldToCamera(const glm::vec3& eye, const glm::vec3& target);
/// W2C with its x column times sx (1 / tan(fov / 2)) and its y column times sy (aspect sx); z copied
[[nodiscard]] AffineMatrix WorldToClipping(const AffineMatrix& worldToCamera, float sx, float sy);
/// The lens scales of a field of view: half = fov x 0.5 (a float product), T = tan(half) at full precision,
/// sx = 1 / T rounded once, sy = aspect x sx (aspect = W / H). `horizontalFov` in radians
/// (Camera::GetHorizontalFieldOfView). (approximate) 1 / T in double, then rounded: the original rounds once from the
/// FPU's extended value, so a halfway case could differ by one ulp
struct LensScales
{
	float sx;
	float sy;
};
[[nodiscard]] LensScales LensScalesFromFov(float horizontalFov, float aspect);
/// One camera as the original computes it each frame: W2C (WorldToCamera), W = the world-to-clipping matrix and its
/// inverse (Inverse)
struct CameraMatrices
{
	AffineMatrix worldToCamera;
	AffineMatrix worldToClipping;
	AffineMatrix clippingToWorld;
};
[[nodiscard]] CameraMatrices FrameMatrices(const glm::vec3& eye, const glm::vec3& target, float horizontalFov, float aspect);
/// A world point through W, the world-to-clipping matrix: (X, Y, Z), Z the camera depth. Each site of the original
/// sums in its own order, so each has its function.
/// The point-on-screen test: every column ((W[c] x + W[6+c] z) + W[3+c] y) + W[9+c]
[[nodiscard]] glm::vec3 ToClipForScreenTest(const AffineMatrix& w, const glm::vec3& p);
/// The bounding box on-screen test: every column ((W[6+c] z + W[3+c] y) + W[c] x) + W[9+c]
[[nodiscard]] glm::vec3 ToClipForBoxTest(const AffineMatrix& w, const glm::vec3& p);
/// The shadow blocks: X = ((W6 z + W3 y) + x W0) + W9, Y = ((W1 x + W7 z) + W4 y) + W10,
/// Z = ((W8 z + W5 y) + W2 x) + W11
[[nodiscard]] glm::vec3 ToClipForShadowBlocks(const AffineMatrix& w, const glm::vec3& p);
/// The bounding box on-screen test: the box centre b through the object's matrix,
/// cx = ((m3 by + m6 bz) + bx m0) + m9, cy = ((m1 bx + m4 by) + m7 bz) + m10, cz = ((m2 bx + m5 by) + m8 bz) + m11
[[nodiscard]] glm::vec3 BoxCentreThroughObject(const AffineMatrix& object, const glm::vec3& b);
/// A glm model matrix (column vectors) as an AffineMatrix: its columns 0..2 are the rows, column 3 the translation. The cells
/// are copied, not recomputed: an object's matrix keeps openblack's own rounding
[[nodiscard]] AffineMatrix FromModel(const glm::mat4& model);
/// The root of the object's bones: its matrix through W, the world-to-clipping matrix, every cell summed x, z, y:
/// R[3r+c] = (W[c] o[3r] + W[6+c] o[3r+2]) + W[3+c] o[3r+1], R[9+c] = ((W[c] o9 + W[6+c] o11) + W[3+c] o10) + W[9+c].
/// Not Mul(object, w), whose column 0 sums z, y, x
[[nodiscard]] AffineMatrix BoneRootToClip(const AffineMatrix& object, const AffineMatrix& w);
/// For each bone in order (parents first), out[i] = Mul(locals[i], parents[i] == UINT32_MAX ? root :
/// out[parents[i]]); `out` is resized to the bones. The original counts the parent indices from the start of each part
/// of the mesh; every boned mesh of AllMeshes.g3d has its bones in one part, so here they count from 0
void SkinBones(const std::vector<AffineMatrix>& locals, const std::vector<uint32_t>& parents, const AffineMatrix& root,
               std::vector<AffineMatrix>& out);

} // namespace openblack::affine
