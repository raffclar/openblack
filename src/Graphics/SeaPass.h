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

#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>

#include "Graphics/RenderModes.h"
#include "Graphics/RenderPass.h"

/// The pass under the sea of the land draw, CPU side; the GPU twin is assets/shaders/sea_plane.sh and the two must stay
/// the same. Wiki: docs/bw1-notes/rendering-objects.md, "La pasada bajo el mar".
///
/// The original draws, before the sea and into the frame itself, three kinds of things:
/// - A, the mirrored land: the height unit 0.67 times -1.0, the land blocks swap their indices, the light table >> 1,
///   no small bump and no Z write;
/// - B, an object's under-water draw (static, animated and complex objects alike): the object mirrored in y = 0,
///   clipped against the user plane on the mirrored point (d > 0 out), in a constant colour and specular set before
///   the call;
/// - C, an object's cut-by-plane draw: NOT mirrored, clipped on the point itself (d < 0 out), lit 90 + 165 I >> 8 in
///   the object's colour with the light in the object's space.
/// B and C share one CPU clipper, against the one user plane (world and view).
///
/// openblack draws that pass into the reflection target with a mirrored camera (ReflectionXZCamera) instead, which
/// already gives B's mirror; what C draws there is mirrored back ("unmirror"), and the plane is a fragment discard on
/// the REAL world y (fs_object). Every rule here only says which of those three things a draw is and what follows.
namespace openblack::graphics::sea_pass
{

/// The user plane read on the real world y: which side of y = 0 a draw keeps
enum class SeaPlane : int8_t
{
	None = 0,      ///< not a sea draw: no discard
	KeepAbove = 1, ///< y >= 0 (the default plane, with B and with C)
	KeepBelow = -1 ///< y <= 0 (C with a plane (0, -1, 0, 0): the shark's, the net's, the swimmers')
};

/// The plane set at start-up (world and view: y = 1, the rest 0), and the one every user puts back
inline constexpr glm::vec4 k_DefaultPlane {0.0f, 1.0f, 0.0f, 0.0f};
/// The swimmers' plane, set by the land draw before their loop
inline constexpr glm::vec4 k_SwimPlane {0.0f, -1.0f, 0.0f, 0.0f};
/// The net's own plane, then the default plane again. It is set before the swimmers' loop: not k_SwimPlane's write,
/// the same values
inline constexpr glm::vec4 k_NetPlane {0.0f, -1.0f, 0.0f, 0.0f};
/// The shark's own plane under the water, then the default plane again. It is set before the swimmers' loop: not
/// k_SwimPlane's write, the same values
inline constexpr glm::vec4 k_SharkPlane {0.0f, -1.0f, 0.0f, 0.0f};

/// The two object draws of the pass
enum class Mechanism : uint8_t
{
	UnderWater, ///< B: the plane tested on the mirrored point, d > 0 out
	CutByPlane  ///< C: the plane tested on the point, d < 0 out
};

/// The side a plane (0, b, 0, 0) keeps on the real y. B: d = b (-y) > 0 is out, so b > 0 keeps y >= 0; C: d = b y < 0
/// is out, so b > 0 keeps y >= 0 too: the mirror and the opposite test cancel. Both compare d with 0. (openblack) a
/// plane with x, z or w != 0 is not one any caller of the pass sets: None
[[nodiscard]] constexpr SeaPlane Kept([[maybe_unused]] Mechanism mechanism, glm::vec4 plane)
{
	if (plane.x != 0.0f || plane.z != 0.0f || plane.w != 0.0f || plane.y == 0.0f)
	{
		return SeaPlane::None;
	}
	return plane.y > 0.0f ? SeaPlane::KeepAbove : SeaPlane::KeepBelow;
}

/// CPU twin of sea_plane.sh SeaPlaneDiscard: whether a point at the real worldY stays. (approximate) strict per pixel,
/// y = 0 is kept by both sides; the original clips whole triangles
[[nodiscard]] constexpr bool KeptAt(SeaPlane plane, float worldY)
{
	switch (plane)
	{
	case SeaPlane::KeepAbove:
		return !(worldY < 0.0f);
	case SeaPlane::KeepBelow:
		return !(worldY > 0.0f);
	case SeaPlane::None:
		break;
	}
	return true;
}

/// What a sea draw lights with: one rule per mechanism, never mixed
enum class SeaLight : uint8_t
{
	Normal,   ///< not a sea draw: the model light of the normal Draw
	Constant, ///< B: the object's constant colour and specular, no vertex light. The hand sets both; the boat sets
	          ///< only the colour (0xFF303070) and leaves the specular as its hull's last Draw left it, so its
	          ///< specular 0 is (inferred)
	LastDraw, ///< B: the colour and specular as the last Draw left them, the land light and cell specular (the
	          ///< physics objects' draw), no vertex light, no haze
	Cut       ///< C: 90 + 165 I >> 8 in the object's colour, + its specular
};

/// One sea draw: the light, the plane and whether it is mirrored back
struct SeaDraw
{
	SeaLight light {SeaLight::Normal};
	SeaPlane plane {SeaPlane::None};
	/// a cut-by-plane draw (never mirrored) inside openblack's mirrored Reflection pass
	bool unmirror {false};
	uint32_t argb {0xFFFFFFFFu}; ///< Constant / Cut: the object's colour, 0xAARRGGBB
	uint32_t specular {0u};      ///< Constant / Cut: the object's specular
	/// Cut: each instance's own colour instead of argb (the PSys mesh atoms set their colour and specular before the
	/// cut draw)
	bool perInstanceColour {false};
};

/// B, a constant colour: KeepAbove with the default plane, mirrored by the pass
[[nodiscard]] constexpr SeaDraw UnderWater(uint32_t argb, uint32_t specular)
{
	return {.light = SeaLight::Constant,
	        .plane = Kept(Mechanism::UnderWater, k_DefaultPlane),
	        .unmirror = false,
	        .argb = argb,
	        .specular = specular};
}
/// B, in the colour the last Draw left (the physics objects, the held object)
[[nodiscard]] constexpr SeaDraw UnderWaterLastDraw()
{
	return {.light = SeaLight::LastDraw, .plane = Kept(Mechanism::UnderWater, k_DefaultPlane), .unmirror = false};
}
/// C in a pass: mirrored back inside openblack's mirrored Reflection pass, as it is everywhere else
[[nodiscard]] constexpr SeaDraw Cut(SeaPlane plane, uint32_t argb, uint32_t specular, RenderPass pass)
{
	return {
	    .light = SeaLight::Cut, .plane = plane, .unmirror = pass == RenderPass::Reflection, .argb = argb, .specular = specular};
}

/// C for the PSys mesh atoms with DrawCutByPlane, in the pass's default plane (they draw in the model pass, after the
/// land draw put it back) and each in its own colour and specular. (approximate) the vertex alpha is 0xFF: the
/// original takes it from the alpha of the object's colour, which psys::mesh_atoms::Instance does not carry yet
[[nodiscard]] constexpr SeaDraw CutAtoms(RenderPass pass)
{
	auto draw = Cut(Kept(Mechanism::CutByPlane, k_DefaultPlane), 0xFFFFFFFFu, 0u, pass);
	draw.perInstanceColour = true;
	return draw;
}

/// The surfaces whose culling the pass decides
enum class Surface : uint8_t
{
	Model, ///< every object: the material's CULLMODE
	Sky,   ///< the sky dome, culled as a whole (inferred: the sky meshes' own modes)
	Land   ///< the land blocks
};

/// One pass's state: the land draw's set-up of the pass
struct SeaPassState
{
	bool mirrored;        ///< RenderPass::Reflection: openblack's mirrored camera (ReflectionXZCamera)
	float landLightScale; ///< 0.5: the land light table >> 1, else 1
	bool landWriteZ;      ///< false: ZWRITEENABLE 0 around the mirrored land
	bool smallBump;       ///< false: no small bump

	/// D3DRS_CULLMODE in one place. Model: the material's two-sided flag (the normal draw, B and C alike), flipped by
	/// the mirrored camera and flipped back by unmirror. Sky: the CCW of the dome, flipped by the mirrored camera. Land:
	/// openblack's blocks wind the other way (an openblack fact), so it starts from Cw; the original's mirrored land
	/// swaps its indices instead
	[[nodiscard]] constexpr render_modes::Cull FaceCull(Surface surface, bool twoSided, bool unmirror) const
	{
		switch (surface)
		{
		case Surface::Sky:
			return mirrored ? render_modes::Cull::Cw : render_modes::Cull::Ccw;
		case Surface::Land:
			return mirrored ? render_modes::Cull::Ccw : render_modes::Cull::Cw;
		case Surface::Model:
			break;
		}
		return render_modes::CullFor(twoSided, mirrored && !unmirror);
	}
};

/// The state of a pass: only RenderPass::Reflection is the pass under the sea
[[nodiscard]] constexpr SeaPassState ForPass(RenderPass pass)
{
	const bool reflection = pass == RenderPass::Reflection;
	return {.mirrored = reflection,
	        .landLightScale = reflection ? 0.5f : 1.0f,
	        .landWriteZ = !reflection,
	        .smallBump = !reflection};
}

/// u_objectClip (sea_plane.sh): x the plane (1 KeepAbove, -1 KeepBelow, 0 None), y 1 = unmirror
[[nodiscard]] constexpr glm::vec4 PackClip(const SeaDraw& draw)
{
	return {static_cast<float>(static_cast<int8_t>(draw.plane)), draw.unmirror ? 1.0f : 0.0f, 0.0f, 0.0f};
}
/// u_objectClip of a draw that is not a sea draw (the sky, the mesh viewer): no plane, no unmirror
inline constexpr glm::vec4 k_NoClip {0.0f, 0.0f, 0.0f, 0.0f};

/// A point drawn through the mirrored camera back to where the original draws it: (x, -y, z), the under-water draw's
/// mirror undone
[[nodiscard]] constexpr glm::vec3 Unmirror(glm::vec3 p)
{
	return {p.x, -p.y, p.z};
}
/// A view matrix of the mirrored camera times diag(1, -1, 1, 1): the main view (ReflectionXZCamera::GetViewMatrix), so
/// what it draws lands where Unmirror puts it (the moon's under-water draw)
[[nodiscard]] inline glm::mat4 UnmirrorView(const glm::mat4& view)
{
	glm::mat4 result = view;
	result[1] = -view[1];
	return result;
}

// The constant colours and speculars of the pass, set on the object before the draw
inline constexpr uint32_t k_HandColour = 0x65A0A0A0u; ///< the hand
inline constexpr uint32_t k_HandSpecular = 0u;
inline constexpr uint32_t k_CreatureColour = 0x65A0A0D0u; ///< the creature
inline constexpr uint32_t k_CreatureSpecular = 0x30u;
inline constexpr uint32_t k_SwimmerColour = 0xFF303070u; ///< the swimming SuperVillagers
inline constexpr uint32_t k_SwimmerSpecular = 0u;
// the boat's 0xFF303070 stays ecs::missionary_boat::k_ReflectionColour, the shark's in components::CutByPlane::belowColour

/// The land draw's creature test before its under-water draw: its land block visible, the block's camera distance
/// <= 100000, its y < 6 and one more value of the creature < 0.2 (inferred: what the block's distance and that value
/// mean)
inline constexpr float k_CreatureMaxBlockDistance = 100000.0f;
inline constexpr float k_CreatureMaxY = 6.0f;
inline constexpr float k_CreatureMaxA0 = 0.2f;

} // namespace openblack::graphics::sea_pass
