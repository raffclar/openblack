/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <array>
#include <memory>
#include <string>
#include <vector>

#include <entt/core/fwd.hpp>
#include <entt/entity/entity.hpp>
#include <glm/mat4x4.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

#include "3D/L3DSubMesh.h"
#include "Graphics/Haze.h"
#include "Graphics/WorldTriangles.h"

namespace openblack::ecs::physics
{
/// A building copied into world-space triangles that a rock can knock pieces out of.
/// Every LOD 0 primitive keeps its material; the triangles know their edge neighbours and a size class (how many
/// times they may still be halved).
class FragMesh
{
public:
	struct Vertex
	{
		glm::vec3 pos;
		glm::vec2 uv;
	};
	struct Triangle
	{
		int group {-1};
		std::array<Vertex, 3> v;
		std::array<int, 3> neighbour {-1, -1, -1}; ///< edge k = (v[k], v[k+1]); -1 = open edge (a side wall is drawn)
		int sizeClass {0};
		int sizeClassRef {0};
	};
	struct Primitive
	{
		graphics::L3DSubMesh::Primitive material;
		std::vector<Triangle> triangles;
		/// The primitive of the building's own mesh it was copied from: its material (texture, mode, flag bits,
		/// ALPHAREF) for the draw
		graphics::world_triangles::MaterialRef source;
	};
	/// A piece that broke off, centred on its centroid, and how it starts flying.
	struct Piece
	{
		std::shared_ptr<FragMesh> mesh;
		glm::vec3 centre;
		glm::vec3 velocity;
		glm::vec3 angularVelocity;
		size_t lifeTriangles {0}; ///< the triangles when the piece was made (its lifetime: 100 turns each)
	};

	/// The LOD 0 sub-meshes of the object's mesh through its world matrix.
	static std::shared_ptr<FragMesh> FromEntity(entt::entity entity);

	/// An infinite cylinder along vel through pos of radius r knocks triangles out (one
	/// flying piece per primitive, split into connected pieces), then everything left without ground contact falls.
	std::vector<Piece> Impact(glm::vec3 pos, glm::vec3 vel, float radius);
	/// Pieces connected by shared edges; with groundCheck the ones touching the
	/// landscape (y < ground + 0.1) stay, otherwise only group 0 stays. Lone triangles vanish.
	std::vector<Piece> SplitUnconnectedGroups(bool groundCheck, glm::vec3 offset);
	/// The triangles still counting / the original count, 0..1.
	float GetRemaining();
	/// The size class of a triangle: how many times it may still be halved
	[[nodiscard]] static int SizeClassOf(const Triangle& triangle);
	[[nodiscard]] float Remaining() const { return _remaining; }
	[[nodiscard]] glm::vec3 Centroid() const;
	void Translate(glm::vec3 offset);
	[[nodiscard]] float Area() const;
	[[nodiscard]] size_t TriangleCount() const;
	/// The triangles' distinct vertices (exact match), each with the normal of the
	/// first triangle it was found in.
	void UniqueVertices(std::vector<glm::vec3>& positions, std::vector<glm::vec3>& normals) const;
	/// Adds a landed piece back as rubble when it comes to rest, its triangles through the matrix.
	void Merge(const FragMesh& piece, const glm::mat4& transform);
	/// The same geometry as AppendDraw as an L3D mesh, in the space given by worldToLocal, plus `extra` (the partly
	/// built draw). Its FragMesh primitives are cpuDrawn: the main view draws AppendDraw's instead and this mesh serves
	/// the other views (reflections), the picking and the bounds. Registers it in the mesh cache; returns its id.
	[[nodiscard]] entt::id_type BuildMesh(const glm::mat4& worldToLocal, const std::string& name,
	                                      std::vector<graphics::L3DSubMesh::GeneratedPrimitive> extra = {}) const;

	/// What every FragMesh draw of a frame shares: the light position, the ambient, this frame's haze and
	/// the view it is measured in, and whether the land light is loaded (without it, white and unlit, as vs_object)
	struct FrameLight
	{
		bool lit {false};
		glm::vec3 light {0.0f};
		int ambient {90};
		graphics::haze::Params haze {};
		glm::mat4 view {1.0f};
	};
	/// The lighting of one draw, the same for every primitive
	struct DrawLight
	{
		bool lit {false};
		glm::vec3 direction {0.0f};    ///< normalize(light - position), in the world
		uint32_t colour {0xFFFFFFFFu}; ///< the land light under the position, tinted and hazed (ARGB)
		uint32_t specular {0};         ///< its specular, + the tint's and the haze's
		int ambient {90};
	};
	/// L = (light - position) per axis, times InverseSquareRoot((y y + z z) + x x)
	[[nodiscard]] static glm::vec3 LightDirection(glm::vec3 light, glm::vec3 position);
	/// The land light at the position (land_light::At); unless the tint pair is 0xFFFFFFFF / 0 the colour times the
	/// tint (c t) >> 8 in the 4 channels and the specular + the tint's, saturated in the 4; then the haze at the
	/// position
	[[nodiscard]] static DrawLight ObjectLight(glm::vec3 position, uint32_t tint, uint32_t tintSpecular,
	                                           const FrameLight& frame);
	/// One triangle of the draw, pass 0. With a matrix its corners go through it first,
	/// ((z r2 + y r1) + r0 x) + t and the like; the face normal is (v1 - v0) x (v2 - v0)
	/// normalised with InverseSquareRoot ((x x + y y) + z z), lit once
	/// (model_light::TwoSided of (l.z n.z + l.y n.y) + l.x n.x). Out come the front (v0, v1, v2) in the
	/// front colour, the back copy 0.45 behind along -n in the back colour and drawn
	/// (b2, b1, b0), and for each edge k with no neighbour
	/// the wall (fk, bk, fk+1), (fk+1, bk, bk+1) on those same vertices. Both
	/// copies keep the triangle's uv and take the specular
	static void AppendTriangle(std::vector<graphics::world_triangles::Vertex>& out, const Triangle& triangle,
	                           const glm::mat4* matrix, const DrawLight& light);
	/// The triangles of each primitive in its source material, lit and in the world. The original flushes them
	/// every 256 vertices or triangles: one batch per primitive here, the same triangles in order.
	/// Not ported: its second pass, the snow over the mesh (from the snow map)
	void AppendDraw(graphics::world_triangles::Frame& out, const glm::mat4* matrix, const DrawLight& light) const;

private:
	void ComputeAdjacency(Primitive& primitive) const;

	std::vector<Primitive> _primitives;
	int _originalTriangleCount {0};
	float _remaining {1.0f};
};
} // namespace openblack::ecs::physics
