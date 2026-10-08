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

#include <functional>
#include <span>
#include <string>
#include <vector>

#include <glm/mat4x4.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>

namespace openblack
{
class LandIslandInterface;
namespace graphics
{
class ShaderManager;
class ShaderProgram;
} // namespace graphics
} // namespace openblack

/// Everything that follows the ground, one function per algorithm of the original (wiki: rendering-objects.md, "Meshes
/// stuck to the ground"). Every height is the island's altitude (LandIsland::HeightAt on the CPU, LandAltitude of
/// assets/shaders/land_altitude.sh on the GPU), at the world x, z of the point (x 65536 x 0.1 truncated to 16.16 map
/// coordinates).
///
/// - A, RaiseAboveLandscape: the mesh cut along the land's cells and both diagonals, then y += H(v) - H(origin). Once,
///   when a ZR_SurfRevol with DoRaiseAboveLandscape is made.
/// - B, Melting: one delta per vertex, (H(v) - H(origin)) / scale, added along the model's local Y by the morphable
///   draw. In openblack the vertex shader does it (components::MorphWithTerrain).
/// - C, Bake: the same delta written into the vertices (the fragment meshes, ClampToLandscape, the melted borders, the
///   citadel).
/// - D, built on the ground: y = H + a constant (the blobs, the influence circle, the leash, the creature's quads).
/// Not here: the shadows on the land (which redraw the land itself) and the foundations (a rigid sink).
namespace openblack::land_morph
{

/// The ground height at a world x, z
using Ground = std::function<float(glm::vec2)>;

/// LandIslandInterface::GetHeightAt (the altitude with the sea flattening on)
[[nodiscard]] Ground Altitude(const LandIslandInterface& land);
/// Altitude of the Locator's island, 0 while there is none
[[nodiscard]] Ground CurrentAltitude();

/// The delta every algorithm adds, as the original orders the float operations: (H(p) - originHeight) + p.y
[[nodiscard]] float Raised(const Ground& ground, const glm::vec3& point, float originHeight);

// ---- A: RaiseAboveLandscape --------------------------------------------------------------------------------------

/// One particle mesh primitive: positions, diffuse ARGB, specular ARGB, UVs, triangles and normals. The colours and
/// normals are cut only when there is one per position; the original always cuts the UVs ((port guard) here only when
/// there is one per position too).
/// The cut and the raise do one float operation per step in the original's order: the original runs the FPU at single
/// precision (wiki: camera-tracks.md), so each step rounds to float as here.
struct Primitive
{
	std::vector<glm::vec3> positions;
	std::vector<glm::vec2> uvs;
	std::vector<glm::vec3> normals;
	std::vector<uint32_t> diffuse;
	std::vector<uint32_t> specular;
	std::vector<uint32_t> indices; ///< three per triangle
};

/// The triangles that cross the plane n.p + w = 0 are split there. dist <= 0 counts as negative; the lone vertex A is
/// the first whose sign is the product of the three; on the edges to B and C, t = -dA / (dO - dA), position, UV and
/// normal (not renormalised) are A + t (O - A) and each colour byte cA + ((cO - cA) truncated(t 255) >> 8). The new
/// vertices n1 (on AB) and n2 (on AC) go at the end; the triangle becomes (A, n1, n2) and (B, C, n2), (B, n2, n1) are
/// added. Only the triangles there were before are tested.
void SplitByPlane(Primitive& primitive, const glm::vec4& plane);

/// The cutting planes for a box from min to max (taken as centre and half extents of the positions), in the original's
/// order: x = 10 i, z = 10 j, x + z = 10 k, x - z = 10 k
[[nodiscard]] std::vector<glm::vec4> CellPlanes(const glm::vec3& minimum, const glm::vec3& maximum);

/// On a mesh already in world space: every primitive's box, then the first primitive alone is cut by CellPlanes and
/// raised by H(v) - H(origin). The caller moves the mesh to the world and back (with M, then its inverse).
void RaiseAboveLandscape(const Ground& ground, std::span<Primitive> worldPrimitives, glm::vec2 originXZ);

// ---- B: Melting ------------------------------------------------------------------------------------------------------

/// When the deltas are taken (components::MorphWithTerrain). The original takes them once (Snapshot) except for
/// PhysicalShield, which takes them again on every shield draw. (approximate) The GPU path of openblack
/// computes them on every draw for both: they only differ when the land changes after the object is made, and the land
/// of openblack changes only in FlattenLandUnderTemple (CitadelArchetype.cpp) while the map loads.
enum class Melting : uint8_t
{
	Snapshot,
	Live,
};

/// The melting on the CPU: for each model vertex v, w = v M + pos, delta = (H(w) - H(pos)) x (1 / scale), in model
/// units along the local Y. The guards (the object has a delta buffer and is morphable) are the caller's. `object` is
/// the object's matrix with its scale.
void MeltingDeltas(const Ground& ground, const glm::mat4& object, float scale, std::span<const glm::vec3> modelVertices,
                   std::span<float> outDeltas);

// ---- C: Bake ---------------------------------------------------------------------------------------------------------

/// The delta written into world vertices: y = (H(v) - originHeight) + y. ClampToLandscape (the particle mesh draw,
/// every primitive, every frame, no cut) and the fragment meshes
void Bake(const Ground& ground, std::span<glm::vec3> worldVertices, float originHeight);

/// The melted borders and the citadel: model vertices, y += H(w) - pos.y with w the vertex in the world (no 1 / scale)
void BakeAgainstY(const Ground& ground, const glm::mat4& object, std::span<glm::vec3> modelVertices);

// ---- D: built on the ground ------------------------------------------------------------------------------------------

/// y = H(x, z) + lift
[[nodiscard]] float OnGround(const Ground& ground, glm::vec2 xz, float lift);

constexpr float k_BlobLift = 0.2f;          ///< the blobs
constexpr float k_LeashRibbonLift = 0.1f;   ///< the leash ribbon
constexpr float k_CreatureQuadLift = 0.15f; ///< the creature's quads

/// The influence circle curtain: three vertices per segment at H, H + 20 and H + 40, the last segment
/// closing the ring at angle 0
struct Curtain
{
	std::vector<glm::vec3> positions;
	std::vector<glm::vec2> uvs;
	std::vector<uint32_t> colours;
	std::vector<uint32_t> indices; ///< four triangles per segment
};

/// Made with the influence circle: N = clamp(truncated(2 pi r 0.05), 8, 250); vertex row k of segment i at angle
/// 2 pi i / N, (cos r + cx, (H - H0) + H0 + {0, 20, 40}, sin r + cz); u += (1 + trunc(2 pi r / 111)) / N,
/// v += min(truncated(r / 60), 6) / N, rows v, v + 0.2, v + 0.4; the colour of every vertex is the player's;
/// triangles (b, b+3, b+4), (b, b+4, b+1), (b+1, b+4, b+5), (b+1, b+5, b+2), b = 3 i
[[nodiscard]] Curtain InfluenceCurtain(const Ground& ground, const glm::vec3& centre, float radius, uint32_t colour);

// ---- GPU -------------------------------------------------------------------------------------------------------------

/// The fragment shader the object pass uses: fs_object, or fs_object_shadow (the hand's shadow on objects)
enum class ObjectPass : uint8_t
{
	Main,
	Shadow,
};

/// The one choice between the plain object program and the one with the land morph (vs_object_hm_instanced: the
/// morphable draw against the static one)
[[nodiscard]] const std::string& ObjectProgramName(bool morph, ObjectPass pass = ObjectPass::Main);
[[nodiscard]] const graphics::ShaderProgram* ObjectProgram(const graphics::ShaderManager& shaders, bool morph,
                                                           ObjectPass pass = ObjectPass::Main);

} // namespace openblack::land_morph
