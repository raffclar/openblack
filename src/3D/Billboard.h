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
#include <optional>

#include <glm/mat3x3.hpp>
#include <glm/mat4x4.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

#include "3D/AffineMatrix.h"

namespace openblack
{
class Camera;
}

/// Everything that is turned to the camera, one function per mode of the original (wiki: rendering-objects.md,
/// "Objects that face the camera"). Pure math: no rendering state.
///
/// Conventions. The original uses row vectors (p' = p M: x' = m0 x + m3 y + m6 z + m9): row k of its matrix is the
/// image of local axis k and m9..m11 the translation. glm uses column vectors, so column k here is row k there, with
/// the same memory. The original's rotations turn the other way from glm::rotate: AngleY(a) (rows (c, 0, s),
/// (0, 1, 0), (-s, 0, c)) is glm::rotate(-a, Y). The in-place rotations RotateY and RotateZ mix rows: m * R(-a), on
/// the right; TurnRows mixes each row's components: R(-a) * m, on the left (3D/ObjectMatrix.h, affine).
namespace openblack::graphics::billboard
{

/// The camera of one pass, built once from the camera being drawn (the main one, or the mirrored one of the
/// reflection pass)
struct CameraFrame
{
	// eye, right, up and forward come from Camera::GetOrigin / GetRotationMatrix: in the reflection pass they stay the
	// main camera's (ReflectionXZCamera only mirrors GetViewMatrix), while view, inverseView, worldToCamera and mist are
	// mirrored. Do not mix the two groups in that pass
	glm::vec3 eye {0.0f};                 ///< the camera's origin
	glm::vec3 right {1.0f, 0.0f, 0.0f};   ///< Camera::GetRight (the transpose of the view rotation): screen right
	glm::vec3 up {0.0f, 1.0f, 0.0f};      ///< Camera::GetUp: screen up
	glm::vec3 forward {0.0f, 0.0f, 1.0f}; ///< Camera::GetForward: away from the eye (lookAtLH)
	/// The world to camera matrix: columns (R, U, D) and -(R.eye, U.eye, D.eye); in openblack the view matrix,
	/// glm::lookAtLH (x right, y up, z forward, as the original's camera space)
	glm::mat4 view {1.0f};
	glm::mat4 inverseView {1.0f};   ///< glm::inverse(view): the inverse of the world to camera matrix
	glm::mat3 worldToCamera {1.0f}; ///< the world to camera rotation, mat3(view)
	/// The near plane, read back from the camera's projection. (approximate) Game.cpp computes it as the original does
	/// but only re-sets the projection when it moves by more than 0.01, so it can lag the original's by up to 0.01
	float nearZ {1.0f};
	/// The world to camera rotation with its columns swizzled to rows (A[3i], -A[3i+2], A[3i+1]) then inverted, so rows
	/// R, -D, U: in glm mat3(right, -forward, up). Taken from the columns of inverseView, as the mists and clouds always
	/// did (the transpose and the inverse of the view rotation agree to rounding)
	glm::mat3 mist {1.0f};
	/// (w2c) the camera as the original computes it on the CPU (affine::FrameMatrices: over the camera's origin and
	/// focus, with the field of view's sx / sy): world to camera, world to clipping and its inverse. For the CPU tests
	/// and the bone chains; the GPU keeps `view` and the projection. In the reflection pass they are the main camera's,
	/// as `eye` is (GetOrigin / GetFocus are not mirrored). (not ported) The original also keeps the normalised forward
	/// D every update; its CPU readers use openblack's `forward` (glm) today. D is clipMatrices.worldToCamera's third column
	/// (cells 2, 5, 8) when they move to it
	affine::CameraMatrices clipMatrices;

	[[nodiscard]] static CameraFrame From(const Camera& camera);
};

/// The useful fields of the original's sprite
struct Sprite
{
	glm::vec3 position {0.0f};
	float size {1.0f};           ///< the half width
	float height {1.0f};         ///< the stretch: half height = size x height
	float angle {0.0f};          ///< Screen: the roll on the screen; Horizontal: the yaw
	glm::vec2 origin {0.0f};     ///< world units, subtracted from the local corners
	uint32_t argb {0xFFFFFFFFu}; ///< the colour of the four vertices (the specular is not used here)
	uint8_t cell {0};            ///< 6 bits
	bool horizontal {false};     ///< lies flat in the XZ plane (mode B; circles use it)
	uint8_t cellsPerRow {8};
};

/// Four world-space corners in the original's order v0..v3 and their UVs. Screen: top left, top right, bottom
/// right, bottom left. Horizontal: (-x, -z), (+x, -z), (+x, +z), (-x, +z) of the sprite's own axes.
struct Quad
{
	std::array<glm::vec3, 4> corners;
	std::array<glm::vec2, 4> uv;
};
/// The two triangles of a sprite
inline constexpr std::array<int, 6> k_SpriteTriangles = {0, 1, 2, 0, 2, 3};

/// col = cell % n, row = cell / n, u = col (1/n) + (8/n) {0, .125, .125, 0}[i], v = row (1/n) + (8/n)
/// {0, 0, .125, .125}[i]
[[nodiscard]] std::array<glm::vec2, 4> CellUv(uint8_t cell, uint8_t cellsPerRow);

/// Mode A, a sprite that is not horizontal: the quad lies in the plane of the screen at the sprite's depth (parallel
/// to the screen, not turned to the eye). Local x = {-s - ox, s - ox}, y = {hs - oy, -hs - oy}; with the angle a,
/// local x goes to (cos a, -sin a) on the screen and local y to (sin a, cos a) (m0 = c fx, m1 = -s fy, m3 = s fx,
/// m4 = c fy). No near test (see SpriteQuad).
[[nodiscard]] Quad Screen(const Sprite& sprite, const CameraFrame& frame);
/// Mode A draws nothing when the sprite's depth in the camera is at or before the near plane
[[nodiscard]] bool InFrontOfNear(const glm::vec3& position, const CameraFrame& frame);
/// Mode B, horizontal: Ry(angle) with rows (c, 0, s) / (0, 1, 0) / (-s, 0, c) and the position, the quad in the local
/// XZ plane: x = {-s - ox, s - ox}, z = {-hs - oy, hs - oy}. Ignores the camera.
[[nodiscard]] Quad Horizontal(const Sprite& sprite);
/// A sprite's draw as a whole: Horizontal when horizontal, otherwise Screen, nothing when Screen fails the near test
[[nodiscard]] std::optional<Quad> SpriteQuad(const Sprite& sprite, const CameraFrame& frame);

/// The vs_sprite.sc model matrix of a Screen sprite without origin offset: T(position) Rz(-angle) S(half width, half
/// height, 1) (no turn when the angle is 0). The shader adds u_invView x (model x (x, y, 0, 0)) to the
/// translation, on the plane -1..1 with v = 0 at the top: that is Screen with ox = oy = 0
[[nodiscard]] glm::mat4 ScreenSpriteModel(const glm::vec3& position, const glm::vec2& halfSize, float angle);

/// The quad of mode B in the XZ plane of a given matrix (columns = its rows), turned about its local Y by the angle
/// when it is not 0 (r0' = c r0 + s r2, r2' = c r2 - s r0). The sprite's own position is not used. (The original's
/// matrix already includes world to clip; here it is the world matrix.)
[[nodiscard]] Quad PlaneOfMatrix(const Sprite& sprite, const glm::mat4& matrix);

/// Mode C, the inline billboards (town centre particles, town desire flags, script highlights): theta =
/// atan2(eye.z - p.z, eye.x - p.x) + pi/2 and the axes affine::AngleY(theta): local +Z points from the eye to the
/// object in XZ. No user is ported yet.
[[nodiscard]] float YawToEyeAngle(const glm::vec3& position, const glm::vec3& eye);
[[nodiscard]] glm::mat3 YawToEye(const glm::vec3& position, const glm::vec3& eye);

/// A 3D particle that faces the camera: the frame turned about its local Y so that it faces the camera in x, z, and
/// its Y axis x HeightStretch (axes = the original's rows as columns)
void ParticleYaw(glm::mat3& axes, const glm::vec3& position, const glm::vec3& eye, float heightStretch);

/// A 3D particle with FaceCameraSprite: the identity x the scale, then every row
/// (x, y) := (cos phi x + sin phi y, cos phi y - sin phi x) with phi = pi/2 - atan2(d.y, |d.xz|), then
/// (x, z) := (cos psi x - sin psi z, sin psi x + cos psi z), psi = atan2(d.z, d.x), d = eye - p.
/// Local +Y points at the eye, local Z stays horizontal. The PSR's rotation and stretch are dropped. No spell file
/// sets FaceCameraSprite.
[[nodiscard]] glm::mat3 FullSprite(const glm::vec3& position, const glm::vec3& eye, float scale);

/// The miracle bubble of a one-off spell seed
struct LookAt
{
	glm::mat3 axes;   ///< mat3(U x D, -D, U): local +Y towards the eye
	glm::vec3 offset; ///< drawn = axes (scale v) + position + offset: the pivot is the box centre
};
/// W = the box centre in the world (the object's matrix: position + scale x centre for the orb, which has no
/// rotation), d = W - eye; when |d.x| and |d.z| are both under 1e-4 d.x is replaced by +1e-4 when it is above 0, else
/// -1e-4; D = normalize(d), U = normalize(Y - (Y.D) D) (Y = (0, 1, 0)); the matrix with columns (U x D, -D, U) is
/// inverted in place, so its rows are U x D, -D, U; then M = T(-c) R, x the scale, translation W - c R s. After the
/// 1e-4 push d and U are never zero, so the original's zero test cannot fire and has no port.
[[nodiscard]] LookAt LookAtCentre(const glm::vec3& position, const glm::vec3& boxCentre, float scale, const glm::vec3& eye);

/// The power-up bands of a spell seed graphic (always on: the switch starts set and is never cleared), drawn with the
/// band's matrix: its translation T is taken out, d = T - eye, pushed off the vertical as the bubble's (|d.x| and |d.z|
/// under 1e-4: d.x = +1e-4 when above 0, else -1e-4), D = d / sqrt(d.y^2 + d.z^2 + d.x^2), U' = Y - (Y.D) D with
/// Y = (0, 1, 0), U = U' / sqrt(U'.z^2 + U'.y^2 + U'.x^2); the matrix with columns (-D, U, U x D) inverted in place, so
/// its rows are -D, U, U x D; then the band's 9 cells M = M R (each row times R) and T put back. In glm terms: R (the
/// returned axes, columns -D, U, U x D) times the band's own rotation and scale. After the push the zero tests cannot
/// fire and are not ported
[[nodiscard]] glm::mat3 BandToEye(const glm::vec3& position, const glm::vec3& eye);

/// v = position in the camera, n = normalize(v), t = normalize(n.z, 0, -n.x), u = n x t; the halo's matrix (t, u, n)
/// in the camera space goes to the world with the inverse of the world to camera matrix and its 9 rotation cells x 4.
/// Local +Z points from the eye to the moon, local X is horizontal in the view. @return the axes (columns)
[[nodiscard]] glm::mat3 MoonBasis(const glm::mat4& view, const glm::mat4& inverseView, const glm::vec3& position);
/// The moon mesh's matrix: the halo's, tilted by RotateZ(alpha) (r0' = cos a r0 - sin a r1, r1' = sin a r0 +
/// cos a r1) with alpha = -0.1309 (the drawn moon: k = +1), then RotateY(phase + pi) (r0' = c r0 + s r2,
/// r2' = c r2 - s r0), then x 0.65 (9 cells). In glm terms: basis Rz(+7.5 deg) Ry(-(phase + pi)) S(0.65).
[[nodiscard]] glm::mat4 MoonModel(const glm::mat3& basis, const glm::vec3& position, float phase);
/// The halo's corners v0 = p - 500 (r0 + r1), v1 = p + 500 (r0 - r1), v2 = p + 500 (r1 - r0), v3 = p + 500 (r0 + r1)
/// (the rows already x 4: half width 2000) and the UVs (0.25, 0.25), (0.49375, 0.25), (0.25, 0.49375),
/// (0.49375, 0.49375)
[[nodiscard]] Quad MoonHalo(const glm::mat3& basis, const glm::vec3& position);
/// The halo's triangles
inline constexpr std::array<int, 6> k_MoonHaloTriangles = {0, 1, 3, 3, 2, 0};

/// The mist's 9 cells are CameraFrame::mist
[[nodiscard]] const glm::mat3& MistBasis(const CameraFrame& frame);
/// The mist's effect branch: row 0 keeps the size, rows 1 and 2 take
/// size / (1 + (k - 1)(1 - |d.y| / |d|)), d = mist - eye, with no clamp. (approximate) 1 / |d| by std::sqrt, not the
/// table of InverseSquareRoot; (inferred) d = 0 returns the size.
[[nodiscard]] float MistShrunkSize(float size, float k, const glm::vec3& toMist);

/// The orient-sprite-with-velocity and flocking rules: the vector w in the camera, x = A0 w.x + A3 w.y + A6 w.z =
/// w.right, y = A1 w.x + A4 w.y + A7 w.z = w.up (the world to camera rotation), then AngleY(atan2(-y, x) + pi/2).
/// Screen then puts the sprite's +y along (x, y). @return the angle
[[nodiscard]] float ScreenVelocity(const glm::vec3& w, const glm::vec3& right, const glm::vec3& up);

/// The chains' ribbon: the side of each end of a segment is (eye - joint) x (tail - head) (the view from that joint,
/// head then tail), normalised (by InverseSquareRoot, whose table error a later square root takes out, see
/// RibbonHalfWidth): the same direction as normalize(cross(normalize(segment), normalize(joint - eye))); nothing when
/// either is (almost) zero (openblack's 1e-4 guard)
[[nodiscard]] std::optional<glm::vec3> RibbonSide(const glm::vec3& segment, const glm::vec3& joint, const glm::vec3& eye);
/// The ribbon's half width: each vertex is joint +- side x (the joint's scale) / |side|, the joint's scale being the
/// PSR scale, so the scale itself
[[nodiscard]] float RibbonHalfWidth(float scale);

/// The volume blend mesh particle (always on): a = normalize(the PSR's row 0), d = normalize(eye - p),
/// b = normalize(a x d), c = d x b (then c is normalised again: (inferred) a no-op on that unit vector); rows c, b, d x
/// the PSR scale. No spell file uses ParticleVolBlendMeshCreator.
[[nodiscard]] glm::mat3 VolumeBlendBasis(const glm::vec3& axisX, const glm::vec3& position, const glm::vec3& eye, float scale);

} // namespace openblack::graphics::billboard
