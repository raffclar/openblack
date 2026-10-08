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

#include <string>
#include <vector>

#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

#include "Particles/PSys.h"

// ZR_SurfRevol: a create rule that makes one atom per collection carrying a textured surface of revolution (a mesh
// with rotating UVs): the teleport's vortex pool (SF_TeleportVortex, S_TileLandscape.raw) and the spell dispensers'
// discs. The renderer draws it with Graphics/RendererRevolvedSurface.cpp.
// Wiki: docs/bw1-notes/miracles.md, "SF_TeleportVortex and ZR_SurfRevol".

namespace openblack::psys
{

/// The profiles of FunctionIndex: Eval(t) -> (radius, height) for t in 0..1
enum class SurfProfile : int
{
	Disk = 0,        ///< r = t, y = 0 (also any other index)
	Funnel = 1,      ///< r = t, y = 3 (sqrt t - 1)
	FunnelSpout = 2, ///< r = 1.5 t, y = 3 (sqrt 2t - 1)
	FunnelParab = 3, ///< r = t, y = 3 (t^2 - 1)
};

/// The mesh the rule builds: NumU x NumV vertices, row by row
struct SurfMesh
{
	int numU {0};
	int numV {0};
	std::vector<glm::vec3> positions; ///< local, radius 1 at t = 1
	std::vector<glm::vec2> uvs;
	std::vector<uint32_t> colours;   ///< ARGB
	std::vector<uint32_t> speculars; ///< ARGB
	std::vector<uint16_t> indices;   ///< triangles
};

namespace manager
{
struct FrameInputs;
}

namespace surf_revol
{
/// Eval of the profile (radius, height)
[[nodiscard]] glm::vec2 Profile(int functionIndex, float t);

/// Vertex (i, j) at u = i / (NumU - 1), t = j / (NumV - 1): (r(t) cos 2 pi u, y(t), r(t) sin 2 pi u), uv (u, t).
/// With FadeAlphas: t < fadeIn -> RGB 255 t / fadeIn (alpha 255); t > 1 - fadeOut -> alpha 255 (1 - (t - (1 - fadeOut)) /
/// fadeOut) (RGB 255); the specular is the player colour x (1 - the RGB factor) with ChangeSpecColor. Without FadeAlphas
/// the colour is white and the specular 0. Triangles per row j and column i < NumU - 1: (b+U+i, b+i, b+U+1+i) and
/// (b+U+1+i, b+i, b+i+1), b = j U.
[[nodiscard]] SurfMesh Build(int numU, int numV, int functionIndex, bool fadeAlphas, float fadeIn, float fadeOut,
                             bool changeSpecColour, uint32_t playerColour);
/// The UVs x (TextureWidth / 256, TextureHeight / 256)
void ScaleUVs(SurfMesh& mesh, float u, float v);
/// u += (1 - t_j)^2 x MaxUVChange x amount (from the saved UVs)
void TwistUVs(SurfMesh& mesh, const std::vector<glm::vec2>& original, float maxUVChange, float amount);
/// Each row turned about Y by t_j x MaxVertexChange x amount (from the saved positions)
void TwistVertices(SurfMesh& mesh, const std::vector<glm::vec3>& original, float maxVertexChange, float amount);
/// The player's 3D colour from the player colour table, alpha 0xFF; the neutral player's entry is 0xFF000000
[[nodiscard]] uint32_t PlayerColour(int player);

/// One surface to draw this frame, in world space (draped over the land when the rule asks for it)
struct Surface
{
	std::string texture;      ///< the .raw base name ("S_TileLandscape")
	bool additive {false};    ///< UseAdditiveAlpha
	bool writeDepth {false};  ///< MaterialUpdateZBuffer
	bool doubleSided {false}; ///< MaterialSetDoubleSided
	struct Vertex
	{
		glm::vec3 position;
		glm::vec2 uv;
		uint32_t abgr;     ///< the diffuse (vertex colour x the atom's colour and alpha)
		uint32_t specular; ///< ABGR, alpha = the diffuse alpha
	};
	std::vector<Vertex> vertices;
	std::vector<uint16_t> indices;
	/// The effect's origin: the key of a Queued effect's single Z object, which the surface is drawn inside
	glm::vec3 origin {0.0f};
	/// Its effect's draw path. The surface's draw never queues itself on its own (the specular branch, see
	/// RendererRevolvedSurface.cpp): Sorted, drawn at once when its effect is drawn, unsorted; Queued / Immediate, at its
	/// place in its effect's items (manager::OrderedEffect, matched by `atom`)
	DrawPath path {DrawPath::Sorted};
	uint32_t effect {0};        ///< the effect's id
	const Atom* atom {nullptr}; ///< the ZR_SurfRevol atom
};
/// The creators of the effects that are gone go with them (once a frame, Renderer::PreDraw, before Collect)
void PruneCreators();
/// Every ZR_SurfRevol atom of the running effects (reads only), at the frame's turn fraction (the draw's)
[[nodiscard]] std::vector<Surface> Collect(const manager::FrameInputs& inputs);
} // namespace surf_revol

} // namespace openblack::psys
