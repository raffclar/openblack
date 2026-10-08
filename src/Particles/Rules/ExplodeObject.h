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
#include <memory>
#include <optional>
#include <vector>

#include <entt/core/fwd.hpp>
#include <entt/entity/entity.hpp>
#include <glm/mat3x3.hpp>
#include <glm/mat4x4.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

#include "Particles/PSys.h"

// The meshes thrown to pieces: the queues QueueObject / QueueMesh fill, the always-on EXPLODE_OBJECT effect
// (SF_ExplodeObject, PARTICLE_TYPE 23) that empties them once a turn (GameLoopEnd), its rule UR_ExplodeObject and the
// pieces' draw (here world triangles: mesh_pieces below and Graphics/WorldTriangles.h). Wiki: docs/bw1-notes/miracles.md,
// "The pieces".

namespace openblack::graphics::world_triangles
{
struct Frame;
}

namespace openblack::psys
{
struct Atom;

namespace manager
{
struct FrameInputs;
}

namespace explode_object
{

/// What the explosion reads of a mesh: the sub-meshes with their flags and primitives; of each primitive its vertices
/// (position, uv, normal), its triangles (uint16 x 3) and its material. Read from the mesh's own L3D data.
struct SourcePrimitive
{
	std::vector<glm::vec3> positions;
	std::vector<glm::vec2> uvs;
	std::vector<glm::vec3> normals;
	std::vector<std::array<uint16_t, 3>> triangles;
};
struct SourceSubMesh
{
	uint32_t flags {0}; ///< the L3D sub-mesh flags as the original keeps them (0x20000000: LOD 0; 0x3F0: the status)
	std::vector<SourcePrimitive> primitives;
};
struct SourceMesh
{
	entt::id_type meshId {0}; ///< the resource the pieces take their materials from (the same sub-mesh / primitive)
	std::vector<SourceSubMesh> subMeshes;
};

/// The mesh of the mesh pack at index (AllMeshes.g3d), read once; nullptr if it is not there
[[nodiscard]] std::shared_ptr<const SourceMesh> PackMesh(uint32_t index);
/// The CPU copy of an L3D mesh's sub-meshes and primitives (`index` gives its mesh id); nothing when it is not an L3D
[[nodiscard]] std::optional<SourceMesh> ReadSourceMesh(uint32_t index, const std::vector<uint8_t>& bytes);

/// One queue entry: {mesh, world matrix, origin, speed, spread}
struct QueuedMesh
{
	std::shared_ptr<const SourceMesh> mesh;
	glm::mat3 axes {1.0f};     ///< the matrix rows as columns: world = position + axes x local
	glm::vec3 position {0.0f}; ///< the matrix translation
	glm::vec3 origin {0.0f};   ///< the point the pieces fly away from
	float speed {0.0f};
	float spread {0.0f}; ///< stored, read by nobody (the explosion does not get it)
};

/// Queues a mesh with its matrix, origin, speed and spread; an object without a mesh is not queued.
/// `second` queues in UR_ExplodeObject2's queue, else in UR_ExplodeObject's
void QueueMesh(std::shared_ptr<const SourceMesh> mesh, const glm::mat3& axes, const glm::vec3& position,
               const glm::vec3& origin, float speed, float spread, bool second = false);
/// The object's mesh with its world matrix. False when the object has none or its mesh is not one of the pack's.
bool QueueObject(entt::entity object, const glm::vec3& origin, float speed, float spread, bool second = false);
/// The entries waiting in a queue (for the tests and the traces)
[[nodiscard]] size_t QueuedCount(bool second = false);

/// Once a turn, at the end of the game loop: the EXPLODE_OBJECT effect made if there is none (type 23 at the world
/// origin, magnitude 1, no spell) and processed; once it says 5 (finished) it is deleted and made again on the next turn
void GameLoopEnd();
/// When the map is cleared: empties the two queues (the effect goes with the particle system)
void Clear();

/// The pieces of one primitive: lists of triangle indices, in the order they are made
[[nodiscard]] std::vector<std::vector<uint32_t>> SplitPrimitive(const SourcePrimitive& primitive, int maxTrigsPerFrag);
/// The velocity of a piece whose centroid is `centre`: d = centre - origin set to `speed` long (if not 0), r (a random
/// point in the unit ball) set to |d| x RandomFactor long (if not 0), then (d.x + r.x, d.y + r.y / 2, d.z + r.z)
[[nodiscard]] glm::vec3 PieceVelocity(const glm::vec3& centre, const glm::vec3& origin, float speed, float randomFactor,
                                      glm::vec3 random);

/// A piece atom's mesh: three vertices per triangle (positions about the centroid; uvs; normals, the source's as they
/// are, not turned by the matrix), the triangles b, b + 1, b + 2, no colours of its own and the source primitive as its
/// material. Drawn as the original does, as world triangles: one CPU-made triangle list per frame (mesh_pieces::Build,
/// graphics::world_triangles), Creator::Kind::MeshPiece, no bgfx buffer per piece and no handle limit
struct Piece
{
	std::shared_ptr<const SourceMesh> source; ///< the mesh of its material
	uint16_t subMesh {0};
	uint16_t primitive {0};
	std::vector<glm::vec3> positions;
	std::vector<glm::vec2> uvs;
	std::vector<glm::vec3> normals;
	uint32_t triangles {0};
};
/// The piece an atom carries, nullptr for the other atoms
[[nodiscard]] const Piece* PieceOf(const Atom& atom);

/// The draw colour (0xAARRGGBB) times the land light under the drawn position, byte by byte, alpha included (the pieces
/// are lit by the land)
[[nodiscard]] uint32_t LitColour(uint32_t argb, const glm::vec3& position);
/// The land light under a point (0xAARRGGBB, alpha 0xFF)
[[nodiscard]] uint32_t LandLight(const glm::vec3& position);

/// RegisterExplosionRules (Rules/Explosion.cpp) calls it: UR_ExplodeObject, UR_ExplodeObject2
void RegisterRules();

} // namespace explode_object

/// The draw of the exploded pieces, to graphics::world_triangles. Called WHERE THEY ARE DRAWN (after
/// model_light::UpdateFrameLight of the frame): the original draws them at once, with no Z object; EXPLODE_OBJECT is
/// Sorted, drawn in the particle draw loop after the game's draw and before the sort queue is drained.
namespace mesh_pieces
{
/// The draw colour of a drawn atom, 0xAARRGGBB: its colour and its alpha byte (0..255, truncated)
[[nodiscard]] uint32_t DrawDataColour(const Effect::DrawAtom& atom);
/// The draw matrix of a drawn atom: rotation x scale, the Y axis x the stretch, the translation its position
[[nodiscard]] glm::mat4 DrawMatrix(const Effect::DrawAtom& atom);
/// One piece with the drawn matrix `model` and the draw colour `argb`:
/// - the base colour: argb x the land light under the matrix's translation (LitColour);
/// - every vertex through the matrix into the world; the pieces' vertices are never stuck to the land;
/// - every vertex of the base colour, then the model light (one normal per vertex): the light through the inverse of
///   the matrix, I = round(255 n.l), the ambient (model_light::Apply). The second light is 0;
/// - the global-alpha render mode table when the draw colour's alpha byte is not 0xFF, back to the normal one after
///   the draw.
/// Appends one batch (or joins the last one) with `tag`
void AppendPiece(graphics::world_triangles::Frame& out, const explode_object::Piece& piece, const glm::mat4& model,
                 uint32_t argb, const void* tag = nullptr);
/// Every piece of the running effects drawn by `path`, in effect order and, inside, their collections' order
/// (Effect::Collect). Sorted: no tag; Queued / Immediate: tagged with their atom (Effect::DrawAtom::atom), for
/// world_triangles::Submit's `only` at the atom's place among its effect's items. At the frame's turn fraction (the
/// draw's, manager::FrameInputs)
void Build(graphics::world_triangles::Frame& out, DrawPath path, const manager::FrameInputs& inputs);
/// One piece atom (an item of a Queued / Immediate effect whose creator is Kind::MeshPiece), tagged with its atom
void BuildAtom(graphics::world_triangles::Frame& out, const Effect::DrawAtom& atom);
} // namespace mesh_pieces
} // namespace openblack::psys
